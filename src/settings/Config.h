#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class Config : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString paletteId READ paletteId WRITE setPaletteId NOTIFY paletteIdChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme WRITE setIconTheme NOTIFY iconThemeChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(QString iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY densityChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
    Q_PROPERTY(bool singleClickOpen READ singleClickOpen WRITE setSingleClickOpen NOTIFY singleClickOpenChanged)
    Q_PROPERTY(bool thumbnails READ thumbnails WRITE setThumbnails NOTIFY thumbnailsChanged)
    Q_PROPERTY(bool infoPanel READ infoPanel WRITE setInfoPanel NOTIFY infoPanelChanged)
    Q_PROPERTY(bool lightweight READ lightweight WRITE setLightweight NOTIFY lightweightChanged)
    Q_PROPERTY(QString terminalCommand READ terminalCommand WRITE setTerminalCommand NOTIFY terminalCommandChanged)
    Q_PROPERTY(QString lastPath READ lastPath WRITE setLastPath NOTIFY lastPathChanged)
    Q_PROPERTY(QVariantList customPlaces READ customPlaces NOTIFY customPlacesChanged)
    Q_PROPERTY(bool restoreSession READ restoreSession WRITE setRestoreSession NOTIFY restoreSessionChanged)
    Q_PROPERTY(bool singleInstance READ singleInstance WRITE setSingleInstance NOTIFY singleInstanceChanged)
    Q_PROPERTY(bool alwaysShowTabBar READ alwaysShowTabBar WRITE setAlwaysShowTabBar NOTIFY alwaysShowTabBarChanged)

public:
    explicit Config(QObject *parent = nullptr);
    ~Config() override;

    bool load();
    bool save();

    [[nodiscard]] QString configPath() const;
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] bool isValid() const { return m_lastError.isEmpty(); }

    [[nodiscard]] QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &mode);

    [[nodiscard]] QString paletteId() const { return m_paletteId; }
    void setPaletteId(const QString &id);

    [[nodiscard]] QString iconTheme() const { return m_iconTheme; }
    void setIconTheme(const QString &name);

    [[nodiscard]] double fontScale() const { return m_fontScale; }
    void setFontScale(double scale);

    [[nodiscard]] QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);

    [[nodiscard]] QString iconSize() const { return m_iconSize; }
    void setIconSize(const QString &size);

    [[nodiscard]] QString density() const { return m_density; }
    void setDensity(const QString &density);
    [[nodiscard]] QString language() const { return m_language; }
    void setLanguage(const QString &language);

    [[nodiscard]] QString viewMode() const { return m_viewMode; }
    void setViewMode(const QString &mode);

    [[nodiscard]] bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);

    [[nodiscard]] int sortMode() const { return m_sortMode; }
    void setSortMode(int mode);

    [[nodiscard]] bool sortDescending() const { return m_sortDescending; }
    void setSortDescending(bool descending);

    [[nodiscard]] bool singleClickOpen() const { return m_singleClickOpen; }
    void setSingleClickOpen(bool enabled);

    [[nodiscard]] bool thumbnails() const { return m_thumbnails; }

    static constexpr int kMaxCustomPlaces = 64;
    [[nodiscard]] QVariantList customPlaces() const { return m_customPlaces; }
    bool addPlace(const QString &path, const QString &name = {});
    void removePlace(const QString &path);
    static constexpr int kMaxPlacesOrder = 160;
    [[nodiscard]] QStringList placesOrder() const { return m_placesOrder; }
    void setPlacesOrder(const QStringList &order);
    void setThumbnails(bool enabled);

    [[nodiscard]] bool infoPanel() const { return m_infoPanel; }
    void setInfoPanel(bool enabled);

    [[nodiscard]] bool lightweight() const { return m_lightweight; }
    void setLightweight(bool enabled);

    [[nodiscard]] QString terminalCommand() const { return m_terminalCommand; }
    void setTerminalCommand(const QString &command);

    [[nodiscard]] QString lastPath() const { return m_lastPath; }
    void setLastPath(const QString &path);

    [[nodiscard]] bool restoreSession() const { return m_restoreSession; }
    void setRestoreSession(bool enabled);
    [[nodiscard]] bool singleInstance() const { return m_singleInstance; }
    void setSingleInstance(bool enabled);
    void setPersistent(bool persistent) { m_persistent = persistent; }

    [[nodiscard]] bool alwaysShowTabBar() const { return m_alwaysShowTabBar; }
    void setAlwaysShowTabBar(bool enabled);

    [[nodiscard]] int maxTabs() const { return m_maxTabs; }
    void setMaxTabs(int count);

    [[nodiscard]] QVariantList sessionTabs() const { return m_sessionTabs; }
    [[nodiscard]] int sessionActiveTab() const { return m_sessionActive; }
    void setSession(const QVariantList &tabs, int active);

    void scheduleSave();

signals:
    void themeModeChanged();
    void paletteIdChanged();
    void iconThemeChanged();
    void fontScaleChanged();
    void fontFamilyChanged();
    void iconSizeChanged();
    void densityChanged();
    void languageChanged();
    void viewModeChanged();
    void showHiddenChanged();
    void sortModeChanged();
    void sortDescendingChanged();
    void singleClickOpenChanged();
    void thumbnailsChanged();
    void infoPanelChanged();
    void lightweightChanged();
    void customPlacesChanged();
    void placesOrderChanged();
    void terminalCommandChanged();
    void lastPathChanged();
    void restoreSessionChanged();
    void singleInstanceChanged();
    void alwaysShowTabBarChanged();
    void maxTabsChanged();
    void sessionChanged();
    void errorOccurred(const QString &message);

private:
    void applyJson(const QVariantMap &map);

    QString m_path;
    QString m_lastError;
    QString m_themeMode = QStringLiteral("system");
    QString m_paletteId = QStringLiteral("builtin");
    QString m_iconTheme;
    double m_fontScale = 1.0;
    QString m_fontFamily;
    QString m_iconSize = QStringLiteral("normal");
    QString m_density = QStringLiteral("normal");
    QString m_language = QStringLiteral("system");
    QString m_viewMode = QStringLiteral("auto");
    bool m_showHidden = false;
    int m_sortMode = 0;
    bool m_sortDescending = false;
    bool m_singleClickOpen = false;
    bool m_thumbnails = true;
    bool m_infoPanel = false;
    bool m_lightweight = false;
    QVariantList m_customPlaces;
    QStringList m_placesOrder;
    QString m_terminalCommand;
    QString m_lastPath;
    bool m_restoreSession = true;
    bool m_singleInstance = true;
    bool m_persistent = true;
    bool m_alwaysShowTabBar = false;
    int m_maxTabs = 16;
    QVariantList m_sessionTabs;
    int m_sessionActive = 0;
};
