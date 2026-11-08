#include "app/TabController.h"

#include "core/DirectoryLoader.h"
#include "core/DirectoryWatcher.h"
#include "core/FileListModel.h"
#include "core/FileListProxyModel.h"
#include "core/FileOperations.h"
#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

#include <QLoggingCategory>
#include <QCoreApplication>

Q_LOGGING_CATEGORY(lcTab, "ooze.files.tab")

TabController::TabController(DirectoryLoader *loader, QObject *parent)
    : QObject(parent)
    , m_loader(loader)
    , m_model(new FileListModel(this))
    , m_proxy(new FileListProxyModel(this))
    , m_watcher(new DirectoryWatcher(this))
{
    m_proxy->setSourceModel(m_model);

    connectLoader();

    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        if (!m_active) {
            m_dirty = true;
            return;
        }
        if (!m_currentUrl.isLocalFile() && !PathUtils::isVirtual(m_currentUrl))
            return;
        refresh();
    });

    connect(m_watcher, &DirectoryWatcher::directoryGone, this, [this] {
        if (!m_active) {
            m_directoryGone = true;
            m_dirty = true;
            return;
        }
        handleDirectoryGone();
    });

    connect(m_proxy, &QAbstractItemModel::rowsInserted, this,
            &TabController::itemCountChanged);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this,
            &TabController::itemCountChanged);
    connect(m_proxy, &QAbstractItemModel::modelReset, this,
            &TabController::itemCountChanged);

    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this,
            &TabController::pruneSelection);
    connect(m_proxy, &QAbstractItemModel::modelReset, this,
            &TabController::pruneSelection);

    connect(m_proxy, &QAbstractItemModel::rowsInserted, this,
            &TabController::trySelectPending);
    connect(m_proxy, &QAbstractItemModel::modelReset, this,
            &TabController::trySelectPending);
}

TabController::~TabController() = default;

void TabController::connectLoader()
{
    if (!m_loader)
        return;

    connect(m_loader, &DirectoryLoader::loadStarted, this,
            &TabController::onLoadStarted);
    connect(m_loader, &DirectoryLoader::loadFinished, this,
            &TabController::onLoadFinished);
    connect(m_loader, &DirectoryLoader::statsFinished, this,
            &TabController::onStatsFinished);
}

void TabController::onLoadStarted(const QUrl &directory)
{
    if (!m_active || directory != m_currentUrl)
        return;

    setLoading(true);
    setStatus({});
}

void TabController::onLoadFinished(const QUrl &directory, const VfsError &error)
{
    if (!m_active || directory != m_currentUrl)
        return;

    setLoading(false);
    m_refreshing = false;
    m_loaded = true;
    m_settled = true;

    if (error.isValid() && !error.isCancelled()) {
        const QString message = error.message.isEmpty()
            ? QCoreApplication::translate("Ooze", "No se pudo leer el directorio")
            : error.message;
        setStatus(message);
        qCWarning(lcTab) << "Error de listado" << error.url << error.code << message;
    }

    emit itemCountChanged();
    pruneSelection();

    if (m_refreshPending) {
        m_refreshPending = false;
        refresh();
    }
}

void TabController::onStatsFinished()
{
    if (!m_active)
        return;

    m_settled = true;
}

QString TabController::currentPath() const
{
    return m_currentUrl.isLocalFile() ? m_currentUrl.toLocalFile()
                                      : m_currentUrl.toDisplayString();
}

QString TabController::title() const
{
    if (!m_currentUrl.isValid())
        return {};

    if (m_currentUrl.scheme() == QLatin1String("recent"))
        return QCoreApplication::translate("Ooze", "Recientes");

    if (m_currentUrl.scheme() == QLatin1String("trash")) {
        const QVariantList crumbs = PathUtils::breadcrumb(m_currentUrl);
        return crumbs.last().toMap().value(QStringLiteral("name")).toString();
    }

    if (m_currentUrl.isLocalFile()) {
        const QString path = QDir::cleanPath(m_currentUrl.toLocalFile());
        if (path == QLatin1String("/"))
            return QStringLiteral("/");
        const QString name = QFileInfo(path).fileName();
        return name.isEmpty() ? path : name;
    }

    const QString name = m_currentUrl.fileName();
    return name.isEmpty() ? m_currentUrl.toDisplayString() : name;
}

QVariantList TabController::breadcrumb() const
{
    return PathUtils::breadcrumb(m_currentUrl);
}

bool TabController::canGoBack() const
{
    return m_historyIndex > 0;
}

bool TabController::canGoForward() const
{
    return m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1;
}

int TabController::itemCount() const
{
    return m_proxy->rowCount();
}

