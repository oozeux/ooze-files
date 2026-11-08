#include "settings/Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTest>

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void defaultsWhenMissing();
    void invalidJsonKeepsDefaults();
    void readsValidConfiguration();
    void persistsChanges();
    void rejectsUnknownEnumerations();
    void readsAndValidatesFontFamily();
    void readsAndValidatesIconSize();
    void thumbnailsDefaultOnAndPersist();
    void customPlacesAreValidatedAndPersisted();
    void placesOrderIsValidatedAndPersisted();
    void sessionDefaultsAndPersists();
    void sessionValidatesTabsAndLimit();

private:
    void writeConfig(const QByteArray &content);
};

void TestConfig::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestConfig::cleanupTestCase()
{
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                  + QStringLiteral("/ooze-files/config.json"));
}

void TestConfig::init()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/ooze-files/config.json");
    QFile::remove(path);
    QDir().mkpath(QFileInfo(path).absolutePath());
}

void TestConfig::writeConfig(const QByteArray &content)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/ooze-files/config.json");
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
}

void TestConfig::defaultsWhenMissing()
{
    Config config;
    QVERIFY(config.load());
    QCOMPARE(config.themeMode(), QStringLiteral("system"));
    QCOMPARE(config.density(), QStringLiteral("normal"));
    QCOMPARE(config.viewMode(), QStringLiteral("auto"));
    QCOMPARE(config.showHidden(), false);
    QCOMPARE(config.sortMode(), 0);
}

void TestConfig::invalidJsonKeepsDefaults()
{
    writeConfig("{ this is not json ");

    Config config;
    QVERIFY(!config.load());
    QVERIFY(!config.lastError().isEmpty());
    QCOMPARE(config.themeMode(), QStringLiteral("system"));
}

void TestConfig::readsValidConfiguration()
{
    writeConfig(QByteArrayLiteral(
        "{"
        "\"version\": 1,"
        "\"ui\": { \"theme\": \"dark\", \"density\": \"compact\", \"viewMode\": \"list\" },"
        "\"listing\": { \"showHidden\": true, \"sortMode\": 2, \"sortDescending\": true },"
        "\"behavior\": { \"terminal\": \"foot\", \"lastPath\": \"file:///tmp\" }"
        "}"));

    Config config;
    QVERIFY(config.load());
    QCOMPARE(config.themeMode(), QStringLiteral("dark"));
    QCOMPARE(config.density(), QStringLiteral("compact"));
    QCOMPARE(config.viewMode(), QStringLiteral("list"));
    QCOMPARE(config.showHidden(), true);
    QCOMPARE(config.sortMode(), 2);
    QCOMPARE(config.sortDescending(), true);
    QCOMPARE(config.terminalCommand(), QStringLiteral("foot"));
}

void TestConfig::persistsChanges()
{
    Config writer;
    QVERIFY(writer.load());
    writer.setThemeMode(QStringLiteral("light"));
    writer.setShowHidden(true);
    QVERIFY(writer.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.themeMode(), QStringLiteral("light"));
    QCOMPARE(reader.showHidden(), true);
}

void TestConfig::rejectsUnknownEnumerations()
{
    writeConfig(QByteArrayLiteral(
        "{ \"ui\": { \"theme\": \"neon\", \"density\": \"huge\", \"viewMode\": \"carousel\" } }"));

    Config config;
    QVERIFY(config.load());
    QCOMPARE(config.themeMode(), QStringLiteral("system"));
    QCOMPARE(config.density(), QStringLiteral("normal"));
    QCOMPARE(config.viewMode(), QStringLiteral("auto"));
}

void TestConfig::readsAndValidatesIconSize()
{
    Config defaults;
    QVERIFY(defaults.load());
    QCOMPARE(defaults.iconSize(), QStringLiteral("normal"));

    writeConfig(QByteArrayLiteral("{ \"ui\": { \"iconSize\": \"large\" } }"));

    Config config;
    QVERIFY(config.load());
    QCOMPARE(config.iconSize(), QStringLiteral("large"));

    config.setIconSize(QStringLiteral("gigante"));
    QCOMPARE(config.iconSize(), QStringLiteral("large"));

    config.setIconSize(QStringLiteral("small"));
    QVERIFY(config.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.iconSize(), QStringLiteral("small"));

    writeConfig(QByteArrayLiteral("{ \"ui\": { \"iconSize\": \"enorme\" } }"));

    Config invalid;
    QVERIFY(invalid.load());
    QCOMPARE(invalid.iconSize(), QStringLiteral("normal"));
}

