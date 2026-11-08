#include "core/FileOperations.h"
#include "core/PathUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <fcntl.h>
#include <unistd.h>

namespace {
QList<QVariant> finish(QSignalSpy &spy)
{
    if (spy.isEmpty())
        spy.wait(5000);
    return spy.isEmpty() ? QList<QVariant>() : spy.takeFirst();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(content);
    file.close();
}

QStringList listNames(const QString &path)
{
    QStringList names = QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    names.sort();
    return names;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
}

class TestFileOperations : public QObject
{
    Q_OBJECT

private slots:
    void createsDirectoryAndFile();
    void copiesFilesAndDirectories();
    void movesFiles();
    void renamesAndRejectsInvalidTargets();
    void renamesBatchWithChainsAndCycles();
    void removesRecursively();
    void refusesToRemoveRoot();
    void suggestsUniqueNames();
    void resolvesArchiveTargetNames();
    void copiesByteExactNames();
    void conflictWithoutHandlerRenames();
    void conflictOverwriteReplacesFile();
    void conflictOverwriteMovesFile();
    void conflictOverwriteReplacesDirectory();
    void conflictMergeCombinesDirectories();
    void conflictMergeReplaceKeepsOtherFiles();
    void conflictCancelLeavesTargetUntouched();
    void conflictTypeMismatchOffersOnlyRename();
    void conflictApplyToAllAsksOnce();
    void uniqueNamesKeepTarExtension();
};

void TestFileOperations::resolvesArchiveTargetNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeFile(temp.filePath(QStringLiteral("nota.txt")), "x");
    writeFile(temp.filePath(QStringLiteral("backup.zip")), "x");

    const QVector<QUrl> sources{QUrl::fromLocalFile(temp.filePath(QStringLiteral("nota.txt")))};
    QCOMPARE(FileOperations::suggestedArchiveBase(sources), QStringLiteral("nota"));

    QString error;
    QCOMPARE(FileOperations::archiveTargetName(sources, QStringLiteral("salida"),
                                               QStringLiteral("zip"), &error),
             QStringLiteral("salida.zip"));
    QVERIFY(error.isEmpty());

    QCOMPARE(FileOperations::archiveTargetName(sources, QStringLiteral(" salida.ZIP "),
                                               QStringLiteral("zip")),
             QStringLiteral("salida.zip"));
    QCOMPARE(FileOperations::archiveTargetName(sources, QStringLiteral("backup"),
                                               QStringLiteral("zip")),
             QStringLiteral("backup (2).zip"));

    QVERIFY(FileOperations::archiveTargetName(sources, QStringLiteral("a/b"),
                                              QStringLiteral("zip"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(FileOperations::archiveTargetName(sources, QString(), QStringLiteral("zip")).isEmpty());
    QVERIFY(FileOperations::archiveTargetName(sources, QStringLiteral("x"),
                                              QStringLiteral("nope")).isEmpty());
    QVERIFY(FileOperations::archiveTargetName(sources, QString(QLatin1Char('a')).repeated(300),
                                              QStringLiteral("zip")).isEmpty());
}

void TestFileOperations::createsDirectoryAndFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    FileOperations operations;
    const QUrl directory = QUrl::fromLocalFile(temp.path());

    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.createDirectory(directory, QStringLiteral("carpeta"));
    const QList<QVariant> directoryResult = finish(spy);
    QVERIFY(!directoryResult.isEmpty());
    QCOMPARE(directoryResult.at(0).toInt(), int(FileOperations::CreateDirectory));
    QVERIFY(directoryResult.at(1).toBool());
    QCOMPARE(directoryResult.at(3).toString(), QStringLiteral("carpeta"));
    QVERIFY(directoryResult.at(4).toBool());
    QVERIFY(QDir(temp.path()).exists(QStringLiteral("carpeta")));

    operations.createFile(directory, QStringLiteral("archivo.txt"));
    const QList<QVariant> fileResult = finish(spy);
    QVERIFY(!fileResult.isEmpty());
    QCOMPARE(fileResult.at(0).toInt(), int(FileOperations::CreateFile));
    QVERIFY(fileResult.at(1).toBool());
    QVERIFY(QFile::exists(temp.filePath(QStringLiteral("archivo.txt"))));

    operations.createFile(directory, QStringLiteral("archivo.txt"));
    const QList<QVariant> duplicate = finish(spy);
    QVERIFY(!duplicate.isEmpty());
    QVERIFY(!duplicate.at(1).toBool());
}

void TestFileOperations::copiesFilesAndDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceDirectory = temp.filePath(QStringLiteral("origen"));
    const QString destinationDirectory = temp.filePath(QStringLiteral("destino"));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    writeFile(sourceDirectory + QStringLiteral("/datos.txt"), QByteArray("contenido"));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.copy({QUrl::fromLocalFile(sourceDirectory)},
                    QUrl::fromLocalFile(destinationDirectory));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.at(0).toInt(), int(FileOperations::Copy));
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    const QString copied = destinationDirectory + QStringLiteral("/origen/datos.txt");
    QCOMPARE(readFile(copied), QByteArray("contenido"));

