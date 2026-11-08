#include "app/SearchModel.h"

#include "core/FileIcons.h"
#include "core/PathUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QThread>
#include <QUrl>

#include <atomic>

namespace {

constexpr int kMaxResults = 2000;
constexpr int kBatchSize = 64;

QString iconFor(const QFileInfo &info, const QString &mimeName,
                const QMimeDatabase &mimeDb)
{
    if (info.isSymLink())
        return info.isDir() ? QStringLiteral("folder-symlink")
                            : QStringLiteral("text-x-generic");
    if (info.isDir())
        return QStringLiteral("folder");

    const QMimeType mime = mimeDb.mimeTypeForName(mimeName);
    QString icon = mime.iconName();
    if (icon.isEmpty())
        icon = QStringLiteral("text-x-generic");

    return FileIcons::iconForFile(info.absoluteFilePath(), icon);
}

bool matchesType(const QString &type, const QFileInfo &info, const QString &mimeName)
{
    if (type.isEmpty() || type == QLatin1String("all"))
        return true;
    if (type == QLatin1String("files"))
        return !info.isDir();
    if (type == QLatin1String("dirs"))
        return info.isDir();
    if (info.isDir())
        return false;

    if (type == QLatin1String("images"))
        return mimeName.startsWith(QLatin1String("image/"));
    if (type == QLatin1String("videos"))
        return mimeName.startsWith(QLatin1String("video/"));
    if (type == QLatin1String("audio"))
        return mimeName.startsWith(QLatin1String("audio/"));
    if (type == QLatin1String("text"))
        return mimeName.startsWith(QLatin1String("text/"))
            || mimeName == QLatin1String("application/json")
            || mimeName == QLatin1String("application/xml");
    if (type == QLatin1String("pdf"))
        return mimeName == QLatin1String("application/pdf");
    return true;
}

} // namespace

class SearchWorker : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    void cancel() { m_token.store(0, std::memory_order_release); }

public slots:
    void run(quint64 token, const QString &rootPath, const QString &query,
             const QString &type, bool includeHidden)
    {
        m_token.store(token, std::memory_order_release);

        const QUrl rootUrl(rootPath);
        const QString root = rootUrl.isLocalFile() ? rootUrl.toLocalFile() : rootPath;
        if (root.isEmpty())
            return;

        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
        if (includeHidden)
            filters |= QDir::Hidden;
        filters |= QDir::System;

        const QString needle = query.trimmed();
        const bool filterType = !type.isEmpty() && type != QLatin1String("all");

        QMimeDatabase mimeDb;
        QDirIterator iterator(root, filters, QDirIterator::Subdirectories);

        QVariantList batch;
        batch.reserve(kBatchSize);
        int total = 0;
        bool truncated = false;

        while (iterator.hasNext()) {
            if (m_token.load(std::memory_order_acquire) != token)
                return;

            iterator.next();
            const QFileInfo info = iterator.fileInfo();
            const QString name = info.fileName();
            if (name.isEmpty())
                continue;

            if (!needle.isEmpty()
                && !name.contains(needle, Qt::CaseInsensitive)) {
                continue;
            }

            const QString mimeName = mimeDb.mimeTypeForFile(
                info, QMimeDatabase::MatchExtension).name();

            if (filterType && !matchesType(type, info, mimeName))
                continue;

            const QString absolute = info.absoluteFilePath();
            const QString relative = QDir(root).relativeFilePath(absolute);

            QVariantMap entry;
            entry.insert(QStringLiteral("name"),
                         info.isDir() ? name + QLatin1Char('/') : name);
            entry.insert(QStringLiteral("url"),
                         QUrl::fromLocalFile(absolute).toString());
            entry.insert(QStringLiteral("relativePath"), relative);
            entry.insert(QStringLiteral("isDir"), info.isDir());
            entry.insert(QStringLiteral("iconName"), iconFor(info, mimeName, mimeDb));
            entry.insert(QStringLiteral("sizeText"),
                         info.isDir() ? QString() : PathUtils::formatSize(info.size()));
            entry.insert(QStringLiteral("mimeType"), mimeName);
            batch.append(entry);

            ++total;
            if (batch.size() >= kBatchSize) {
                emit results(token, batch, false);
                batch.clear();
            }

            if (total >= kMaxResults) {
                truncated = true;
                break;
            }
        }

        if (m_token.load(std::memory_order_acquire) != token)
            return;

        if (!batch.isEmpty())
            emit results(token, batch, truncated);

        emit finished(token, truncated);
    }

