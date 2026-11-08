#include "app/ApplicationController.h"

#include "app/I18n.h"
#include "app/TabController.h"
#include "app/TabListModel.h"
#include "core/Archive.h"
#include "core/Devices.h"
#include "core/DirectoryLoader.h"
#include "core/FileListModel.h"
#include "core/FileListProxyModel.h"
#include "core/FileOperations.h"
#include "core/FilePreview.h"
#include "core/OpenWith.h"
#include "core/PathUtils.h"
#include "core/Places.h"
#include "core/TrashService.h"
#include "core/TerminalLauncher.h"
#include "settings/Config.h"
#include "theme/PortalSettings.h"
#include "theme/ThemeStore.h"
#include "vfs/VfsRegistry.h"

#include <algorithm>
#include <utility>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QGuiApplication>
#include <QLocale>
#include <QMimeData>
#include <QMimeDatabase>
#include <QStyleHints>

#include <QLoggingCategory>
#include <QCoreApplication>
#include <QProcess>

Q_LOGGING_CATEGORY(lcApp, "ooze.files.app")

namespace {
QString countTextHelper(int count)
{
    return count == 1 ? QCoreApplication::translate("Ooze", "1 elemento") : QCoreApplication::translate("Ooze", "%1 elementos").arg(count);
}

QString mimeForIndex(const QModelIndex &index)
{
    const QString known = index.data(FileListModel::MimeRole).toString();
    if (!known.isEmpty())
        return known;

    const QUrl url(index.data(FileListModel::UrlRole).toString());
    return url.isLocalFile() ? QMimeDatabase().mimeTypeForFile(url.toLocalFile()).name()
                             : QString();
}
}

ApplicationController::ApplicationController(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_registry(new VfsRegistry(this))
    , m_loader(new DirectoryLoader(m_registry, this))
    , m_tabModel(new TabListModel(this))
    , m_operations(new FileOperations(this))
    , m_terminal(new TerminalLauncher(this))
    , m_theme(new ThemeStore(this))
    , m_portal(new PortalSettings(this))
    , m_places(Places::build(config->customPlaces(), config->placesOrder()))
{
    m_activeTab = createTab();
    m_tabs.append(m_activeTab);
    m_activeIndex = 0;
    m_activeTab->activate();
    rebuildTabModel();

    connect(m_operations, &FileOperations::conflictRequested, this, [this](const QVariantMap &info) {
        QVariantMap details = info;
        const auto sizeText = [](qint64 bytes) {
            return bytes < 0 ? QString() : PathUtils::formatSize(bytes);
        };
        const auto dateText = [](qint64 milliseconds) {
            return QLocale().toString(QDateTime::fromMSecsSinceEpoch(milliseconds), QLocale::ShortFormat);
        };
        details.insert(QStringLiteral("sourceSizeText"), sizeText(info.value(QStringLiteral("sourceSize")).toLongLong()));
        details.insert(QStringLiteral("targetSizeText"), sizeText(info.value(QStringLiteral("targetSize")).toLongLong()));
        details.insert(QStringLiteral("sourceModifiedText"), dateText(info.value(QStringLiteral("sourceModified")).toLongLong()));
        details.insert(QStringLiteral("targetModifiedText"), dateText(info.value(QStringLiteral("targetModified")).toLongLong()));
        emit conflictRequested(details);
    });
    connect(m_operations, &FileOperations::operationStarted, this,
            [this](const QString &description) {
                if (m_operationTab.isNull())
                    m_operationTab = m_activeTab;
                setStatus(description);
            });
    connect(m_operations, &FileOperations::operationProgress, this,
            [this](const QString &description) { setStatus(description); });
    connect(m_operations, &FileOperations::operationFinished, this,
            [this](int action, bool success, const QString &message,
                   const QString &createdName, bool createdIsDirectory) {
                setStatus(message);

                QPointer<TabController> origin = m_operationTab;
                m_operationTab.clear();

                if (!success) {
                    m_pendingCutPaste = false;
                    emit errorOccurred(message);
                    if (origin)
                        origin->refreshAfterOperation();
                    else if (m_activeTab)
                        m_activeTab->refresh();
                    markOtherTabsDirty(origin);
                    return;
                }

                if (action == FileOperations::Move && m_pendingCutPaste)
                    clearCutClipboard();
                m_pendingCutPaste = false;

                if (!createdName.isEmpty()) {
                    TabController *target = origin ? origin.data() : m_activeTab;
                    if (target)
                        target->setPendingSelection(createdName);
                }

                if (origin)
                    origin->refreshAfterOperation();
                else if (m_activeTab)
                    m_activeTab->refresh();
                markOtherTabsDirty(origin);

                Q_UNUSED(createdIsDirectory)
            });

    m_deviceMonitor = new DeviceMonitor(this);
    connect(m_deviceMonitor, &DeviceMonitor::devicesChanged, this,
            &ApplicationController::devicesChanged);
    connect(m_deviceMonitor, &DeviceMonitor::mounted, this,
            [this](const QString &objectPath, const QString &mountPoint) {
                setStatus(tr("Montado en %1").arg(mountPoint));
                if (m_pendingDeviceOpen == objectPath) {
                    m_pendingDeviceOpen.clear();
                    goToUrl(QUrl::fromLocalFile(mountPoint));
                }
            });
    connect(m_deviceMonitor, &DeviceMonitor::finished, this,
            [this](const QString &message) { setStatus(message); });
    connect(m_deviceMonitor, &DeviceMonitor::errorOccurred, this,
            [this](const QString &message) {
                m_pendingDeviceOpen.clear();
                setStatus(message);
                emit errorOccurred(message);
            });
    m_deviceMonitor->start();

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged, this, &ApplicationController::syncClipboard);
        syncClipboard();
    }

    connect(m_config, &Config::customPlacesChanged, this, &ApplicationController::rebuildPlaces);
    connect(m_config, &Config::placesOrderChanged, this, &ApplicationController::rebuildPlaces);

    connect(m_operations, &FileOperations::changeRecorded, this,
            [this](int kind, const QString &label, const QStringList &firsts,
                   const QStringList &seconds) {
                JournalEntry entry;
                entry.kind = static_cast<JournalEntry::Kind>(kind);
                entry.label = label;
                entry.firsts = firsts;
                entry.seconds = seconds;
                m_undo.push(entry);
                emit undoStateChanged();
            });
    connect(m_operations, &FileOperations::journalApplied, this,
            [this](bool undo, bool success, const QString &, const QStringList &firsts,
                   const QStringList &seconds) {
                m_undoBusy = false;
                if (success) {
                    JournalEntry entry = undo ? m_undo.nextUndo() : m_undo.nextRedo();
                    entry.firsts = firsts;
                    entry.seconds = seconds;
                    if (undo)
                        m_undo.commitUndo(entry);
                    else
                        m_undo.commitRedo(entry);
                }
                emit undoStateChanged();

                if (success)
                    refreshOperationTab();
            });

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this,
            &ApplicationController::clipboardChanged);

    connect(m_config, &Config::themeModeChanged,
            this, &ApplicationController::themeModeChanged);
    connect(m_config, &Config::densityChanged,
            this, &ApplicationController::densityChanged);
    connect(m_config, &Config::languageChanged, this, [this] {
        I18n::apply(m_config->language());
        rebuildPlaces();
        emit languageChanged();
    });
    connect(m_config, &Config::iconSizeChanged,
            this, &ApplicationController::iconSizeChanged);
    connect(m_config, &Config::showHiddenChanged, this, [this] {
        for (TabController *tab : m_tabs)
            tab->setShowHidden(m_config->showHidden());
        if (m_activeTab)
            m_activeTab->clearSelection();
        emit showHiddenChanged();
    });
    connect(m_config, &Config::singleClickOpenChanged,
            this, &ApplicationController::singleClickOpenChanged);
    connect(m_config, &Config::thumbnailsChanged,
            this, &ApplicationController::thumbnailsChanged);
    connect(m_config, &Config::infoPanelChanged,
            this, &ApplicationController::infoPanelChanged);
    connect(m_config, &Config::sortModeChanged, this, [this] {
        for (TabController *tab : m_tabs)
            tab->setSortMode(m_config->sortMode());
        emit sortModeChanged();
    });
    connect(m_config, &Config::sortDescendingChanged, this, [this] {
        for (TabController *tab : m_tabs)
            tab->setSortDescending(m_config->sortDescending());
        emit sortDescendingChanged();
    });
    connect(m_config, &Config::viewModeChanged,
            this, &ApplicationController::viewModeChanged);
    connect(m_config, &Config::restoreSessionChanged,
            this, &ApplicationController::restoreSessionChanged);
    connect(m_config, &Config::singleInstanceChanged,
            this, &ApplicationController::singleInstanceChanged);
    connect(m_config, &Config::alwaysShowTabBarChanged,
            this, &ApplicationController::alwaysShowTabBarChanged);
    connect(m_config, &Config::errorOccurred, this, &ApplicationController::errorOccurred);

    if (QStyleHints *hints = QGuiApplication::styleHints()) {
        m_systemDark = hints->colorScheme() == Qt::ColorScheme::Dark;
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
            const bool dark = scheme == Qt::ColorScheme::Dark;
            if (dark == m_systemDark)
                return;
            m_systemDark = dark;
            emit systemDarkChanged();
            emit effectiveDarkChanged();
            bumpIconRevision();
        });
    }

    connect(m_portal, &PortalSettings::accentChanged, this, [this](const QColor &accent) {
        m_theme->setSystemAccent(accent);
    });
    connect(m_portal, &PortalSettings::colorSchemeChanged, this, [this](int scheme) {
        const QStyleHints *hints = QGuiApplication::styleHints();
        if (!hints || hints->colorScheme() != Qt::ColorScheme::Unknown)
            return;
        if (scheme != 1 && scheme != 2)
            return;

        const bool dark = scheme == 1;
        if (dark == m_systemDark)
            return;

        m_systemDark = dark;
        emit systemDarkChanged();
        emit effectiveDarkChanged();
        bumpIconRevision();
    });

    m_theme->apply(m_config->paletteId(), effectiveDark());

    connect(m_config, &Config::paletteIdChanged, this, [this] {
        m_theme->apply(m_config->paletteId(), effectiveDark());
        emit paletteIdChanged();
    });
    connect(m_config, &Config::iconThemeChanged, this, [this] {
        emit iconThemeChanged();
        bumpIconRevision();
    });
    connect(m_config, &Config::fontScaleChanged, this, &ApplicationController::uiFontScaleChanged);
    connect(m_config, &Config::fontFamilyChanged, this,
            &ApplicationController::uiFontFamilyChanged);
    connect(this, &ApplicationController::effectiveDarkChanged, this, [this] {
        m_theme->apply(m_config->paletteId(), effectiveDark());
    });
    connect(m_theme, &ThemeStore::lastErrorChanged, this, [this] {
        if (!m_theme->lastError().isEmpty())
            setStatus(m_theme->lastError());
    });
}