    operations.copy({QUrl::fromLocalFile(copied)},
                    QUrl::fromLocalFile(destinationDirectory + QStringLiteral("/origen")));
    const QList<QVariant> duplicate = finish(spy);
    QVERIFY(!duplicate.isEmpty());
    QVERIFY(duplicate.at(1).toBool());
    QVERIFY(QFile::exists(destinationDirectory + QStringLiteral("/origen/datos (1).txt")));

    operations.copy({QUrl::fromLocalFile(sourceDirectory)},
                    QUrl::fromLocalFile(sourceDirectory + QStringLiteral("/sub")));
    const QList<QVariant> recursive = finish(spy);
    QVERIFY(!recursive.isEmpty());
    QVERIFY(!recursive.at(1).toBool());
}

void TestFileOperations::movesFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString first = temp.filePath(QStringLiteral("primero"));
    const QString second = temp.filePath(QStringLiteral("segundo"));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("primero")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("segundo")));
    writeFile(first + QStringLiteral("/mover.txt"), QByteArray("mover"));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.move({QUrl::fromLocalFile(first + QStringLiteral("/mover.txt"))},
                    QUrl::fromLocalFile(second));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.at(0).toInt(), int(FileOperations::Move));
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));
    QVERIFY(!QFile::exists(first + QStringLiteral("/mover.txt")));
    QCOMPARE(readFile(second + QStringLiteral("/mover.txt")), QByteArray("mover"));

    operations.move({QUrl::fromLocalFile(second + QStringLiteral("/mover.txt"))},
                    QUrl::fromLocalFile(second));
    const QList<QVariant> skipped = finish(spy);
    QVERIFY(!skipped.isEmpty());
    QVERIFY(skipped.at(1).toBool());
    QVERIFY(QFile::exists(second + QStringLiteral("/mover.txt")));
}

void TestFileOperations::renamesAndRejectsInvalidTargets()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString path = temp.filePath(QStringLiteral("antes.txt"));
    writeFile(path, QByteArray("x"));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.rename(QUrl::fromLocalFile(path), QStringLiteral("despues.txt"));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.at(0).toInt(), int(FileOperations::Rename));
    QVERIFY(result.at(1).toBool());
    QVERIFY(QFile::exists(temp.filePath(QStringLiteral("despues.txt"))));

    writeFile(temp.filePath(QStringLiteral("otro.txt")), QByteArray("y"));

    operations.rename(QUrl::fromLocalFile(temp.filePath(QStringLiteral("otro.txt"))),
                      QStringLiteral("despues.txt"));
    const QList<QVariant> conflict = finish(spy);
    QVERIFY(!conflict.isEmpty());
    QVERIFY(!conflict.at(1).toBool());
    QVERIFY(QFile::exists(temp.filePath(QStringLiteral("otro.txt"))));

    operations.rename(QUrl::fromLocalFile(temp.filePath(QStringLiteral("otro.txt"))),
                      QStringLiteral("sub/otro.txt"));
    const QList<QVariant> invalid = finish(spy);
    QVERIFY(!invalid.isEmpty());
    QVERIFY(!invalid.at(1).toBool());
    QVERIFY(QFile::exists(temp.filePath(QStringLiteral("otro.txt"))));
}

