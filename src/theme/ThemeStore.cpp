#include "theme/ThemeStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>

#include <QLoggingCategory>
#include <QCoreApplication>

Q_LOGGING_CATEGORY(lcTheme, "ooze.files.theme")

namespace {

constexpr qint64 kMaxThemeBytes = 256 * 1024;
constexpr int kMaxRadius = 32;
constexpr int kMaxBorderWidth = 8;

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QStringList dataDirs()
{
    QStringList dirs;
    const QString writable =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!writable.isEmpty())
        dirs.append(writable);

    for (const QString &dir : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
        if (!dirs.contains(dir))
            dirs.append(dir);
    }

    return dirs;
}

bool isSafeName(const QString &name)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 ._-]{0,63}$"));
    return pattern.match(name).hasMatch();
}

QColor mix(const QColor &a, const QColor &b, double amount)
{
    return QColor::fromRgbF(a.redF() * (1.0 - amount) + b.redF() * amount,
                            a.greenF() * (1.0 - amount) + b.greenF() * amount,
                            a.blueF() * (1.0 - amount) + b.blueF() * amount);
}

QColor selection(const QColor &accent, bool dark)
{
    QColor color = accent;
    color.setAlphaF(dark ? 0.22 : 0.12);
    return color;
}

bool isDarkColor(const QColor &color)
{
    return color.lightnessF() < 0.5;
}

QColor opaqueOr(const QColor &color, const QColor &fallback)
{
    if (!color.isValid())
        return fallback;
    return color.alpha() == 255 ? color : fallback;
}

QColor parseColor(const QString &text)
{
    return QColor::fromString(text.trimmed());
}

QColor parseKdeColor(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return {};

    int channels[4] = {0, 0, 0, 255};
    for (int i = 0; i < parts.size() && i < 4; ++i) {
        bool ok = false;
        const int value = parts.at(i).trimmed().toInt(&ok);
        if (!ok || value < 0 || value > 255)
            return {};
        channels[i] = value;
    }

    return QColor(channels[0], channels[1], channels[2], channels[3]);
}

QByteArray readSmallFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("Ooze", "No se pudo leer %1: %2").arg(path, file.errorString());
        return {};
    }

    if (file.size() > kMaxThemeBytes) {
        *error = QStringLiteral("%1 supera %2 KiB")
                     .arg(path)
                     .arg(kMaxThemeBytes / 1024);
        return {};
    }

    const QByteArray data = file.readAll();
    if (data.trimmed().isEmpty())
        *error = QCoreApplication::translate("Ooze", "%1 está vacío").arg(path);

    return data;
}

QString qt6ctConfigPath()
{
    return configDir() + QStringLiteral("/qt6ct/qt6ct.conf");
}

QString qt6ctSchemePath(QStringList *sources)
{
    const QString config = qt6ctConfigPath();
    if (QFileInfo::exists(config))
        sources->append(config);

    QSettings settings(config, QSettings::IniFormat);

    QString path = settings.value(QStringLiteral("Appearance/color_scheme_path")).toString();
    if (path.startsWith(QLatin1Char('~')))
        path.replace(0, 1, QDir::homePath());

    if (!path.isEmpty() && path.endsWith(QLatin1String(".conf"))
        && QFileInfo(path).isFile()) {
        sources->append(path);
        return path;
    }

    const QString name = settings.value(QStringLiteral("Appearance/color_scheme")).toString();
    if (!isSafeName(name))
        return {};

    QStringList dirs;
    dirs.append(configDir() + QStringLiteral("/qt6ct/colors"));
    for (const QString &dir : dataDirs())
        dirs.append(dir + QStringLiteral("/qt6ct/colors"));

    for (const QString &dir : dirs) {
        const QString candidate = dir + QLatin1Char('/') + name + QStringLiteral(".conf");
        if (QFileInfo(candidate).isFile()) {
            sources->append(candidate);
            return candidate;
        }
    }

    return {};
}

QString kdeGlobalsPath()
{
    return configDir() + QStringLiteral("/kdeglobals");
}

