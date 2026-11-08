#include "theme/ThemeStore.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void builtinPalettesMatchUi();
    void loadsCustomPalette();
    void partialThemeFallsBackPerToken();
    void derivesMissingShades();
    void rejectsInvalidFiles();
    void rejectsUnsafeIds();
    void listsChoices();
    void importsOozeShell();
    void importsQt6ct();
    void importsKde();
    void missingSystemSourcesFallBack();
    void reloadPicksUpChanges();
    void choicesListSystemSources();
    void systemAccentCompletesBuiltin();

private:
    static QString themesDir();
    static QString configDir();
    static QString dataDir();
    static QString cacheDir();
    static void writeFile(const QString &path, const QByteArray &content);
    static void writeTheme(const QString &file, const QByteArray &content);
};

QString TestTheme::themesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/ooze-files/themes");
}

QString TestTheme::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QString TestTheme::dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

QString TestTheme::cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
}

void TestTheme::writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

void TestTheme::writeTheme(const QString &file, const QByteArray &content)
{
    writeFile(themesDir() + QLatin1Char('/') + file, content);
}

void TestTheme::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestTheme::init()
{
    QDir(themesDir()).removeRecursively();
    QDir().mkpath(themesDir());

    QFile::remove(ThemeStore::oozeShellColorsPath());
    QFile::remove(configDir() + QStringLiteral("/qt6ct/qt6ct.conf"));
    QFile::remove(configDir() + QStringLiteral("/kdeglobals"));
    QDir(dataDir() + QStringLiteral("/color-schemes")).removeRecursively();
}

void TestTheme::builtinPalettesMatchUi()
{
    ThemeStore store;

    store.apply(QStringLiteral("builtin"), true);
    QCOMPARE(store.paletteId(), QStringLiteral("builtin"));
    QCOMPARE(store.paletteName(), QStringLiteral("Incorporado"));
    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#14161a")));
    QCOMPARE(store.surface(), QColor(QStringLiteral("#1c1f25")));
    QCOMPARE(store.surfaceHover(), QColor(QStringLiteral("#24282f")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#e6e8ec")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#7aa2ff")));
    QCOMPARE(store.radius(), 10);
    QCOMPARE(store.radiusSmall(), 6);
    QCOMPARE(store.borderWidth(), 1);
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.22f) < 0.01f);
    QCOMPARE(store.selectionBg().rgb(), store.accent().rgb());

    store.apply(QStringLiteral("builtin"), false);
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));
    QCOMPARE(store.surface(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#1c1f24")));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.12f) < 0.01f);
}

void TestTheme::loadsCustomPalette()
{
    writeTheme(QStringLiteral("bosque.json"),
               R"({
                   "schema": 1,
                   "name": "Bosque",
                   "light": {
                       "bg": "#f2f4ef",
                       "surface": "#ffffff",
                       "fg": "#1d211b",
                       "accent": "#3f7d4e"
                   },
                   "metrics": { "radius": 14, "radiusSmall": 8 }
               })");

    ThemeStore store;
    store.apply(QStringLiteral("bosque"), false);

    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.paletteId(), QStringLiteral("bosque"));
    QCOMPARE(store.paletteName(), QStringLiteral("Bosque"));
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f2f4ef")));
    QCOMPARE(store.surface(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#3f7d4e")));
    QCOMPARE(store.radius(), 14);
    QCOMPARE(store.radiusSmall(), 8);
    QCOMPARE(store.borderWidth(), 1);
    QCOMPARE(store.selectionBg().rgb(), QColor(QStringLiteral("#3f7d4e")).rgb());
}

void TestTheme::partialThemeFallsBackPerToken()
{
    writeTheme(QStringLiteral("nocturno.json"),
               R"({
                   "schema": 1,
                   "name": "Nocturno",
                   "dark": { "bg": "#101418", "fg": "#f0f2f5" }
               })");

    ThemeStore store;
    store.apply(QStringLiteral("nocturno"), true);
    QCOMPARE(store.bg(), QColor(QStringLiteral("#101418")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#f0f2f5")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#7aa2ff")));

    store.apply(QStringLiteral("nocturno"), false);
    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#1c1f24")));
    QCOMPARE(store.paletteName(), QStringLiteral("Nocturno"));
}

void TestTheme::derivesMissingShades()
{
    writeTheme(QStringLiteral("llano.json"),
               R"({
                   "schema": 1,
                   "light": {
                       "surface": "#808080",
                       "fg": "#000000",
                       "accent": "#ff0000"
                   }
               })");

    ThemeStore store;
    store.apply(QStringLiteral("llano"), false);

    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.paletteName(), QStringLiteral("llano"));
    QVERIFY(store.surfaceHover().lightnessF() < store.surface().lightnessF());
    QVERIFY(store.surfaceActive().lightnessF() < store.surfaceHover().lightnessF());
    QVERIFY(store.fgMuted().lightnessF() > store.fg().lightnessF());
    QCOMPARE(store.selectionBg().rgb(), QColor(QStringLiteral("#ff0000")).rgb());
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.12f) < 0.01f);
}