void TestFileOperations::renamesBatchWithChainsAndCycles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const auto url = [&temp](const QString &name) {
        return QUrl::fromLocalFile(temp.filePath(name));
    };

    writeFile(temp.filePath(QStringLiteral("a.txt")), QByteArray("A"));
    writeFile(temp.filePath(QStringLiteral("b.txt")), QByteArray("B"));
    writeFile(temp.filePath(QStringLiteral("c.txt")), QByteArray("C"));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    // Chain: a→b, b→c, c→d.
    operations.renameBatch({url(QStringLiteral("a.txt")), url(QStringLiteral("b.txt")),
                            url(QStringLiteral("c.txt"))},
                           {QStringLiteral("b.txt"), QStringLiteral("c.txt"),
                            QStringLiteral("d.txt")});
    const QList<QVariant> chain = finish(spy);
    QVERIFY(!chain.isEmpty());
    QVERIFY2(chain.at(1).toBool(), qPrintable(chain.at(2).toString()));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("b.txt"))), QByteArray("A"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("c.txt"))), QByteArray("B"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("d.txt"))), QByteArray("C"));
    QVERIFY(!QFile::exists(temp.filePath(QStringLiteral("a.txt"))));

    // Cycle: b↔c.
    operations.renameBatch({url(QStringLiteral("b.txt")), url(QStringLiteral("c.txt"))},
                           {QStringLiteral("c.txt"), QStringLiteral("b.txt")});
    const QList<QVariant> cycle = finish(spy);
    QVERIFY(!cycle.isEmpty());
    QVERIFY2(cycle.at(1).toBool(), qPrintable(cycle.at(2).toString()));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("b.txt"))), QByteArray("B"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("c.txt"))), QByteArray("A"));
}

void TestFileOperations::removesRecursively()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString target = temp.filePath(QStringLiteral("arbol"));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("arbol")));
    QVERIFY(QDir(target).mkdir(QStringLiteral("sub")));
    writeFile(target + QStringLiteral("/sub/archivo.txt"), QByteArray("x"));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.remove({QUrl::fromLocalFile(target)});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.at(0).toInt(), int(FileOperations::Remove));
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));
    QVERIFY(!QDir(target).exists());
}

void TestFileOperations::refusesToRemoveRoot()
{
    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);

    operations.remove({QUrl::fromLocalFile(QStringLiteral("/"))});
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY(!result.at(1).toBool());
    QVERIFY(QDir(QStringLiteral("/")).exists());
}

void TestFileOperations::suggestsUniqueNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("Nueva carpeta")));

    const QUrl directory = QUrl::fromLocalFile(temp.path());
    QCOMPARE(FileOperations::suggestedName(directory, QStringLiteral("Nueva carpeta"), true),
             QStringLiteral("Nueva carpeta (1)"));
    QCOMPARE(FileOperations::suggestedName(directory, QStringLiteral("Nuevo archivo"), false),
             QStringLiteral("Nuevo archivo"));
}

void TestFileOperations::conflictWithoutHandlerRenames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    writeFile(temp.filePath(QStringLiteral("origen/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/a.txt")), "viejo");

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/a.txt")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a.txt"))), QByteArray("viejo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a (1).txt"))), QByteArray("nuevo"));
}

void TestFileOperations::conflictOverwriteReplacesFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    writeFile(temp.filePath(QStringLiteral("origen/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/a.txt")), "viejo");

    FileOperations operations;
    QVariantMap seen;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         seen = info;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictOverwrite, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/a.txt")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QVERIFY(seen.value(QStringLiteral("canOverwrite")).toBool());
    QVERIFY(!seen.value(QStringLiteral("canMerge")).toBool());
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a.txt"))), QByteArray("nuevo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("origen/a.txt"))), QByteArray("nuevo"));
    const QStringList names = listNames(temp.filePath(QStringLiteral("destino")));
    QVERIFY(names.contains(QStringLiteral("a.txt")));
    for (const QString &name : names)
        QVERIFY2(!name.startsWith(QStringLiteral(".ooze-tmp-")), qPrintable(name));
}

