#include "core/FileOperations.h"
#include "core/UndoStack.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class TestUndo : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void stackWalksBackAndForward();
    void stackDropsRedoTailOnPush();
    void stackKeepsOnlyTheLatestEntries();
    void stackIgnoresEmptyEntries();

    void renameCanBeUndoneAndRedone();
    void moveCanBeUndoneAndRedone();
    void copyCanBeUndoneAndRedone();
    void createdFolderCanBeUndoneAndRedone();
    void trashCanBeUndoneAndRedone();
    void undoFailsWhenTheOriginalIsTaken();
    void permanentRemovalIsNotRecorded();

private:
    QTemporaryDir m_dataDir;

    static QString touch(const QString &path, const QByteArray &content = "x");
    static JournalEntry lastEntry(QSignalSpy &spy);
    static bool apply(FileOperations &ops, const JournalEntry &entry, bool undo,
                      JournalEntry *updated = nullptr);
};

void TestUndo::initTestCase()
{
    QVERIFY(m_dataDir.isValid());
    qputenv("XDG_DATA_HOME", m_dataDir.path().toUtf8());
}

QString TestUndo::touch(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(content);
    return path;
}

JournalEntry TestUndo::lastEntry(QSignalSpy &spy)
{
    JournalEntry entry;
    if (spy.isEmpty())
        return entry;

    const QList<QVariant> args = spy.last();
    entry.kind = static_cast<JournalEntry::Kind>(args.at(0).toInt());
    entry.label = args.at(1).toString();
    entry.firsts = args.at(2).toStringList();
    entry.seconds = args.at(3).toStringList();
    return entry;
}

bool TestUndo::apply(FileOperations &ops, const JournalEntry &entry, bool undo,
                     JournalEntry *updated)
{
    QSignalSpy applied(&ops, &FileOperations::journalApplied);
    ops.applyJournal(entry, undo);
    if (!QTest::qWaitFor([&] { return applied.count() > 0; }, 5000))
        return false;

    const QList<QVariant> args = applied.last();
    if (updated) {
        *updated = entry;
        updated->firsts = args.at(3).toStringList();
        updated->seconds = args.at(4).toStringList();
    }
    return args.at(1).toBool();
}

void TestUndo::stackWalksBackAndForward()
{
    UndoStack stack;
    QVERIFY(!stack.canUndo());
    QVERIFY(!stack.canRedo());

    JournalEntry a;
    a.label = QStringLiteral("A");
    a.firsts = {QStringLiteral("/a")};
    JournalEntry b;
    b.label = QStringLiteral("B");
    b.firsts = {QStringLiteral("/b")};
    stack.push(a);
    stack.push(b);

    QVERIFY(stack.canUndo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("B"));

    stack.commitUndo(stack.nextUndo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("A"));
    QCOMPARE(stack.redoLabel(), QStringLiteral("B"));

    stack.commitUndo(stack.nextUndo());
    QVERIFY(!stack.canUndo());

    stack.commitRedo(stack.nextRedo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("A"));
    stack.commitRedo(stack.nextRedo());
    QVERIFY(!stack.canRedo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("B"));
}

void TestUndo::stackDropsRedoTailOnPush()
{
    UndoStack stack;
    JournalEntry entry;
    entry.firsts = {QStringLiteral("/x")};

    entry.label = QStringLiteral("1");
    stack.push(entry);
    entry.label = QStringLiteral("2");
    stack.push(entry);

    stack.commitUndo(stack.nextUndo());
    QVERIFY(stack.canRedo());

    entry.label = QStringLiteral("3");
    stack.push(entry);
    QVERIFY(!stack.canRedo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("3"));

    stack.commitUndo(stack.nextUndo());
    QCOMPARE(stack.undoLabel(), QStringLiteral("1"));
}

void TestUndo::stackKeepsOnlyTheLatestEntries()
{
    UndoStack stack;
    JournalEntry entry;
    entry.firsts = {QStringLiteral("/x")};

    for (int i = 0; i < UndoStack::kLimit + 10; ++i) {
        entry.label = QString::number(i);
        stack.push(entry);
    }

    QCOMPARE(stack.undoLabel(), QString::number(UndoStack::kLimit + 9));

    int steps = 0;
    while (stack.canUndo()) {
        stack.commitUndo(stack.nextUndo());
        ++steps;
    }

    QCOMPARE(steps, UndoStack::kLimit);
    QCOMPARE(stack.redoLabel(), QString::number(10));
}

void TestUndo::stackIgnoresEmptyEntries()
{
    UndoStack stack;
    stack.push(JournalEntry());
    QVERIFY(!stack.canUndo());
}