QString kdeSchemePath(QString *schemeName, QStringList *sources)
{
    const QString globals = kdeGlobalsPath();
    if (QFileInfo::exists(globals))
        sources->append(globals);

    QSettings settings(globals, QSettings::IniFormat);

    // Plasma 6 guarda el esquema en [KDE]; versiones anteriores en [General].
    // QSettings trata [General] como raíz, así que también se prueba sin grupo.
    QString name = settings.value(QStringLiteral("KDE/ColorScheme")).toString();
    if (name.isEmpty())
        name = settings.value(QStringLiteral("General/ColorScheme")).toString();
    if (name.isEmpty())
        name = settings.value(QStringLiteral("ColorScheme")).toString();

    if (!isSafeName(name))
        return {};

    for (const QString &dir : dataDirs()) {
        const QString candidate =
            dir + QStringLiteral("/color-schemes/") + name + QStringLiteral(".colors");
        if (QFileInfo(candidate).isFile()) {
            sources->append(candidate);
            *schemeName = name;
            return candidate;
        }
    }

    return {};
}

}

ThemeStore::ThemeStore(QObject *parent)
    : QObject(parent)
{
    m_palette = builtinPalette(false);
    refreshChoices();

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(200);
    connect(&m_reloadTimer, &QTimer::timeout, this, &ThemeStore::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this] { m_reloadTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_reloadTimer.start(); });
}

void ThemeStore::apply(const QString &paletteId, bool dark)
{
    const QString requested = paletteId.isEmpty() ? QStringLiteral("builtin") : paletteId;

    Palette palette = builtinPalette(dark);
    QString name = QCoreApplication::translate("Ooze", "Incorporado");
    QString error;
    QStringList sources;

    if (requested == QLatin1String("builtin")) {
        // tema incorporado
    } else if (requested.startsWith(QLatin1String("system:"))) {
        const QString kind = requested.sliced(7);
        bool ok = false;

        if (kind == QLatin1String("ooze"))
            ok = loadOozeShell(dark, &palette, &name, &sources, &error);
        else if (kind == QLatin1String("qt6ct"))
            ok = loadQt6ct(dark, &palette, &name, &sources, &error);
        else if (kind == QLatin1String("kde"))
            ok = loadKde(dark, &palette, &name, &sources, &error);
        else
            error = QCoreApplication::translate("Ooze", "Fuente de paleta desconocida: %1").arg(kind);

        if (!ok) {
            palette = builtinPalette(dark);
            name = QCoreApplication::translate("Ooze", "Incorporado");
            sources.clear();
        }
    } else if (!isValidId(requested)) {
        error = QCoreApplication::translate("Ooze", "Nombre de tema no válido: %1").arg(requested);
    } else {
        const QString path = themesDirectory() + QLatin1Char('/') + requested
            + QStringLiteral(".json");
        if (!QFileInfo::exists(path)) {
            error = QCoreApplication::translate("Ooze", "No se encontró el tema %1").arg(requested);
        } else if (!loadFile(path, dark, requested, &palette, &name, &error)) {
            palette = builtinPalette(dark);
            name = QCoreApplication::translate("Ooze", "Incorporado");
        }
    }

    if (requested == QLatin1String("builtin") && m_systemAccent.isValid()) {
        palette.accent = m_systemAccent;
        palette.selectionBg = selection(palette.accent, dark);
    }

    m_requested = requested;
    m_dark = dark;
    m_palette = palette;
    m_name = name;

    qCInfo(lcTheme) << "Paleta aplicada:" << m_name << "(" << m_requested << ")";

    emit paletteChanged();
    setError(error);
    updateWatchPaths(sources);
}

void ThemeStore::reload()
{
    apply(m_requested, m_dark);
}

void ThemeStore::setSystemAccent(const QColor &accent)
{
    const QColor color = accent.isValid() ? accent : QColor();
    if (color == m_systemAccent)
        return;

    m_systemAccent = color;

    // El acento del escritorio solo completa el tema incorporado: un tema
    // propio o una fuente del sistema ya traen el suyo y no se pisan.
    if (m_requested == QLatin1String("builtin"))
        apply(m_requested, m_dark);
}

bool ThemeStore::isValidId(const QString &id)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"));
    return pattern.match(id).hasMatch();
}

bool ThemeStore::isValidRef(const QString &ref)
{
    return ref == QLatin1String("builtin") || ref == QLatin1String("system:ooze")
        || ref == QLatin1String("system:qt6ct") || ref == QLatin1String("system:kde")
        || isValidId(ref);
}

