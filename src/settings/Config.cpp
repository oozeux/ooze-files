#include "settings/Config.h"

#include "theme/ThemeStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcConfig, "ooze.files.config")

namespace {
constexpr int kConfigVersion = 1;

const QStringList kThemeModes = {QStringLiteral("system"), QStringLiteral("light"),
                                 QStringLiteral("dark")};
const QStringList kLanguages = {QStringLiteral("system"), QStringLiteral("es"),
                                QStringLiteral("en")};
const QStringList kDensities = {QStringLiteral("compact"), QStringLiteral("normal"),
                                QStringLiteral("comfortable")};
const QStringList kIconSizes = {QStringLiteral("small"), QStringLiteral("normal"),
                                QStringLiteral("large"), QStringLiteral("huge")};
const QStringList kViewModes = {QStringLiteral("auto"), QStringLiteral("grid"),
                                QStringLiteral("list")};

bool isValidFontFamily(const QString &family)
{
    if (family.size() > 128)
        return false;

    for (const QChar character : family) {
        if (character.unicode() < 0x20 || character == QLatin1Char('/'))
            return false;
    }

    return true;
}
}

Config::Config(QObject *parent)
    : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    m_path = base + QStringLiteral("/ooze-files/config.json");
}

Config::~Config() = default;

QString Config::configPath() const
{
    return m_path;
}