ApplicationController::~ApplicationController()
{
    // DirectoryLoader worker tasks hold backend pointers owned by the
    // registry. Destroying the loader first cancels and joins those tasks
    // while the registry is still alive.
    delete m_loader;
    m_loader = nullptr;
}

TabController *ApplicationController::createTab(const QUrl &url, const QString &selectName)
{
    auto *tab = new TabController(m_loader, this);
    applyProxySettings(tab);
    connectTab(tab);

    if (url.isValid())
        tab->start(url, selectName);
    else if (!selectName.isEmpty())
        tab->setPendingSelection(selectName);

    return tab;
}

void ApplicationController::connectTab(TabController *tab)
{
    connect(tab, &TabController::currentUrlChanged, this, [this, tab] {
        const int index = indexOfTab(tab);
        if (index >= 0)
            syncTabEntry(index);

        if (tab != m_activeTab)
            return;

        m_config->setLastPath(tab->currentUrl().toString());
        emit currentUrlChanged();
        saveSession();
    });

    connect(tab, &TabController::titleChanged, this, [this, tab] {
        const int index = indexOfTab(tab);
        if (index >= 0)
            syncTabEntry(index);
    });

    connect(tab, &TabController::historyChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit historyChanged();
    });
    connect(tab, &TabController::itemCountChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit itemCountChanged();
    });
    connect(tab, &TabController::selectionChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit selectionChanged();
    });
    connect(tab, &TabController::statusMessageChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit statusMessageChanged();
    });
    connect(tab, &TabController::pendingSelectionChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit pendingSelectionChanged();
    });
    connect(tab, &TabController::filterTextChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit filterTextChanged();
    });
    connect(tab, &TabController::loadingChanged, this, [this, tab] {
        if (tab == m_activeTab)
            emit loadingChanged();
    });
    connect(tab, &TabController::selectRowRequested, this, [this, tab](int row) {
        if (tab == m_activeTab)
            emit selectRowRequested(row);
    });
}

void ApplicationController::insertTab(TabController *tab, int index)
{
    index = qBound(0, index, int(m_tabs.size()));
    m_tabs.insert(index, tab);

    if (m_activeTab && index <= m_activeIndex)
        ++m_activeIndex;

    rebuildTabModel();
    emit tabsChanged();
    saveSession();
}

void ApplicationController::removeTabAt(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    TabController *tab = m_tabs.takeAt(index);

    if (tab == m_activeTab) {
        m_activeTab = nullptr;
        m_activeIndex = qMin(index, int(m_tabs.size()) - 1);
        if (m_activeIndex >= 0) {
            m_activeTab = m_tabs.at(m_activeIndex);
            m_activeTab->activate();
        }
    } else if (index < m_activeIndex) {
        --m_activeIndex;
    }

    tab->deleteLater();
    rebuildTabModel();
    emit tabsChanged();
    saveSession();
}

void ApplicationController::activateTabInternal(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    if (index == m_activeIndex)
        return;

    TabController *old = m_activeTab;
    m_activeTab = nullptr;
    if (old)
        old->deactivate();

    m_activeIndex = index;
    m_activeTab = m_tabs.at(index);
    m_activeTab->activate();
    m_tabModel->setActive(index);

    emit filesChanged();
    emit currentUrlChanged();
    emit historyChanged();
    emit itemCountChanged();
    emit loadingChanged();
    emit statusMessageChanged();
    emit pendingSelectionChanged();
    emit selectionChanged();
    emit filterTextChanged();
    emit activeTabChanged();

    m_config->setLastPath(m_activeTab->currentUrl().toString());
    saveSession();
}