QString ThemeStore::themesDirectory()
{
    return configDir() + QStringLiteral("/ooze-files/themes");
}

QString ThemeStore::oozeShellColorsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/oozeshell/colors.json");
}

void ThemeStore::refreshChoices()
{
    QVariantList choices;
    choices.append(QVariantMap{{QStringLiteral("value"), QStringLiteral("builtin")},
                               {QStringLiteral("label"), QCoreApplication::translate("Ooze", "Incorporado")}});

    if (QFileInfo::exists(oozeShellColorsPath())) {
        choices.append(QVariantMap{{QStringLiteral("value"), QStringLiteral("system:ooze")},
                                   {QStringLiteral("label"), QStringLiteral("OozeShell")}});
    }

    QStringList ignored;
    if (!qt6ctSchemePath(&ignored).isEmpty()) {
        choices.append(QVariantMap{{QStringLiteral("value"), QStringLiteral("system:qt6ct")},
                                   {QStringLiteral("label"), QStringLiteral("qt6ct")}});
    }

    QString schemeName;
    if (!kdeSchemePath(&schemeName, &ignored).isEmpty()) {
        choices.append(QVariantMap{{QStringLiteral("value"), QStringLiteral("system:kde")},
                                   {QStringLiteral("label"), QStringLiteral("KDE")}});
    }

    const QDir dir(themesDirectory());
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

    for (const QString &file : files) {
        const QString id = QFileInfo(file).completeBaseName();
        if (!isValidId(id))
            continue;

        QString label = id;
        QFile themeFile(dir.filePath(file));
        if (themeFile.open(QIODevice::ReadOnly) && themeFile.size() <= kMaxThemeBytes) {
            const QJsonDocument doc = QJsonDocument::fromJson(themeFile.readAll());
            const QString name = doc.object().value(QStringLiteral("name")).toString().trimmed();
            if (!name.isEmpty())
                label = name;
        }

        choices.append(QVariantMap{{QStringLiteral("value"), id},
                                   {QStringLiteral("label"), label}});
    }

    if (choices == m_choices)
        return;

    m_choices = choices;
    emit choicesChanged();
}

void ThemeStore::updateWatchPaths(const QStringList &sources)
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    QSet<QString> dirs;
    for (const QString &path : sources) {
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

        const QString dir = QFileInfo(path).absolutePath();
        if (dir != configDir() && QFileInfo(dir).isDir())
            dirs.insert(dir);
    }

    for (const QString &dir : dirs)
        m_watcher.addPath(dir);
}

ThemeStore::Palette ThemeStore::builtinPalette(bool dark)
{
    Palette palette;

    if (dark) {
        palette.bg = QColor(QStringLiteral("#14161a"));
        palette.surface = QColor(QStringLiteral("#1c1f25"));
        palette.surfaceAlt = QColor(QStringLiteral("#181b21"));
        palette.surfaceHover = QColor(QStringLiteral("#24282f"));
        palette.surfaceActive = QColor(QStringLiteral("#2c313a"));
        palette.border = QColor(QStringLiteral("#2b3038"));
        palette.fg = QColor(QStringLiteral("#e6e8ec"));
        palette.fgMuted = QColor(QStringLiteral("#939aa6"));
        palette.accent = QColor(QStringLiteral("#7aa2ff"));
        palette.danger = QColor(QStringLiteral("#ff7b7b"));
    } else {
        palette.bg = QColor(QStringLiteral("#f5f6f8"));
        palette.surface = QColor(QStringLiteral("#ffffff"));
        palette.surfaceAlt = QColor(QStringLiteral("#eef0f3"));
        palette.surfaceHover = QColor(QStringLiteral("#e9ecf1"));
        palette.surfaceActive = QColor(QStringLiteral("#dfe3ea"));
        palette.border = QColor(QStringLiteral("#dde1e7"));
        palette.fg = QColor(QStringLiteral("#1c1f24"));
        palette.fgMuted = QColor(QStringLiteral("#6b7280"));
        palette.accent = QColor(QStringLiteral("#3d6fe0"));
        palette.danger = QColor(QStringLiteral("#c93c3c"));
    }

    palette.selectionBg = selection(palette.accent, dark);
    return palette;
}

