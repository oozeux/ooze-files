#include "vfs/TrashVfs.h"

#include "core/CancelToken.h"
#include "core/PathUtils.h"
#include "core/TrashService.h"
#include "vfs/LocalVfs.h"

#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr int kBatchSize = 500;

EntryKind kindFromMode(mode_t mode)
{
    if (S_ISDIR(mode))
        return EntryKind::Directory;
    if (S_ISLNK(mode))
        return EntryKind::Symlink;
    if (S_ISREG(mode))
        return EntryKind::File;
    return EntryKind::Unknown;
}
}

TrashVfs::TrashVfs(QObject *parent)
    : VfsBackend(parent)
    , m_local(new LocalVfs(this))
{
}

QString TrashVfs::scheme() const
{
    return QStringLiteral("trash");
}

VfsCapabilities TrashVfs::capabilities(const QUrl &) const
{
    VfsCapabilities caps;
    caps.canList = true;
    caps.canRead = true;
    caps.canWatch = true;
    return caps;
}

void TrashVfs::list(const QUrl &dir, const ListCallback &callback, CancelToken &token)
{
    TrashService::Location location;
    if (!TrashService::resolve(dir, &location)) {
        callback({}, VfsError::fromErrno(ENOENT, dir));
        return;
    }

    if (!location.root) {
        m_local->list(PathUtils::fromLocalBytes(location.realPath), callback, token);
        return;
    }

    QVector<VfsEntry> batch;
    const QVector<TrashDir> dirs = TrashService::discoverDirs();

    for (const TrashDir &trashDir : dirs) {
        const QVector<TrashItem> items = TrashService::items(trashDir);
        for (const TrashItem &item : items) {
            if (token.isCancelled()) {
                VfsError error;
                error.code = VfsError::Cancelled;
                error.url = dir;
                callback({}, error);
                return;
            }

            struct stat st {};
            if (::lstat(item.path.constData(), &st) != 0)
                continue;

            VfsEntry entry;
            entry.id = item.path;
            entry.path = item.path;
            entry.url = PathUtils::fromLocalBytes(item.path);
            entry.displayName = PathUtils::displayName(item.displayBytes());
            entry.kind = kindFromMode(st.st_mode);
            entry.size = entry.kind == EntryKind::Directory ? -1 : qint64(st.st_size);
            entry.modified = QDateTime::fromSecsSinceEpoch(qint64(st.st_mtime));
            entry.iconName = entry.kind == EntryKind::Directory ? QStringLiteral("folder")
                                                                : QStringLiteral("text-x-generic");
            entry.activity = item.deleted;

            if (item.hasInfo) {
                const qsizetype slash = item.originalPath.lastIndexOf('/');
                entry.location = PathUtils::homeAbbreviated(
                    slash <= 0 ? QByteArrayLiteral("/") : item.originalPath.left(slash));
            }

            const QByteArray display = item.displayBytes();
            const qsizetype dot = display.lastIndexOf('.');
            if (dot > 0 && dot < display.size() - 1)
                entry.suffix = QString::fromLatin1(display.mid(dot + 1).toLower());

            if (::access(item.path.constData(), R_OK) == 0)
                entry.flags |= FlagReadable;

            batch.append(std::move(entry));
            if (batch.size() >= kBatchSize) {
                callback(std::move(batch), {});
                batch = QVector<VfsEntry>();
            }
        }
    }

    if (!batch.isEmpty())
        callback(std::move(batch), {});

    callback({}, {});
}

VfsEntry TrashVfs::stat(const QUrl &url, VfsError *error)
{
    TrashService::Location location;
    if (!TrashService::resolve(url, &location)) {
        if (error)
            *error = VfsError::fromErrno(ENOENT, url);
        return {};
    }

    if (error)
        *error = {};
    if (location.root)
        return {};
    return m_local->stat(PathUtils::fromLocalBytes(location.realPath), error);
}

bool TrashVfs::exists(const QUrl &url)
{
    TrashService::Location location;
    if (!TrashService::resolve(url, &location))
        return false;
    if (location.root)
        return true;
    return m_local->exists(PathUtils::fromLocalBytes(location.realPath));
}