bool Config::load()
{
    m_lastError.clear();

    QFile file(m_path);
    if (!file.exists()) {
        QDir().mkpath(QFileInfo(m_path).absolutePath());
        save();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("No se pudo leer %1: %2")
                          .arg(m_path, file.errorString());
        qCWarning(lcConfig) << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("config.json inválido (offset %1): %2")
                          .arg(parseError.offset)
                          .arg(parseError.errorString());
        qCWarning(lcConfig) << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    applyJson(doc.object().toVariantMap());
    qCInfo(lcConfig) << "Configuración cargada desde" << m_path;
    return true;
}

bool Config::save()
{
    if (!m_persistent)
        return true;

    QDir().mkpath(QFileInfo(m_path).absolutePath());

    QVariantMap root;
    root.insert(QStringLiteral("version"), kConfigVersion);

    QVariantMap ui;
    ui.insert(QStringLiteral("theme"), m_themeMode);
    ui.insert(QStringLiteral("palette"), m_paletteId);
    ui.insert(QStringLiteral("iconTheme"), m_iconTheme);
    ui.insert(QStringLiteral("fontScale"), m_fontScale);
    ui.insert(QStringLiteral("fontFamily"), m_fontFamily);
    ui.insert(QStringLiteral("iconSize"), m_iconSize);
    ui.insert(QStringLiteral("thumbnails"), m_thumbnails);
    ui.insert(QStringLiteral("infoPanel"), m_infoPanel);
    ui.insert(QStringLiteral("lightweight"), m_lightweight);
    ui.insert(QStringLiteral("density"), m_density);
    ui.insert(QStringLiteral("language"), m_language);
    ui.insert(QStringLiteral("viewMode"), m_viewMode);
    ui.insert(QStringLiteral("tabBarAlways"), m_alwaysShowTabBar);
    root.insert(QStringLiteral("ui"), ui);

    QVariantMap listing;
    listing.insert(QStringLiteral("showHidden"), m_showHidden);
    listing.insert(QStringLiteral("sortMode"), m_sortMode);
    listing.insert(QStringLiteral("sortDescending"), m_sortDescending);
    listing.insert(QStringLiteral("singleClickOpen"), m_singleClickOpen);
    root.insert(QStringLiteral("listing"), listing);

    QVariantMap behavior;
    behavior.insert(QStringLiteral("terminal"), m_terminalCommand);
    behavior.insert(QStringLiteral("lastPath"), m_lastPath);
    behavior.insert(QStringLiteral("singleInstance"), m_singleInstance);
    root.insert(QStringLiteral("behavior"), behavior);
    root.insert(QStringLiteral("places"), m_customPlaces);
    root.insert(QStringLiteral("placesOrder"), m_placesOrder);

    QVariantMap session;
    session.insert(QStringLiteral("restore"), m_restoreSession);
    session.insert(QStringLiteral("maxTabs"), m_maxTabs);
    session.insert(QStringLiteral("tabs"), m_sessionTabs);
    session.insert(QStringLiteral("active"), m_sessionActive);
    root.insert(QStringLiteral("session"), session);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("No se pudo escribir %1: %2")
                          .arg(m_path, file.errorString());
        qCWarning(lcConfig) << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    file.write(QJsonDocument(QJsonObject::fromVariantMap(root)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_lastError = QStringLiteral("No se pudo guardar %1: %2")
                          .arg(m_path, file.errorString());
        qCWarning(lcConfig) << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

void Config::applyJson(const QVariantMap &map)
{
    const int version = map.value(QStringLiteral("version"), 0).toInt();
    if (version > kConfigVersion) {
        qCWarning(lcConfig) << "config.json es de una versión futura:" << version;
    }

    const QVariantMap ui = map.value(QStringLiteral("ui")).toMap();
    const QVariantMap listing = map.value(QStringLiteral("listing")).toMap();
    const QVariantMap behavior = map.value(QStringLiteral("behavior")).toMap();
    const QVariantMap session = map.value(QStringLiteral("session")).toMap();

    const QString theme = ui.value(QStringLiteral("theme")).toString();
    if (kThemeModes.contains(theme))
        m_themeMode = theme;

    const QString palette = ui.value(QStringLiteral("palette")).toString();
    if (ThemeStore::isValidRef(palette))
        m_paletteId = palette;

    const QString iconTheme = ui.value(QStringLiteral("iconTheme")).toString().trimmed();
    if (iconTheme.size() <= 64 && !iconTheme.contains(QLatin1Char('/')))
        m_iconTheme = iconTheme;

    const double fontScale = ui.value(QStringLiteral("fontScale"), 1.0).toDouble();
    if (fontScale >= 0.75 && fontScale <= 2.0)
        m_fontScale = fontScale;

    const QString fontFamily = ui.value(QStringLiteral("fontFamily")).toString().trimmed();
    if (isValidFontFamily(fontFamily))
        m_fontFamily = fontFamily;

    const QString iconSize = ui.value(QStringLiteral("iconSize")).toString();
    if (kIconSizes.contains(iconSize))
        m_iconSize = iconSize;

    if (ui.contains(QStringLiteral("infoPanel")))
        m_infoPanel = ui.value(QStringLiteral("infoPanel")).toBool();

    if (ui.contains(QStringLiteral("lightweight")))
        m_lightweight = ui.value(QStringLiteral("lightweight")).toBool();

    if (ui.contains(QStringLiteral("thumbnails")))
        m_thumbnails = ui.value(QStringLiteral("thumbnails")).toBool();

    const QString density = ui.value(QStringLiteral("density")).toString();
    if (kDensities.contains(density))
        m_density = density;

    const QString language = ui.value(QStringLiteral("language")).toString();
    if (kLanguages.contains(language))
        m_language = language;

    const QString viewMode = ui.value(QStringLiteral("viewMode")).toString();
    if (kViewModes.contains(viewMode))
        m_viewMode = viewMode;

    if (ui.contains(QStringLiteral("tabBarAlways")))
        m_alwaysShowTabBar = ui.value(QStringLiteral("tabBarAlways")).toBool();

    if (listing.contains(QStringLiteral("showHidden")))
        m_showHidden = listing.value(QStringLiteral("showHidden")).toBool();

    const int sortMode = listing.value(QStringLiteral("sortMode"), 0).toInt();
    if (sortMode >= 0 && sortMode <= 3)
        m_sortMode = sortMode;

    if (listing.contains(QStringLiteral("sortDescending")))
        m_sortDescending = listing.value(QStringLiteral("sortDescending")).toBool();

    if (listing.contains(QStringLiteral("singleClickOpen")))
        m_singleClickOpen = listing.value(QStringLiteral("singleClickOpen")).toBool();

    m_customPlaces.clear();
    const QVariantList places = map.value(QStringLiteral("places")).toList();
    for (const QVariant &value : places) {
        if (m_customPlaces.size() >= kMaxCustomPlaces)
            break;

        const QVariantMap entry = value.toMap();
        const QString path = QDir::cleanPath(entry.value(QStringLiteral("path")).toString());
        if (!QDir::isAbsolutePath(path) || path == QLatin1String("/"))
            continue;

        bool duplicate = false;
        for (const QVariant &known : std::as_const(m_customPlaces)) {
            if (known.toMap().value(QStringLiteral("path")).toString() == path)
                duplicate = true;
        }
        if (duplicate)
            continue;

        QString name = entry.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty() || name.size() > 80)
            name = QFileInfo(path).fileName();

        QVariantMap place;
        place.insert(QStringLiteral("name"), name);
        place.insert(QStringLiteral("path"), path);
        m_customPlaces.append(place);
    }

    m_placesOrder.clear();
    const QVariantList order = map.value(QStringLiteral("placesOrder")).toList();
    for (const QVariant &value : order) {
        if (m_placesOrder.size() >= kMaxPlacesOrder)
            break;

        const QString path = QDir::cleanPath(value.toString());
        if (!QDir::isAbsolutePath(path) || m_placesOrder.contains(path))
            continue;
        m_placesOrder.append(path);
    }

    m_terminalCommand = behavior.value(QStringLiteral("terminal")).toString();
    m_lastPath = behavior.value(QStringLiteral("lastPath")).toString();
    if (behavior.contains(QStringLiteral("singleInstance")))
        m_singleInstance = behavior.value(QStringLiteral("singleInstance")).toBool();

    if (session.contains(QStringLiteral("restore")))
        m_restoreSession = session.value(QStringLiteral("restore")).toBool();

    const int maxTabs = session.value(QStringLiteral("maxTabs"), 16).toInt();
    if (maxTabs >= 1 && maxTabs <= 32)
        m_maxTabs = maxTabs;

    m_sessionTabs.clear();
    const QVariantList storedTabs = session.value(QStringLiteral("tabs")).toList();
    for (const QVariant &value : storedTabs) {
        if (m_sessionTabs.size() >= m_maxTabs)
            break;

        const QString text = value.toString();
        const QUrl url(text);
        if (text.isEmpty() || !url.isValid() || url.scheme().isEmpty())
            continue;
        m_sessionTabs.append(text);
    }

    const int active = session.value(QStringLiteral("active"), 0).toInt();
    m_sessionActive = m_sessionTabs.isEmpty()
        ? 0
        : qBound(0, active, int(m_sessionTabs.size()) - 1);
}

void Config::scheduleSave()
{
    QTimer::singleShot(300, this, [this] { save(); });
}

void Config::setThemeMode(const QString &mode)
{
    if (!kThemeModes.contains(mode) || m_themeMode == mode)
        return;
    m_themeMode = mode;
    emit themeModeChanged();
    scheduleSave();
}

void Config::setPaletteId(const QString &id)
{
    if (!ThemeStore::isValidRef(id) || m_paletteId == id)
        return;
    m_paletteId = id;
    emit paletteIdChanged();
    scheduleSave();
}

void Config::setIconTheme(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.size() > 64 || trimmed.contains(QLatin1Char('/')) || m_iconTheme == trimmed)
        return;
    m_iconTheme = trimmed;
    emit iconThemeChanged();
    scheduleSave();
}

void Config::setFontScale(double scale)
{
    const double clamped = qBound(0.75, scale, 2.0);
    if (qFuzzyCompare(m_fontScale, clamped))
        return;
    m_fontScale = clamped;
    emit fontScaleChanged();
    scheduleSave();
}

void Config::setFontFamily(const QString &family)
{
    const QString trimmed = family.trimmed();
    if (!isValidFontFamily(trimmed) || m_fontFamily == trimmed)
        return;
    m_fontFamily = trimmed;
    emit fontFamilyChanged();
    scheduleSave();
}

void Config::setIconSize(const QString &size)
{
    if (!kIconSizes.contains(size) || m_iconSize == size)
        return;
    m_iconSize = size;
    emit iconSizeChanged();
    scheduleSave();
}

void Config::setDensity(const QString &density)
{
    if (!kDensities.contains(density) || m_density == density)
        return;
    m_density = density;
    emit densityChanged();
    scheduleSave();
}

void Config::setLanguage(const QString &language)
{
    if (!kLanguages.contains(language) || m_language == language)
        return;
    m_language = language;
    emit languageChanged();
    scheduleSave();
}

void Config::setViewMode(const QString &mode)
{
    if (!kViewModes.contains(mode) || m_viewMode == mode)
        return;
    m_viewMode = mode;
    emit viewModeChanged();
    scheduleSave();
}

void Config::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    emit showHiddenChanged();
    scheduleSave();
}

void Config::setSortMode(int mode)
{
    if (mode < 0 || mode > 3 || m_sortMode == mode)
        return;
    m_sortMode = mode;
    emit sortModeChanged();
    scheduleSave();
}

void Config::setSortDescending(bool descending)
{
    if (m_sortDescending == descending)
        return;
    m_sortDescending = descending;
    emit sortDescendingChanged();
    scheduleSave();
}

bool Config::addPlace(const QString &path, const QString &name)
{
    const QString clean = QDir::cleanPath(path);
    if (!QDir::isAbsolutePath(clean) || clean == QLatin1String("/")
        || m_customPlaces.size() >= kMaxCustomPlaces)
        return false;

    for (const QVariant &known : std::as_const(m_customPlaces)) {
        if (known.toMap().value(QStringLiteral("path")).toString() == clean)
            return false;
    }

    QString label = name.trimmed();
    if (label.isEmpty() || label.size() > 80)
        label = QFileInfo(clean).fileName();

    QVariantMap place;
    place.insert(QStringLiteral("name"), label);
    place.insert(QStringLiteral("path"), clean);
    m_customPlaces.append(place);
    emit customPlacesChanged();
    scheduleSave();
    return true;
}

void Config::removePlace(const QString &path)
{
    const QString clean = QDir::cleanPath(path);
    for (int i = 0; i < m_customPlaces.size(); ++i) {
        if (m_customPlaces.at(i).toMap().value(QStringLiteral("path")).toString() == clean) {
            m_customPlaces.removeAt(i);
            if (m_placesOrder.removeAll(clean) > 0)
                emit placesOrderChanged();
            emit customPlacesChanged();
            scheduleSave();
            return;
        }
    }
}

void Config::setPlacesOrder(const QStringList &order)
{
    QStringList clean;
    for (const QString &value : order) {
        const QString path = QDir::cleanPath(value);
        if (clean.size() >= kMaxPlacesOrder)
            break;
        if (!QDir::isAbsolutePath(path) || clean.contains(path))
            continue;
        clean.append(path);
    }

    if (m_placesOrder == clean)
        return;
    m_placesOrder = clean;
    emit placesOrderChanged();
    scheduleSave();
}

void Config::setInfoPanel(bool enabled)
{
    if (m_infoPanel == enabled)
        return;
    m_infoPanel = enabled;
    emit infoPanelChanged();
    scheduleSave();
}

void Config::setLightweight(bool enabled)
{
    if (m_lightweight == enabled)
        return;
    m_lightweight = enabled;
    emit lightweightChanged();
    scheduleSave();
}

void Config::setThumbnails(bool enabled)
{
    if (m_thumbnails == enabled)
        return;
    m_thumbnails = enabled;
    emit thumbnailsChanged();
    scheduleSave();
}

void Config::setSingleClickOpen(bool enabled)
{
    if (m_singleClickOpen == enabled)
        return;
    m_singleClickOpen = enabled;
    emit singleClickOpenChanged();
    scheduleSave();
}

void Config::setTerminalCommand(const QString &command)
{
    if (m_terminalCommand == command)
        return;
    m_terminalCommand = command;
    emit terminalCommandChanged();
    scheduleSave();
}

void Config::setLastPath(const QString &path)
{
    if (m_lastPath == path)
        return;
    m_lastPath = path;
    emit lastPathChanged();
    scheduleSave();
}

void Config::setRestoreSession(bool enabled)
{
    if (m_restoreSession == enabled)
        return;
    m_restoreSession = enabled;
    emit restoreSessionChanged();
    scheduleSave();
}

void Config::setSingleInstance(bool enabled)
{
    if (m_singleInstance == enabled)
        return;
    m_singleInstance = enabled;
    emit singleInstanceChanged();
    scheduleSave();
}

void Config::setAlwaysShowTabBar(bool enabled)
{
    if (m_alwaysShowTabBar == enabled)
        return;
    m_alwaysShowTabBar = enabled;
    emit alwaysShowTabBarChanged();
    scheduleSave();
}

void Config::setMaxTabs(int count)
{
    const int clamped = qBound(1, count, 32);
    if (m_maxTabs == clamped)
        return;
    m_maxTabs = clamped;
    emit maxTabsChanged();
    scheduleSave();
}

void Config::setSession(const QVariantList &tabs, int active)
{
    const int clampedActive = tabs.isEmpty()
        ? 0
        : qBound(0, active, int(tabs.size()) - 1);

    if (m_sessionTabs == tabs && m_sessionActive == clampedActive)
        return;

    m_sessionTabs = tabs;
    m_sessionActive = clampedActive;
    emit sessionChanged();
    scheduleSave();
}