void ApplicationController::rebuildTabModel()
{
    QVector<TabListModel::Entry> entries;
    entries.reserve(m_tabs.size());

    for (int index = 0; index < m_tabs.size(); ++index) {
        TabController *tab = m_tabs.at(index);
        TabListModel::Entry entry;
        entry.title = tab->title();
        entry.url = tab->currentUrl();
        entry.active = index == m_activeIndex;
        entries.append(entry);
    }

    m_tabModel->setEntries(entries);
}

void ApplicationController::syncTabEntry(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    TabController *tab = m_tabs.at(index);
    m_tabModel->updateEntry(index, tab->title(), tab->currentUrl());
}

int ApplicationController::indexOfTab(const TabController *tab) const
{
    for (int index = 0; index < m_tabs.size(); ++index) {
        if (m_tabs.at(index) == tab)
            return index;
    }
    return -1;
}

void ApplicationController::markOtherTabsDirty(const TabController *except)
{
    for (TabController *tab : m_tabs) {
        if (tab != except)
            tab->markDirty();
    }
}

void ApplicationController::refreshOperationTab()
{
    QPointer<TabController> origin = m_operationTab;
    m_operationTab.clear();

    if (origin)
        origin->refreshAfterOperation();
    else if (m_activeTab)
        m_activeTab->refresh();

    markOtherTabsDirty(origin);
}

void ApplicationController::applyProxySettings(TabController *tab)
{
    tab->setShowHidden(m_config->showHidden());
    tab->setSortMode(m_config->sortMode());
    tab->setSortDescending(m_config->sortDescending());
}

void ApplicationController::start(const QUrl &url, const QString &selectName,
                                  bool explicitTarget)
{
    if (!m_activeTab)
        return;

    if (!explicitTarget && m_config->restoreSession()) {
        QVector<QUrl> restored;
        const int limit = m_config->maxTabs();
        const int storedActive = m_config->sessionActiveTab();

        for (const QVariant &value : m_config->sessionTabs()) {
            if (restored.size() >= limit)
                break;

            const QUrl candidate(value.toString());
            if (!candidate.isValid() || !candidate.isLocalFile())
                continue;
            if (!QFileInfo(candidate.toLocalFile()).isDir())
                continue;

            restored.append(candidate);
        }

        if (!restored.isEmpty()) {
            m_activeTab->start(restored.constFirst());

            for (int index = 1; index < restored.size(); ++index) {
                TabController *tab = createTab(restored.at(index));
                insertTab(tab, int(m_tabs.size()));
            }

            const int active = qBound(0, storedActive, int(restored.size()) - 1);
            if (active > 0)
                activateTabInternal(active);

            saveSession();
            return;
        }
    }

    m_activeTab->start(url, selectName);
}

void ApplicationController::saveSession()
{
    if (!m_config)
        return;

    QVariantList urls;
    urls.reserve(m_tabs.size());
    for (const TabController *tab : std::as_const(m_tabs))
        urls.append(tab->currentUrl().toString());

    m_config->setSession(urls, m_activeIndex);
}

void ApplicationController::flushSession()
{
    saveSession();
    if (m_config)
        m_config->save();
}

QAbstractItemModel *ApplicationController::files() const
{
    return m_activeTab ? m_activeTab->proxy() : nullptr;
}

QAbstractItemModel *ApplicationController::tabs() const
{
    return m_tabModel;
}

bool ApplicationController::singleInstance() const
{
    return m_config->singleInstance();
}

void ApplicationController::setSingleInstance(bool enabled)
{
    m_config->setSingleInstance(enabled);
}

bool ApplicationController::openNewWindow()
{
    QStringList arguments{QStringLiteral("--new-window")};

    const QUrl url = currentUrl();
    if (url.isLocalFile())
        arguments << QStringLiteral("--") << url.toLocalFile();

    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments)) {
        setStatus(tr("No se pudo abrir una ventana nueva"));
        return false;
    }
    return true;
}

bool ApplicationController::restoreSession() const
{
    return m_config->restoreSession();
}

void ApplicationController::setRestoreSession(bool enabled)
{
    m_config->setRestoreSession(enabled);
}

bool ApplicationController::alwaysShowTabBar() const
{
    return m_config->alwaysShowTabBar();
}

void ApplicationController::setAlwaysShowTabBar(bool enabled)
{
    m_config->setAlwaysShowTabBar(enabled);
}

QUrl ApplicationController::currentUrl() const
{
    return m_activeTab ? m_activeTab->currentUrl() : QUrl();
}

QString ApplicationController::currentPath() const
{
    return m_activeTab ? m_activeTab->currentPath() : QString();
}

QVariantList ApplicationController::breadcrumb() const
{
    return m_activeTab ? m_activeTab->breadcrumb() : QVariantList();
}

QVariantList ApplicationController::places() const
{
    return m_places;
}

QVariantList ApplicationController::devices() const
{
    return m_deviceMonitor ? m_deviceMonitor->devices() : QVariantList();
}

bool ApplicationController::canGoBack() const
{
    return m_activeTab && m_activeTab->canGoBack();
}

bool ApplicationController::canGoForward() const
{
    return m_activeTab && m_activeTab->canGoForward();
}

int ApplicationController::itemCount() const
{
    return m_activeTab ? m_activeTab->itemCount() : 0;
}

QString ApplicationController::freeSpaceText() const
{
    return m_activeTab ? m_activeTab->freeSpaceText() : QString();
}

bool ApplicationController::loading() const
{
    return m_activeTab && m_activeTab->loading();
}

QString ApplicationController::statusMessage() const
{
    return m_activeTab ? m_activeTab->statusMessage() : QString();
}

QString ApplicationController::pendingSelection() const
{
    return m_activeTab ? m_activeTab->pendingSelection() : QString();
}

int ApplicationController::selectionCount() const
{
    return m_activeTab ? m_activeTab->selectionCount() : 0;
}

int ApplicationController::selectionRevision() const
{
    return m_activeTab ? m_activeTab->selectionRevision() : 0;
}

bool ApplicationController::canRenameSelection() const
{
    return m_activeTab && m_activeTab->canRenameSelection();
}

bool ApplicationController::selectedIsDirectory() const
{
    return m_activeTab && m_activeTab->selectedIsDirectory();
}

QString ApplicationController::selectedName() const
{
    return m_activeTab ? m_activeTab->selectedName() : QString();
}

bool ApplicationController::inVirtualLocation() const
{
    if (!m_activeTab)
        return false;
    const QUrl url = m_activeTab->currentUrl();
    return url.isValid() && !url.isLocalFile();
}

bool ApplicationController::inTrash() const
{
    return m_activeTab && m_activeTab->currentUrl().scheme() == QLatin1String("trash");
}

