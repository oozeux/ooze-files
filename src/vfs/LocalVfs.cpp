#include "vfs/LocalVfs.h"

#include "core/CancelToken.h"
#include "core/FileIcons.h"
#include "core/PathUtils.h"

#include <QDir>
#include <QHash>
#include <QMimeDatabase>
#include <QMimeType>

#include <dirent.h>
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
    if (S_ISSOCK(mode))
        return EntryKind::Socket;
    if (S_ISFIFO(mode))
        return EntryKind::Fifo;
    if (S_ISBLK(mode))
        return EntryKind::BlockDevice;
    if (S_ISCHR(mode))
        return EntryKind::CharDevice;
    return EntryKind::Unknown;
}

EntryKind kindFromDType(unsigned char dtype)
{
    switch (dtype) {
    case DT_DIR:
        return EntryKind::Directory;
    case DT_REG:
        return EntryKind::File;
    case DT_LNK:
        return EntryKind::Symlink;
    case DT_SOCK:
        return EntryKind::Socket;
    case DT_FIFO:
        return EntryKind::Fifo;
    case DT_BLK:
        return EntryKind::BlockDevice;
    case DT_CHR:
        return EntryKind::CharDevice;
    default:
        return EntryKind::Unknown;
    }
}

QByteArray suffixOf(const QByteArray &name)
{
    const qsizetype dot = name.lastIndexOf('.');
    if (dot <= 0 || dot == name.size() - 1)
        return {};
    return name.mid(dot + 1).toLower();
}

QString quickIcon(const EntryKind kind, const QByteArray &name, bool linkToDirectory)
{
    switch (kind) {
    case EntryKind::Directory:
        return QStringLiteral("folder");
    case EntryKind::Symlink:
        if (linkToDirectory)
            return QStringLiteral("folder-symlink");
        break;
    case EntryKind::Socket:
    case EntryKind::Fifo:
    case EntryKind::BlockDevice:
    case EntryKind::CharDevice:
        return QStringLiteral("application-x-special-desktop");
    default:
        break;
    }

    static const QHash<QByteArray, QString> map = {
        {"png", "image-x-generic"},       {"jpg", "image-x-generic"},
        {"jpeg", "image-x-generic"},      {"webp", "image-x-generic"},
        {"gif", "image-x-generic"},       {"bmp", "image-x-generic"},
        {"svg", "image-x-generic"},       {"avif", "image-x-generic"},
        {"heic", "image-x-generic"},      {"tiff", "image-x-generic"},
        {"mp4", "video-x-generic"},       {"mkv", "video-x-generic"},
        {"webm", "video-x-generic"},      {"avi", "video-x-generic"},
        {"mov", "video-x-generic"},       {"flv", "video-x-generic"},
        {"mp3", "audio-x-generic"},       {"flac", "audio-x-generic"},
        {"ogg", "audio-x-generic"},       {"opus", "audio-x-generic"},
        {"wav", "audio-x-generic"},       {"m4a", "audio-x-generic"},
        {"pdf", "application-pdf"},       {"zip", "package-x-generic"},
        {"tar", "package-x-generic"},     {"gz", "package-x-generic"},
        {"xz", "package-x-generic"},      {"zst", "package-x-generic"},
        {"7z", "package-x-generic"},      {"rar", "package-x-generic"},
        {"iso", "application-x-cd-image"}, {"txt", "text-x-generic"},
        {"md", "text-x-generic"},         {"log", "text-x-generic"},
        {"json", "text-x-generic"},       {"yaml", "text-x-generic"},
        {"yml", "text-x-generic"},        {"toml", "text-x-generic"},
        {"conf", "text-x-generic"},       {"ini", "text-x-generic"},
        {"sh", "text-x-script"},          {"py", "text-x-script"},
        {"rs", "text-x-script"},          {"js", "text-x-script"},
        {"ts", "text-x-script"},          {"c", "text-x-script"},
        {"cpp", "text-x-script"},         {"h", "text-x-script"},
        {"hpp", "text-x-script"},         {"qml", "text-x-script"},
        {"nix", "text-x-script"},         {"desktop", "application-x-desktop"},
    };

    const auto it = map.constFind(suffixOf(name));
    if (it != map.constEnd())
        return it.value();
    if (name.startsWith("README") || name.startsWith("LICENSE"))
        return QStringLiteral("text-x-generic");
    return QStringLiteral("text-x-generic");
}

QByteArray fullPath(const QUrl &dir, const QByteArray &name)
{
    return PathUtils::childBytes(PathUtils::toLocalBytes(dir), name);
}
}

LocalVfs::LocalVfs(QObject *parent)
    : VfsBackend(parent)
{
}

QString LocalVfs::scheme() const
{
    return QStringLiteral("file");
}

VfsCapabilities LocalVfs::capabilities(const QUrl &) const
{
    VfsCapabilities caps;
    caps.canList = true;
    caps.canRead = true;
    caps.canWrite = true;
    caps.canDelete = true;
    caps.canRename = true;
    caps.canCreateDirectory = true;
    caps.canWatch = true;
    caps.canSearch = true;
    caps.supportsTrash = true;
    return caps;
}