signals:
    void results(quint64 token, const QVariantList &entries, bool truncated);
    void finished(quint64 token, bool truncated);

private:
    std::atomic<quint64> m_token{0};
};

SearchModel::SearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_worker = new SearchWorker;
    m_worker->moveToThread(&m_thread);

    connect(m_worker, &SearchWorker::results, this, &SearchModel::appendBatch);
    connect(m_worker, &SearchWorker::finished, this, &SearchModel::finishSearch);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // The thread is started on the first search: an idle file manager should
    // not pay for a worker it never uses.
    m_thread.setObjectName(QStringLiteral("ooze-files-search"));
}

SearchModel::~SearchModel()
{
    if (m_worker)
        m_worker->cancel();

    if (!m_thread.isRunning()) {
        delete m_worker;
        m_worker = nullptr;
        return;
    }

    m_thread.quit();
    if (!m_thread.wait(3000)) {
        m_thread.terminate();
        m_thread.wait();
    }
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case UrlRole:
        return entry.url;
    case RelativePathRole:
        return entry.relativePath;
    case IsDirRole:
        return entry.isDir;
    case IconRole:
        return entry.iconName;
    case SizeTextRole:
        return entry.sizeText;
    case MimeRole:
        return entry.mimeType;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {UrlRole, "url"},
        {RelativePathRole, "relativePath"},
        {IsDirRole, "isDir"},
        {IconRole, "iconName"},
        {SizeTextRole, "sizeText"},
        {MimeRole, "mimeType"},
    };
}

void SearchModel::start(const QString &rootUrl, const QString &query,
                        const QString &type, bool includeHidden)
{
    const QUrl url(rootUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : rootUrl;
    if (path.isEmpty())
        return;

    clear();
    m_rootPath = path;
    emit rootPathChanged();

    const quint64 token = ++m_nextToken;
    m_activeToken = token;
    m_worker->cancel();

    if (!m_thread.isRunning())
        m_thread.start();

    setSearching(true);
    m_truncated = false;
    emit truncatedChanged();

    QMetaObject::invokeMethod(m_worker, "run", Qt::QueuedConnection,
                              Q_ARG(quint64, token),
                              Q_ARG(QString, rootUrl),
                              Q_ARG(QString, query),
                              Q_ARG(QString, type),
                              Q_ARG(bool, includeHidden));
}

void SearchModel::cancel()
{
    if (!m_searching)
        return;
    m_activeToken = 0;
    m_worker->cancel();
    setSearching(false);
}

void SearchModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

QString SearchModel::urlAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row).url;
}

bool SearchModel::isDirAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return false;
    return m_entries.at(row).isDir;
}

void SearchModel::appendBatch(quint64 token, const QVariantList &entries, bool truncated)
{
    if (token != m_activeToken || entries.isEmpty())
        return;

    const int first = int(m_entries.size());
    beginInsertRows({}, first, first + int(entries.size()) - 1);

    m_entries.reserve(m_entries.size() + entries.size());
    for (const QVariant &value : entries) {
        const QVariantMap map = value.toMap();
        Entry entry;
        entry.name = map.value(QStringLiteral("name")).toString();
        entry.url = map.value(QStringLiteral("url")).toString();
        entry.relativePath = map.value(QStringLiteral("relativePath")).toString();
        entry.iconName = map.value(QStringLiteral("iconName")).toString();
        entry.sizeText = map.value(QStringLiteral("sizeText")).toString();
        entry.mimeType = map.value(QStringLiteral("mimeType")).toString();
        entry.isDir = map.value(QStringLiteral("isDir")).toBool();
        m_entries.append(entry);
    }

    endInsertRows();
    emit countChanged();

    if (truncated && !m_truncated) {
        m_truncated = true;
        emit truncatedChanged();
    }
}

void SearchModel::finishSearch(quint64 token, bool truncated)
{
    if (token != m_activeToken)
        return;

    m_activeToken = 0;
    setSearching(false);

    if (truncated && !m_truncated) {
        m_truncated = true;
        emit truncatedChanged();
    }
}

void SearchModel::setSearching(bool searching)
{
    if (m_searching == searching)
        return;
    m_searching = searching;
    emit searchingChanged();
}

#include "SearchModel.moc"