void TestTheme::rejectsInvalidFiles()
{
    writeTheme(QStringLiteral("roto.json"), "{ esto no es json");
    writeTheme(QStringLiteral("futuro.json"),
               R"({ "schema": 2, "light": { "bg": "#000000" } })");

    ThemeStore store;

    store.apply(QStringLiteral("roto"), false);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));

    store.apply(QStringLiteral("futuro"), false);
    QVERIFY(store.lastError().contains(QStringLiteral("schema")));
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));

    store.apply(QStringLiteral("inexistente"), false);
    QVERIFY(store.lastError().contains(QStringLiteral("No se encontró")));
}

void TestTheme::rejectsUnsafeIds()
{
    ThemeStore store;

    store.apply(QStringLiteral("../fuera"), false);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));

    store.apply(QStringLiteral("con espacio"), false);
    QVERIFY(!store.lastError().isEmpty());

    QVERIFY(!ThemeStore::isValidId(QStringLiteral("a/b")));
    QVERIFY(!ThemeStore::isValidId(QStringLiteral("..")));
    QVERIFY(ThemeStore::isValidId(QStringLiteral("bosque-oscuro_2")));
}

void TestTheme::listsChoices()
{
    writeTheme(QStringLiteral("bosque.json"),
               R"({ "schema": 1, "name": "Bosque", "light": {} })");
    writeTheme(QStringLiteral("sin-nombre.json"),
               R"({ "schema": 1, "light": {} })");
    writeTheme(QStringLiteral("mal id.json"), R"({ "schema": 1 })");
    writeTheme(QStringLiteral("notas.txt"), "hola");

    ThemeStore store;
    store.refreshChoices();

    const QVariantList choices = store.choices();
    QCOMPARE(choices.size(), 3);

    const QVariantMap first = choices.at(0).toMap();
    QCOMPARE(first.value(QStringLiteral("value")).toString(), QStringLiteral("builtin"));
    QCOMPARE(first.value(QStringLiteral("label")).toString(), QStringLiteral("Incorporado"));

    QCOMPARE(choices.at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Bosque"));

    const QVariantMap unnamed = choices.at(2).toMap();
    QCOMPARE(unnamed.value(QStringLiteral("value")).toString(), QStringLiteral("sin-nombre"));
    QCOMPARE(unnamed.value(QStringLiteral("label")).toString(), QStringLiteral("sin-nombre"));
}

void TestTheme::importsOozeShell()
{
    writeFile(ThemeStore::oozeShellColorsPath(), R"({
        "colors": {
            "surface":                 { "dark": { "color": "#0c141b" }, "light": { "color": "#f6f9ff" } },
            "surface_container":       { "dark": { "color": "#182127" }, "light": { "color": "#e6eff8" } },
            "surface_container_low":   { "dark": { "color": "#141d23" }, "light": { "color": "#ecf4fe" } },
            "surface_container_high":  { "dark": { "color": "#232b32" }, "light": { "color": "#e1e9f3" } },
            "surface_container_highest": { "dark": { "color": "#2d363d" }, "light": { "color": "#dbe3ed" } },
            "outline_variant":         { "dark": { "color": "#3f484f" }, "light": { "color": "#bfc7d1" } },
            "on_surface":              { "dark": { "color": "#dbe3ed" }, "light": { "color": "#141d23" } },
            "on_surface_variant":      { "dark": { "color": "#bfc7d1" }, "light": { "color": "#3f484f" } },
            "primary":                 { "dark": { "color": "#8cceff" }, "light": { "color": "#006492" } },
            "error":                   { "dark": { "color": "#ffb4ab" }, "light": { "color": "#ba1a1a" } }
        }
    })");

    ThemeStore store;

    store.apply(QStringLiteral("system:ooze"), true);
    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.paletteId(), QStringLiteral("system:ooze"));
    QCOMPARE(store.paletteName(), QStringLiteral("OozeShell"));
    QCOMPARE(store.bg(), QColor(QStringLiteral("#0c141b")));
    QCOMPARE(store.surface(), QColor(QStringLiteral("#182127")));
    QCOMPARE(store.surfaceAlt(), QColor(QStringLiteral("#141d23")));
    QCOMPARE(store.surfaceHover(), QColor(QStringLiteral("#232b32")));
    QCOMPARE(store.surfaceActive(), QColor(QStringLiteral("#2d363d")));
    QCOMPARE(store.border(), QColor(QStringLiteral("#3f484f")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#dbe3ed")));
    QCOMPARE(store.fgMuted(), QColor(QStringLiteral("#bfc7d1")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#8cceff")));
    QCOMPARE(store.danger(), QColor(QStringLiteral("#ffb4ab")));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.22f) < 0.01f);

    store.apply(QStringLiteral("system:ooze"), false);
    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f6f9ff")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#006492")));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.12f) < 0.01f);
}

