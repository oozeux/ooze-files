#include "core/OpenWith.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {
int appIndexById(const QVariantList &apps, const QString &id)
{
    for (int i = 0; i < apps.size(); ++i) {
        if (apps.at(i).toMap().value(QStringLiteral("id")).toString() == id)
            return i;
    }
    return -1;
}
}

class TestOpenWith : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void listsRegisteredApplications();
    void textSubtypesInheritPlainTextApplications();
    void otherApplicationsExcludeListedOnes();
    void reportsCurrentDefaultAndCandidates();
    void setDefaultWritesTheUserPreference();
    void setDefaultCoversEveryTypeOfTheCategory();
    void setDefaultRejectsUnknownInput();
    void launchRunsTheApplication();

private:
    QTemporaryDir m_root;
    QString m_configHome;

    static void writeFile(const QString &path, const QByteArray &content);
    static QVariantMap category(const QVariantList &states, const QString &id);
    QByteArray mimeApps() const;
};

void TestOpenWith::writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QVariantMap TestOpenWith::category(const QVariantList &states, const QString &id)
{
    for (const QVariant &state : states) {
        if (state.toMap().value(QStringLiteral("id")).toString() == id)
            return state.toMap();
    }
    return {};
}

QByteArray TestOpenWith::mimeApps() const
{
    QFile file(m_configHome + QStringLiteral("/mimeapps.list"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void TestOpenWith::initTestCase()
{
    QVERIFY(m_root.isValid());

    const QString dataHome = m_root.filePath(QStringLiteral("data"));
    m_configHome = m_root.filePath(QStringLiteral("config"));
    QDir().mkpath(m_root.filePath(QStringLiteral("sysdata")));
    QDir().mkpath(m_root.filePath(QStringLiteral("sysconfig")));

    qputenv("XDG_DATA_HOME", dataHome.toUtf8());
    qputenv("XDG_CONFIG_HOME", m_configHome.toUtf8());
    qputenv("XDG_DATA_DIRS", m_root.filePath(QStringLiteral("sysdata")).toUtf8());
    qputenv("XDG_CONFIG_DIRS", m_root.filePath(QStringLiteral("sysconfig")).toUtf8());

    const QString launched = m_root.filePath(QStringLiteral("launched.txt"));
    const auto app = [&](const QString &id, const QString &name, const QString &mimes) {
        writeFile(dataHome + QStringLiteral("/applications/") + id,
                  QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n"
                                 "Exec=sh -c \"echo %2 $0 >> %3\" %f\nMimeType=%4\n")
                      .arg(name, id, launched, mimes)
                      .toUtf8());
    };

    app(QStringLiteral("editor-a.desktop"), QStringLiteral("Editor A"),
        QStringLiteral("text/plain;"));
    app(QStringLiteral("editor-b.desktop"), QStringLiteral("Editor B"),
        QStringLiteral("text/plain;text/markdown;"));
    app(QStringLiteral("visor.desktop"), QStringLiteral("Visor"),
        QStringLiteral("image/png;image/jpeg;image/gif;"));
    app(QStringLiteral("sonido.desktop"), QStringLiteral("Sonido"),
        QStringLiteral("audio/mpeg;"));

    writeFile(dataHome + QStringLiteral("/applications/mimeinfo.cache"),
              "[MIME Cache]\n"
              "text/plain=editor-a.desktop;editor-b.desktop;\n"
              "text/markdown=editor-b.desktop;\n"
              "image/png=visor.desktop;\n"
              "image/jpeg=visor.desktop;\n"
              "image/gif=visor.desktop;\n"
              "audio/mpeg=sonido.desktop;\n");

    writeFile(m_configHome + QStringLiteral("/mimeapps.list"),
              "[Default Applications]\n"
              "text/plain=editor-a.desktop;\n"
              "audio/mpeg=sonido.desktop;\n");
}

void TestOpenWith::listsRegisteredApplications()
{
    // NixOS aplica glib-appinfo-watch.patch: GIO añade los perfiles del sistema
    // aunque XDG_DATA_DIRS esté aislado. Por eso se comprueba presencia y orden
    // relativo, no cantidades exactas.
    const QVariantList apps = OpenWith::appsForMime(QStringLiteral("text/plain"));
    QVERIFY(apps.size() >= 2);

    QCOMPARE(apps.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("editor-a.desktop"));
    QVERIFY(apps.first().toMap().value(QStringLiteral("isDefault")).toBool());

    const int editorB = appIndexById(apps, QStringLiteral("editor-b.desktop"));
    QVERIFY(editorB > 0);
    QCOMPARE(apps.at(editorB).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Editor B"));
    QVERIFY(!apps.at(editorB).toMap().value(QStringLiteral("isDefault")).toBool());
}

void TestOpenWith::textSubtypesInheritPlainTextApplications()
{
    const QVariantList apps = OpenWith::appsForMime(QStringLiteral("text/x-python"));
    QVERIFY(appIndexById(apps, QStringLiteral("editor-a.desktop")) >= 0);
    QVERIFY(appIndexById(apps, QStringLiteral("editor-b.desktop")) >= 0);
}

void TestOpenWith::otherApplicationsExcludeListedOnes()
{
    const QVariantList listed = OpenWith::appsForMime(QStringLiteral("image/png"));
    const QVariantList others = OpenWith::otherApps(QStringLiteral("image/png"));

    QVERIFY(appIndexById(listed, QStringLiteral("visor.desktop")) >= 0);
    QCOMPARE(appIndexById(others, QStringLiteral("visor.desktop")), -1);
    QVERIFY(appIndexById(others, QStringLiteral("editor-a.desktop")) >= 0);
}

void TestOpenWith::reportsCurrentDefaultAndCandidates()
{
    const QVariantList states = OpenWith::defaultAppStates();
    QVERIFY(states.size() >= 8);

    const QVariantMap text = category(states, QStringLiteral("text"));
    QCOMPARE(text.value(QStringLiteral("label")).toString(), QStringLiteral("Texto"));
    QCOMPARE(text.value(QStringLiteral("appId")).toString(), QStringLiteral("editor-a.desktop"));
    QCOMPARE(text.value(QStringLiteral("appName")).toString(), QStringLiteral("Editor A"));

    // Las aplicaciones del sistema también aparecen en NixOS; se comprueba que
    // las del fixture están y que mantienen su orden relativo.
    QStringList ids;
    for (const QVariant &app : text.value(QStringLiteral("apps")).toList())
        ids.append(app.toMap().value(QStringLiteral("id")).toString());
    QVERIFY(ids.contains(QStringLiteral("editor-a.desktop")));
    QVERIFY(ids.contains(QStringLiteral("editor-b.desktop")));
    QVERIFY(ids.indexOf(QStringLiteral("editor-a.desktop"))
            < ids.indexOf(QStringLiteral("editor-b.desktop")));

    const QVariantMap images = category(states, QStringLiteral("images"));
    const QVariantList imageApps = images.value(QStringLiteral("apps")).toList();
    QVERIFY(appIndexById(imageApps, QStringLiteral("visor.desktop")) >= 0);
    QCOMPARE(images.value(QStringLiteral("appId")).toString(), QStringLiteral("visor.desktop"));

    const QVariantMap audio = category(states, QStringLiteral("audio"));
    const QVariantList audioApps = audio.value(QStringLiteral("apps")).toList();
    QVERIFY(appIndexById(audioApps, QStringLiteral("sonido.desktop")) >= 0);
    QCOMPARE(audio.value(QStringLiteral("appId")).toString(), QStringLiteral("sonido.desktop"));
}

void TestOpenWith::setDefaultWritesTheUserPreference()
{
    QString error;
    QVERIFY2(OpenWith::setDefault(QStringLiteral("text"), QStringLiteral("editor-b.desktop"), &error),
             qPrintable(error));

    const QByteArray list = mimeApps();
    QVERIFY2(list.contains("text/plain=editor-b.desktop"), list.constData());

    const QVariantMap text = category(OpenWith::defaultAppStates(), QStringLiteral("text"));
    QCOMPARE(text.value(QStringLiteral("appId")).toString(), QStringLiteral("editor-b.desktop"));
    QCOMPARE(OpenWith::appsForMime(QStringLiteral("text/plain")).first().toMap()
                 .value(QStringLiteral("id")).toString(),
             QStringLiteral("editor-b.desktop"));
}

void TestOpenWith::setDefaultCoversEveryTypeOfTheCategory()
{
    QString error;
    QVERIFY2(OpenWith::setDefault(QStringLiteral("images"), QStringLiteral("visor.desktop"), &error),
             qPrintable(error));

    const QByteArray list = mimeApps();
    for (const char *mime : {"image/png", "image/jpeg", "image/gif", "image/webp", "image/svg+xml"})
        QVERIFY2(list.contains(QByteArray(mime) + "=visor.desktop"), mime);

    QCOMPARE(category(OpenWith::defaultAppStates(), QStringLiteral("images"))
                 .value(QStringLiteral("appId")).toString(),
             QStringLiteral("visor.desktop"));
}

void TestOpenWith::setDefaultRejectsUnknownInput()
{
    QString error;
    QVERIFY(!OpenWith::setDefault(QStringLiteral("inventada"), QStringLiteral("visor.desktop"), &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!OpenWith::setDefault(QStringLiteral("text"), QStringLiteral("no-existe.desktop"), &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(category(OpenWith::defaultAppStates(), QStringLiteral("text"))
                .value(QStringLiteral("appId")).toString() == QStringLiteral("editor-b.desktop"));
}

void TestOpenWith::launchRunsTheApplication()
{
    const QString target = m_root.filePath(QStringLiteral("nota.txt"));
    writeFile(target, "x");

    QString error;
    QVERIFY2(OpenWith::launch(QStringLiteral("editor-a.desktop"), QStringLiteral("text/plain"),
                              {QUrl::fromLocalFile(target)}, &error),
             qPrintable(error));

    const QString launched = m_root.filePath(QStringLiteral("launched.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(launched), 5000);

    QByteArray content;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        QFile file(launched);
        content = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        return content.contains("editor-a.desktop") && content.contains("nota.txt");
    }()), 5000);
}

QTEST_GUILESS_MAIN(TestOpenWith)
#include "tst_openwith.moc"