QString ApplicationController::virtualKind() const
{
    if (!m_activeTab)
        return {};
    const QString scheme = m_activeTab->currentUrl().scheme();
    return PathUtils::isVirtual(m_activeTab->currentUrl()) ? scheme : QString();
}

bool ApplicationController::infoColumns() const
{
    if (!m_activeTab)
        return false;
    const QUrl url = m_activeTab->currentUrl();
    if (url.scheme() == QLatin1String("recent"))
        return true;
    return url.scheme() == QLatin1String("trash")
        && (url.path().isEmpty() || url.path() == QLatin1String("/"));
}

void ApplicationController::restoreSelection()
{
    if (!m_activeTab || !inTrash())
        return;

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;

    m_operations->restoreTrashItems(urls);
    setStatus(QCoreApplication::translate("Ooze", "Restaurando…"));
}

void ApplicationController::requestTrashDeletion()
{
    if (!m_activeTab)
        return;

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;

    m_pendingTrashDelete = urls;
    const QString message = urls.size() == 1
        ? QCoreApplication::translate("Ooze", "El elemento se eliminará de forma permanente y no se podrá recuperar.")
        : QCoreApplication::translate("Ooze", "Los %1 elementos se eliminarán de forma permanente y no se podrán recuperar.")
              .arg(urls.size());
    emit confirmationRequested(QStringLiteral("trashDelete"),
                               QCoreApplication::translate("Ooze", "¿Eliminar permanentemente?"), message,
                               QCoreApplication::translate("Ooze", "Eliminar"));
}

void ApplicationController::emptyTrash()
{
    if (!inTrash())
        return;

    emit confirmationRequested(QStringLiteral("trashEmpty"),
                               QCoreApplication::translate("Ooze", "¿Vaciar la papelera?"),
                               QCoreApplication::translate("Ooze", "Todos los elementos se eliminarán de forma permanente y no se podrán recuperar."),
                               QCoreApplication::translate("Ooze", "Vaciar"));
}

void ApplicationController::confirmAction(const QString &id)
{
    if (id == QLatin1String("trashDelete")) {
        const QVector<QUrl> urls = std::exchange(m_pendingTrashDelete, {});
        if (urls.isEmpty())
            return;
        m_operations->deleteTrashItems(urls);
        setStatus(QCoreApplication::translate("Ooze", "Eliminando…"));
    } else if (id == QLatin1String("trashEmpty")) {
        m_operations->emptyTrash();
        setStatus(QCoreApplication::translate("Ooze", "Vaciando la papelera…"));
    }
}

void ApplicationController::trashUrls(const QList<QUrl> &urls)
{
    QVector<QUrl> local;
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            local.append(url);
    }

    if (local.isEmpty()) {
        setStatus(QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
        return;
    }

    m_operations->trash(local);
    setStatus(QCoreApplication::translate("Ooze", "Enviando a la papelera…"));
}

bool ApplicationController::inRecents() const
{
    return m_activeTab && m_activeTab->currentUrl().scheme() == QLatin1String("recent");
}

void ApplicationController::showInFolder(int row)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return;

    const QUrl url(index.data(FileListModel::UrlRole).toString());
    if (!url.isLocalFile())
        return;

    const QUrl folder = PathUtils::parent(url);
    if (!QFileInfo(folder.toLocalFile()).isDir()) {
        setStatus(QCoreApplication::translate("Ooze", "La carpeta contenedora ya no existe"));
        return;
    }

    m_activeTab->start(folder, index.data(FileListModel::NameRole).toString());
}

bool ApplicationController::canPaste() const
{
    if (!m_activeTab || !m_activeTab->currentUrl().isLocalFile())
        return false;
    if (!m_clipboardUrls.isEmpty())
        return true;

    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime || !mime->hasUrls())
        return false;

    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile())
            return true;
    }
    return false;
}

QString ApplicationController::themeMode() const
{
    return m_config->themeMode();
}

QString ApplicationController::paletteId() const
{
    return m_config->paletteId();
}

QString ApplicationController::iconTheme() const
{
    return m_config->iconTheme();
}

double ApplicationController::uiFontScale() const
{
    return m_config->fontScale();
}

bool ApplicationController::systemDark() const
{
    return m_systemDark;
}

bool ApplicationController::effectiveDark() const
{
    const QString mode = m_config->themeMode();
    if (mode == QStringLiteral("dark"))
        return true;
    if (mode == QStringLiteral("light"))
        return false;
    return m_systemDark;
}

QString ApplicationController::density() const
{
    return m_config->density();
}

QString ApplicationController::iconSize() const
{
    return m_config->iconSize();
}

QString ApplicationController::uiFontFamily() const
{
    const QString configured = m_config->fontFamily();
    return configured.isEmpty() ? QGuiApplication::font().family() : configured;
}

QString ApplicationController::fontFamilySetting() const
{
    return m_config->fontFamily();
}

void ApplicationController::setFontFamilySetting(const QString &family)
{
    m_config->setFontFamily(family);
}

QString ApplicationController::viewMode() const
{
    return m_config->viewMode();
}

bool ApplicationController::showHidden() const
{
    return m_config->showHidden();
}

bool ApplicationController::singleClickOpen() const
{
    return m_config->singleClickOpen();
}

bool ApplicationController::thumbnails() const
{
    return m_config->thumbnails();
}

bool ApplicationController::infoPanel() const
{
    return m_config->infoPanel();
}

bool ApplicationController::lightweight() const
{
    return m_config->lightweight();
}

bool ApplicationController::canUndo() const
{
    return !m_undoBusy && m_undo.canUndo();
}

bool ApplicationController::canRedo() const
{
    return !m_undoBusy && m_undo.canRedo();
}

QString ApplicationController::undoText() const
{
    return m_undo.undoLabel();
}

QString ApplicationController::redoText() const
{
    return m_undo.redoLabel();
}

int ApplicationController::sortMode() const
{
    return m_config->sortMode();
}

bool ApplicationController::sortDescending() const
{
    return m_config->sortDescending();
}

QString ApplicationController::filterText() const
{
    return m_activeTab ? m_activeTab->filterText() : QString();
}

QString ApplicationController::terminalCommand() const
{
    return m_config->terminalCommand();
}

void ApplicationController::goTo(const QString &url)
{
    QUrl target(url);
    if (target.scheme().isEmpty())
        target = QUrl::fromLocalFile(url);
    goToUrl(target);
}

void ApplicationController::goToUrl(const QUrl &url)
{
    if (m_activeTab)
        m_activeTab->navigate(url, true);
}

void ApplicationController::goBack()
{
    if (m_activeTab)
        m_activeTab->goBack();
}

void ApplicationController::goForward()
{
    if (m_activeTab)
        m_activeTab->goForward();
}

void ApplicationController::goUp()
{
    if (m_activeTab)
        m_activeTab->goUp();
}

void ApplicationController::reload()
{
    if (m_activeTab)
        m_activeTab->reload();
}

void ApplicationController::newTab()
{
    openInNewTab({});
}

