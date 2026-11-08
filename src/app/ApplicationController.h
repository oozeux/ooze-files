#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QPointer>
#include "core/UndoStack.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

class Config;
class DirectoryLoader;
class DeviceMonitor;
class FileOperations;
class PortalSettings;
class TabController;
class TabListModel;
class TerminalLauncher;
class ThemeStore;
class VfsRegistry;

class ApplicationController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel *files READ files NOTIFY filesChanged)
    Q_PROPERTY(QAbstractItemModel *tabs READ tabs CONSTANT)
    Q_PROPERTY(int activeTab READ activeTab NOTIFY activeTabChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabsChanged)
    Q_PROPERTY(bool canReopenTab READ canReopenTab NOTIFY reopenStateChanged)
    Q_PROPERTY(bool restoreSession READ restoreSession WRITE setRestoreSession NOTIFY restoreSessionChanged)
    Q_PROPERTY(bool singleInstance READ singleInstance WRITE setSingleInstance NOTIFY singleInstanceChanged)
    Q_PROPERTY(bool alwaysShowTabBar READ alwaysShowTabBar WRITE setAlwaysShowTabBar NOTIFY alwaysShowTabBarChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentUrlChanged)
    Q_PROPERTY(QVariantList breadcrumb READ breadcrumb NOTIFY currentUrlChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(QString freeSpaceText READ freeSpaceText NOTIFY currentUrlChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString pendingSelection READ pendingSelection NOTIFY pendingSelectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(bool canRenameSelection READ canRenameSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedIsDirectory READ selectedIsDirectory NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardCut READ clipboardCut NOTIFY clipboardChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(bool inVirtualLocation READ inVirtualLocation NOTIFY currentUrlChanged)
    Q_PROPERTY(bool inRecents READ inRecents NOTIFY currentUrlChanged)
    Q_PROPERTY(bool inTrash READ inTrash NOTIFY currentUrlChanged)
    Q_PROPERTY(QString virtualKind READ virtualKind NOTIFY currentUrlChanged)
    Q_PROPERTY(bool infoColumns READ infoColumns NOTIFY currentUrlChanged)

    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString paletteId READ paletteId WRITE setPaletteId NOTIFY paletteIdChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme NOTIFY iconThemeChanged)
    Q_PROPERTY(double uiFontScale READ uiFontScale NOTIFY uiFontScaleChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)
    Q_PROPERTY(bool effectiveDark READ effectiveDark NOTIFY effectiveDarkChanged)
    Q_PROPERTY(int iconRevision READ iconRevision NOTIFY iconRevisionChanged)
    Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY densityChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily NOTIFY uiFontFamilyChanged)
    Q_PROPERTY(QString fontFamilySetting READ fontFamilySetting WRITE setFontFamilySetting NOTIFY uiFontFamilyChanged)

    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool singleClickOpen READ singleClickOpen WRITE setSingleClickOpen NOTIFY singleClickOpenChanged)
    Q_PROPERTY(bool thumbnails READ thumbnails WRITE setThumbnails NOTIFY thumbnailsChanged)
    Q_PROPERTY(bool infoPanel READ infoPanel WRITE setInfoPanel NOTIFY infoPanelChanged)
    Q_PROPERTY(bool lightweight READ lightweight WRITE setLightweight NOTIFY lightweightChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString terminalCommand READ terminalCommand WRITE setTerminalCommand NOTIFY terminalCommandChanged)

public:
    explicit ApplicationController(Config *config, QObject *parent = nullptr);
    ~ApplicationController() override;

    void start(const QUrl &url, const QString &selectName = {}, bool explicitTarget = false);
    void saveSession();
    void flushSession();

    [[nodiscard]] QAbstractItemModel *files() const;
    [[nodiscard]] QAbstractItemModel *tabs() const;
    [[nodiscard]] int activeTab() const { return m_activeIndex; }
    [[nodiscard]] int tabCount() const { return int(m_tabs.size()); }
    [[nodiscard]] bool canReopenTab() const { return !m_closedTabs.isEmpty(); }
    [[nodiscard]] bool restoreSession() const;
    void setRestoreSession(bool enabled);
    [[nodiscard]] bool singleInstance() const;
    void setSingleInstance(bool enabled);
    [[nodiscard]] bool alwaysShowTabBar() const;
    void setAlwaysShowTabBar(bool enabled);

    [[nodiscard]] QUrl currentUrl() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QVariantList breadcrumb() const;
    [[nodiscard]] QVariantList places() const;
    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;
    [[nodiscard]] int itemCount() const;
    [[nodiscard]] QString freeSpaceText() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString pendingSelection() const;
    [[nodiscard]] int selectionCount() const;
    [[nodiscard]] int selectionRevision() const;
    [[nodiscard]] bool canRenameSelection() const;
    [[nodiscard]] bool selectedIsDirectory() const;
    [[nodiscard]] QString selectedName() const;
    [[nodiscard]] bool hasClipboard() const { return !m_clipboardUrls.isEmpty(); }
    [[nodiscard]] bool clipboardCut() const { return m_clipboardCut; }
    [[nodiscard]] bool canPaste() const;
    [[nodiscard]] bool inVirtualLocation() const;
    [[nodiscard]] bool inRecents() const;
    [[nodiscard]] bool inTrash() const;
    [[nodiscard]] QString virtualKind() const;
    [[nodiscard]] bool infoColumns() const;

    [[nodiscard]] QString themeMode() const;
    [[nodiscard]] QString paletteId() const;
    [[nodiscard]] QString iconTheme() const;
    [[nodiscard]] double uiFontScale() const;
    [[nodiscard]] bool systemDark() const;
    [[nodiscard]] bool effectiveDark() const;
    [[nodiscard]] int iconRevision() const { return m_iconRevision; }
    [[nodiscard]] QString density() const;
    [[nodiscard]] QString language() const;
    [[nodiscard]] QString iconSize() const;
    [[nodiscard]] QString uiFontFamily() const;
    [[nodiscard]] QString fontFamilySetting() const;
    [[nodiscard]] ThemeStore *themeStore() const { return m_theme; }

    [[nodiscard]] QString viewMode() const;
    [[nodiscard]] bool showHidden() const;
    [[nodiscard]] bool singleClickOpen() const;
    [[nodiscard]] bool thumbnails() const;
    [[nodiscard]] bool infoPanel() const;
    [[nodiscard]] bool lightweight() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;
    [[nodiscard]] int sortMode() const;
    [[nodiscard]] bool sortDescending() const;
    [[nodiscard]] QString filterText() const;
    [[nodiscard]] QString terminalCommand() const;

