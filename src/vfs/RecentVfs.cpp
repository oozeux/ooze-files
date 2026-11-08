#include "vfs/RecentVfs.h"

#include "core/CancelToken.h"
#include "core/PathUtils.h"
#include "core/RecentFiles.h"

#include <QDir>
#include <QFileInfo>

#include <sys/stat.h>
#include <unistd.h>

namespace {
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

QString locationText(const QByteArray &path)
{
    const qsizetype slash = path.lastIndexOf('/');
    return PathUtils::homeAbbreviated(slash <= 0 ? QByteArrayLiteral("/") : path.left(slash));
}

bool isRootUrl(const QUrl &url)
{
    return url.scheme() == QLatin1String("recent")
        && (url.path().isEmpty() || url.path() == QLatin1String("/"));
}
}

RecentVfs::RecentVfs(QString filePath, QObject *parent)
    : VfsBackend(parent)
    , m_filePath(std::move(filePath))
{
}

QString RecentVfs::scheme() const
{
    return QStringLiteral("recent");
}

QUrl RecentVfs::rootUrl()
{
    return QUrl(QStringLiteral("recent:///"));
}

VfsCapabilities RecentVfs::capabilities(const QUrl &) const
{
    VfsCapabilities caps;
    caps.canList = true;
    caps.canRead = true;
    caps.canWatch = true;
    return caps;
}

void RecentVfs::list(const QUrl &dir, const ListCallback &callback, CancelToken &token)
{
    if (!isRootUrl(dir)) {
        callback({}, VfsError::fromErrno(ENOENT, dir));
        return;
    }

    const QVector<RecentItem> items = RecentFiles::load(m_filePath);

    QVector<VfsEntry> batch;
    int examined = 0;

    for (const RecentItem &item : items) {
        if (token.isCancelled()) {
            VfsError error;
            error.code = VfsError::Cancelled;
            error.url = dir;
            callback({}, error);
            return;
        }

        if (++examined > kMaxCandidates || batch.size() >= kMaxEntries)
            break;

        struct stat st {};
        bool verified = true;
        if (::lstat(item.path.constData(), &st) != 0) {
            if (errno == ENOENT || errno == ENOTDIR)
                continue;
            verified = false;
        }

        const qsizetype slash = item.path.lastIndexOf('/');
        const QByteArray name = item.path.mid(slash + 1);

        VfsEntry entry;
        entry.id = item.path;
        entry.path = item.path;
        entry.url = PathUtils::fromLocalBytes(item.path);
        entry.displayName = PathUtils::displayName(name);
        entry.kind = verified ? kindFromMode(st.st_mode) : EntryKind::File;
        entry.mimeType = item.mimeType;
        entry.iconName = entry.kind == EntryKind::Directory ? QStringLiteral("folder")
                                                            : QStringLiteral("text-x-generic");
        entry.location = locationText(item.path);
        entry.activity = item.activity;

        if (verified) {
            entry.size = entry.kind == EntryKind::Directory ? -1 : qint64(st.st_size);
            entry.modified = QDateTime::fromSecsSinceEpoch(qint64(st.st_mtime));
            if (::access(item.path.constData(), R_OK) == 0)
                entry.flags |= FlagReadable;
        }

        const qsizetype dot = name.lastIndexOf('.');
        if (dot > 0 && dot < name.size() - 1)
            entry.suffix = QString::fromLatin1(name.mid(dot + 1).toLower());

        batch.append(std::move(entry));
    }

    if (!batch.isEmpty())
        callback(std::move(batch), {});

    callback({}, {});
}

VfsEntry RecentVfs::stat(const QUrl &url, VfsError *error)
{
    if (error)
        *error = isRootUrl(url) ? VfsError() : VfsError::fromErrno(ENOENT, url);
    return {};
}

bool RecentVfs::exists(const QUrl &url)
{
    return isRootUrl(url);
}