void TestTheme::importsQt6ct()
{
    const QString scheme = configDir() + QStringLiteral("/qt6ct/colors/Prueba.conf");
    writeFile(scheme, R"([ColorScheme]
active_colors=#ffeeeeee, #ff111111, #ff222222, #ff333333, #ff444444, #ff555555, #ffdddddd, #ff666666, #ff777777, #ffabcdef, #ff010203, #ff888888, #ff999999, #ffaaaaaa, #ffbbbbbb, #ffcccccc, #ff121212, #ff131313, #ff141414, #ff151515, #ff161616, #ff123456
inactive_colors=#ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000, #ff000000
)");

    writeFile(configDir() + QStringLiteral("/qt6ct/qt6ct.conf"),
              QStringLiteral("[Appearance]\ncolor_scheme_path=%1\n").arg(scheme).toUtf8());

    ThemeStore store;
    store.apply(QStringLiteral("system:qt6ct"), true);

    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.paletteName(), QStringLiteral("qt6ct: Prueba"));
    QCOMPARE(store.bg(), QColor(QStringLiteral("#010203")));
    QCOMPARE(store.surface(), QColor(QStringLiteral("#abcdef")));
    QCOMPARE(store.surfaceAlt(), QColor(QStringLiteral("#111111")));
    QCOMPARE(store.fg(), QColor(QStringLiteral("#eeeeee")));
    QCOMPARE(store.fgMuted(), QColor(QStringLiteral("#161616")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#123456")));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.22f) < 0.01f);
}

void TestTheme::importsKde()
{
    const QString scheme = dataDir() + QStringLiteral("/color-schemes/Prueba.colors");
    writeFile(scheme, R"([General]
ColorScheme=Prueba
Name=Prueba

[Colors:Window]
BackgroundNormal=10,20,30
BackgroundAlternate=15,25,35
ForegroundNormal=200,210,220
ForegroundInactive=120,130,140
DecorationFocus=1,2,3

[Colors:View]
BackgroundNormal=40,50,60
ForegroundNormal=200,210,220
ForegroundNegative=250,60,60

[Colors:Button]
BackgroundNormal=70,80,90

[Colors:Selection]
BackgroundNormal=5,150,250
)");

    writeFile(configDir() + QStringLiteral("/kdeglobals"),
              "[General]\nColorScheme=Prueba\n");

    ThemeStore store;
    store.apply(QStringLiteral("system:kde"), true);

    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.paletteName(), QStringLiteral("KDE: Prueba"));
    QCOMPARE(store.bg(), QColor(10, 20, 30));
    QCOMPARE(store.surface(), QColor(40, 50, 60));
    QCOMPARE(store.surfaceAlt(), QColor(70, 80, 90));
    QCOMPARE(store.fg(), QColor(200, 210, 220));
    QCOMPARE(store.fgMuted(), QColor(120, 130, 140));
    QCOMPARE(store.accent(), QColor(5, 150, 250));
    QCOMPARE(store.danger(), QColor(250, 60, 60));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.22f) < 0.01f);
}