void ApplicationController::openInNewTab(const QString &url)
{
    const int limit = m_config->maxTabs();
    if (m_tabs.size() >= limit) {
        setStatus(tr("Máximo de %1 pestañas alcanzado").arg(limit));
        return;
    }

    QUrl target;
    if (url.trimmed().isEmpty()) {
        if (m_activeTab)
            target = m_activeTab->currentUrl();
    } else {
        target = QUrl(url);
        if (target.scheme().isEmpty())
            target = QUrl::fromLocalFile(url);
    }

    if (!target.isValid()) {
        setStatus(QCoreApplication::translate("Ooze", "La ruta no es válida"));
        return;
    }

    QString selectName;
    if (target.isLocalFile()) {
        const QFileInfo info(target.toLocalFile());
        if (info.isFile()) {
            selectName = info.fileName();
            target = QUrl::fromLocalFile(info.absolutePath());
        } else if (!info.isDir()) {
            setStatus(QCoreApplication::translate("Ooze", "La carpeta no existe"));
            return;
        }
    }

    TabController *tab = createTab(target, selectName);
    insertTab(tab, int(m_tabs.size()));
    activateTabInternal(int(m_tabs.size()) - 1);
}

void ApplicationController::openRowInNewTab(int row)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return;

    openInNewTab(index.data(FileListModel::UrlRole).toString());
}

void ApplicationController::activateTab(int index)
{
    activateTabInternal(index);
}

void ApplicationController::duplicateTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    const int limit = m_config->maxTabs();
    if (m_tabs.size() >= limit) {
        setStatus(tr("Máximo de %1 pestañas alcanzado").arg(limit));
        return;
    }

    TabController *tab = createTab(m_tabs.at(index)->currentUrl());
    insertTab(tab, index + 1);
    activateTabInternal(index + 1);
}

void ApplicationController::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    if (m_tabs.size() <= 1) {
        emit closeWindowRequested();
        return;
    }

    TabController *tab = m_tabs.at(index);
    const ClosedTab closed{tab->currentUrl(), index};

    if (tab == m_activeTab) {
        const int neighbor = index + 1 < m_tabs.size() ? index + 1 : index - 1;
        activateTabInternal(neighbor);
    }

    removeTabAt(index);

    m_closedTabs.append(closed);
    if (m_closedTabs.size() > 10)
        m_closedTabs.removeFirst();
    emit reopenStateChanged();
}

void ApplicationController::closeOtherTabs(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    if (index != m_activeIndex)
        activateTabInternal(index);

    for (int current = int(m_tabs.size()) - 1; current >= 0; --current) {
        if (current != index)
            closeTab(current);
    }
}

void ApplicationController::closeTabsToRight(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    for (int current = int(m_tabs.size()) - 1; current > index; --current)
        closeTab(current);
}

void ApplicationController::nextTab()
{
    if (m_tabs.size() < 2)
        return;
    activateTabInternal((m_activeIndex + 1) % int(m_tabs.size()));
}

void ApplicationController::previousTab()
{
    if (m_tabs.size() < 2)
        return;
    activateTabInternal((m_activeIndex - 1 + int(m_tabs.size())) % int(m_tabs.size()));
}

void ApplicationController::reopenClosedTab()
{
    const int limit = m_config->maxTabs();

    while (!m_closedTabs.isEmpty()) {
        const ClosedTab closed = m_closedTabs.takeLast();

        if (!closed.url.isValid())
            continue;
        if (closed.url.isLocalFile() && !QFileInfo(closed.url.toLocalFile()).isDir())
            continue;

        if (m_tabs.size() >= limit) {
            m_closedTabs.append(closed);
            setStatus(tr("Máximo de %1 pestañas alcanzado").arg(limit));
            break;
        }

        TabController *tab = createTab(closed.url);
        const int index = qBound(0, closed.index, int(m_tabs.size()));
        insertTab(tab, index);
        activateTabInternal(index);
        break;
    }

    emit reopenStateChanged();
}

void ApplicationController::copyTabPath(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    const QUrl url = m_tabs.at(index)->currentUrl();
    QGuiApplication::clipboard()->setText(url.isLocalFile() ? url.toLocalFile()
                                                            : url.toDisplayString());
    setStatus(QCoreApplication::translate("Ooze", "Ruta copiada"));
}

void ApplicationController::openRow(int row)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return;

    const QUrl url(index.data(FileListModel::UrlRole).toString());
    const bool isDir = index.data(FileListModel::IsDirRole).toBool();

    if (isDir) {
        if (inTrash()) {
            const QUrl virtualUrl = TrashService::virtualUrlForPath(PathUtils::toLocalBytes(url));
            if (virtualUrl.isValid()) {
                m_activeTab->navigate(virtualUrl, true);
                return;
            }
        }
        m_activeTab->navigate(url, true);
        return;
    }

    if (url.isLocalFile() && !QFileInfo(url.toLocalFile()).isReadable()) {
        setStatus(QCoreApplication::translate("Ooze", "No se puede abrir: no existe o no hay permiso de acceso"));
        return;
    }

    if (!QDesktopServices::openUrl(url))
        setStatus(QCoreApplication::translate("Ooze", "No hay ninguna aplicación para abrir este archivo"));
}

void ApplicationController::openRowInTerminal(int row)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    const QUrl url = index.isValid()
        ? QUrl(index.data(FileListModel::UrlRole).toString())
        : m_activeTab->currentUrl();

    if (!m_terminal->openAt(url, m_config->terminalCommand()))
        setStatus(m_terminal->lastError());
}

void ApplicationController::openCurrentInTerminal()
{
    if (!m_activeTab)
        return;

    if (!m_terminal->openAt(m_activeTab->currentUrl(), m_config->terminalCommand()))
        setStatus(m_terminal->lastError());
}

void ApplicationController::copyPath(int row)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    const QUrl url = index.isValid()
        ? QUrl(index.data(FileListModel::UrlRole).toString())
        : m_activeTab->currentUrl();

    QGuiApplication::clipboard()->setText(url.isLocalFile() ? url.toLocalFile()
                                                            : url.toDisplayString());
    setStatus(QCoreApplication::translate("Ooze", "Ruta copiada"));
}

void ApplicationController::selectRow(int row, bool toggle, bool range)
{
    if (m_activeTab)
        m_activeTab->selectRow(row, toggle, range);
}

void ApplicationController::selectRows(const QVector<int> &rows, int mode)
{
    if (m_activeTab)
        m_activeTab->selectRows(rows, mode);
}

void ApplicationController::selectAll()
{
    if (m_activeTab)
        m_activeTab->selectAll();
}

void ApplicationController::clearSelection()
{
    if (m_activeTab)
        m_activeTab->clearSelection();
}

void ApplicationController::copySelection()
{
    if (!m_activeTab)
        return;

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;

    publishClipboard(urls, false);
    setStatus(countTextHelper(int(urls.size())));
}

void ApplicationController::cutSelection()
{
    if (!m_activeTab)
        return;
    if (inVirtualLocation()) {
        setStatus(QCoreApplication::translate("Ooze", "No disponible en esta ubicación"));
        return;
    }

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;

    publishClipboard(urls, true);
    setStatus(QCoreApplication::translate("Ooze", "%1 listo para mover").arg(countTextHelper(int(urls.size()))));
}