void TestFileOperations::conflictOverwriteMovesFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    writeFile(temp.filePath(QStringLiteral("origen/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/a.txt")), "viejo");

    FileOperations operations;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictOverwrite, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.move({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/a.txt")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QVERIFY(!QFile::exists(temp.filePath(QStringLiteral("origen/a.txt"))));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a.txt"))), QByteArray("nuevo"));
    QCOMPARE(listNames(temp.filePath(QStringLiteral("destino"))), QStringList{QStringLiteral("a.txt")});
}

void TestFileOperations::conflictOverwriteReplacesDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("origen/d")));
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("destino/d")));
    writeFile(temp.filePath(QStringLiteral("origen/d/nuevo.txt")), "n");
    writeFile(temp.filePath(QStringLiteral("destino/d/viejo.txt")), "v");

    FileOperations operations;
    QVariantMap seen;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         seen = info;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictOverwrite, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/d")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QVERIFY(seen.value(QStringLiteral("canMerge")).toBool());
    QCOMPARE(listNames(temp.filePath(QStringLiteral("destino/d"))), QStringList{QStringLiteral("nuevo.txt")});
    QCOMPARE(listNames(temp.filePath(QStringLiteral("destino"))), QStringList{QStringLiteral("d")});
}

void TestFileOperations::conflictMergeCombinesDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("origen/d/sub")));
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("destino/d/sub")));
    writeFile(temp.filePath(QStringLiteral("origen/d/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("origen/d/b.txt")), "b");
    writeFile(temp.filePath(QStringLiteral("origen/d/sub/x.txt")), "x-nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/d/a.txt")), "viejo");
    writeFile(temp.filePath(QStringLiteral("destino/d/keep.txt")), "keep");
    writeFile(temp.filePath(QStringLiteral("destino/d/sub/x.txt")), "x-viejo");

    FileOperations operations;
    int requests = 0;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         ++requests;
                         const int decision = info.value(QStringLiteral("targetIsDirectory")).toBool()
                             ? int(FileOperations::ConflictMerge)
                             : int(FileOperations::ConflictRename);
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(), decision, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.move({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/d")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(requests, 4);
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/a.txt"))), QByteArray("viejo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/a (1).txt"))), QByteArray("nuevo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/b.txt"))), QByteArray("b"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/keep.txt"))), QByteArray("keep"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/sub/x.txt"))), QByteArray("x-viejo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/sub/x (1).txt"))), QByteArray("x-nuevo"));
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("origen/d"))));
}

void TestFileOperations::conflictMergeReplaceKeepsOtherFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("origen/d/sub")));
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("destino/d/sub")));
    writeFile(temp.filePath(QStringLiteral("origen/d/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("origen/d/sub/x.txt")), "x-nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/d/a.txt")), "viejo");
    writeFile(temp.filePath(QStringLiteral("destino/d/b.txt")), "b");
    writeFile(temp.filePath(QStringLiteral("destino/d/c.txt")), "c");
    writeFile(temp.filePath(QStringLiteral("destino/d/sub/x.txt")), "x-viejo");
    writeFile(temp.filePath(QStringLiteral("destino/d/sub/y.txt")), "y");

    FileOperations operations;
    int requests = 0;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         ++requests;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictMergeReplace, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.move({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/d")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(requests, 1);
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/a.txt"))), QByteArray("nuevo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/b.txt"))), QByteArray("b"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/c.txt"))), QByteArray("c"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/sub/x.txt"))), QByteArray("x-nuevo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/d/sub/y.txt"))), QByteArray("y"));
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("origen/d"))));
}