public slots:
    void goTo(const QString &url);
    void goToUrl(const QUrl &url);
    void goBack();
    void goForward();
    void goUp();
    void reload();

    void newTab();
    Q_INVOKABLE bool openNewWindow();
    void openInNewTab(const QString &url);
    void openRowInNewTab(int row);
    void activateTab(int index);
    void duplicateTab(int index);
    void closeTab(int index);
    void closeOtherTabs(int index);
    void closeTabsToRight(int index);
    void nextTab();
    void previousTab();
    void reopenClosedTab();
    void copyTabPath(int index);

    void openRow(int row);
    void openRowInTerminal(int row);
    void openCurrentInTerminal();
    void copyPath(int row);

    void selectRow(int row, bool toggle, bool range);
    void selectRows(const QVector<int> &rows, int mode);
    void selectAll();
    void clearSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void trashSelection();
    void deleteSelection();
    void renameSelection(const QString &newName);
    void createFolder(const QString &name);
    void createFile(const QString &name);

    Q_INVOKABLE QVariantList compressFormats();
    Q_INVOKABLE bool isArchiveRow(int row) const;
    Q_INVOKABLE void compressSelection(const QString &formatId);
    Q_INVOKABLE void compressSelectionAs(const QString &formatId, const QString &name);
    Q_INVOKABLE QString suggestedArchiveName() const;
    Q_INVOKABLE QVariantMap archiveTarget(const QString &name, const QString &formatId) const;
    Q_INVOKABLE void extractRow(int row);
    Q_INVOKABLE bool addPlace(const QString &url);
    Q_INVOKABLE bool addPlaceForRow(int row);
    Q_INVOKABLE bool addCurrentFolderToPlaces();
    Q_INVOKABLE void removePlace(const QString &url);
    Q_INVOKABLE bool movePlace(const QString &id, int insertIndex);
    Q_INVOKABLE void resolveConflict(int id, int decision, bool applyToAll);
    Q_INVOKABLE void showInFolder(int row);
    Q_INVOKABLE void restoreSelection();
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void confirmAction(const QString &id);
    Q_INVOKABLE void trashUrls(const QList<QUrl> &urls);
    Q_INVOKABLE bool movePlaceBy(const QString &id, int delta);
    Q_INVOKABLE bool canMovePlace(const QString &id, int delta) const;
    Q_INVOKABLE bool isCustomPlace(const QString &url) const;
    Q_INVOKABLE void openDevice(const QString &objectPath);
    Q_INVOKABLE void unmountDevice(const QString &objectPath);
    Q_INVOKABLE void ejectDevice(const QString &objectPath);
    Q_INVOKABLE QVariantList defaultAppStates() const;
    Q_INVOKABLE bool setDefaultApp(const QString &categoryId, const QString &appId);
    Q_INVOKABLE QVariantMap previewCurrentFolder() const;
    Q_INVOKABLE QVariantMap previewCurrentFolderWithSize() const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE QVariantList openWithApps(int row) const;
    Q_INVOKABLE QVariantList openWithOtherApps(int row) const;
    Q_INVOKABLE void openRowWith(int row, const QString &appId);
    Q_INVOKABLE QVariantMap previewRow(int row) const;
    Q_INVOKABLE QVariantMap previewRowWithSize(int row) const;
    Q_INVOKABLE QVariantList selectedEntries() const;
    Q_INVOKABLE QStringList currentNames() const;
    Q_INVOKABLE void renameEntries(const QVariantList &entries);
    Q_INVOKABLE void openSearchResult(const QString &url, bool isDirectory);

    Q_INVOKABLE bool isSelected(int row) const;
    Q_INVOKABLE int clipboardMark(const QString &url) const;
    Q_INVOKABLE QVector<int> selectedRows() const;
    Q_INVOKABLE QString suggestedFolderName() const;
    Q_INVOKABLE QString suggestedFileName() const;
    Q_INVOKABLE QString dragUriList(int row) const;
    Q_INVOKABLE void dropUrls(const QList<QUrl> &urls, int action, const QString &targetUrl);

    void setThemeMode(const QString &mode);
    void setPaletteId(const QString &id);
    void setDensity(const QString &density);
    void setLanguage(const QString &language);
    void setIconSize(const QString &size);
    void setViewMode(const QString &mode);
    void setShowHidden(bool show);
    void setSingleClickOpen(bool enabled);
    void setThumbnails(bool enabled);
    void setInfoPanel(bool enabled);
    void setLightweight(bool enabled);
    void setSortMode(int mode);
    void setSortDescending(bool descending);
    void setFilterText(const QString &text);
    void setTerminalCommand(const QString &command);
    void setFontFamilySetting(const QString &family);

    Q_INVOKABLE void openThemesFolder();

    Q_INVOKABLE void clearPendingSelection();
    Q_INVOKABLE int rowForName(const QString &name) const;

    void handleExternalOpen(const QUrl &url, bool select);