void ApplicationController::paste()
{
    if (!m_activeTab)
        return;

    const QUrl target = m_activeTab->currentUrl();
    if (!target.isLocalFile()) {
        setStatus(QCoreApplication::translate("Ooze", "No se puede pegar en esta ubicación"));
        return;
    }

    syncClipboard();

    const QVector<QUrl> sources = m_clipboardUrls;
    const bool cut = m_clipboardCut && !sources.isEmpty();

    if (sources.isEmpty()) {
        setStatus(QCoreApplication::translate("Ooze", "El portapapeles no contiene archivos locales"));
        return;
    }

    m_pendingCutPaste = cut;
    if (cut)
        m_operations->move(sources, target);
    else
        m_operations->copy(sources, target);

    setStatus(cut ? QCoreApplication::translate("Ooze", "Moviendo…") : QCoreApplication::translate("Ooze", "Copiando…"));
}

void ApplicationController::trashSelection()
{
    if (!m_activeTab)
        return;
    if (inTrash()) {
        requestTrashDeletion();
        return;
    }
    if (inVirtualLocation()) {
        setStatus(QCoreApplication::translate("Ooze", "No disponible en esta ubicación"));
        return;
    }

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;
    m_operations->trash(urls);
    setStatus(QCoreApplication::translate("Ooze", "Enviando a la papelera…"));
}

void ApplicationController::deleteSelection()
{
    if (!m_activeTab)
        return;
    if (inTrash()) {
        requestTrashDeletion();
        return;
    }
    if (inVirtualLocation()) {
        setStatus(QCoreApplication::translate("Ooze", "No disponible en esta ubicación"));
        return;
    }

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;
    m_operations->remove(urls);
    setStatus(QCoreApplication::translate("Ooze", "Eliminando…"));
}

void ApplicationController::renameSelection(const QString &newName)
{
    if (!m_activeTab)
        return;
    if (inVirtualLocation()) {
        setStatus(QCoreApplication::translate("Ooze", "No disponible en esta ubicación"));
        return;
    }

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.size() != 1)
        return;
    m_operations->rename(urls.constFirst(), newName);
    setStatus(QCoreApplication::translate("Ooze", "Renombrando…"));
}

void ApplicationController::createFolder(const QString &name)
{
    if (!m_activeTab || !m_activeTab->currentUrl().isLocalFile())
        return;
    const QString target = name.isEmpty() ? suggestedFolderName() : name;
    m_operations->createDirectory(m_activeTab->currentUrl(), target);
    setStatus(QCoreApplication::translate("Ooze", "Creando carpeta…"));
}

void ApplicationController::createFile(const QString &name)
{
    if (!m_activeTab || !m_activeTab->currentUrl().isLocalFile())
        return;
    const QString target = name.isEmpty() ? suggestedFileName() : name;
    m_operations->createFile(m_activeTab->currentUrl(), target);
    setStatus(QCoreApplication::translate("Ooze", "Creando archivo…"));
}

bool ApplicationController::addPlace(const QString &url)
{
    const QUrl target(url);
    if (!target.isLocalFile() || !QFileInfo(target.toLocalFile()).isDir())
        return false;

    const bool added = m_config->addPlace(target.toLocalFile());
    setStatus(added ? tr("«%1» añadida a Lugares").arg(QFileInfo(target.toLocalFile()).fileName())
                    : tr("Esa carpeta ya está en Lugares"));
    return added;
}

bool ApplicationController::addPlaceForRow(int row)
{
    if (!m_activeTab)
        return false;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid() || !index.data(FileListModel::IsDirRole).toBool())
        return false;

    return addPlace(index.data(FileListModel::UrlRole).toString());
}

bool ApplicationController::addCurrentFolderToPlaces()
{
    if (!m_activeTab)
        return false;
    return addPlace(m_activeTab->currentUrl().toString());
}

void ApplicationController::removePlace(const QString &url)
{
    const QUrl target(url);
    if (target.isLocalFile())
        m_config->removePlace(target.toLocalFile());
}

void ApplicationController::resolveConflict(int id, int decision, bool applyToAll)
{
    m_operations->resolveConflict(id, decision, applyToAll);
}

bool ApplicationController::canMovePlace(const QString &id, int delta) const
{
    const QStringList ids = Places::orderedIds(m_places);
    const int from = int(ids.indexOf(id));
    if (from < 0)
        return false;
    const int target = from + delta;
    return target >= 0 && target < ids.size();
}

bool ApplicationController::movePlaceBy(const QString &id, int delta)
{
    if (!canMovePlace(id, delta))
        return false;
    const int from = int(Places::orderedIds(m_places).indexOf(id));
    return movePlace(id, delta < 0 ? from + delta : from + delta + 1);
}

bool ApplicationController::movePlace(const QString &id, int insertIndex)
{
    QStringList ids = Places::orderedIds(m_places);
    const int from = int(ids.indexOf(id));
    if (from < 0)
        return false;

    int target = std::clamp(insertIndex, 0, int(ids.size()));
    if (target > from)
        --target;
    if (target == from)
        return false;

    ids.move(from, target);

    QStringList order = ids;
    for (const QString &known : m_config->placesOrder()) {
        if (!order.contains(known))
            order.append(known);
    }
    m_config->setPlacesOrder(order);
    return true;
}

void ApplicationController::rebuildPlaces()
{
    m_places = Places::build(m_config->customPlaces(), m_config->placesOrder());
    emit placesChanged();
}

bool ApplicationController::isCustomPlace(const QString &url) const
{
    const QUrl target(url);
    if (!target.isLocalFile())
        return false;

    const QString path = QDir::cleanPath(target.toLocalFile());
    for (const QVariant &value : m_config->customPlaces()) {
        if (value.toMap().value(QStringLiteral("path")).toString() == path)
            return true;
    }
    return false;
}

void ApplicationController::openDevice(const QString &objectPath)
{
    for (const QVariant &value : m_deviceMonitor->devices()) {
        const QVariantMap device = value.toMap();
        if (device.value(QStringLiteral("objectPath")).toString() != objectPath)
            continue;

        if (device.value(QStringLiteral("mounted")).toBool()) {
            goToUrl(QUrl::fromLocalFile(device.value(QStringLiteral("mountPoint")).toString()));
        } else {
            m_pendingDeviceOpen = objectPath;
            setStatus(tr("Montando «%1»…").arg(device.value(QStringLiteral("name")).toString()));
            m_deviceMonitor->mount(objectPath);
        }
        return;
    }
}

void ApplicationController::unmountDevice(const QString &objectPath)
{
    m_deviceMonitor->unmount(objectPath);
}

void ApplicationController::ejectDevice(const QString &objectPath)
{
    m_deviceMonitor->eject(objectPath);
}

QVariantList ApplicationController::defaultAppStates() const
{
    return OpenWith::defaultAppStates();
}

bool ApplicationController::setDefaultApp(const QString &categoryId, const QString &appId)
{
    QString error;
    if (!OpenWith::setDefault(categoryId, appId, &error)) {
        setStatus(error);
        emit errorOccurred(error);
        return false;
    }

    setStatus(tr("Aplicación predeterminada actualizada"));
    return true;
}

QVariantMap ApplicationController::previewCurrentFolder() const
{
    return FilePreview::describe(currentUrl());
}

