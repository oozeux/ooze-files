#include "core/TrashService.h"

#include "core/PathUtils.h"

#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace {
constexpr qint64 kMaxInfoBytes = 64 * 1024;

QByteArray joinPath(const QByteArray &base, const QByteArray &name)
{
    return PathUtils::childBytes(base, name);
}

bool isDirectory(const QByteArray &path, bool followLinks)
{
    struct stat st {};
    const int result = followLinks ? ::stat(path.constData(), &st) : ::lstat(path.constData(), &st);
    return result == 0 && S_ISDIR(st.st_mode);
}

bool hasUnsafeSegment(const QByteArray &relative)
{
    if (relative.contains('\0'))
        return true;
    const QList<QByteArray> parts = relative.split('/');
    for (const QByteArray &part : parts) {
        if (part == "." || part == "..")
            return true;
    }
    return false;
}

QByteArray readSmallFile(const QByteArray &path)
{
    QFile file(QFile::decodeName(path));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.read(kMaxInfoBytes);
}

void applyInfo(TrashItem *item)
{
    const QByteArray text = readSmallFile(item->infoPath);
    if (text.isEmpty())
        return;

    const QList<QByteArray> lines = text.split('\n');
    bool inSection = false;
    QByteArray pathValue;
    QByteArray dateValue;

    for (QByteArray line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.startsWith('[')) {
            inSection = line == "[Trash Info]";
            continue;
        }
        if (!inSection)
            continue;
        if (line.startsWith("Path=") && pathValue.isEmpty())
            pathValue = line.mid(5);
        else if (line.startsWith("DeletionDate=") && dateValue.isEmpty())
            dateValue = line.mid(13);
    }

    if (!dateValue.isEmpty()) {
        item->deleted = QDateTime::fromString(QString::fromLatin1(dateValue.trimmed()), Qt::ISODate);
    }

    if (pathValue.isEmpty())
        return;

    QByteArray decoded = QByteArray::fromPercentEncoding(pathValue);
    if (decoded.isEmpty() || decoded.contains('\0'))
        return;

    if (decoded.startsWith('/')) {
        if (hasUnsafeSegment(decoded))
            return;
        item->originalPath = decoded;
    } else {
        if (item->dir.topdir.isEmpty() || hasUnsafeSegment(decoded))
            return;
        item->originalPath = joinPath(item->dir.topdir, decoded);
    }
    item->hasInfo = true;
}

TrashItem makeItem(const TrashDir &dir, const QByteArray &name)
{
    TrashItem item;
    item.dir = dir;
    item.name = name;
    item.path = joinPath(joinPath(dir.root, "files"), name);
    item.infoPath = joinPath(joinPath(dir.root, "info"), name + ".trashinfo");
    applyInfo(&item);
    return item;
}