QString TabController::freeSpaceText() const
{
    if (!m_currentUrl.isLocalFile())
        return {};
    const QStorageInfo storage(m_currentUrl.toLocalFile());
    if (!storage.isValid() || storage.bytesAvailable() < 0)
        return {};
    return QCoreApplication::translate("Ooze", "%1 libres de %2")
        .arg(PathUtils::formatSize(storage.bytesAvailable()),
             PathUtils::formatSize(storage.bytesTotal()));
}

void TabController::start(const QUrl &url, const QString &selectName)
{
    if (!selectName.isEmpty())
        setPendingSelection(selectName);
    navigate(url, true);
}

void TabController::navigate(const QUrl &url, bool recordHistory)
{
    if (!url.isValid())
        return;

    const QUrl target = PathUtils::normalized(url);

    if (recordHistory) {
        if (m_historyIndex < 0 || m_historyIndex >= m_history.size()
            || m_history.at(m_historyIndex) != target) {
            while (m_history.size() > m_historyIndex + 1)
                m_history.removeLast();
            m_history.append(target);
            m_historyIndex = m_history.size() - 1;
            emit historyChanged();
        }
    }

    m_currentUrl = target;
    m_proxy->setActivityOrder(target.scheme() == QLatin1String("recent")
                              || (target.scheme() == QLatin1String("trash")
                                  && PathUtils::breadcrumb(target).size() == 1));
    m_directoryGone = false;
    m_refreshPending = false;
    emit currentUrlChanged();
    emit titleChanged();

    clearSelection();

    m_proxy->setFilterText({});
    emit filterTextChanged();

    m_watcher->watch(target);

    if (m_active) {
        m_loader->attach(m_model, target);
        m_loaded = false;
        m_dirty = false;
        m_partial = false;
        m_settled = false;
        m_refreshing = false;
        m_loader->load(target);
    } else {
        m_loaded = false;
        m_partial = true;
        m_dirty = true;
        m_settled = false;
    }
}

void TabController::goBack()
{
    if (!canGoBack())
        return;
    m_historyIndex--;
    emit historyChanged();
    navigate(m_history.at(m_historyIndex), false);
}

void TabController::goForward()
{
    if (!canGoForward())
        return;
    m_historyIndex++;
    emit historyChanged();
    navigate(m_history.at(m_historyIndex), false);
}

void TabController::goUp()
{
    const QUrl parent = PathUtils::parent(m_currentUrl);
    if (parent == m_currentUrl)
        return;
    navigate(parent, true);
}

void TabController::reload()
{
    if (!m_currentUrl.isValid())
        return;

    if (m_active) {
        navigate(m_currentUrl, false);
    } else {
        m_loaded = false;
        m_partial = true;
        m_dirty = true;
    }
}

void TabController::activate()
{
    m_active = true;

    if (!m_currentUrl.isValid())
        return;

    if (m_directoryGone) {
        handleDirectoryGone();
        return;
    }

    m_loader->attach(m_model, m_currentUrl);
    m_watcher->watch(m_currentUrl);

    if (m_partial || !m_loaded) {
        m_loaded = false;
        m_dirty = false;
        m_partial = false;
        m_settled = false;
        m_refreshing = false;
        m_loader->load(m_currentUrl);
        return;
    }

    if (m_dirty) {
        m_dirty = false;
        refresh();
    }
}

void TabController::deactivate()
{
    if (!m_active)
        return;

    m_active = false;

    if (m_loading)
        m_partial = true;
    if (m_refreshing || !m_settled)
        m_dirty = true;

    m_refreshing = false;
    m_refreshPending = false;

    if (m_loader)
        m_loader->detach();

    setLoading(false);
}

void TabController::refresh()
{
    if (!m_active) {
        m_dirty = true;
        return;
    }

    if (!m_currentUrl.isValid())
        return;

    if (m_loading || m_refreshing) {
        m_refreshPending = true;
        return;
    }

    m_refreshing = true;
    if (!m_loader || m_loader->currentDirectory() != m_currentUrl) {
        m_refreshing = false;
        m_dirty = true;
        return;
    }

    m_loader->refresh();
}

void TabController::markDirty()
{
    m_dirty = true;
}

void TabController::refreshAfterOperation()
{
    if (m_active) {
        refresh();
    } else {
        m_dirty = true;
    }
}

void TabController::setShowHidden(bool show)
{
    m_proxy->setShowHidden(show);
}

void TabController::setSortMode(int mode)
{
    m_proxy->setSortMode(FileListProxyModel::SortMode(mode));
}

void TabController::setSortDescending(bool descending)
{
    m_proxy->setDescending(descending);
}