void TestConfig::thumbnailsDefaultOnAndPersist()
{
    Config panelDefaults;
    QVERIFY(panelDefaults.load());
    QCOMPARE(panelDefaults.infoPanel(), false);
    panelDefaults.setInfoPanel(true);
    QVERIFY(panelDefaults.save());

    Config panelReader;
    QVERIFY(panelReader.load());
    QCOMPARE(panelReader.infoPanel(), true);
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                  + QStringLiteral("/ooze-files/config.json"));

    Config defaults;
    QVERIFY(defaults.load());
    QCOMPARE(defaults.thumbnails(), true);

    defaults.setThumbnails(false);
    QVERIFY(defaults.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.thumbnails(), false);

    writeConfig(QByteArrayLiteral("{ \"ui\": { \"thumbnails\": true } }"));

    Config enabled;
    QVERIFY(enabled.load());
    QCOMPARE(enabled.thumbnails(), true);
}

void TestConfig::customPlacesAreValidatedAndPersisted()
{
    Config config;
    QVERIFY(config.load());
    QVERIFY(config.customPlaces().isEmpty());

    QSignalSpy changed(&config, &Config::customPlacesChanged);

    QVERIFY(config.addPlace(QStringLiteral("/tmp/proyectos/")));
    QVERIFY(config.addPlace(QStringLiteral("/tmp/otra"), QStringLiteral("  Mi sitio  ")));
    QCOMPARE(changed.count(), 2);

    QVERIFY(!config.addPlace(QStringLiteral("/tmp/proyectos")));
    QVERIFY(!config.addPlace(QStringLiteral("relativa/ruta")));
    QVERIFY(!config.addPlace(QStringLiteral("/")));
    QVERIFY(!config.addPlace(QString()));
    QCOMPARE(changed.count(), 2);

    QCOMPARE(config.customPlaces().size(), 2);
    QCOMPARE(config.customPlaces().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("proyectos"));
    QCOMPARE(config.customPlaces().at(0).toMap().value(QStringLiteral("path")).toString(),
             QStringLiteral("/tmp/proyectos"));
    QCOMPARE(config.customPlaces().at(1).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Mi sitio"));

    QVERIFY(config.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.customPlaces().size(), 2);

    reader.removePlace(QStringLiteral("/tmp/proyectos"));
    QCOMPARE(reader.customPlaces().size(), 1);
    reader.removePlace(QStringLiteral("/no/existe"));
    QCOMPARE(reader.customPlaces().size(), 1);

    writeConfig(QByteArrayLiteral(
        "{ \"places\": [ {\"path\": \"/a\"}, {\"path\": \"/a/\"}, {\"path\": \"rel\"},"
        " {\"path\": \"/\"}, 7, {\"name\": \"x\"}, {\"path\": \"/b\", \"name\": \"B\"} ] }"));

    Config tolerant;
    QVERIFY(tolerant.load());
    QCOMPARE(tolerant.customPlaces().size(), 2);
    QCOMPARE(tolerant.customPlaces().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("a"));

    QString big = QStringLiteral("{ \"places\": [");
    for (int i = 0; i < Config::kMaxCustomPlaces + 20; ++i)
        big += QStringLiteral("{\"path\": \"/p%1\"},").arg(i);
    big += QStringLiteral("{\"path\": \"/ultimo\"} ] }");
    writeConfig(big.toUtf8());

    Config limited;
    QVERIFY(limited.load());
    QCOMPARE(limited.customPlaces().size(), Config::kMaxCustomPlaces);
    QVERIFY(!limited.addPlace(QStringLiteral("/otro-mas")));
}

void TestConfig::readsAndValidatesFontFamily()
{
    writeConfig(QByteArrayLiteral(
        "{ \"ui\": { \"fontFamily\": \"  DejaVu Sans  \" } }"));

    Config config;
    QVERIFY(config.load());
    QCOMPARE(config.fontFamily(), QStringLiteral("DejaVu Sans"));

    config.setFontFamily(QStringLiteral("Noto Sans"));
    QVERIFY(config.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.fontFamily(), QStringLiteral("Noto Sans"));

    writeConfig(QByteArrayLiteral(
        "{ \"ui\": { \"fontFamily\": \"ruta/con barra\" } }"));

    Config invalid;
    QVERIFY(invalid.load());
    QCOMPARE(invalid.fontFamily(), QString());

    invalid.setFontFamily(QString(200, QLatin1Char('a')));
    QCOMPARE(invalid.fontFamily(), QString());
}

void TestConfig::placesOrderIsValidatedAndPersisted()
{
    Config config;
    QVERIFY(config.load());
    QVERIFY(config.placesOrder().isEmpty());

    QSignalSpy changed(&config, &Config::placesOrderChanged);

    config.setPlacesOrder({QStringLiteral("/tmp/b/"), QStringLiteral("rel"),
                           QStringLiteral("/tmp/a"), QStringLiteral("/tmp/b")});
    QCOMPARE(config.placesOrder(), (QStringList{QStringLiteral("/tmp/b"), QStringLiteral("/tmp/a")}));
    QCOMPARE(changed.count(), 1);

    config.setPlacesOrder({QStringLiteral("/tmp/b"), QStringLiteral("/tmp/a")});
    QCOMPARE(changed.count(), 1);

    QVERIFY(config.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.placesOrder(), config.placesOrder());

    QVERIFY(reader.addPlace(QStringLiteral("/tmp/a")));
    reader.removePlace(QStringLiteral("/tmp/a"));
    QCOMPARE(reader.placesOrder(), (QStringList{QStringLiteral("/tmp/b")}));

    writeConfig(QByteArrayLiteral("{ \"placesOrder\": [ \"/x\", \"/x/\", \"rel\", 7, \"/y\" ] }"));
    Config tolerant;
    QVERIFY(tolerant.load());
    QCOMPARE(tolerant.placesOrder(), (QStringList{QStringLiteral("/x"), QStringLiteral("/y")}));
}

void TestConfig::sessionDefaultsAndPersists()
{
    Config defaults;
    QVERIFY(defaults.load());
    QCOMPARE(defaults.restoreSession(), true);
    QCOMPARE(defaults.alwaysShowTabBar(), false);
    QCOMPARE(defaults.maxTabs(), 16);
    QVERIFY(defaults.sessionTabs().isEmpty());
    QCOMPARE(defaults.sessionActiveTab(), 0);

    QSignalSpy sessionSpy(&defaults, &Config::sessionChanged);
    QSignalSpy restoreSpy(&defaults, &Config::restoreSessionChanged);
    QSignalSpy tabBarSpy(&defaults, &Config::alwaysShowTabBarChanged);

    const QVariantList tabs = {
        QStringLiteral("file:///tmp"),
        QStringLiteral("file:///home"),
    };
    defaults.setSession(tabs, 1);
    QCOMPARE(sessionSpy.count(), 1);
    QCOMPARE(defaults.sessionTabs(), tabs);
    QCOMPARE(defaults.sessionActiveTab(), 1);

    defaults.setRestoreSession(false);
    defaults.setAlwaysShowTabBar(true);
    QCOMPARE(restoreSpy.count(), 1);
    QCOMPARE(tabBarSpy.count(), 1);

    QVERIFY(defaults.save());

    Config reader;
    QVERIFY(reader.load());
    QCOMPARE(reader.restoreSession(), false);
    QCOMPARE(reader.alwaysShowTabBar(), true);
    QCOMPARE(reader.sessionTabs(), tabs);
    QCOMPARE(reader.sessionActiveTab(), 1);

    writeConfig(QByteArrayLiteral(
        "{ \"ui\": { \"tabBarAlways\": true },"
        " \"session\": { \"restore\": false,"
        " \"tabs\": [\"file:///a\", \"file:///b\"], \"active\": 1 } }"));

    Config stored;
    QVERIFY(stored.load());
    QCOMPARE(stored.restoreSession(), false);
    QCOMPARE(stored.alwaysShowTabBar(), true);
    QCOMPARE(stored.sessionTabs().size(), 2);
    QCOMPARE(stored.sessionActiveTab(), 1);
}

void TestConfig::sessionValidatesTabsAndLimit()
{
    Config config;
    QVERIFY(config.load());

    config.setMaxTabs(0);
    QCOMPARE(config.maxTabs(), 1);
    config.setMaxTabs(99);
    QCOMPARE(config.maxTabs(), 32);
    config.setMaxTabs(3);
    QCOMPARE(config.maxTabs(), 3);

    QString json = QStringLiteral("{ \"session\": { \"maxTabs\": 99, \"tabs\": [");
    for (int i = 0; i < 40; ++i) {
        if (i > 0)
            json += QLatin1Char(',');
        json += QStringLiteral("\"file:///p%1\"").arg(i);
    }
    json += QStringLiteral("], \"active\": 999 } }");
    writeConfig(json.toUtf8());

    Config limited;
    QVERIFY(limited.load());
    QCOMPARE(limited.maxTabs(), 16);
    QCOMPARE(limited.sessionTabs().size(), 16);
    QCOMPARE(limited.sessionActiveTab(), 15);

    writeConfig(QByteArrayLiteral(
        "{ \"session\": { \"maxTabs\": 0, \"tabs\": [\"file:///a\", 7, \"\"],"
        " \"active\": -5 } }"));

    Config invalid;
    QVERIFY(invalid.load());
    QCOMPARE(invalid.maxTabs(), 16);
    QCOMPARE(invalid.sessionTabs().size(), 1);
    QCOMPARE(invalid.sessionTabs().constFirst().toString(), QStringLiteral("file:///a"));
    QCOMPARE(invalid.sessionActiveTab(), 0);
}

QTEST_MAIN(TestConfig)
#include "tst_config.moc"
