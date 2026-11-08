#include "app/TabListModel.h"

TabListModel::TabListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TabListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(m_entries.size());
}

QVariant TabListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());

    switch (role) {
    case TitleRole:
        return entry.title;
    case UrlRole:
        return entry.url.toString();
    case ActiveRole:
        return entry.active;
    default:
        return {};
    }
}

QHash<int, QByteArray> TabListModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {UrlRole, "url"},
        {ActiveRole, "active"},
    };
}

void TabListModel::setEntries(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void TabListModel::setActive(int index)
{
    if (index < -1 || index >= m_entries.size())
        return;

    int previous = -1;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).active) {
            previous = row;
            break;
        }
    }

    if (previous == index)
        return;

    if (previous >= 0) {
        m_entries[previous].active = false;
        emit dataChanged(this->index(previous, 0), this->index(previous, 0),
                         {ActiveRole});
    }

    if (index >= 0) {
        m_entries[index].active = true;
        emit dataChanged(this->index(index, 0), this->index(index, 0),
                         {ActiveRole});
    }
}

void TabListModel::updateEntry(int index, const QString &title, const QUrl &url)
{
    if (index < 0 || index >= m_entries.size())
        return;

    Entry &entry = m_entries[index];
    if (entry.title == title && entry.url == url)
        return;

    entry.title = title;
    entry.url = url;
    emit dataChanged(this->index(index, 0), this->index(index, 0),
                     {TitleRole, UrlRole});
}
