#include "core/FileOperations.h"
#include "core/PathUtils.h"
#include "core/TrashService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

#include <algorithm>

namespace {
void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(content);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void addTrashed(const QString &trashRoot, const QString &name, const QByteArray &content,
                const QString &original, const QString &date = QStringLiteral("2026-03-01T10:20:30"))
{
    writeFile(trashRoot + QStringLiteral("/files/") + name, content);
    const QByteArray info = "[Trash Info]\nPath=" + QUrl::toPercentEncoding(original, "/")
        + "\nDeletionDate=" + date.toLatin1() + "\n";
    writeFile(trashRoot + QStringLiteral("/info/") + name + QStringLiteral(".trashinfo"), info);
}

void addTrashedDirectory(const QString &trashRoot, const QString &name, const QString &original)
{
    QDir().mkpath(trashRoot + QStringLiteral("/files/") + name);
    const QByteArray info = "[Trash Info]\nPath=" + QUrl::toPercentEncoding(original, "/")
        + "\nDeletionDate=2026-03-01T10:20:30\n";
    writeFile(trashRoot + QStringLiteral("/info/") + name + QStringLiteral(".trashinfo"), info);
}

QList<QVariant> finish(QSignalSpy &spy)
{
    if (spy.isEmpty())
        spy.wait(5000);
    return spy.isEmpty() ? QList<QVariant>() : spy.takeFirst();
}
}

class TestTrash : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_temp;
    QString m_trash;

    void resetTrash()
    {
        QDir(m_trash).removeRecursively();
        QDir().mkpath(m_trash + QStringLiteral("/files"));
        QDir().mkpath(m_trash + QStringLiteral("/info"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_temp.isValid());
        qputenv("OOZE_FILES_TRASH_HOME_ONLY", "1");
        qputenv("XDG_DATA_HOME", m_temp.filePath(QStringLiteral("data")).toUtf8());
        qputenv("HOME", m_temp.filePath(QStringLiteral("home")).toUtf8());
        m_trash = m_temp.filePath(QStringLiteral("data/Trash"));
    }

    void init() { resetTrash(); }

    void readsInfoAndOrphans();
    void relativePathsOnlyForMountTrash();
    void discoversHomeTrashAndLocatesTopLevelOnly();
    void virtualUrlsRoundTripAndRejectTraversal();
    void forgetRemovesInfoAndDirectorySizes();
    void removeItemHandlesReadOnlyDirectories();
    void purgeOrphansRemovesDanglingInfo();
    void restoreMovesBackAndCleansMetadata();
    void restoreRecreatesMissingFolder();
    void restoreCanFallBackToHome();
    void restoreAsksOnNameConflict();
    void restoreRejectsPathsOutsideTheTrash();
    void deleteAndEmptyRemoveEverything();
};

void TestTrash::readsInfoAndOrphans()
{
    addTrashed(m_trash, QStringLiteral("a.txt"), "x", QStringLiteral("/home/u/Docs/a b.txt"));
    writeFile(m_trash + QStringLiteral("/files/huerfano.bin"), "y");

    const TrashDir dir{QFile::encodeName(m_trash), {}};
    QVector<TrashItem> items = TrashService::items(dir);
    QCOMPARE(items.size(), 2);
    std::sort(items.begin(), items.end(), [](const TrashItem &a, const TrashItem &b) { return a.name < b.name; });

    QCOMPARE(items.at(0).name, QByteArray("a.txt"));
    QVERIFY(items.at(0).hasInfo);
    QCOMPARE(items.at(0).originalPath, QByteArray("/home/u/Docs/a b.txt"));
    QCOMPARE(items.at(0).displayBytes(), QByteArray("a b.txt"));
    QCOMPARE(items.at(0).deleted, QDateTime::fromString(QStringLiteral("2026-03-01T10:20:30"), Qt::ISODate));

    QCOMPARE(items.at(1).name, QByteArray("huerfano.bin"));
    QVERIFY(!items.at(1).hasInfo);
    QCOMPARE(items.at(1).displayBytes(), QByteArray("huerfano.bin"));
    QVERIFY(items.at(1).originalPath.isEmpty());
}

