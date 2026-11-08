#include "core/FileListModel.h"

#include "core/PathUtils.h"

#include <QHash>
#include <QMimeDatabase>
#include <QSet>

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(m_entries.size());
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const VfsEntry &entry = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.displayName;
    case UrlRole:
        return entry.url.toString();
    case IsDirRole:
        return entry.kind == EntryKind::Directory || hasFlag(entry, FlagSymlinkDir);
    case IsSymlinkRole:
        return entry.kind == EntryKind::Symlink;
    case KindRole:
        return int(entry.kind);
    case SizeRole:
        return entry.size;
    case SizeTextRole:
        return entry.kind == EntryKind::Directory ? QString()
                                                  : PathUtils::formatSize(entry.size);
    case ModifiedRole:
        return entry.modified;
    case ModifiedTextRole:
        return PathUtils::formatDate(entry.modified);
    case MimeRole:
        return entry.mimeType;
    case IconRole:
        return entry.iconName.isEmpty() ? QStringLiteral("text-x-generic")
                                        : entry.iconName;
    case HiddenRole:
        return hasFlag(entry, FlagHidden);
    case SuffixRole:
        return entry.suffix;
    case LocationRole:
        return entry.location;
    case ActivityRole:
        return entry.activity;
    case ActivityTextRole:
        return entry.activity.isValid() ? PathUtils::formatDate(entry.activity) : QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {UrlRole, "url"},
        {IsDirRole, "isDir"},
        {IsSymlinkRole, "isSymlink"},
        {KindRole, "kind"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {ModifiedRole, "modified"},
        {ModifiedTextRole, "modifiedText"},
        {MimeRole, "mimeType"},
        {IconRole, "iconName"},
        {HiddenRole, "hidden"},
        {SuffixRole, "suffix"},
        {LocationRole, "locationText"},
        {ActivityRole, "activity"},
        {ActivityTextRole, "activityText"},
    };
}

void FileListModel::beginLoad()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void FileListModel::appendEntries(const QVector<VfsEntry> &entries)
{
    if (entries.isEmpty())
        return;

    const int first = int(m_entries.size());
    const int last = first + int(entries.size()) - 1;

    beginInsertRows({}, first, last);
    m_entries.reserve(m_entries.size() + entries.size());
    for (const VfsEntry &entry : entries)
        m_entries.append(entry);
    endInsertRows();
}

void FileListModel::syncEntries(const QVector<VfsEntry> &entries)
{
    QSet<QByteArray> incoming;
    incoming.reserve(entries.size());
    for (const VfsEntry &entry : entries)
        incoming.insert(entry.path);

    int end = int(m_entries.size()) - 1;
    while (end >= 0) {
        if (incoming.contains(m_entries.at(end).path)) {
            --end;
            continue;
        }

        int start = end;
        while (start > 0 && !incoming.contains(m_entries.at(start - 1).path))
            --start;

        beginRemoveRows({}, start, end);
        m_entries.remove(start, end - start + 1);
        endRemoveRows();
        end = start - 1;
    }

    QHash<QByteArray, int> indexByPath;
    indexByPath.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row)
        indexByPath.insert(m_entries.at(row).path, row);

    const QVector<int> changedRoles = {NameRole, UrlRole, IsDirRole, IsSymlinkRole,
                                       KindRole, IconRole, HiddenRole, LocationRole,
                                       ActivityRole, ActivityTextRole};

    QVector<VfsEntry> added;
    added.reserve(entries.size());

    for (const VfsEntry &entry : entries) {
        const auto it = indexByPath.constFind(entry.path);
        if (it == indexByPath.constEnd()) {
            added.append(entry);
            continue;
        }

        VfsEntry &current = m_entries[it.value()];
        const quint32 flags = (current.flags & ~quint32(FlagHidden))
                            | (entry.flags & quint32(FlagHidden));

        if (current.displayName == entry.displayName
            && current.kind == entry.kind
            && current.iconName == entry.iconName
            && current.location == entry.location
            && current.activity == entry.activity
            && current.flags == flags) {
            continue;
        }

        current.location = entry.location;
        current.activity = entry.activity;
        current.displayName = entry.displayName;
        current.kind = entry.kind;
        current.iconName = entry.iconName;
        current.flags = flags;
        emit dataChanged(index(it.value()), index(it.value()), changedRoles);
    }

    if (!added.isEmpty()) {
        const int first = int(m_entries.size());
        const int last = first + int(added.size()) - 1;
        beginInsertRows({}, first, last);
        m_entries += added;
        endInsertRows();
    }
}

void FileListModel::applyStats(const QVector<VfsStat> &stats)
{
    if (stats.isEmpty())
        return;

    QVector<int> roles = {SizeRole, SizeTextRole, ModifiedRole, ModifiedTextRole,
                          MimeRole, IconRole, KindRole, IsDirRole, IsSymlinkRole};

    int blockStart = -1;
    int blockEnd = -1;

    const auto flush = [&] {
        if (blockStart >= 0) {
            emit dataChanged(index(blockStart), index(blockEnd), roles);
            blockStart = blockEnd = -1;
        }
    };

    for (const VfsStat &stat : stats) {
        if (stat.index < 0 || stat.index >= m_entries.size())
            continue;

        VfsEntry &entry = m_entries[stat.index];
        entry.size = stat.size;
        if (stat.mtimeSecs > 0)
            entry.modified = QDateTime::fromSecsSinceEpoch(stat.mtimeSecs);
        if (!stat.mimeType.isEmpty())
            entry.mimeType = stat.mimeType;
        if (!stat.iconName.isEmpty())
            entry.iconName = stat.iconName;
        entry.flags = stat.flags;

        if (blockStart < 0) {
            blockStart = stat.index;
            blockEnd = stat.index;
        } else if (stat.index == blockEnd + 1) {
            blockEnd = stat.index;
        } else {
            flush();
            blockStart = blockEnd = stat.index;
        }
    }

    flush();
}

const VfsEntry *FileListModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries.at(row);
}

QString FileListModel::urlAt(int row) const
{
    const VfsEntry *entry = entryAt(row);
    return entry ? entry->url.toString() : QString();
}

QString FileListModel::nameAt(int row) const
{
    const VfsEntry *entry = entryAt(row);
    return entry ? entry->displayName : QString();
}

bool FileListModel::isDirAt(int row) const
{
    const VfsEntry *entry = entryAt(row);
    return entry && entry->kind == EntryKind::Directory;
}

QString FileListModel::mimeAt(int row) const
{
    const VfsEntry *entry = entryAt(row);
    return entry ? entry->mimeType : QString();
}
