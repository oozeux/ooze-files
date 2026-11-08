#include "core/FileListProxyModel.h"

#include "core/FileListModel.h"

#include <QDateTime>

FileListProxyModel::FileListProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    setDynamicSortFilter(true);

    sort(0, Qt::AscendingOrder);
}

void FileListProxyModel::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    updateFilter();
}

void FileListProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    updateFilter();
}

void FileListProxyModel::setSortMode(SortMode mode)
{
    if (m_sortMode == mode)
        return;
    m_sortMode = mode;
    resort();
}

void FileListProxyModel::setDescending(bool descending)
{
    if (m_descending == descending)
        return;
    m_descending = descending;
    resort();
}

void FileListProxyModel::setFoldersFirst(bool foldersFirst)
{
    if (m_foldersFirst == foldersFirst)
        return;
    m_foldersFirst = foldersFirst;
    resort();
}

void FileListProxyModel::updateFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateRowsFilter();
#endif
}

int FileListProxyModel::visibleCount() const
{
    return rowCount();
}

void FileListProxyModel::resort()
{
    sort(0, Qt::AscendingOrder);
    invalidate();
}

bool FileListProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
        return false;

    if (!m_showHidden && idx.data(FileListModel::HiddenRole).toBool())
        return false;

    if (m_filterText.isEmpty())
        return true;

    const QString name = idx.data(FileListModel::NameRole).toString();
    return name.contains(m_filterText, Qt::CaseInsensitive);
}

void FileListProxyModel::setActivityOrder(bool enabled)
{
    if (m_activityOrder == enabled)
        return;
    m_activityOrder = enabled;
    resort();
}

bool FileListProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_activityOrder) {
        const QDateTime l = left.data(FileListModel::ActivityRole).toDateTime();
        const QDateTime r = right.data(FileListModel::ActivityRole).toDateTime();
        if (l.isValid() != r.isValid())
            return l.isValid();
        if (l != r)
            return l > r;
        return m_collator.compare(left.data(FileListModel::NameRole).toString(),
                                  right.data(FileListModel::NameRole).toString()) < 0;
    }

    const bool leftIsDir = left.data(FileListModel::IsDirRole).toBool();
    const bool rightIsDir = right.data(FileListModel::IsDirRole).toBool();

    if (m_foldersFirst && leftIsDir != rightIsDir)
        return leftIsDir;

    int comparison = 0;

    switch (m_sortMode) {
    case SortBySize: {
        const qint64 l = left.data(FileListModel::SizeRole).toLongLong();
        const qint64 r = right.data(FileListModel::SizeRole).toLongLong();
        comparison = (l < r) ? -1 : (l > r ? 1 : 0);
        break;
    }
    case SortByDate: {
        const QDateTime l = left.data(FileListModel::ModifiedRole).toDateTime();
        const QDateTime r = right.data(FileListModel::ModifiedRole).toDateTime();
        comparison = (l < r) ? -1 : (l > r ? 1 : 0);
        break;
    }
    case SortByType: {
        const QString l = left.data(FileListModel::MimeRole).toString();
        const QString r = right.data(FileListModel::MimeRole).toString();
        comparison = QString::compare(l, r, Qt::CaseInsensitive);
        break;
    }
    case SortByName:
    default: {
        const QString l = left.data(FileListModel::NameRole).toString();
        const QString r = right.data(FileListModel::NameRole).toString();
        comparison = m_collator.compare(l, r);
        break;
    }
    }

    if (comparison == 0) {
        comparison = m_collator.compare(left.data(FileListModel::NameRole).toString(),
                                        right.data(FileListModel::NameRole).toString());
    }

    return m_descending ? comparison > 0 : comparison < 0;
}