void TabController::handleDirectoryGone()
{
    m_directoryGone = false;
    setStatus(QCoreApplication::translate("Ooze", "La carpeta ya no existe"));

    if (canGoBack()) {
        m_historyIndex--;
        emit historyChanged();
        navigate(m_history.at(m_historyIndex), false);
        return;
    }

    goUp();
}

void TabController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void TabController::selectRow(int row, bool toggle, bool range)
{
    if (row < 0 || row >= m_proxy->rowCount())
        return;

    const QString url = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
    if (url.isEmpty())
        return;

    if (range) {
        int anchorRow = rowForUrl(m_anchorUrl);
        if (anchorRow < 0)
            anchorRow = row;

        if (!toggle)
            m_selectedUrls.clear();

        const int first = qMin(anchorRow, row);
        const int last = qMax(anchorRow, row);
        for (int current = first; current <= last; ++current)
            m_selectedUrls.insert(
                m_proxy->index(current, 0).data(FileListModel::UrlRole).toString());

        if (m_anchorUrl.isEmpty())
            m_anchorUrl = m_proxy->index(anchorRow, 0)
                              .data(FileListModel::UrlRole).toString();
    } else if (toggle) {
        if (m_selectedUrls.contains(url))
            m_selectedUrls.remove(url);
        else
            m_selectedUrls.insert(url);
        m_anchorUrl = url;
    } else {
        m_selectedUrls.clear();
        m_selectedUrls.insert(url);
        m_anchorUrl = url;
    }

    notifySelection();
}

void TabController::selectRows(const QVector<int> &rows, int mode)
{
    if (mode == 0) {
        QSet<QString> next;
        next.reserve(rows.size());
        QString first;

        for (int row : rows) {
            if (row < 0 || row >= m_proxy->rowCount())
                continue;
            const QString url = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
            if (url.isEmpty())
                continue;
            next.insert(url);
            if (first.isEmpty())
                first = url;
        }

        if (next == m_selectedUrls) {
            m_anchorUrl = first;
            return;
        }

        m_selectedUrls = next;
        m_anchorUrl = first;
        notifySelection();
        return;
    }

    bool changed = false;
    QString first;

    for (int row : rows) {
        if (row < 0 || row >= m_proxy->rowCount())
            continue;
        const QString url = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
        if (url.isEmpty())
            continue;

        if (mode == 2 && m_selectedUrls.contains(url)) {
            m_selectedUrls.remove(url);
            changed = true;
            if (first.isEmpty())
                first = url;
        } else if (mode == 1) {
            if (!m_selectedUrls.contains(url)) {
                m_selectedUrls.insert(url);
                changed = true;
            }
            if (first.isEmpty())
                first = url;
        } else if (mode == 2) {
            m_selectedUrls.insert(url);
            changed = true;
            if (first.isEmpty())
                first = url;
        }
    }

    if (!changed)
        return;

    if (!first.isEmpty())
        m_anchorUrl = first;
    notifySelection();
}

void TabController::selectAll()
{
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QString url = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
        if (!url.isEmpty())
            m_selectedUrls.insert(url);
    }

    if (!m_anchorUrl.isEmpty() && rowForUrl(m_anchorUrl) < 0)
        m_anchorUrl.clear();

    notifySelection();
}

void TabController::clearSelection()
{
    if (m_selectedUrls.isEmpty() && m_anchorUrl.isEmpty())
        return;

    m_selectedUrls.clear();
    m_anchorUrl.clear();
    notifySelection();
}

bool TabController::isSelected(int row) const
{
    if (row < 0 || row >= m_proxy->rowCount())
        return false;
    return m_selectedUrls.contains(
        m_proxy->index(row, 0).data(FileListModel::UrlRole).toString());
}

QVector<int> TabController::selectedRows() const
{
    QVector<int> rows;
    if (m_selectedUrls.isEmpty())
        return rows;

    rows.reserve(qMin(int(m_selectedUrls.size()), m_proxy->rowCount()));
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QString url = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
        if (m_selectedUrls.contains(url))
            rows.append(row);
    }
    return rows;
}

QVector<QUrl> TabController::selectedUrlsInOrder() const
{
    QVector<QUrl> result;
    result.reserve(m_selectedUrls.size());

    for (int row = 0; row < m_proxy->rowCount() && result.size() < m_selectedUrls.size(); ++row) {
        const QUrl url(m_proxy->index(row, 0).data(FileListModel::UrlRole).toString());
        if (m_selectedUrls.contains(url.toString()) && url.isLocalFile())
            result.append(url);
    }

    return result;
}

int TabController::rowForUrl(const QString &url) const
{
    if (url.isEmpty())
        return -1;

    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        if (m_proxy->index(row, 0).data(FileListModel::UrlRole).toString() == url)
            return row;
    }
    return -1;
}