void TestTheme::missingSystemSourcesFallBack()
{
    ThemeStore store;

    store.apply(QStringLiteral("system:ooze"), false);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#f5f6f8")));

    store.apply(QStringLiteral("system:qt6ct"), true);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#14161a")));

    store.apply(QStringLiteral("system:kde"), true);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(store.bg(), QColor(QStringLiteral("#14161a")));

    store.apply(QStringLiteral("system:otro"), true);
    QVERIFY(store.lastError().contains(QStringLiteral("desconocida")));
}

void TestTheme::reloadPicksUpChanges()
{
    writeFile(ThemeStore::oozeShellColorsPath(), R"({
        "colors": { "surface": { "dark": { "color": "#112233" } } }
    })");

    ThemeStore store;
    store.apply(QStringLiteral("system:ooze"), true);
    QCOMPARE(store.bg(), QColor(QStringLiteral("#112233")));

    writeFile(ThemeStore::oozeShellColorsPath(), R"({
        "colors": { "surface": { "dark": { "color": "#445566" } } }
    })");

    store.reload();
    QCOMPARE(store.bg(), QColor(QStringLiteral("#445566")));
    QVERIFY(store.lastError().isEmpty());
}

void TestTheme::choicesListSystemSources()
{
    writeFile(ThemeStore::oozeShellColorsPath(), R"({ "colors": { "surface": { "dark": { "color": "#000000" } } } })");

    const QString scheme = configDir() + QStringLiteral("/qt6ct/colors/Prueba.conf");
    writeFile(scheme, "[ColorScheme]\nactive_colors=#ff000000\n");
    writeFile(configDir() + QStringLiteral("/qt6ct/qt6ct.conf"),
              QStringLiteral("[Appearance]\ncolor_scheme_path=%1\n").arg(scheme).toUtf8());

    writeFile(dataDir() + QStringLiteral("/color-schemes/Prueba.colors"),
              "[General]\nColorScheme=Prueba\n");
    writeFile(configDir() + QStringLiteral("/kdeglobals"),
              "[General]\nColorScheme=Prueba\n");

    ThemeStore store;
    store.refreshChoices();

    QStringList values;
    for (const QVariant &choice : store.choices())
        values.append(choice.toMap().value(QStringLiteral("value")).toString());

    QCOMPARE(values,
             QStringList({QStringLiteral("builtin"), QStringLiteral("system:ooze"),
                          QStringLiteral("system:qt6ct"), QStringLiteral("system:kde")}));
}

void TestTheme::systemAccentCompletesBuiltin()
{
    ThemeStore store;
    store.apply(QStringLiteral("builtin"), true);

    const QColor original = store.accent();
    store.setSystemAccent(QColor(QStringLiteral("#ff8800")));

    QCOMPARE(store.accent(), QColor(QStringLiteral("#ff8800")));
    QVERIFY(qAbs(store.selectionBg().alphaF() - 0.22f) < 0.01f);

    writeTheme(QStringLiteral("acento.json"),
               R"({ "schema": 1, "dark": { "accent": "#00ff00" } })");

    store.apply(QStringLiteral("acento"), true);
    store.setSystemAccent(QColor(QStringLiteral("#0000ff")));
    QCOMPARE(store.accent(), QColor(QStringLiteral("#00ff00")));

    store.apply(QStringLiteral("builtin"), true);
    QCOMPARE(store.accent(), QColor(QStringLiteral("#0000ff")));

    store.setSystemAccent(QColor());
    QCOMPARE(store.accent(), original);
}

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"