bool removeTree(const QByteArray &path, QString *error)
{
    struct stat st {};
    if (::lstat(path.constData(), &st) != 0) {
        if (errno == ENOENT)
            return true;
        *error = QStringLiteral("%1: %2").arg(PathUtils::displayName(path), QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        if ((st.st_mode & S_IRWXU) != S_IRWXU)
            (void)::chmod(path.constData(), st.st_mode | S_IRWXU);

        DIR *dir = ::opendir(path.constData());
        if (!dir) {
            *error = QStringLiteral("%1: %2").arg(PathUtils::displayName(path), QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }

        QList<QByteArray> names;
        while (struct dirent *entry = ::readdir(dir)) {
            const QByteArray name(entry->d_name);
            if (name != "." && name != "..")
                names.append(name);
        }
        ::closedir(dir);

        for (const QByteArray &name : std::as_const(names)) {
            if (!removeTree(joinPath(path, name), error))
                return false;
        }

        if (::rmdir(path.constData()) != 0) {
            *error = QStringLiteral("%1: %2").arg(PathUtils::displayName(path), QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }
        return true;
    }

    if (::unlink(path.constData()) != 0) {
        *error = QStringLiteral("%1: %2").arg(PathUtils::displayName(path), QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

void dropDirectorySizesEntry(const TrashDir &dir, const QByteArray &name)
{
    const QByteArray path = joinPath(dir.root, "directorysizes");
    QFile file(QFile::decodeName(path));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray content = file.read(kMaxInfoBytes * 64);
    file.close();

    QList<QByteArray> kept;
    bool changed = false;
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &line : lines) {
        if (line.isEmpty())
            continue;
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() >= 3 && QByteArray::fromPercentEncoding(fields.mid(2).join(' ')) == name) {
            changed = true;
            continue;
        }
        kept.append(line);
    }

    if (!changed)
        return;

    const QByteArray temporary = path + ".ooze-tmp";
    QFile out(QFile::decodeName(temporary));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    for (const QByteArray &line : std::as_const(kept)) {
        out.write(line);
        out.write("\n");
    }
    out.close();
    if (::rename(temporary.constData(), path.constData()) != 0)
        ::unlink(temporary.constData());
}
}

QByteArray TrashItem::displayBytes() const
{
    if (hasInfo && !originalPath.isEmpty()) {
        const qsizetype slash = originalPath.lastIndexOf('/');
        const QByteArray base = originalPath.mid(slash + 1);
        if (!base.isEmpty())
            return base;
    }
    return name;
}

namespace TrashService {
QByteArray homeTrashRoot()
{
    return QFile::encodeName(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        + "/Trash";
}

QVector<TrashDir> discoverDirs()
{
    QVector<TrashDir> dirs;
    QSet<QByteArray> seen;

    const auto add = [&](const QByteArray &root, const QByteArray &topdir, bool followLinks) {
        if (seen.contains(root) || !isDirectory(root, followLinks))
            return;
        seen.insert(root);
        dirs.append(TrashDir{root, topdir});
    };

    add(homeTrashRoot(), {}, true);

    if (qEnvironmentVariableIsSet("OOZE_FILES_TRASH_HOME_ONLY"))
        return dirs;

    const QByteArray uid = QByteArray::number(qulonglong(::getuid()));
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly())
            continue;
        if (!volume.device().startsWith("/dev/"))
            continue;

        const QByteArray top = QFile::encodeName(volume.rootPath());

        const QByteArray shared = joinPath(top, ".Trash");
        struct stat st {};
        if (::lstat(shared.constData(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_ISVTX))
            add(joinPath(shared, uid), top, false);

        add(joinPath(top, ".Trash-" + uid), top, false);
    }

    return dirs;
}

QVector<TrashItem> items(const TrashDir &dir)
{
    QVector<TrashItem> out;
    DIR *d = ::opendir(joinPath(dir.root, "files").constData());
    if (!d)
        return out;

    while (struct dirent *entry = ::readdir(d)) {
        const QByteArray name(entry->d_name);
        if (name == "." || name == "..")
            continue;
        out.append(makeItem(dir, name));
    }
    ::closedir(d);
    return out;
}

QVector<TrashItem> allItems()
{
    QVector<TrashItem> out;
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs)
        out += items(dir);
    return out;
}

bool hasItems()
{
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs) {
        DIR *d = ::opendir(joinPath(dir.root, "files").constData());
        if (!d)
            continue;
        bool found = false;
        while (struct dirent *entry = ::readdir(d)) {
            const QByteArray name(entry->d_name);
            if (name != "." && name != "..") {
                found = true;
                break;
            }
        }
        ::closedir(d);
        if (found)
            return true;
    }
    return false;
}

bool locate(const QByteArray &path, TrashItem *item)
{
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs) {
        const QByteArray prefix = joinPath(dir.root, "files") + "/";
        if (!path.startsWith(prefix))
            continue;

        const QByteArray name = path.mid(prefix.size());
        if (name.isEmpty() || name.contains('/') || name == "." || name == "..")
            return false;

        struct stat st {};
        if (::lstat(path.constData(), &st) != 0)
            return false;

        *item = makeItem(dir, name);
        return true;
    }
    return false;
}

QUrl rootUrl()
{
    return QUrl(QStringLiteral("trash:///"));
}

QUrl virtualUrl(const TrashDir &dir, const QByteArray &relative)
{
    QByteArray encoded = "trash:///" + dir.root.toHex();
    if (!relative.isEmpty())
        encoded += "/" + relative.toPercentEncoding("/");
    return QUrl::fromEncoded(encoded);
}

QUrl virtualUrlForPath(const QByteArray &path)
{
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs) {
        const QByteArray prefix = joinPath(dir.root, "files") + "/";
        if (path.startsWith(prefix) && path.size() > prefix.size())
            return virtualUrl(dir, path.mid(prefix.size()));
    }
    return {};
}

