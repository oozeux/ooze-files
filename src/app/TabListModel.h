#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVector>

// Model used by the tab bar. It stores plain values so deleting a
// TabController never leaves a dangling pointer in QML.
class TabListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        UrlRole,
        ActiveRole,
    };

    struct Entry {
        QString title;
        QUrl url;
        bool active = false;
    };

    explicit TabListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QVector<Entry> &entries);
    void setActive(int index);
    void updateEntry(int index, const QString &title, const QUrl &url);

private:
    QVector<Entry> m_entries;
};
