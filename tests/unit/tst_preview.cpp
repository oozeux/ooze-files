#include "core/FilePreview.h"
#include "core/OpenWith.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

class TestPreview : public QObject
{
    Q_OBJECT

private slots:
    void textFileProducesTextPreview();
    void longTextIsTruncated();
    void binaryFileHasNoPreview();
    void emptyFileHasNoTextPreview();
    void imageReportsDimensions();
    void directoryReportsItemCount();
    void directoryReportsSizeWhenRequested();
    void missingFileReportsError();
    void remoteUrlIsNotInspected();
    void permissionsHaveNineCharacters();
    void openWithRejectsUnknownApplication();
    void openWithIgnoresEmptyMime();

private:
    QString write(const QTemporaryDir &dir, const QString &name, const QByteArray &content);
};

QString TestPreview::write(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(content);
    return path;
}

void TestPreview::textFileProducesTextPreview()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = write(dir, QStringLiteral("nota.txt"), "hola\nmundo ñ\n");

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(path));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("text"));
    QCOMPARE(data.value(QStringLiteral("text")).toString(), QStringLiteral("hola\nmundo ñ\n"));
    QCOMPARE(data.value(QStringLiteral("truncated")).toBool(), false);
    QCOMPARE(data.value(QStringLiteral("name")).toString(), QStringLiteral("nota.txt"));
    QVERIFY(!data.value(QStringLiteral("sizeText")).toString().isEmpty());
    QVERIFY(!data.value(QStringLiteral("mimeDescription")).toString().isEmpty());
}

void TestPreview::longTextIsTruncated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QByteArray lines;
    for (int i = 0; i < FilePreview::kMaxTextLines * 3; ++i)
        lines.append("linea ").append(QByteArray::number(i)).append('\n');
    const QString byLines = write(dir, QStringLiteral("lineas.txt"), lines);

    QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(byLines));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("text"));
    QCOMPARE(data.value(QStringLiteral("truncated")).toBool(), true);
    QCOMPARE(data.value(QStringLiteral("text")).toString().count(QLatin1Char('\n')),
             FilePreview::kMaxTextLines - 1);

    const QString byBytes = write(dir, QStringLiteral("larga.txt"),
                                  QByteArray(FilePreview::kMaxTextBytes * 2, 'a'));
    data = FilePreview::describe(QUrl::fromLocalFile(byBytes));
    QCOMPARE(data.value(QStringLiteral("truncated")).toBool(), true);
    QCOMPARE(data.value(QStringLiteral("text")).toString().size(),
             int(FilePreview::kMaxTextBytes));
}

void TestPreview::binaryFileHasNoPreview()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = write(dir, QStringLiteral("datos.txt"),
                               QByteArray("abc\0def", 7));

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(path));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("none"));
    QVERIFY(!data.contains(QStringLiteral("text")));
}

void TestPreview::emptyFileHasNoTextPreview()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = write(dir, QStringLiteral("vacio.txt"), QByteArray());

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(path));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("none"));
}

void TestPreview::imageReportsDimensions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage image(40, 20, QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString path = dir.filePath(QStringLiteral("rojo.png"));
    QVERIFY(image.save(path));

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(path));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("image"));
    QCOMPARE(data.value(QStringLiteral("dimensions")).toString(), QStringLiteral("40 × 20"));
    QCOMPARE(data.value(QStringLiteral("mime")).toString(), QStringLiteral("image/png"));
}

void TestPreview::directoryReportsItemCount()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir, QStringLiteral("a"), "x");
    write(dir, QStringLiteral("b"), "x");
    write(dir, QStringLiteral(".oculto"), "x");
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(dir.path()));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("directory"));
    QCOMPARE(data.value(QStringLiteral("itemCountText")).toString(), QStringLiteral("4"));
    QVERIFY(!data.contains(QStringLiteral("sizeText")));
}

void TestPreview::directoryReportsSizeWhenRequested()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir, QStringLiteral("a"), "abc");
    write(dir, QStringLiteral("b"), "de");
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
    write(dir, QStringLiteral("sub/c"), "fghi");

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(dir.path()), true);
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("directory"));
    QCOMPARE(data.value(QStringLiteral("sizeText")).toString(),
             QLocale().formattedDataSize(9));

    const QVariantMap shallow = FilePreview::describe(QUrl::fromLocalFile(dir.path()));
    QVERIFY(!shallow.contains(QStringLiteral("sizeText")));
}

void TestPreview::missingFileReportsError()
{
    const QVariantMap data = FilePreview::describe(
        QUrl::fromLocalFile(QStringLiteral("/tmp/ooze-files-no-existe-xyz")));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("none"));
    QVERIFY(!data.value(QStringLiteral("error")).toString().isEmpty());
}

void TestPreview::remoteUrlIsNotInspected()
{
    const QVariantMap data = FilePreview::describe(QUrl(QStringLiteral("sftp://host/dir/a.txt")));
    QCOMPARE(data.value(QStringLiteral("kind")).toString(), QStringLiteral("none"));
    QCOMPARE(data.value(QStringLiteral("name")).toString(), QStringLiteral("a.txt"));
    QVERIFY(!data.contains(QStringLiteral("path")));
}

void TestPreview::permissionsHaveNineCharacters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = write(dir, QStringLiteral("p.txt"), "x");
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                            | QFileDevice::ReadGroup));

    const QVariantMap data = FilePreview::describe(QUrl::fromLocalFile(path));
    QCOMPARE(data.value(QStringLiteral("permissions")).toString(), QStringLiteral("rw-r-----"));
}

void TestPreview::openWithRejectsUnknownApplication()
{
    QString error;
    QVERIFY(!OpenWith::launch(QStringLiteral("ooze-no-existe.desktop"),
                              QStringLiteral("text/plain"),
                              {QUrl::fromLocalFile(QStringLiteral("/tmp/x.txt"))}, &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!OpenWith::launch(QString(), QStringLiteral("text/plain"), {}, &error));
    QVERIFY(!error.isEmpty());

    const QVariantList apps = OpenWith::appsForMime(QStringLiteral("application/x-ooze-inexistente"));
    QVERIFY(apps.isEmpty());
}

void TestPreview::openWithIgnoresEmptyMime()
{
    QVERIFY(OpenWith::appsForMime(QString()).isEmpty());
}

QTEST_GUILESS_MAIN(TestPreview)
#include "tst_preview.moc"