bool resolve(const QUrl &url, Location *location)
{
    if (url.scheme() != QLatin1String("trash"))
        return false;

    const QByteArray encoded = url.path(QUrl::FullyEncoded).toLatin1();
    QList<QByteArray> segments;
    const QList<QByteArray> raw = encoded.split('/');
    for (const QByteArray &segment : raw) {
        if (!segment.isEmpty())
            segments.append(segment);
    }

    *location = Location();
    if (segments.isEmpty()) {
        location->root = true;
        return true;
    }

    const QByteArray root = QByteArray::fromHex(segments.first());
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs) {
        if (dir.root == root) {
            location->dir = dir;
            break;
        }
    }
    if (!location->dir.isValid())
        return false;

    QByteArray relative;
    for (qsizetype i = 1; i < segments.size(); ++i) {
        const QByteArray part = QByteArray::fromPercentEncoding(segments.at(i));
        if (part.isEmpty() || part == "." || part == ".." || part.contains('/') || part.contains('\0'))
            return false;
        if (!relative.isEmpty())
            relative += '/';
        relative += part;
    }

    location->relative = relative;
    location->realPath = relative.isEmpty() ? joinPath(location->dir.root, "files")
                                            : joinPath(joinPath(location->dir.root, "files"), relative);
    return true;
}

QVector<QByteArray> watchPaths()
{
    QVector<QByteArray> paths;
    const QVector<TrashDir> dirs = discoverDirs();
    for (const TrashDir &dir : dirs) {
        const QByteArray files = joinPath(dir.root, "files");
        const QByteArray info = joinPath(dir.root, "info");
        if (isDirectory(files, true))
            paths.append(files);
        if (isDirectory(info, true))
            paths.append(info);
    }
    return paths;
}

bool forget(const TrashItem &item)
{
    bool ok = true;
    if (!item.infoPath.isEmpty() && ::unlink(item.infoPath.constData()) != 0 && errno != ENOENT)
        ok = false;
    dropDirectorySizesEntry(item.dir, item.name);
    return ok;
}

bool removeItem(const TrashItem &item, QString *error)
{
    if (!removeTree(item.path, error))
        return false;
    forget(item);
    return true;
}

int purgeOrphans(const TrashDir &dir)
{
    const QByteArray infoDir = joinPath(dir.root, "info");
    DIR *d = ::opendir(infoDir.constData());
    if (!d)
        return 0;

    QList<QByteArray> names;
    while (struct dirent *entry = ::readdir(d)) {
        const QByteArray name(entry->d_name);
        if (name.endsWith(".trashinfo"))
            names.append(name);
    }
    ::closedir(d);

    int removed = 0;
    for (const QByteArray &name : std::as_const(names)) {
        const QByteArray base = name.chopped(10);
        const QByteArray target = joinPath(joinPath(dir.root, "files"), base);
        struct stat st {};
        if (::lstat(target.constData(), &st) != 0 && errno == ENOENT) {
            if (::unlink(joinPath(infoDir, name).constData()) == 0)
                ++removed;
        }
    }
    return removed;
}
}
