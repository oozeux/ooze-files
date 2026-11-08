#pragma once

#include "core/CancelToken.h"
#include "vfs/VfsTypes.h"

#include <QObject>
#include <QThreadPool>
#include <QUrl>

class CancelToken;
class FileListModel;
class LoaderSignals;
class VfsRegistry;

class DirectoryLoader : public QObject
{
    Q_OBJECT
public:
    explicit DirectoryLoader(VfsRegistry *registry, QObject *parent = nullptr);
    ~DirectoryLoader() override;

    void setModel(FileListModel *model);

    // Shared-loader support. attach() binds a tab model without touching its
    // rows; detach() cancels the current generation and releases the model.
    void attach(FileListModel *model, const QUrl &directory);
    void detach();

    void load(const QUrl &dir);
    void refresh();
    void cancel();

    [[nodiscard]] bool isLoading() const { return m_loading; }
    [[nodiscard]] QUrl currentDirectory() const { return m_directory; }

signals:
    void loadingChanged(bool loading);
    void loadStarted(const QUrl &directory);
    void loadFinished(const QUrl &directory, const VfsError &error);
    void statsFinished();

private:
    void onBatch(quint64 generation, QVector<VfsEntry> entries);
    void onStats(quint64 generation, QVector<VfsStat> stats);
    void onFinished(quint64 generation, VfsError error);
    void onStatsDone(quint64 generation);
    void onRefreshed(quint64 generation, QVector<VfsEntry> entries, VfsError error);
    void requestStats(quint64 generation);
    void setLoading(bool loading);
    void finishStatsIfIdle();

    VfsRegistry *m_registry = nullptr;
    FileListModel *m_model = nullptr;
    LoaderSignals *m_signals = nullptr;

    QThreadPool m_listPool;
    QThreadPool m_statPool;

    CancelTokenPtr m_token;
    QUrl m_directory;
    quint64 m_generation = 0;
    int m_statsPending = 0;
    bool m_statsNotified = false;
    bool m_loading = false;
};