void TestTrash::relativePathsOnlyForMountTrash()
{
    const QString top = m_temp.filePath(QStringLiteral("mnt"));
    const QString mountTrash = top + QStringLiteral("/.Trash-1000");
    QDir().mkpath(mountTrash + QStringLiteral("/files"));
    QDir().mkpath(mountTrash + QStringLiteral("/info"));

    writeFile(mountTrash + QStringLiteral("/files/x.txt"), "x");
    writeFile(mountTrash + QStringLiteral("/info/x.txt.trashinfo"),
              "[Trash Info]\nPath=docs/x.txt\nDeletionDate=2026-01-01T00:00:00\n");
    writeFile(mountTrash + QStringLiteral("/files/malo.txt"), "x");
    writeFile(mountTrash + QStringLiteral("/info/malo.txt.trashinfo"),
              "[Trash Info]\nPath=../../etc/passwd\nDeletionDate=2026-01-01T00:00:00\n");

    const TrashDir mountDir{QFile::encodeName(mountTrash), QFile::encodeName(top)};
    const QVector<TrashItem> items = TrashService::items(mountDir);
    QCOMPARE(items.size(), 2);
    for (const TrashItem &item : items) {
        if (item.name == "x.txt") {
            QVERIFY(item.hasInfo);
            QCOMPARE(item.originalPath, QFile::encodeName(top) + "/docs/x.txt");
        } else {
            QVERIFY(!item.hasInfo);
            QVERIFY(item.originalPath.isEmpty());
        }
    }

    addTrashed(m_trash, QStringLiteral("rel.txt"), "x", QStringLiteral("relativo/rel.txt"));
    const TrashDir homeDir{QFile::encodeName(m_trash), {}};
    const QVector<TrashItem> homeItems = TrashService::items(homeDir);
    QCOMPARE(homeItems.size(), 1);
    QVERIFY(!homeItems.first().hasInfo);
}

void TestTrash::discoversHomeTrashAndLocatesTopLevelOnly()
{
    addTrashedDirectory(m_trash, QStringLiteral("d"), QStringLiteral("/home/u/d"));
    QDir().mkpath(m_trash + QStringLiteral("/files/d/sub"));
    writeFile(m_trash + QStringLiteral("/files/d/sub/f.txt"), "f");

    const QVector<TrashDir> dirs = TrashService::discoverDirs();
    QVERIFY(!dirs.isEmpty());
    QCOMPARE(dirs.first().root, QFile::encodeName(m_trash));

    TrashItem item;
    QVERIFY(TrashService::locate(QFile::encodeName(m_trash + QStringLiteral("/files/d")), &item));
    QCOMPARE(item.name, QByteArray("d"));
    QCOMPARE(item.originalPath, QByteArray("/home/u/d"));

    QVERIFY(!TrashService::locate(QFile::encodeName(m_trash + QStringLiteral("/files/d/sub")), &item));
    QVERIFY(!TrashService::locate(QFile::encodeName(m_trash + QStringLiteral("/files/no-existe")), &item));
    QVERIFY(!TrashService::locate(QByteArray("/etc/passwd"), &item));
    QVERIFY(TrashService::hasItems());
}

void TestTrash::virtualUrlsRoundTripAndRejectTraversal()
{
    QDir().mkpath(m_trash + QStringLiteral("/files/Carpeta ñ/sub dir"));

    const TrashDir dir{QFile::encodeName(m_trash), {}};

    const QUrl nested = TrashService::virtualUrl(dir, QByteArray("Carpeta ñ/sub dir"));
    QCOMPARE(nested.scheme(), QStringLiteral("trash"));

    TrashService::Location location;
    QVERIFY(TrashService::resolve(nested, &location));
    QVERIFY(!location.root);
    QCOMPARE(location.relative, QByteArray("Carpeta ñ/sub dir"));
    QCOMPARE(location.realPath, QFile::encodeName(m_trash) + "/files/Carpeta ñ/sub dir");

    QCOMPARE(TrashService::virtualUrlForPath(location.realPath), nested);

    QVERIFY(TrashService::resolve(TrashService::rootUrl(), &location));
    QVERIFY(location.root);

    QVERIFY(!TrashService::resolve(QUrl::fromEncoded("trash:///" + dir.root.toHex() + "/../etc"), &location));
    QVERIFY(!TrashService::resolve(QUrl::fromEncoded("trash:///" + dir.root.toHex() + "/a/%2E%2E/b"), &location));
    QVERIFY(!TrashService::resolve(QUrl::fromEncoded("trash:///" + QByteArray("/tmp").toHex() + "/x"), &location));
    QVERIFY(!TrashService::resolve(QUrl(QStringLiteral("file:///tmp")), &location));

    QCOMPARE(PathUtils::parent(nested), TrashService::virtualUrl(dir, QByteArray("Carpeta ñ")));
    QCOMPARE(PathUtils::parent(TrashService::virtualUrl(dir, QByteArray("Carpeta ñ"))), TrashService::rootUrl());
    QCOMPARE(PathUtils::parent(TrashService::rootUrl()), TrashService::rootUrl());
    QCOMPARE(PathUtils::breadcrumb(nested).size(), 3);
}