signals:
    void filesChanged();
    void tabsChanged();
    void activeTabChanged();
    void reopenStateChanged();
    void closeWindowRequested();
    void currentUrlChanged();
    void historyChanged();
    void itemCountChanged();
    void selectRowRequested(int row);
    void loadingChanged();
    void statusMessageChanged();
    void pendingSelectionChanged();
    void themeModeChanged();
    void paletteIdChanged();
    void iconThemeChanged();
    void uiFontScaleChanged();
    void uiFontFamilyChanged();
    void systemDarkChanged();
    void effectiveDarkChanged();
    void iconRevisionChanged();
    void densityChanged();
    void languageChanged();
    void iconSizeChanged();
    void viewModeChanged();
    void showHiddenChanged();
    void singleClickOpenChanged();
    void thumbnailsChanged();
    void infoPanelChanged();
    void lightweightChanged();
    void undoStateChanged();
    void placesChanged();
    void devicesChanged();
    void sortModeChanged();
    void sortDescendingChanged();
    void filterTextChanged();
    void terminalCommandChanged();
    void selectionChanged();
    void clipboardChanged();
    void restoreSessionChanged();
    void singleInstanceChanged();
    void alwaysShowTabBarChanged();
    void errorOccurred(const QString &message);
    void conflictRequested(const QVariantMap &info);
    void confirmationRequested(const QString &id, const QString &title, const QString &message,
                               const QString &acceptLabel);

private:
    struct ClosedTab {
        QUrl url;
        int index = 0;
    };

    [[nodiscard]] QString openWithMime(int row) const;
    TabController *createTab(const QUrl &url = {}, const QString &selectName = {});
    void connectTab(TabController *tab);
    void insertTab(TabController *tab, int index);
    void removeTabAt(int index);
    void activateTabInternal(int index);
    void rebuildTabModel();
    void syncTabEntry(int index);
    [[nodiscard]] int indexOfTab(const TabController *tab) const;
    void markOtherTabsDirty(const TabController *except);
    void refreshOperationTab();
    void setStatus(const QString &message);
    void applyProxySettings(TabController *tab);
    void bumpIconRevision();
    void clearCutClipboard();
    void publishClipboard(const QVector<QUrl> &urls, bool cut);
    void syncClipboard();
    void rebuildClipboardKeys();
    void rebuildPlaces();
    void requestTrashDeletion();
    QVector<QUrl> m_pendingTrashDelete;

    Config *m_config = nullptr;
    VfsRegistry *m_registry = nullptr;
    DirectoryLoader *m_loader = nullptr;
    TabListModel *m_tabModel = nullptr;
    QVector<TabController *> m_tabs;
    TabController *m_activeTab = nullptr;
    int m_activeIndex = -1;
    QVector<ClosedTab> m_closedTabs;

    FileOperations *m_operations = nullptr;
    TerminalLauncher *m_terminal = nullptr;
    ThemeStore *m_theme = nullptr;
    PortalSettings *m_portal = nullptr;
    QVariantList m_places;
    DeviceMonitor *m_deviceMonitor = nullptr;
    QString m_pendingDeviceOpen;
    UndoStack m_undo;
    bool m_undoBusy = false;
    QVariantList m_archiveFormats;
    bool m_archiveFormatsReady = false;

    bool m_systemDark = false;
    int m_iconRevision = 0;

    QVector<QUrl> m_clipboardUrls;
    QSet<QString> m_clipboardKeys;
    bool m_clipboardCut = false;
    bool m_publishingClipboard = false;
    bool m_pendingCutPaste = false;
    QPointer<TabController> m_operationTab;
};