void TestFileOperations::conflictCancelLeavesTargetUntouched()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    writeFile(temp.filePath(QStringLiteral("origen/a.txt")), "nuevo");
    writeFile(temp.filePath(QStringLiteral("destino/a.txt")), "viejo");

    FileOperations operations;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictCancel, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.move({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/a.txt")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY(!result.at(1).toBool());
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a.txt"))), QByteArray("viejo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("origen/a.txt"))), QByteArray("nuevo"));
    QCOMPARE(listNames(temp.filePath(QStringLiteral("destino"))), QStringList{QStringLiteral("a.txt")});
}

void TestFileOperations::conflictTypeMismatchOffersOnlyRename()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("destino/x")));
    writeFile(temp.filePath(QStringLiteral("origen/x")), "archivo");

    FileOperations operations;
    QVariantMap seen;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         seen = info;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictRename, false);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/x")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QVERIFY(!seen.value(QStringLiteral("canOverwrite")).toBool());
    QVERIFY(!seen.value(QStringLiteral("canMerge")).toBool());
    QVERIFY(QFileInfo(temp.filePath(QStringLiteral("destino/x"))).isDir());
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/x (1)"))), QByteArray("archivo"));
}

void TestFileOperations::conflictApplyToAllAsksOnce()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("origen")));
    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));
    for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
        writeFile(temp.filePath(QStringLiteral("origen/") + name), "nuevo");
        writeFile(temp.filePath(QStringLiteral("destino/") + name), "viejo");
    }

    FileOperations operations;
    int requests = 0;
    QObject::connect(&operations, &FileOperations::conflictRequested, &operations,
                     [&](const QVariantMap &info) {
                         ++requests;
                         operations.resolveConflict(info.value(QStringLiteral("id")).toInt(),
                                                    FileOperations::ConflictOverwrite, true);
                     });
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/a.txt"))),
                     QUrl::fromLocalFile(temp.filePath(QStringLiteral("origen/b.txt")))},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    QCOMPARE(requests, 1);
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/a.txt"))), QByteArray("nuevo"));
    QCOMPARE(readFile(temp.filePath(QStringLiteral("destino/b.txt"))), QByteArray("nuevo"));
}

void TestFileOperations::uniqueNamesKeepTarExtension()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeFile(temp.filePath(QStringLiteral("datos.tar.gz")), "x");
    writeFile(temp.filePath(QStringLiteral("nota.txt")), "x");

    const QUrl directory = QUrl::fromLocalFile(temp.path());
    QCOMPARE(FileOperations::suggestedName(directory, QStringLiteral("datos.tar.gz"), false),
             QStringLiteral("datos (1).tar.gz"));
    QCOMPARE(FileOperations::suggestedName(directory, QStringLiteral("nota.txt"), false),
             QStringLiteral("nota (1).txt"));
}

void TestFileOperations::copiesByteExactNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QByteArray sourceDirectory = temp.path().toUtf8();
    const QByteArray sourcePath = sourceDirectory + "/raro-\xFF.bin";
    const int fd = ::open(sourcePath.constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    QVERIFY(fd >= 0);
    QCOMPARE(::write(fd, "abc", 3), ssize_t(3));
    ::close(fd);

    QVERIFY(QDir(temp.path()).mkdir(QStringLiteral("destino")));

    FileOperations operations;
    QSignalSpy spy(&operations, &FileOperations::operationFinished);
    operations.copy({PathUtils::fromLocalBytes(sourcePath)},
                    QUrl::fromLocalFile(temp.filePath(QStringLiteral("destino"))));
    const QList<QVariant> result = finish(spy);
    QVERIFY(!result.isEmpty());
    QVERIFY2(result.at(1).toBool(), qPrintable(result.at(2).toString()));

    const QByteArray copied = sourceDirectory + "/destino/raro-\xFF.bin";
    QVERIFY(::access(copied.constData(), F_OK) == 0);
    const int readFd = ::open(copied.constData(), O_RDONLY);
    QVERIFY(readFd >= 0);
    char content[4] = {};
    QCOMPARE(::read(readFd, content, 3), ssize_t(3));
    ::close(readFd);
    QCOMPARE(QByteArray(content, 3), QByteArray("abc"));
}

QTEST_MAIN(TestFileOperations)
#include "tst_fileoperations.moc"
