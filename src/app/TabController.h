#pragma once

#include "vfs/VfsTypes.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

class DirectoryLoader;
class DirectoryWatcher;
class FileListModel;
class FileListProxyModel;

// Per-tab state: model, proxy, history, selection, filter, status and watcher.
// Global services (config, clipboard, undo, file operations) stay in
// ApplicationController. Only the active tab is attached to the shared loader.
class TabController : public QObject
{
    Q_OBJECT

public:
    explicit TabController(DirectoryLoader *loader, QObject *parent = nullptr);
    ~TabController() override;

    [[nodiscard]] FileListModel *model() const { return m_model; }
    [[nodiscard]] FileListProxyModel *proxy() const { return m_proxy; }

    [[nodiscard]] QUrl currentUrl() const { return m_currentUrl; }
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QVariantList breadcrumb() const;
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;
    [[nodiscard]] int itemCount() const;
    [[nodiscard]] QString freeSpaceText() const;
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] QString statusMessage() const { return m_statusMessage; }
    [[nodiscard]] QString pendingSelection() const { return m_pendingSelection; }
    [[nodiscard]] int selectionCount() const { return int(m_selectedUrls.size()); }
    [[nodiscard]] int selectionRevision() const { return m_selectionRevision; }
    [[nodiscard]] bool canRenameSelection() const { return m_selectedUrls.size() == 1 && m_currentUrl.isLocalFile(); }
    [[nodiscard]] bool selectedIsDirectory() const;
    [[nodiscard]] QString selectedName() const;
    [[nodiscard]] QString filterText() const;

    [[nodiscard]] bool isActive() const { return m_active; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] bool isPartial() const { return m_partial; }

    void start(const QUrl &url, const QString &selectName = {});
    void navigate(const QUrl &url, bool recordHistory);
    void goBack();
    void goForward();
    void goUp();
    void reload();

    // Loader lifecycle. activate() binds this tab to the shared loader and
    // loads only when needed; deactivate() cancels and marks pending work.
    void activate();
    void deactivate();
    void refresh();
    void markDirty();
    void refreshAfterOperation();

    void setShowHidden(bool show);
    void setSortMode(int mode);
    void setSortDescending(bool descending);

    void selectRow(int row, bool toggle, bool range);
    void selectRows(const QVector<int> &rows, int mode);
    void selectAll();
    void clearSelection();
    [[nodiscard]] bool isSelected(int row) const;
    [[nodiscard]] QVector<int> selectedRows() const;
    [[nodiscard]] QVector<QUrl> selectedUrlsInOrder() const;
    [[nodiscard]] int rowForUrl(const QString &url) const;
    [[nodiscard]] int rowForName(const QString &name) const;
    [[nodiscard]] QVariantList selectedEntries() const;
    [[nodiscard]] QStringList currentNames() const;
    [[nodiscard]] QString suggestedFolderName() const;
    [[nodiscard]] QString suggestedFileName() const;
    [[nodiscard]] QString dragUriList(int row) const;

    void setFilterText(const QString &text);
    void setPendingSelection(const QString &name);
    void clearPendingSelection();
    void setStatus(const QString &message);

signals:
    void currentUrlChanged();
    void titleChanged();
    void historyChanged();
    void itemCountChanged();
    void selectionChanged();
    void statusMessageChanged();
    void pendingSelectionChanged();
    void filterTextChanged();
    void loadingChanged();
    void selectRowRequested(int row);

private:
    void connectLoader();
    void onLoadStarted(const QUrl &directory);
    void onLoadFinished(const QUrl &directory, const VfsError &error);
    void onStatsFinished();
    void handleDirectoryGone();
    void trySelectPending();
    void pruneSelection();
    void notifySelection();
    void setLoading(bool loading);

    DirectoryLoader *m_loader = nullptr;
    FileListModel *m_model = nullptr;
    FileListProxyModel *m_proxy = nullptr;
    DirectoryWatcher *m_watcher = nullptr;

    QUrl m_currentUrl;
    QVector<QUrl> m_history;
    int m_historyIndex = -1;

    QSet<QString> m_selectedUrls;
    QString m_anchorUrl;
    int m_selectionRevision = 0;

    QString m_statusMessage;
    QString m_pendingSelection;

    bool m_active = false;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_partial = false;
    bool m_settled = false;
    bool m_loading = false;
    bool m_refreshing = false;
    bool m_refreshPending = false;
    bool m_directoryGone = false;
};
