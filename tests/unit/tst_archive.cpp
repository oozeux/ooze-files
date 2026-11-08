#include "core/Archive.h"
#include "core/FileOperations.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class TestArchive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void recognisesArchiveNames();
    void stripsArchiveSuffixes();
    void protectsNamesThatLookLikeOptions();
    void rejectsUnknownFormats();
    void formatsOnlyListInstalledTools();

    void zipRoundTrip();
    void tarGzipRoundTrip();
    void tarXzRoundTrip();
    void multipleItemsUseTheFolderName();
    void namesAreMadeUnique();
    void corruptArchiveLeavesNothingBehind();
    void compressionCanBeUndone();

private:
    QTemporaryDir m_dataDir;

    static QString write(const QString &path, const QByteArray &content);
    static QByteArray read(const QString &path);
    static bool has(const char *tool);
    bool run(FileOperations &ops, const std::function<void()> &start, QString *created = nullptr,
             QString *message = nullptr);
};

void TestArchive::initTestCase()
{
    QVERIFY(m_dataDir.isValid());
    qputenv("XDG_DATA_HOME", m_dataDir.path().toUtf8());
}

QString TestArchive::write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(content);
    return path;
}

QByteArray TestArchive::read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool TestArchive::has(const char *tool)
{
    return !QStandardPaths::findExecutable(QLatin1String(tool)).isEmpty();
}

bool TestArchive::run(FileOperations &ops, const std::function<void()> &start, QString *created,
                      QString *message)
{
    QSignalSpy finished(&ops, &FileOperations::operationFinished);
    start();
    if (!QTest::qWaitFor([&] { return finished.count() > 0; }, 20000))
        return false;

    const QList<QVariant> args = finished.last();
    if (message)
        *message = args.at(2).toString();
    if (created)
        *created = args.at(3).toString();
    return args.at(1).toBool();
}

void TestArchive::recognisesArchiveNames()
{
    for (const char *name : {"a.zip", "A.ZIP", "b.tar", "c.tar.gz", "c.TGZ", "d.tar.xz", "e.tar.zst",
                             "f.tar.bz2", "g.7z", "h.rar", "x.txz"})
        QVERIFY2(Archive::isArchiveName(QLatin1String(name)), name);

    for (const char *name : {"a.txt", "zip", "a.gz", "b.xz", "c.tar.gz.bak", "d.zipx", ""})
        QVERIFY2(!Archive::isArchiveName(QLatin1String(name)), name);
}

void TestArchive::stripsArchiveSuffixes()
{
    QCOMPARE(Archive::baseNameWithoutSuffix(QStringLiteral("proyecto.tar.gz")),
             QStringLiteral("proyecto"));
    QCOMPARE(Archive::baseNameWithoutSuffix(QStringLiteral("proyecto.zip")),
             QStringLiteral("proyecto"));
    QCOMPARE(Archive::baseNameWithoutSuffix(QStringLiteral("a.b.tar.xz")), QStringLiteral("a.b"));
    QCOMPARE(Archive::baseNameWithoutSuffix(QStringLiteral("nota.txt")), QStringLiteral("nota.txt"));
    QCOMPARE(Archive::baseNameWithoutSuffix(QStringLiteral(".zip")), QStringLiteral(".zip"));
    QCOMPARE(Archive::suffixForFormat(QStringLiteral("tar.xz")), QStringLiteral(".tar.xz"));
    QVERIFY(Archive::suffixForFormat(QStringLiteral("rar")).isEmpty());
}

void TestArchive::protectsNamesThatLookLikeOptions()
{
    if (!has("tar"))
        QSKIP("tar no está instalado");

    QString error;
    const Archive::Command command = Archive::compressCommand(
        QStringLiteral("tar.gz"), QStringLiteral("/tmp/out.tar.gz"),
        {QStringLiteral("-rf"), QStringLiteral("normal")}, QStringLiteral("/tmp"), &error);

    QVERIFY(command.isValid());
    QVERIFY(command.arguments.contains(QStringLiteral("./-rf")));
    QVERIFY(command.arguments.contains(QStringLiteral("normal")));
    QVERIFY(!command.arguments.contains(QStringLiteral("-rf")));
}