void TestTrash::forgetRemovesInfoAndDirectorySizes()
{
    addTrashed(m_trash, QStringLiteral("a.txt"), "x", QStringLiteral("/home/u/a.txt"));
    addTrashed(m_trash, QStringLiteral("b c"), "x", QStringLiteral("/home/u/b c"));
    writeFile(m_trash + QStringLiteral("/directorysizes"),
              "10 1700000000 a.txt\n20 1700000001 b%20c\n");

    const TrashDir dir{QFile::encodeName(m_trash), {}};
    QVector<TrashItem> items = TrashService::items(dir);
    QCOMPARE(items.size(), 2);

    for (const TrashItem &item : items) {
        if (item.name == "b c")
            QVERIFY(TrashService::forget(item));
    }

    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/info/b c.trashinfo")));
    QVERIFY(QFile::exists(m_trash + QStringLiteral("/info/a.txt.trashinfo")));
    QCOMPARE(readFile(m_trash + QStringLiteral("/directorysizes")), QByteArray("10 1700000000 a.txt\n"));
}

void TestTrash::removeItemHandlesReadOnlyDirectories()
{
    addTrashedDirectory(m_trash, QStringLiteral("d"), QStringLiteral("/home/u/d"));
    writeFile(m_trash + QStringLiteral("/files/d/sub/f.txt"), "f");
    ::chmod(QFile::encodeName(m_trash + QStringLiteral("/files/d/sub")).constData(), 0500);
    ::chmod(QFile::encodeName(m_trash + QStringLiteral("/files/d")).constData(), 0500);

    TrashItem item;
    QVERIFY(TrashService::locate(QFile::encodeName(m_trash + QStringLiteral("/files/d")), &item));
    QString error;
    QVERIFY2(TrashService::removeItem(item, &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(m_trash + QStringLiteral("/files/d")));
    QVERIFY(!QFileInfo::exists(m_trash + QStringLiteral("/info/d.trashinfo")));
}

void TestTrash::purgeOrphansRemovesDanglingInfo()
{
    addTrashed(m_trash, QStringLiteral("a.txt"), "x", QStringLiteral("/home/u/a.txt"));
    writeFile(m_trash + QStringLiteral("/info/fantasma.trashinfo"), "[Trash Info]\nPath=/x\n");

    const TrashDir dir{QFile::encodeName(m_trash), {}};
    QCOMPARE(TrashService::purgeOrphans(dir), 1);
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/info/fantasma.trashinfo")));
    QVERIFY(QFile::exists(m_trash + QStringLiteral("/info/a.txt.trashinfo")));
}

void TestTrash::restoreMovesBackAndCleansMetadata()
{
    const QString original = m_temp.filePath(QStringLiteral("home/docs/a.txt"));
    QDir().mkpath(QFileInfo(original).absolutePath());
    addTrashed(m_trash, QStringLiteral("a.txt"), "contenido", original);

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.restoreTrashItems({QUrl::fromLocalFile(m_trash + QStringLiteral("/files/a.txt"))});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(readFile(original), QByteArray("contenido"));
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/files/a.txt")));
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/info/a.txt.trashinfo")));
}

void TestTrash::restoreRecreatesMissingFolder()
{
    const QString original = m_temp.filePath(QStringLiteral("home/nueva/sub/b.txt"));
    QVERIFY(!QFileInfo::exists(QFileInfo(original).absolutePath()));
    addTrashed(m_trash, QStringLiteral("b.txt"), "b", original);

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.restoreTrashItems({QUrl::fromLocalFile(m_trash + QStringLiteral("/files/b.txt"))});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));
    QCOMPARE(readFile(original), QByteArray("b"));
}