QVariantMap ApplicationController::previewCurrentFolderWithSize() const
{
    return FilePreview::describe(currentUrl(), true);
}

QVariantList ApplicationController::compressFormats()
{
    if (!m_archiveFormatsReady) {
        m_archiveFormats = Archive::availableFormats();
        m_archiveFormatsReady = true;
    }
    return m_archiveFormats;
}

bool ApplicationController::isArchiveRow(int row) const
{
    if (!m_activeTab)
        return false;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid() || index.data(FileListModel::IsDirRole).toBool())
        return false;

    return Archive::isArchiveName(index.data(FileListModel::NameRole).toString());
}

void ApplicationController::compressSelection(const QString &formatId)
{
    compressSelectionAs(formatId, QString());
}

void ApplicationController::compressSelectionAs(const QString &formatId, const QString &name)
{
    if (!m_activeTab)
        return;
    if (inVirtualLocation()) {
        setStatus(QCoreApplication::translate("Ooze", "No disponible en esta ubicación"));
        return;
    }

    const QVector<QUrl> urls = m_activeTab->selectedUrlsInOrder();
    if (urls.isEmpty())
        return;

    m_operations->compress(urls, formatId, name);
    setStatus(tr("Comprimiendo…"));
}

QString ApplicationController::suggestedArchiveName() const
{
    if (!m_activeTab)
        return {};
    return FileOperations::suggestedArchiveBase(m_activeTab->selectedUrlsInOrder());
}

QVariantMap ApplicationController::archiveTarget(const QString &name, const QString &formatId) const
{
    QVariantMap result;
    QString error;
    QString fileName;

    if (m_activeTab) {
        fileName = FileOperations::archiveTargetName(m_activeTab->selectedUrlsInOrder(), name,
                                                     formatId, &error);
    }

    QString wanted = name.trimmed();
    const QString suffix = Archive::suffixForFormat(formatId);
    if (!suffix.isEmpty() && wanted.endsWith(suffix, Qt::CaseInsensitive))
        wanted.chop(suffix.size());
    wanted = wanted.trimmed() + suffix;

    result.insert(QStringLiteral("fileName"), fileName);
    result.insert(QStringLiteral("error"), error);
    result.insert(QStringLiteral("exists"), !fileName.isEmpty() && fileName != wanted);
    return result;
}

void ApplicationController::extractRow(int row)
{
    if (!isArchiveRow(row) || !m_activeTab || inVirtualLocation())
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    m_operations->extract(QUrl(index.data(FileListModel::UrlRole).toString()));
    setStatus(tr("Extrayendo…"));
}

void ApplicationController::undo()
{
    if (m_undoBusy || !m_undo.canUndo())
        return;

    if (m_operations->busy()) {
        setStatus(tr("Hay una operación en curso"));
        return;
    }

    if (m_operationTab.isNull())
        m_operationTab = m_activeTab;

    m_undoBusy = true;
    emit undoStateChanged();
    m_operations->applyJournal(m_undo.nextUndo(), true);
}

void ApplicationController::redo()
{
    if (m_undoBusy || !m_undo.canRedo())
        return;

    if (m_operations->busy()) {
        setStatus(tr("Hay una operación en curso"));
        return;
    }

    if (m_operationTab.isNull())
        m_operationTab = m_activeTab;

    m_undoBusy = true;
    emit undoStateChanged();
    m_operations->applyJournal(m_undo.nextRedo(), false);
}

QVariantList ApplicationController::openWithApps(int row) const
{
    const QString mime = openWithMime(row);
    return mime.isEmpty() ? QVariantList() : OpenWith::appsForMime(mime);
}

QVariantList ApplicationController::openWithOtherApps(int row) const
{
    const QString mime = openWithMime(row);
    return mime.isEmpty() ? QVariantList() : OpenWith::otherApps(mime);
}

QString ApplicationController::openWithMime(int row) const
{
    if (!m_activeTab)
        return {};

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return {};

    const QUrl url(index.data(FileListModel::UrlRole).toString());
    if (!url.isLocalFile())
        return {};

    if (index.data(FileListModel::IsDirRole).toBool())
        return QStringLiteral("inode/directory");

    return mimeForIndex(index);
}

void ApplicationController::openRowWith(int row, const QString &appId)
{
    if (!m_activeTab)
        return;

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return;

    const QUrl url(index.data(FileListModel::UrlRole).toString());
    QString error;
    if (!OpenWith::launch(appId, mimeForIndex(index), {url}, &error))
        setStatus(error);
}

QVariantMap ApplicationController::previewRow(int row) const
{
    if (!m_activeTab)
        return {};

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return {};

    return FilePreview::describe(QUrl(index.data(FileListModel::UrlRole).toString()));
}

QVariantMap ApplicationController::previewRowWithSize(int row) const
{
    if (!m_activeTab)
        return {};

    const QModelIndex index = m_activeTab->proxy()->index(row, 0);
    if (!index.isValid())
        return {};

    return FilePreview::describe(QUrl(index.data(FileListModel::UrlRole).toString()), true);
}

QVariantList ApplicationController::selectedEntries() const
{
    return m_activeTab ? m_activeTab->selectedEntries() : QVariantList();
}

QStringList ApplicationController::currentNames() const
{
    return m_activeTab ? m_activeTab->currentNames() : QStringList();
}

void ApplicationController::renameEntries(const QVariantList &entries)
{
    QVector<QUrl> sources;
    QStringList names;
    sources.reserve(entries.size());
    names.reserve(entries.size());

    for (const QVariant &value : entries) {
        const QVariantMap entry = value.toMap();
        const QString url = entry.value(QStringLiteral("url")).toString();
        const QString name = entry.value(QStringLiteral("name")).toString();
        if (url.isEmpty() || name.isEmpty())
            return;
        sources.append(QUrl(url));
        names.append(name);
    }

    if (sources.isEmpty())
        return;

    m_operations->renameBatch(sources, names);
}

void ApplicationController::openSearchResult(const QString &url, bool isDirectory)
{
    const QUrl target(url);
    if (!target.isValid())
        return;

    if (isDirectory) {
        goToUrl(target);
        return;
    }

    if (!QDesktopServices::openUrl(target))
        setStatus(QCoreApplication::translate("Ooze", "No hay ninguna aplicación para abrir este archivo"));
}

bool ApplicationController::isSelected(int row) const
{
    return m_activeTab && m_activeTab->isSelected(row);
}

QVector<int> ApplicationController::selectedRows() const
{
    return m_activeTab ? m_activeTab->selectedRows() : QVector<int>();
}

QString ApplicationController::suggestedFolderName() const
{
    return m_activeTab ? m_activeTab->suggestedFolderName() : QString();
}

QString ApplicationController::suggestedFileName() const
{
    return m_activeTab ? m_activeTab->suggestedFileName() : QString();
}

QString ApplicationController::dragUriList(int row) const
{
    return m_activeTab ? m_activeTab->dragUriList(row) : QString();
}

