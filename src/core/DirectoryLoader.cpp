#include "core/DirectoryLoader.h"

#include "core/FileIcons.h"
#include "core/FileListModel.h"
#include "core/LoaderSignals.h"
#include "core/PathUtils.h"
#include "vfs/VfsRegistry.h"

#include <QMimeDatabase>
#include <QMimeType>

#include <sys/stat.h>
#include <QCoreApplication>

namespace {
constexpr int kStatChunk = 256;

struct StatItem
{
    int index = -1;
    QByteArray path;
    QByteArray name;
};

class ListTask : public QRunnable
{
public:
    ListTask(VfsBackend *backend, QUrl dir, quint64 generation,
             CancelTokenPtr token, LoaderSignals *bridge)
        : m_backend(backend)
        , m_dir(std::move(dir))
        , m_generation(generation)
        , m_token(std::move(token))
        , m_bridge(bridge)
    {
    }

    void run() override
    {
        if (!m_backend) {
            VfsError error;
            error.code = VfsError::Unsupported;
            error.url = m_dir;
            error.message = QCoreApplication::translate("Ooze", "No hay backend para esta ubicación");
            emit m_bridge->finished(m_generation, error);
            return;
        }

        m_backend->list(
            m_dir,
            [this](QVector<VfsEntry> &&entries, const VfsError &error) {
                if (!entries.isEmpty())
                    emit m_bridge->batch(m_generation, std::move(entries));
                else
                    emit m_bridge->finished(m_generation, error);
            },
            *m_token);
    }

private:
    VfsBackend *m_backend = nullptr;
    QUrl m_dir;
    quint64 m_generation = 0;
    CancelTokenPtr m_token;
    LoaderSignals *m_bridge = nullptr;
};

class RefreshTask : public QRunnable
{
public:
    RefreshTask(VfsBackend *backend, QUrl dir, quint64 generation,
                CancelTokenPtr token, LoaderSignals *bridge)
        : m_backend(backend)
        , m_dir(std::move(dir))
        , m_generation(generation)
        , m_token(std::move(token))
        , m_bridge(bridge)
    {
    }

    void run() override
    {
        if (!m_backend) {
            VfsError error;
            error.code = VfsError::Unsupported;
            error.url = m_dir;
            error.message = QCoreApplication::translate("Ooze", "No hay backend para esta ubicación");
            emit m_bridge->refreshed(m_generation, {}, error);
            return;
        }

        QVector<VfsEntry> all;
        m_backend->list(
            m_dir,
            [this, &all](QVector<VfsEntry> &&entries, const VfsError &error) {
                if (!entries.isEmpty()) {
                    for (VfsEntry &entry : entries)
                        all.append(std::move(entry));
                    return;
                }
                emit m_bridge->refreshed(m_generation, std::move(all), error);
            },
            *m_token);
    }

private:
    VfsBackend *m_backend = nullptr;
    QUrl m_dir;
    quint64 m_generation = 0;
    CancelTokenPtr m_token;
    LoaderSignals *m_bridge = nullptr;
};

class StatTask : public QRunnable
{
public:
    StatTask(QVector<StatItem> items, quint64 generation, CancelTokenPtr token,
             LoaderSignals *bridge)
        : m_items(std::move(items))
        , m_generation(generation)
        , m_token(std::move(token))
        , m_bridge(bridge)
    {
    }