void TestTrash::restoreCanFallBackToHome()
{
    const QString original = m_temp.filePath(QStringLiteral("desaparecida/c.txt"));
    addTrashed(m_trash, QStringLiteral("c.txt"), "c", original);
    QDir().mkpath(m_temp.filePath(QStringLiteral("home")));

    FileOperations operations;
    QVariantMap seen;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         seen = info;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictRestoreHome, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.restoreTrashItems({QUrl::fromLocalFile(m_trash + QStringLiteral("/files/c.txt"))});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(seen.value(QStringLiteral("kind")).toString(), QStringLiteral("missingFolder"));
    QCOMPARE(readFile(m_temp.filePath(QStringLiteral("home/c.txt"))), QByteArray("c"));
    QVERIFY(!QFileInfo::exists(m_temp.filePath(QStringLiteral("desaparecida"))));
}

void TestTrash::restoreAsksOnNameConflict()
{
    const QString original = m_temp.filePath(QStringLiteral("home/docs/d.txt"));
    writeFile(original, "actual");
    addTrashed(m_trash, QStringLiteral("d.txt"), "papelera", original);

    FileOperations operations;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictRename, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.restoreTrashItems({QUrl::fromLocalFile(m_trash + QStringLiteral("/files/d.txt"))});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(readFile(original), QByteArray("actual"));
    QCOMPARE(readFile(m_temp.filePath(QStringLiteral("home/docs/d (1).txt"))), QByteArray("papelera"));
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/info/d.txt.trashinfo")));
}

void TestTrash::restoreRejectsPathsOutsideTheTrash()
{
    const QString outside = m_temp.filePath(QStringLiteral("home/fuera.txt"));
    writeFile(outside, "x");

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.restoreTrashItems({QUrl::fromLocalFile(outside)});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY(!result.at(1).toBool());
    QVERIFY(QFile::exists(outside));

    QSignalSpy deleteSpy(&operations, &FileOperations::operationFinished);
    operations.deleteTrashItems({QUrl::fromLocalFile(outside)});
    const QList<QVariant> deleted = finish(deleteSpy);
    QVERIFY(!deleted.isEmpty());
    QVERIFY(!deleted.at(1).toBool());
    QVERIFY(QFile::exists(outside));
}

void TestTrash::deleteAndEmptyRemoveEverything()
{
    addTrashed(m_trash, QStringLiteral("a.txt"), "a", QStringLiteral("/home/u/a.txt"));
    addTrashed(m_trash, QStringLiteral("b.txt"), "b", QStringLiteral("/home/u/b.txt"));
    writeFile(m_trash + QStringLiteral("/info/fantasma.trashinfo"), "[Trash Info]\nPath=/x\n");
    writeFile(m_trash + QStringLiteral("/directorysizes"), "1 1 a.txt\n1 1 b.txt\n");

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.deleteTrashItems({QUrl::fromLocalFile(m_trash + QStringLiteral("/files/a.txt"))});
    QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/files/a.txt")));
    QVERIFY(!QFile::exists(m_trash + QStringLiteral("/info/a.txt.trashinfo")));
    QVERIFY(QFile::exists(m_trash + QStringLiteral("/files/b.txt")));

    operations.emptyTrash();
    result = finish(spy);
    if (result.isEmpty()) {
        spy.wait(5000);
        result = finish(spy);
    }
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QVERIFY(QDir(m_trash + QStringLiteral("/files")).isEmpty());
    QVERIFY(QDir(m_trash + QStringLiteral("/info")).isEmpty());
    QVERIFY(!TrashService::hasItems());
}

QTEST_MAIN(TestTrash)
#include "tst_trash.moc"