void LocalVfs::list(const QUrl &dir, const ListCallback &callback, CancelToken &token)
{
    if (!dir.isLocalFile()) {
        callback({}, VfsError::fromErrno(ENOTDIR, dir));
        return;
    }

    const QUrl normalized = PathUtils::normalized(dir);
    const QByteArray path = PathUtils::toLocalBytes(normalized);

    DIR *d = ::opendir(path.constData());
    if (!d) {
        vfsEmitError(callback, dir, errno);
        return;
    }

    QVector<VfsEntry> batch;
    batch.reserve(kBatchSize);

    qint64 counter = 0;
    bool cancelled = false;

    while (struct dirent *ent = ::readdir(d)) {
        if ((counter++ % 64) == 0 && token.isCancelled()) {
            cancelled = true;
            break;
        }

        const QByteArray name(ent->d_name);
        if (name == "." || name == "..")
            continue;

        VfsEntry entry;
        entry.id = name;
        entry.path = fullPath(normalized, name);
        entry.url = PathUtils::fromLocalBytes(entry.path);
        entry.displayName = PathUtils::displayName(name);
        entry.kind = kindFromDType(ent->d_type);

        if (entry.kind == EntryKind::Unknown) {
            struct stat st {};
            const QByteArray full = fullPath(normalized, name);
            if (::lstat(full.constData(), &st) == 0)
                entry.kind = kindFromMode(st.st_mode);
            else
                entry.kind = EntryKind::File;
        }

        if (name.startsWith('.'))
            entry.flags |= FlagHidden;

        if (entry.kind == EntryKind::Symlink) {
            struct stat target {};
            if (::stat(entry.path.constData(), &target) == 0) {
                if (S_ISDIR(target.st_mode))
                    entry.flags |= FlagSymlinkDir;
            } else {
                entry.flags |= FlagSymlinkBroken;
            }
        }

        entry.iconName = quickIcon(entry.kind, name, hasFlag(entry, FlagSymlinkDir));
        if (entry.kind == EntryKind::File)
            entry.iconName = FileIcons::iconForFile(QString::fromUtf8(entry.path),
                                                    entry.iconName);
        batch.append(std::move(entry));

        if (batch.size() >= kBatchSize) {
            callback(std::move(batch), {});
            batch = QVector<VfsEntry>();
            batch.reserve(kBatchSize);
        }
    }

    ::closedir(d);

    if (!batch.isEmpty())
        callback(std::move(batch), {});

    if (cancelled) {
        VfsError error;
        error.code = VfsError::Cancelled;
        error.url = dir;
        callback({}, error);
        return;
    }

    callback({}, {});
}

VfsEntry LocalVfs::stat(const QUrl &url, VfsError *error)
{
    if (error)
        *error = {};

    if (!url.isLocalFile()) {
        if (error)
            *error = VfsError::fromErrno(ENOTDIR, url);
        return {};
    }

    const QUrl normalized = PathUtils::normalized(url);
    const QByteArray path = PathUtils::toLocalBytes(normalized);

    struct stat st {};
    if (::lstat(path.constData(), &st) != 0) {
        if (error)
            *error = VfsError::fromErrno(errno, url);
        return {};
    }

    VfsEntry entry;
    entry.path = path;
    entry.url = PathUtils::fromLocalBytes(path);
    entry.id = path.mid(path.lastIndexOf('/') + 1);
    entry.displayName = PathUtils::displayName(entry.id);
    entry.kind = kindFromMode(st.st_mode);
    entry.modified = QDateTime::fromSecsSinceEpoch(st.st_mtime);
    entry.size = (entry.kind == EntryKind::Directory) ? -1 : qint64(st.st_size);

    if (entry.id.startsWith('.'))
        entry.flags |= FlagHidden;
    if (::access(path.constData(), R_OK) == 0)
        entry.flags |= FlagReadable;
    if (::access(path.constData(), W_OK) == 0)
        entry.flags |= FlagWritable;
    if (::access(path.constData(), X_OK) == 0)
        entry.flags |= FlagExecutable;

    if (entry.kind == EntryKind::Symlink) {
        struct stat target {};
        if (::stat(path.constData(), &target) == 0) {
            if (S_ISDIR(target.st_mode)) {
                entry.flags |= FlagSymlinkDir;
                entry.size = -1;
            }
        } else {
            entry.flags |= FlagSymlinkBroken;
        }
    }

    if (entry.kind == EntryKind::Directory) {
        entry.mimeType = QStringLiteral("inode/directory");
        entry.iconName = QStringLiteral("folder");
    } else if (hasFlag(entry, FlagSymlinkDir)) {
        entry.mimeType = QStringLiteral("inode/directory");
        entry.iconName = QStringLiteral("folder-symlink");
    } else {
        const QMimeDatabase mimeDb;
        const QMimeType mime = mimeDb.mimeTypeForFile(
            QString::fromUtf8(path), QMimeDatabase::MatchExtension);
        entry.mimeType = mime.name();
        entry.iconName = mime.iconName();
        if (entry.iconName.isEmpty())
            entry.iconName = QStringLiteral("text-x-generic");
        if (entry.kind == EntryKind::File)
            entry.iconName = FileIcons::iconForFile(QString::fromUtf8(path),
                                                    entry.iconName);
    }

    return entry;
}

bool LocalVfs::exists(const QUrl &url)
{
    if (!url.isLocalFile())
        return false;
    const QByteArray path = PathUtils::toLocalBytes(PathUtils::normalized(url));
    struct stat st {};
    return ::lstat(path.constData(), &st) == 0;
}