void TestArchive::rejectsUnknownFormats()
{
    QString error;
    QVERIFY(!Archive::compressCommand(QStringLiteral("rar"), QStringLiteral("/tmp/a.rar"),
                                      {QStringLiteral("x")}, QStringLiteral("/tmp"), &error)
                 .isValid());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!Archive::compressCommand(QStringLiteral("zip"), QStringLiteral("/tmp/a.zip"), {},
                                      QStringLiteral("/tmp"), &error)
                 .isValid());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!Archive::extractCommand(QStringLiteral("/tmp/nota.txt"), QStringLiteral("/tmp/x"), &error)
                 .isValid());
    QVERIFY(!error.isEmpty());
}

void TestArchive::formatsOnlyListInstalledTools()
{
    const QVariantList formats = Archive::availableFormats();
    QStringList ids;
    for (const QVariant &format : formats)
        ids.append(format.toMap().value(QStringLiteral("id")).toString());

    QCOMPARE(ids.contains(QStringLiteral("zip")), has("zip") || has("7z") || has("7zz")
                                                      || has("7za") || has("bsdtar"));
    QCOMPARE(ids.contains(QStringLiteral("tar.gz")), has("tar") && has("gzip"));
    QCOMPARE(ids.contains(QStringLiteral("tar.zst")), has("tar") && has("zstd"));
}

void TestArchive::zipRoundTrip()
{
    if (!has("zip") || !has("unzip"))
        QSKIP("zip/unzip no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("informe.txt")), "hola zip");

    FileOperations ops;
    QString created;
    QVERIFY(run(ops, [&] {
        ops.compress({QUrl::fromLocalFile(dir.filePath(QStringLiteral("informe.txt")))},
                     QStringLiteral("zip"));
    }, &created));
    QCOMPARE(created, QStringLiteral("informe.zip"));
    QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("informe.zip"))));

    QVERIFY(QFile::remove(dir.filePath(QStringLiteral("informe.txt"))));
    QVERIFY(run(ops, [&] {
        ops.extract(QUrl::fromLocalFile(dir.filePath(QStringLiteral("informe.zip"))));
    }, &created));
    QCOMPARE(created, QStringLiteral("informe"));
    QCOMPARE(read(dir.filePath(QStringLiteral("informe/informe.txt"))), QByteArray("hola zip"));
}