bool ThemeStore::loadOozeShell(bool dark, Palette *out, QString *name, QStringList *sources,
                               QString *error)
{
    const QString path = oozeShellColorsPath();
    sources->append(path);

    if (!QFileInfo::exists(path)) {
        *error = QCoreApplication::translate("Ooze", "No se encontró la paleta de OozeShell (%1)").arg(path);
        return false;
    }

    const QByteArray data = readSmallFile(path, error);
    if (data.isEmpty())
        return false;

    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QCoreApplication::translate("Ooze", "La paleta de OozeShell no es un JSON válido: %1")
                     .arg(parseError.errorString());
        return false;
    }

    const QJsonObject colors = doc.object().value(QStringLiteral("colors")).toObject();
    if (colors.isEmpty()) {
        *error = QCoreApplication::translate("Ooze", "La paleta de OozeShell no contiene roles de color");
        return false;
    }

    const QString mode = dark ? QStringLiteral("dark") : QStringLiteral("light");
    const auto role = [&](const char *key) {
        const QJsonObject entry = colors.value(QLatin1String(key)).toObject();
        QColor color = parseColor(entry.value(mode).toObject()
                                      .value(QStringLiteral("color"))
                                      .toString());
        if (!color.isValid()) {
            color = parseColor(entry.value(QStringLiteral("default"))
                                   .toObject()
                                   .value(QStringLiteral("color"))
                                   .toString());
        }
        return color;
    };

    const QColor surface = role("surface");
    if (!surface.isValid()) {
        *error = QCoreApplication::translate("Ooze", "La paleta de OozeShell no tiene el rol surface");
        return false;
    }

    Palette palette = builtinPalette(dark);
    palette.bg = surface;

    const QColor onSurface = role("on_surface");
    if (onSurface.isValid())
        palette.fg = onSurface;

    const QColor primary = role("primary");
    if (primary.isValid())
        palette.accent = primary;

    const QColor container = role("surface_container");
    palette.surface = container.isValid() ? container : palette.bg;

    const QColor low = role("surface_container_low");
    palette.surfaceAlt = low.isValid()
        ? low
        : (dark ? mix(palette.surface, palette.bg, 0.5)
                : mix(palette.surface, palette.fg, 0.08));

    const QColor high = role("surface_container_high");
    palette.surfaceHover = high.isValid()
        ? high
        : mix(palette.surface, palette.fg, dark ? 0.045 : 0.085);

    const QColor highest = role("surface_container_highest");
    palette.surfaceActive = highest.isValid()
        ? highest
        : mix(palette.surface, palette.fg, dark ? 0.08 : 0.13);

    const QColor outline = role("outline_variant");
    palette.border = outline.isValid() ? outline
                                       : mix(palette.bg, palette.fg, dark ? 0.14 : 0.10);

    const QColor muted = role("on_surface_variant");
    palette.fgMuted = muted.isValid() ? muted : mix(palette.fg, palette.bg, 0.35);

    const QColor danger = role("error");
    palette.danger = danger.isValid() ? danger : palette.danger;

    palette.selectionBg = selection(palette.accent, dark);

    *name = QStringLiteral("OozeShell");
    *out = palette;
    error->clear();
    return true;
}

