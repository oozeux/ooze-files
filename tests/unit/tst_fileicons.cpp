#include "core/FileIcons.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestFileIcons : public QObject
{
    Q_OBJECT

private slots:
    void absoluteSpecRoundTrips();
    void desktopIconUsesAbsolutePath();
    void desktopIconUsesThemeName();
    void regularFileKeepsFallback();
};

void TestFileIcons::absoluteSpecRoundTrips()
{
    const QString path = QStringLiteral("/tmp/íconos con espacios/foo#bar.png");
    const QString spec = FileIcons::absoluteSpec(path);
    QVERIFY(FileIcons::isAbsoluteSpec(spec));
    QCOMPARE(FileIcons::decodeAbsoluteSpec(spec), path);
}

void TestFileIcons::desktopIconUsesAbsolutePath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iconPath = dir.filePath(QStringLiteral("launcher icon.png"));
    QFile icon(iconPath);
    QVERIFY(icon.open(QIODevice::WriteOnly));
    icon.write("not really an image");
    icon.close();

    const QString desktopPath = dir.filePath(QStringLiteral("test.desktop"));
    QFile desktop(desktopPath);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write("[Desktop Entry]\nType=Application\nName=Test\nIcon=");
    desktop.write(iconPath.toUtf8());
    desktop.write("\n");
    desktop.close();

    const QString resolved = FileIcons::iconForFile(
        desktopPath, QStringLiteral("application-x-desktop"));
    QVERIFY(FileIcons::isAbsoluteSpec(resolved));
    QCOMPARE(FileIcons::decodeAbsoluteSpec(resolved), iconPath);
}

void TestFileIcons::desktopIconUsesThemeName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString desktopPath = dir.filePath(QStringLiteral("test.desktop"));
    QFile desktop(desktopPath);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write("[Desktop Entry]\nIcon=org.example.App\n");
    desktop.close();

    QCOMPARE(FileIcons::iconForFile(desktopPath, QStringLiteral("fallback")),
             QStringLiteral("org.example.App"));
}

void TestFileIcons::regularFileKeepsFallback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("nota.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hola");
    file.close();

    QCOMPARE(FileIcons::iconForFile(path, QStringLiteral("text-x-generic")),
             QStringLiteral("text-x-generic"));
}

QTEST_GUILESS_MAIN(TestFileIcons)
#include "tst_fileicons.moc"