void TestArchive::tarGzipRoundTrip()
{
    if (!has("tar") || !has("gzip"))
        QSKIP("tar/gzip no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("Mi carpeta/uno.txt")), "uno");
    write(dir.filePath(QStringLiteral("Mi carpeta/sub/dos.txt")), "dos");

    FileOperations ops;
    QString created;
    QVERIFY(run(ops, [&] {
        ops.compress({QUrl::fromLocalFile(dir.filePath(QStringLiteral("Mi carpeta")))},
                     QStringLiteral("tar.gz"));
    }, &created));
    QCOMPARE(created, QStringLiteral("Mi carpeta.tar.gz"));

    QVERIFY(QDir(dir.filePath(QStringLiteral("Mi carpeta"))).removeRecursively());
    QVERIFY(run(ops, [&] {
        ops.extract(QUrl::fromLocalFile(dir.filePath(QStringLiteral("Mi carpeta.tar.gz"))));
    }, &created));
    QCOMPARE(created, QStringLiteral("Mi carpeta"));
    QCOMPARE(read(dir.filePath(QStringLiteral("Mi carpeta/Mi carpeta/uno.txt"))), QByteArray("uno"));
    QCOMPARE(read(dir.filePath(QStringLiteral("Mi carpeta/Mi carpeta/sub/dos.txt"))),
             QByteArray("dos"));
}

void TestArchive::tarXzRoundTrip()
{
    if (!has("tar") || !has("xz"))
        QSKIP("tar/xz no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("-raro.txt")), "guion");

    FileOperations ops;
    QString created;
    QVERIFY(run(ops, [&] {
        ops.compress({QUrl::fromLocalFile(dir.filePath(QStringLiteral("-raro.txt")))},
                     QStringLiteral("tar.xz"));
    }, &created));
    QVERIFY(QFileInfo::exists(dir.filePath(created)));

    QVERIFY(QFile::remove(dir.filePath(QStringLiteral("-raro.txt"))));
    QString extracted;
    QVERIFY(run(ops, [&] { ops.extract(QUrl::fromLocalFile(dir.filePath(created))); }, &extracted));
    QCOMPARE(read(dir.filePath(extracted + QStringLiteral("/-raro.txt"))), QByteArray("guion"));
}

void TestArchive::multipleItemsUseTheFolderName()
{
    if (!has("tar") || !has("gzip"))
        QSKIP("tar/gzip no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath(QStringLiteral("Fotos"));
    write(folder + QStringLiteral("/a.txt"), "a");
    write(folder + QStringLiteral("/b.txt"), "b");

    FileOperations ops;
    QString created;
    QVERIFY(run(ops, [&] {
        ops.compress({QUrl::fromLocalFile(folder + QStringLiteral("/a.txt")),
                      QUrl::fromLocalFile(folder + QStringLiteral("/b.txt"))},
                     QStringLiteral("tar.gz"));
    }, &created));
    QCOMPARE(created, QStringLiteral("Fotos.tar.gz"));
}

void TestArchive::namesAreMadeUnique()
{
    if (!has("tar") || !has("gzip"))
        QSKIP("tar/gzip no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("datos.txt")), "x");

    FileOperations ops;
    QString first;
    QString second;
    const QUrl source = QUrl::fromLocalFile(dir.filePath(QStringLiteral("datos.txt")));
    QVERIFY(run(ops, [&] { ops.compress({source}, QStringLiteral("tar.gz")); }, &first));
    QVERIFY(run(ops, [&] { ops.compress({source}, QStringLiteral("tar.gz")); }, &second));

    QCOMPARE(first, QStringLiteral("datos.tar.gz"));
    QCOMPARE(second, QStringLiteral("datos (2).tar.gz"));
}

void TestArchive::corruptArchiveLeavesNothingBehind()
{
    if (!has("unzip"))
        QSKIP("unzip no está instalado");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("roto.zip")), QByteArray("esto no es un zip"));

    FileOperations ops;
    QString message;
    QVERIFY(!run(ops, [&] {
        ops.extract(QUrl::fromLocalFile(dir.filePath(QStringLiteral("roto.zip"))));
    }, nullptr, &message));
    QVERIFY(!message.isEmpty());
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("roto"))));
}

void TestArchive::compressionCanBeUndone()
{
    if (!has("tar") || !has("gzip"))
        QSKIP("tar/gzip no están instalados");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("a.txt")), "a");

    FileOperations ops;
    QSignalSpy recorded(&ops, &FileOperations::changeRecorded);
    QString created;
    QVERIFY(run(ops, [&] {
        ops.compress({QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt")))},
                     QStringLiteral("tar.gz"));
    }, &created));
    QTRY_COMPARE(recorded.count(), 1);

    JournalEntry entry;
    entry.kind = static_cast<JournalEntry::Kind>(recorded.last().at(0).toInt());
    entry.label = recorded.last().at(1).toString();
    entry.firsts = recorded.last().at(2).toStringList();
    entry.seconds = recorded.last().at(3).toStringList();
    QCOMPARE(entry.label, QStringLiteral("Comprimir"));

    QSignalSpy applied(&ops, &FileOperations::journalApplied);
    ops.applyJournal(entry, true);
    QTRY_COMPARE(applied.count(), 1);
    QVERIFY(applied.last().at(1).toBool());
    QVERIFY(!QFileInfo::exists(dir.filePath(created)));
}

QTEST_GUILESS_MAIN(TestArchive)
#include "tst_archive.moc"