void TestUndo::renameCanBeUndoneAndRedone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString original = touch(dir.filePath(QStringLiteral("a.txt")));
    const QString renamed = dir.filePath(QStringLiteral("b.txt"));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.rename(QUrl::fromLocalFile(original), QStringLiteral("b.txt"));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(QFile::exists(renamed));

    const JournalEntry entry = lastEntry(recorded);
    QCOMPARE(entry.kind, JournalEntry::Moved);
    QCOMPARE(entry.firsts, QStringList{original});
    QCOMPARE(entry.seconds, QStringList{renamed});

    QVERIFY(apply(ops, entry, true));
    QVERIFY(QFile::exists(original));
    QVERIFY(!QFile::exists(renamed));

    QVERIFY(apply(ops, entry, false));
    QVERIFY(!QFile::exists(original));
    QVERIFY(QFile::exists(renamed));
}

void TestUndo::moveCanBeUndoneAndRedone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("destino")));
    const QString source = touch(dir.filePath(QStringLiteral("dato.txt")), "contenido");
    const QString moved = dir.filePath(QStringLiteral("destino/dato.txt"));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.move({QUrl::fromLocalFile(source)}, QUrl::fromLocalFile(dir.filePath(QStringLiteral("destino"))));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(QFile::exists(moved));

    const JournalEntry entry = lastEntry(recorded);
    QCOMPARE(entry.label, QStringLiteral("Mover"));

    QVERIFY(apply(ops, entry, true));
    QVERIFY(QFile::exists(source));
    QVERIFY(!QFile::exists(moved));

    QVERIFY(apply(ops, entry, false));
    QVERIFY(QFile::exists(moved));
    QVERIFY(!QFile::exists(source));
}

void TestUndo::copyCanBeUndoneAndRedone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("destino")));
    const QString source = touch(dir.filePath(QStringLiteral("dato.txt")), "contenido");
    const QString copy = dir.filePath(QStringLiteral("destino/dato.txt"));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.copy({QUrl::fromLocalFile(source)}, QUrl::fromLocalFile(dir.filePath(QStringLiteral("destino"))));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(QFile::exists(copy));

    JournalEntry entry = lastEntry(recorded);
    QCOMPARE(entry.kind, JournalEntry::Created);
    QCOMPARE(entry.firsts, QStringList{copy});

    QVERIFY(apply(ops, entry, true, &entry));
    QVERIFY(!QFile::exists(copy));
    QVERIFY(QFile::exists(source));
    QVERIFY(!entry.seconds.first().isEmpty());
    QVERIFY(QFile::exists(entry.seconds.first()));

    QVERIFY(apply(ops, entry, false, &entry));
    QVERIFY(QFile::exists(copy));

    QFile file(copy);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("contenido"));
}

void TestUndo::createdFolderCanBeUndoneAndRedone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath(QStringLiteral("nueva"));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.createDirectory(QUrl::fromLocalFile(dir.path()), QStringLiteral("nueva"));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(QFileInfo(folder).isDir());

    JournalEntry entry = lastEntry(recorded);
    QCOMPARE(entry.label, QStringLiteral("Crear carpeta"));

    QVERIFY(apply(ops, entry, true, &entry));
    QVERIFY(!QFileInfo::exists(folder));

    QVERIFY(apply(ops, entry, false, &entry));
    QVERIFY(QFileInfo(folder).isDir());
}

void TestUndo::trashCanBeUndoneAndRedone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = touch(dir.filePath(QStringLiteral("borrame.txt")), "adios");

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.trash({QUrl::fromLocalFile(path)});
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.last().at(1).toBool());
    QVERIFY(!QFile::exists(path));

    if (recorded.isEmpty())
        QSKIP("La papelera de este sistema de archivos no permite registrar la ruta");

    JournalEntry entry = lastEntry(recorded);
    QCOMPARE(entry.kind, JournalEntry::Trashed);
    QVERIFY(QFile::exists(entry.seconds.first()));

    QVERIFY(apply(ops, entry, true, &entry));
    QVERIFY(QFile::exists(path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("adios"));
    file.close();

    QVERIFY(apply(ops, entry, false, &entry));
    QVERIFY(!QFile::exists(path));
    QVERIFY(QFile::exists(entry.seconds.first()));

    QVERIFY(apply(ops, entry, true, &entry));
    QVERIFY(QFile::exists(path));
}

void TestUndo::undoFailsWhenTheOriginalIsTaken()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString original = touch(dir.filePath(QStringLiteral("a.txt")));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.rename(QUrl::fromLocalFile(original), QStringLiteral("b.txt"));
    QTRY_COMPARE(finished.count(), 1);
    const JournalEntry entry = lastEntry(recorded);

    touch(original, "otro");

    QSignalSpy applied(&ops, &FileOperations::journalApplied);
    ops.applyJournal(entry, true);
    QTRY_COMPARE(applied.count(), 1);
    QCOMPARE(applied.last().at(1).toBool(), false);
    QVERIFY(!applied.last().at(2).toString().isEmpty());
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("b.txt"))));
}

void TestUndo::permanentRemovalIsNotRecorded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = touch(dir.filePath(QStringLiteral("fuera.txt")));

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QSignalSpy finished(&ops, &FileOperations::operationFinished);

    ops.remove({QUrl::fromLocalFile(path)});
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(path));
    QVERIFY(recorded.isEmpty());
}

QTEST_GUILESS_MAIN(TestUndo)
#include "tst_undo.moc"