bool ThemeStore::loadQt6ct(bool dark, Palette *out, QString *name, QStringList *sources,
                           QString *error)
{
    const QString path = qt6ctSchemePath(sources);
    if (path.isEmpty()) {
        *error = QCoreApplication::translate("Ooze", "qt6ct no tiene un esquema de colores activo");
        return false;
    }

    const QByteArray data = readSmallFile(path, error);
    if (data.isEmpty())
        return false;

    QStringList values;
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.startsWith("active_colors=")) {
            const QString list = QString::fromUtf8(line.mid(14));
            for (const QString &part : list.split(QLatin1Char(',')))
                values.append(part.trimmed());
            break;
        }
    }

    QList<QColor> colors;
    for (const QString &value : values)
        colors.append(parseColor(value));

    if (colors.size() < 21) {
        *error = QCoreApplication::translate("Ooze", "El esquema de qt6ct %1 no tiene los 21 colores esperados")
                     .arg(QFileInfo(path).completeBaseName());
        return false;
    }

    const auto at = [&](int index) {
        return index < colors.size() ? colors.at(index) : QColor();
    };

    Palette palette = builtinPalette(dark);
    const QColor window = at(10);
    const QColor base = at(9);

    if (!window.isValid() && !base.isValid()) {
        *error = QCoreApplication::translate("Ooze", "El esquema de qt6ct %1 no tiene colores válidos")
                     .arg(QFileInfo(path).completeBaseName());
        return false;
    }

    palette.bg = window.isValid() ? window : base;
    palette.surface = base.isValid() ? base : palette.bg;
    palette.fg = opaqueOr(at(0), palette.fg);
    palette.surfaceAlt = opaqueOr(at(1), palette.surface);

    const QColor placeholder = at(20);
    const QColor mid = at(5);
    palette.fgMuted = opaqueOr(placeholder, opaqueOr(mid, palette.fgMuted));

    const QColor accent = at(21).isValid() ? at(21) : at(12);
    palette.accent = accent.isValid() ? accent : palette.accent;

    const bool sourceDark = isDarkColor(palette.bg);
    palette.border = mix(palette.bg, palette.fg, sourceDark ? 0.14 : 0.10);
    palette.surfaceHover = mix(palette.surface, palette.fg, sourceDark ? 0.045 : 0.085);
    palette.surfaceActive = mix(palette.surface, palette.fg, sourceDark ? 0.08 : 0.13);
    palette.selectionBg = selection(palette.accent, sourceDark);

    const QString scheme = QFileInfo(path).completeBaseName();
    *name = scheme.isEmpty() ? QStringLiteral("qt6ct")
                             : QStringLiteral("qt6ct: %1").arg(scheme);
    *out = palette;
    error->clear();
    return true;
}

bool ThemeStore::loadKde(bool dark, Palette *out, QString *name, QStringList *sources,
                         QString *error)
{
    QString schemeName;
    const QString path = kdeSchemePath(&schemeName, sources);
    if (path.isEmpty()) {
        *error = QCoreApplication::translate("Ooze", "KDE no tiene un esquema de colores activo");
        return false;
    }

    QSettings scheme(path, QSettings::IniFormat);
    const auto color = [&](const char *group, const char *key) {
        const QVariant value = scheme.value(QLatin1String(group) + QLatin1Char('/')
                                            + QLatin1String(key));
        // QSettings interpreta «10,20,30» como lista; hay que reconstruir el texto.
        if (value.metaType().id() == QMetaType::QStringList)
            return parseKdeColor(value.toStringList().join(QLatin1Char(',')));

        return parseKdeColor(value.toString());
    };

    const QColor window = color("Colors:Window", "BackgroundNormal");
    const QColor view = color("Colors:View", "BackgroundNormal");
    if (!window.isValid() && !view.isValid()) {
        *error = QCoreApplication::translate("Ooze", "El esquema de KDE %1 no tiene colores válidos")
                     .arg(schemeName);
        return false;
    }

    Palette palette = builtinPalette(dark);
    palette.bg = window.isValid() ? window : view;
    palette.surface = view.isValid() ? view : palette.bg;

    palette.fg = color("Colors:Window", "ForegroundNormal").isValid()
        ? color("Colors:Window", "ForegroundNormal")
        : color("Colors:View", "ForegroundNormal");
    if (!palette.fg.isValid())
        palette.fg = builtinPalette(dark).fg;

    const QColor button = color("Colors:Button", "BackgroundNormal");
    const QColor alternate = color("Colors:Window", "BackgroundAlternate");
    palette.surfaceAlt = button.isValid() ? button
                                          : (alternate.isValid() ? alternate
                                                                 : palette.surface);

    const QColor inactive = color("Colors:Window", "ForegroundInactive");
    if (inactive.isValid())
        palette.fgMuted = inactive;

    const QColor selected = color("Colors:Selection", "BackgroundNormal");
    const QColor focus = color("Colors:Window", "DecorationFocus");
    palette.accent = selected.isValid() ? selected : (focus.isValid() ? focus : palette.accent);

    const QColor negative = color("Colors:View", "ForegroundNegative");
    if (negative.isValid())
        palette.danger = negative;

    const bool sourceDark = isDarkColor(palette.bg);
    palette.border = mix(palette.bg, palette.fg, sourceDark ? 0.14 : 0.10);
    palette.surfaceHover = mix(palette.surface, palette.fg, sourceDark ? 0.045 : 0.085);
    palette.surfaceActive = mix(palette.surface, palette.fg, sourceDark ? 0.08 : 0.13);
    palette.selectionBg = selection(palette.accent, sourceDark);

    *name = schemeName.isEmpty() ? QStringLiteral("KDE")
                                 : QStringLiteral("KDE: %1").arg(schemeName);
    *out = palette;
    error->clear();
    return true;
}