void ApplicationController::dropUrls(const QList<QUrl> &urls, int action,
                                     const QString &targetUrl)
{
    const QUrl target = targetUrl.isEmpty()
        ? (m_activeTab ? m_activeTab->currentUrl() : QUrl())
        : QUrl(targetUrl);

    if (target.scheme() == QLatin1String("trash")) {
        trashUrls(urls);
        return;
    }

    if (!target.isValid() || !target.isLocalFile()) {
        setStatus(QCoreApplication::translate("Ooze", "No se puede soltar en esta ubicación"));
        return;
    }

    QVector<QUrl> sources;
    sources.reserve(urls.size());
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            sources.append(url);
    }

    if (sources.isEmpty()) {
        setStatus(QCoreApplication::translate("Ooze", "Solo se admiten archivos locales"));
        return;
    }

    switch (Qt::DropAction(action)) {
    case Qt::MoveAction:
        m_operations->move(sources, target);
        setStatus(QCoreApplication::translate("Ooze", "Moviendo…"));
        break;
    case Qt::LinkAction:
        m_operations->link(sources, target);
        setStatus(QCoreApplication::translate("Ooze", "Enlazando…"));
        break;
    default:
        m_operations->copy(sources, target);
        setStatus(QCoreApplication::translate("Ooze", "Copiando…"));
        break;
    }
}

void ApplicationController::setThemeMode(const QString &mode)
{
    const bool wasDark = effectiveDark();
    m_config->setThemeMode(mode);
    emit effectiveDarkChanged();
    if (effectiveDark() != wasDark)
        bumpIconRevision();
}

void ApplicationController::setPaletteId(const QString &id)
{
    m_config->setPaletteId(id);
}

QString ApplicationController::language() const
{
    return m_config->language();
}

void ApplicationController::setLanguage(const QString &language)
{
    m_config->setLanguage(language);
}

void ApplicationController::setDensity(const QString &density)
{
    m_config->setDensity(density);
}

void ApplicationController::setIconSize(const QString &size)
{
    m_config->setIconSize(size);
}

void ApplicationController::setViewMode(const QString &mode)
{
    m_config->setViewMode(mode);
}

void ApplicationController::setShowHidden(bool show)
{
    m_config->setShowHidden(show);
}

void ApplicationController::setInfoPanel(bool enabled)
{
    m_config->setInfoPanel(enabled);
}

void ApplicationController::setLightweight(bool enabled)
{
    m_config->setLightweight(enabled);
}

void ApplicationController::setThumbnails(bool enabled)
{
    m_config->setThumbnails(enabled);
}

void ApplicationController::setSingleClickOpen(bool enabled)
{
    m_config->setSingleClickOpen(enabled);
}

void ApplicationController::setSortMode(int mode)
{
    m_config->setSortMode(mode);
}

void ApplicationController::setSortDescending(bool descending)
{
    m_config->setSortDescending(descending);
}

void ApplicationController::setTerminalCommand(const QString &command)
{
    if (m_config->terminalCommand() == command)
        return;
    m_config->setTerminalCommand(command);
    emit terminalCommandChanged();
}

void ApplicationController::setFilterText(const QString &text)
{
    if (m_activeTab)
        m_activeTab->setFilterText(text);
}

void ApplicationController::handleExternalOpen(const QUrl &url, bool select)
{
    if (!m_activeTab)
        return;

    QUrl target = url;
    QString selectName;
    if (select && target.isLocalFile()) {
        const QFileInfo info(target.toLocalFile());
        if (info.isFile()) {
            selectName = info.fileName();
            target = QUrl::fromLocalFile(info.absolutePath());
        }
    }

    m_activeTab->start(target, selectName);
}

void ApplicationController::openThemesFolder()
{
    const QString directory = ThemeStore::themesDirectory();
    if (!QDir().mkpath(directory)) {
        setStatus(QCoreApplication::translate("Ooze", "No se pudo crear la carpeta de paletas"));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
        setStatus(QCoreApplication::translate("Ooze", "No se pudo abrir la carpeta de paletas"));
}

void ApplicationController::bumpIconRevision()
{
    ++m_iconRevision;
    emit iconRevisionChanged();
}

void ApplicationController::setStatus(const QString &message)
{
    if (m_activeTab)
        m_activeTab->setStatus(message);
}

void ApplicationController::clearPendingSelection()
{
    if (m_activeTab)
        m_activeTab->clearPendingSelection();
}

int ApplicationController::rowForName(const QString &name) const
{
    return m_activeTab ? m_activeTab->rowForName(name) : -1;
}

void ApplicationController::publishClipboard(const QVector<QUrl> &urls, bool cut)
{
    m_clipboardUrls = urls;
    m_clipboardCut = cut;
    m_pendingCutPaste = false;
    rebuildClipboardKeys();

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        auto *mime = new QMimeData;
        QList<QUrl> list;
        QStringList paths;
        QByteArray gnome = cut ? QByteArrayLiteral("cut") : QByteArrayLiteral("copy");
        for (const QUrl &url : urls) {
            list.append(url);
            paths.append(url.toLocalFile());
            gnome += '\n' + url.toEncoded();
        }
        mime->setUrls(list);
        mime->setText(paths.join(QLatin1Char('\n')));
        mime->setData(QStringLiteral("x-special/gnome-copied-files"), gnome);
        mime->setData(QStringLiteral("application/x-kde-cutselection"), cut ? "1" : "0");

        m_publishingClipboard = true;
        clipboard->setMimeData(mime);
        m_publishingClipboard = false;
    }

    emit clipboardChanged();
}

void ApplicationController::syncClipboard()
{
    if (m_publishingClipboard)
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;

    QVector<QUrl> urls;
    bool cut = false;

    if (const QMimeData *mime = clipboard->mimeData(); mime && mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile())
                urls.append(url);
        }

        if (mime->hasFormat(QStringLiteral("application/x-kde-cutselection")))
            cut = mime->data(QStringLiteral("application/x-kde-cutselection")).trimmed() == "1";
        else if (mime->hasFormat(QStringLiteral("x-special/gnome-copied-files")))
            cut = mime->data(QStringLiteral("x-special/gnome-copied-files")).startsWith("cut");
    }

    cut = cut && !urls.isEmpty();
    if (urls == m_clipboardUrls && cut == m_clipboardCut)
        return;

    m_clipboardUrls = urls;
    m_clipboardCut = cut;
    rebuildClipboardKeys();
    emit clipboardChanged();
}

void ApplicationController::rebuildClipboardKeys()
{
    m_clipboardKeys.clear();
    for (const QUrl &url : std::as_const(m_clipboardUrls))
        m_clipboardKeys.insert(url.toString());
}

int ApplicationController::clipboardMark(const QString &url) const
{
    if (m_clipboardKeys.isEmpty() || !m_clipboardKeys.contains(url))
        return 0;
    return m_clipboardCut ? 2 : 1;
}

void ApplicationController::clearCutClipboard()
{
    if (!m_clipboardCut && m_clipboardUrls.isEmpty())
        return;

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        m_publishingClipboard = true;
        clipboard->clear();
        m_publishingClipboard = false;
    }

    m_clipboardUrls.clear();
    m_clipboardKeys.clear();
    m_clipboardCut = false;
    m_pendingCutPaste = false;
    emit clipboardChanged();
}