    void run() override
    {
        QVector<VfsStat> out;
        out.reserve(kStatChunk);
        QMimeDatabase mimeDb;
        bool cancelled = false;

        for (const StatItem &item : m_items) {
            if (m_token->isCancelled()) {
                cancelled = true;
                break;
            }

            struct stat st {};
            if (::lstat(item.path.constData(), &st) != 0)
                continue;

            const bool isDir = S_ISDIR(st.st_mode);
            const bool isLink = S_ISLNK(st.st_mode);

            VfsStat result;
            result.index = item.index;
            result.size = isDir ? -1 : qint64(st.st_size);
            result.mtimeSecs = qint64(st.st_mtime);

            if (item.name.startsWith('.'))
                result.flags |= FlagHidden;

            if (isDir) {
                result.mimeType = QStringLiteral("inode/directory");
                result.iconName = QStringLiteral("folder");
            } else if (isLink) {
                struct stat target {};
                const bool targetExists = ::stat(item.path.constData(), &target) == 0;
                if (!targetExists)
                    result.flags |= FlagSymlinkBroken;

                if (targetExists && S_ISDIR(target.st_mode)) {
                    result.flags |= FlagSymlinkDir;
                    result.size = -1;
                    result.mimeType = QStringLiteral("inode/directory");
                    result.iconName = QStringLiteral("folder-symlink");
                } else {
                    const QMimeType mime = mimeDb.mimeTypeForFile(
                        QString::fromUtf8(item.path), QMimeDatabase::MatchExtension);
                    result.mimeType = mime.name();
                    result.iconName = mime.iconName();
                    if (result.iconName.isEmpty())
                        result.iconName = QStringLiteral("text-x-generic");
                }
            } else {
                const QMimeType mime = mimeDb.mimeTypeForFile(
                    QString::fromUtf8(item.path), QMimeDatabase::MatchExtension);
                result.mimeType = mime.name();
                result.iconName = mime.iconName();
                if (result.iconName.isEmpty())
                    result.iconName = QStringLiteral("text-x-generic");
            }

            if (!isDir && (result.flags & FlagSymlinkDir) == 0) {
                result.iconName = FileIcons::iconForFile(QString::fromUtf8(item.path),
                                                         result.iconName);
            }

            out.append(result);

            if (out.size() >= kStatChunk) {
                emit m_bridge->stats(m_generation, out);
                out.clear();
                out.reserve(kStatChunk);
            }
        }

        if (!cancelled && !out.isEmpty())
            emit m_bridge->stats(m_generation, out);

        emit m_bridge->statsDone(m_generation);
    }

private:
    QVector<StatItem> m_items;
    quint64 m_generation = 0;
    CancelTokenPtr m_token;
    LoaderSignals *m_bridge = nullptr;
};
}