int TabController::rowForName(const QString &name) const
{
    if (name.isEmpty())
        return -1;

    const int rows = m_proxy->rowCount();
    for (int row = 0; row < rows; ++row) {
        if (m_proxy->index(row, 0).data(FileListModel::NameRole).toString() == name)
            return row;
    }

    return -1;
}

QVariantList TabController::selectedEntries() const
{
    const QVector<int> rows = selectedRows();
    QVariantList entries;
    entries.reserve(rows.size());

    for (const int row : rows) {
        const QModelIndex index = m_proxy->index(row, 0);
        if (!index.isValid())
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("row"), row);
        entry.insert(QStringLiteral("name"), index.data(FileListModel::NameRole).toString());
        entry.insert(QStringLiteral("url"), index.data(FileListModel::UrlRole).toString());
        entry.insert(QStringLiteral("isDir"), index.data(FileListModel::IsDirRole).toBool());
        entries.append(entry);
    }

    return entries;
}

QStringList TabController::currentNames() const
{
    QStringList names;
    names.reserve(m_proxy->rowCount());
    for (int row = 0; row < m_proxy->rowCount(); ++row)
        names.append(m_proxy->index(row, 0).data(FileListModel::NameRole).toString());
    return names;
}

QString TabController::suggestedFolderName() const
{
    return FileOperations::suggestedName(m_currentUrl, QCoreApplication::translate("Ooze", "Nueva carpeta"), true);
}

QString TabController::suggestedFileName() const
{
    return FileOperations::suggestedName(m_currentUrl, QCoreApplication::translate("Ooze", "Nuevo archivo"), false);
}

QString TabController::dragUriList(int row) const
{
    QVector<QUrl> urls;

    if (row >= 0 && row < m_proxy->rowCount()) {
        const QString rowUrl = m_proxy->index(row, 0).data(FileListModel::UrlRole).toString();
        if (m_selectedUrls.contains(rowUrl)) {
            urls = selectedUrlsInOrder();
        } else if (!rowUrl.isEmpty()) {
            const QUrl url(rowUrl);
            if (url.isValid())
                urls.append(url);
        }
    }

    if (urls.isEmpty())
        urls = selectedUrlsInOrder();

    QStringList lines;
    lines.reserve(urls.size());
    for (const QUrl &url : urls)
        lines.append(url.toString());
    return lines.join(QStringLiteral("\r\n"));
}

QString TabController::selectedName() const
{
    if (m_selectedUrls.size() != 1)
        return {};

    const QUrl url(*m_selectedUrls.constBegin());
    if (!url.isLocalFile())
        return url.fileName();

    const QByteArray path = PathUtils::toLocalBytes(PathUtils::normalized(url));
    return PathUtils::displayName(path.mid(path.lastIndexOf('/') + 1));
}

bool TabController::selectedIsDirectory() const
{
    if (m_selectedUrls.size() != 1)
        return false;

    const int row = rowForUrl(*m_selectedUrls.constBegin());
    return row >= 0
        && m_proxy->index(row, 0).data(FileListModel::IsDirRole).toBool();
}

QString TabController::filterText() const
{
    return m_proxy->filterText();
}

void TabController::setFilterText(const QString &text)
{
    if (m_proxy->filterText() == text)
        return;
    m_proxy->setFilterText(text);
    clearSelection();
    emit filterTextChanged();
}

void TabController::setPendingSelection(const QString &name)
{
    if (m_pendingSelection == name)
        return;
    m_pendingSelection = name;
    emit pendingSelectionChanged();
    trySelectPending();
}

void TabController::clearPendingSelection()
{
    if (m_pendingSelection.isEmpty())
        return;
    m_pendingSelection.clear();
    emit pendingSelectionChanged();
}

void TabController::trySelectPending()
{
    if (m_pendingSelection.isEmpty())
        return;

    const int row = rowForName(m_pendingSelection);
    if (row < 0)
        return;

    m_pendingSelection.clear();
    emit pendingSelectionChanged();
    selectRow(row, false, false);
    emit selectRowRequested(row);
}

void TabController::setStatus(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void TabController::notifySelection()
{
    ++m_selectionRevision;
    emit selectionChanged();
}

void TabController::pruneSelection()
{
    if (m_selectedUrls.isEmpty())
        return;

    QSet<QString> present;
    for (int row = 0; row < m_proxy->rowCount(); ++row)
        present.insert(m_proxy->index(row, 0).data(FileListModel::UrlRole).toString());

    bool changed = false;
    for (auto it = m_selectedUrls.begin(); it != m_selectedUrls.end();) {
        if (!present.contains(*it)) {
            it = m_selectedUrls.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        if (rowForUrl(m_anchorUrl) < 0)
            m_anchorUrl.clear();
        notifySelection();
    }
}
