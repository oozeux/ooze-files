#include "core/CancelToken.h"
#include "core/PathUtils.h"
#include "vfs/LocalVfs.h"

#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

namespace {
QVector<VfsEntry> listAll(LocalVfs &vfs, const QUrl &url, VfsError *error, CancelToken &token)
{
    QVector<VfsEntry> collected;
    vfs.list(url,
             [&](QVector<VfsEntry> &&batch, const VfsError &batchError) {
                 if (batch.isEmpty()) {
                     if (error)
                         *error = batchError;
                 } else {
                     collected += batch;
                 }
             },
             token);
    return collected;
}

const VfsEntry *findByName(const QVector<VfsEntry> &entries, const QString &name)
{
    for (const VfsEntry &entry : entries) {
        if (entry.displayName == name)
            return &entry;
    }
    return nullptr;
}
}

class TestLocalVfs : public QObject
{
    Q_OBJECT

private slots:
    void listsDirectory();
    void marksHiddenEntries();
    void statFillsMetadata();
    void linkIconsFollowTargetType();
    void missingDirectoryReportsNotFound();
    void cancelledTokenAbortsListing();
};

void TestLocalVfs::listsDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QFile file(temp.filePath(QStringLiteral("archivo.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("abc");
    file.close();

    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("carpeta")));

    LocalVfs vfs;
    CancelToken token;
    VfsError error;
    const QVector<VfsEntry> entries = listAll(vfs, QUrl::fromLocalFile(temp.path()), &error, token);

    QVERIFY2(!error.isValid(), qPrintable(error.message));
    QCOMPARE(entries.size(), 2);

    const VfsEntry *dir = findByName(entries, QStringLiteral("carpeta"));
    QVERIFY(dir);
    QCOMPARE(dir->kind, EntryKind::Directory);

    const VfsEntry *rest = findByName(entries, QStringLiteral("archivo.txt"));
    QVERIFY(rest);
    QCOMPARE(rest->kind, EntryKind::File);
    QVERIFY(rest->url.isLocalFile());
}

void TestLocalVfs::marksHiddenEntries()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QFile hidden(temp.filePath(QStringLiteral(".oculto")));
    QVERIFY(hidden.open(QIODevice::WriteOnly));
    hidden.close();

    LocalVfs vfs;
    CancelToken token;
    VfsError error;
    const QVector<VfsEntry> entries = listAll(vfs, QUrl::fromLocalFile(temp.path()), &error, token);

    QCOMPARE(entries.size(), 1);
    QVERIFY(hasFlag(entries.first(), FlagHidden));
}

void TestLocalVfs::statFillsMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString path = temp.filePath(QStringLiteral("datos.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(42, 'x'));
    file.close();

    LocalVfs vfs;
    VfsError error;
    const VfsEntry entry = vfs.stat(QUrl::fromLocalFile(path), &error);

    QVERIFY2(!error.isValid(), qPrintable(error.message));
    QCOMPARE(entry.size, 42);
    QVERIFY(entry.modified.isValid());
    QVERIFY(!entry.mimeType.isEmpty());
}

void TestLocalVfs::linkIconsFollowTargetType()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QFile file(temp.filePath(QStringLiteral("archivo.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("abc");
    file.close();
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("carpeta")));
    QVERIFY(QFile::link(temp.filePath(QStringLiteral("archivo.txt")),
                        temp.filePath(QStringLiteral("enlace.txt"))));
    QVERIFY(QFile::link(temp.filePath(QStringLiteral("carpeta")),
                        temp.filePath(QStringLiteral("enlace-carpeta"))));

    LocalVfs vfs;
    CancelToken token;
    VfsError error;
    const QVector<VfsEntry> entries = listAll(
        vfs, QUrl::fromLocalFile(temp.path()), &error, token);

    QVERIFY2(!error.isValid(), qPrintable(error.message));

    const VfsEntry *fileLink = findByName(entries, QStringLiteral("enlace.txt"));
    QVERIFY(fileLink);
    QCOMPARE(fileLink->kind, EntryKind::Symlink);
    QCOMPARE(fileLink->iconName, QStringLiteral("text-x-generic"));
    QVERIFY(!hasFlag(*fileLink, FlagSymlinkDir));

    const VfsEntry *dirLink = findByName(entries, QStringLiteral("enlace-carpeta"));
    QVERIFY(dirLink);
    QCOMPARE(dirLink->kind, EntryKind::Symlink);
    QCOMPARE(dirLink->iconName, QStringLiteral("folder-symlink"));
    QVERIFY(hasFlag(*dirLink, FlagSymlinkDir));

    const VfsEntry stat = vfs.stat(QUrl::fromLocalFile(
        temp.filePath(QStringLiteral("enlace-carpeta"))), &error);
    QVERIFY2(!error.isValid(), qPrintable(error.message));
    QVERIFY(hasFlag(stat, FlagSymlinkDir));
    QCOMPARE(stat.size, -1);
}

void TestLocalVfs::missingDirectoryReportsNotFound()
{
    LocalVfs vfs;
    CancelToken token;
    VfsError error;

    const QVector<VfsEntry> entries = listAll(
        vfs, QUrl::fromLocalFile(QStringLiteral("/nonexistent/ooze-files-test")), &error, token);

    QVERIFY(entries.isEmpty());
    QCOMPARE(error.code, VfsError::NotFound);
}

void TestLocalVfs::cancelledTokenAbortsListing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("sub")));

    LocalVfs vfs;
    CancelToken token;
    token.cancel();

    VfsError error;
    const QVector<VfsEntry> entries = listAll(vfs, QUrl::fromLocalFile(temp.path()), &error, token);

    QCOMPARE(error.code, VfsError::Cancelled);
    QVERIFY(entries.isEmpty());
}

QTEST_MAIN(TestLocalVfs)
#include "tst_localfs.moc"