DirectoryLoader::DirectoryLoader(VfsRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_signals(new LoaderSignals(this))
{
    m_listPool.setMaxThreadCount(1);
    m_listPool.setObjectName(QStringLiteral("ooze-files-list"));

    m_statPool.setMaxThreadCount(2);
    m_statPool.setObjectName(QStringLiteral("ooze-files-stat"));

    qRegisterMetaType<VfsEntry>();
    qRegisterMetaType<VfsStat>();
    qRegisterMetaType<VfsError>();
    qRegisterMetaType<QVector<VfsEntry>>("QVector<VfsEntry>");
    qRegisterMetaType<QVector<VfsStat>>("QVector<VfsStat>");

    connect(m_signals, &LoaderSignals::batch, this, &DirectoryLoader::onBatch);
    connect(m_signals, &LoaderSignals::stats, this, &DirectoryLoader::onStats);
    connect(m_signals, &LoaderSignals::statsDone, this, &DirectoryLoader::onStatsDone);
    connect(m_signals, &LoaderSignals::finished, this, &DirectoryLoader::onFinished);
    connect(m_signals, &LoaderSignals::refreshed, this, &DirectoryLoader::onRefreshed);
}

DirectoryLoader::~DirectoryLoader()
{
    if (m_token)
        m_token->cancel();

    m_listPool.waitForDone();
    m_statPool.waitForDone();
}

void DirectoryLoader::setModel(FileListModel *model)
{
    m_model = model;
}

void DirectoryLoader::attach(FileListModel *model, const QUrl &directory)
{
    if (m_token)
        m_token->cancel();

    m_generation++;
    m_model = model;
    m_directory = PathUtils::normalized(directory);
    m_statsPending = 0;
    m_statsNotified = false;
    setLoading(false);
}

void DirectoryLoader::detach()
{
    if (m_token)
        m_token->cancel();

    m_generation++;
    m_model = nullptr;
    m_directory = QUrl();
    m_statsPending = 0;
    m_statsNotified = false;
    setLoading(false);
}

void DirectoryLoader::load(const QUrl &dir)
{
    if (!m_registry || !m_model)
        return;

    if (m_token)
        m_token->cancel();

    const QUrl target = PathUtils::normalized(dir);
    m_generation++;
    m_token = std::make_shared<CancelToken>();
    m_directory = target;
    m_statsPending = 0;
    m_statsNotified = false;

    m_model->beginLoad();
    setLoading(true);
    emit loadStarted(target);

    const quint64 generation = m_generation;
    VfsBackend *backend = m_registry->backendFor(target);

    m_listPool.start(new ListTask(backend, target, generation, m_token, m_signals));
}

void DirectoryLoader::refresh()
{
    if (!m_registry || !m_model || !m_directory.isValid() || m_loading)
        return;

    if (m_token)
        m_token->cancel();

    m_generation++;
    m_token = std::make_shared<CancelToken>();
    m_statsPending = 0;
    m_statsNotified = false;

    const quint64 generation = m_generation;
    VfsBackend *backend = m_registry->backendFor(m_directory);

    m_listPool.start(new RefreshTask(backend, m_directory, generation, m_token, m_signals));
}

void DirectoryLoader::cancel()
{
    if (m_token)
        m_token->cancel();
}

void DirectoryLoader::onBatch(quint64 generation, QVector<VfsEntry> entries)
{
    if (generation != m_generation || !m_model || entries.isEmpty())
        return;

    const int base = m_model->rowCount();

    QVector<StatItem> items;
    items.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const VfsEntry &entry = entries.at(i);
        items.append(StatItem{base + i,
                              entry.path,
                              entry.id});
    }

    m_model->appendEntries(entries);
    ++m_statsPending;
    m_statPool.start(new StatTask(std::move(items), generation, m_token, m_signals));
}

void DirectoryLoader::onStatsDone(quint64 generation)
{
    if (generation != m_generation)
        return;

    if (m_statsPending > 0)
        --m_statsPending;

    finishStatsIfIdle();
}

void DirectoryLoader::onStats(quint64 generation, QVector<VfsStat> stats)
{
    if (generation != m_generation || !m_model)
        return;
    m_model->applyStats(stats);
}

void DirectoryLoader::onRefreshed(quint64 generation, QVector<VfsEntry> entries,
                                  VfsError error)
{
    if (generation != m_generation || !m_model)
        return;

    if (error.isValid()) {
        if (!error.isCancelled())
            emit loadFinished(m_directory, error);
        return;
    }

    m_model->syncEntries(entries);
    requestStats(generation);
    emit loadFinished(m_directory, {});
    finishStatsIfIdle();
}

void DirectoryLoader::requestStats(quint64 generation)
{
    if (!m_model)
        return;

    QVector<StatItem> items;
    items.reserve(m_model->rowCount());

    for (int row = 0; row < m_model->rowCount(); ++row) {
        const VfsEntry *entry = m_model->entryAt(row);
        if (entry)
            items.append(StatItem{row, entry->path, entry->id});
    }

    if (!items.isEmpty()) {
        ++m_statsPending;
        m_statPool.start(new StatTask(std::move(items), generation, m_token, m_signals));
    }
}

void DirectoryLoader::onFinished(quint64 generation, VfsError error)
{
    if (generation != m_generation)
        return;

    setLoading(false);
    emit loadFinished(m_directory, error);
    finishStatsIfIdle();
}

void DirectoryLoader::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged(loading);
}

void DirectoryLoader::finishStatsIfIdle()
{
    if (m_statsPending > 0 || m_statsNotified)
        return;
    m_statsNotified = true;
    emit statsFinished();
}