bool ThemeStore::loadFile(const QString &path, bool dark, const QString &id,
                          Palette *out, QString *name, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("Ooze", "No se pudo leer el tema %1: %2").arg(id, file.errorString());
        return false;
    }

    if (file.size() > kMaxThemeBytes) {
        *error = QCoreApplication::translate("Ooze", "El tema %1 supera %2 KiB")
                     .arg(id)
                     .arg(kMaxThemeBytes / 1024);
        return false;
    }

    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QCoreApplication::translate("Ooze", "Tema %1 inválido: %2").arg(id, parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schema")).toInt() != 1) {
        *error = QCoreApplication::translate("Ooze", "Tema %1: solo se admite schema 1").arg(id);
        return false;
    }

    const QString themeName = root.value(QStringLiteral("name")).toString().trimmed();
    if (!themeName.isEmpty())
        *name = themeName;
    else
        *name = id;

    const QJsonObject mode = root.value(dark ? QStringLiteral("dark")
                                             : QStringLiteral("light"))
                                 .toObject();

    const auto color = [&](const QString &key, const QColor &fallback) {
        const QJsonValue value = mode.value(key);
        if (value.isUndefined())
            return fallback;

        const QColor parsed = QColor::fromString(value.toString());
        if (!parsed.isValid()) {
            qCWarning(lcTheme) << "Tema" << id << "color inválido en" << key
                               << ":" << value.toString();
            return fallback;
        }

        return parsed;
    };

    Palette palette = builtinPalette(dark);
    palette.bg = color(QStringLiteral("bg"), palette.bg);
    palette.surface = color(QStringLiteral("surface"), palette.surface);
    palette.fg = color(QStringLiteral("fg"), palette.fg);
    palette.accent = color(QStringLiteral("accent"), palette.accent);
    palette.surfaceAlt = color(QStringLiteral("surfaceAlt"),
                               dark ? mix(palette.surface, palette.bg, 0.5)
                                    : mix(palette.surface, palette.fg, 0.08));
    palette.surfaceHover = color(QStringLiteral("surfaceHover"),
                                 mix(palette.surface, palette.fg, dark ? 0.045 : 0.085));
    palette.surfaceActive = color(QStringLiteral("surfaceActive"),
                                  mix(palette.surface, palette.fg, dark ? 0.08 : 0.13));
    palette.border = color(QStringLiteral("border"),
                           mix(palette.bg, palette.fg, dark ? 0.14 : 0.10));
    palette.fgMuted = color(QStringLiteral("fgMuted"), mix(palette.fg, palette.bg, 0.35));
    palette.danger = color(QStringLiteral("danger"), palette.danger);
    palette.selectionBg = selection(palette.accent, dark);

    const auto metric = [&](const QJsonObject &metrics, const QString &key, int fallback,
                            int maximum) {
        const QJsonValue value = metrics.value(key);
        if (value.isUndefined())
            return fallback;

        const int parsed = value.toInt(fallback);
        if (parsed < 0 || parsed > maximum) {
            qCWarning(lcTheme) << "Tema" << id << "métrica fuera de rango en" << key
                               << ":" << parsed;
            return fallback;
        }

        return parsed;
    };

    const QJsonObject metrics = root.value(QStringLiteral("metrics")).toObject();
    palette.radius = metric(metrics, QStringLiteral("radius"), palette.radius, kMaxRadius);
    palette.radiusSmall = metric(metrics, QStringLiteral("radiusSmall"), palette.radiusSmall,
                                 kMaxRadius);
    palette.borderWidth = metric(metrics, QStringLiteral("borderWidth"), palette.borderWidth,
                                 kMaxBorderWidth);

    *out = palette;
    error->clear();
    return true;
}

void ThemeStore::setError(const QString &error)
{
    if (m_lastError == error)
        return;

    m_lastError = error;
    if (!error.isEmpty())
        qCWarning(lcTheme) << error;

    emit lastErrorChanged();
}
