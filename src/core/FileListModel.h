#pragma once

#include "vfs/VfsTypes.h"

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        IsDirRole,
        IsSymlinkRole,
        KindRole,
        SizeRole,
        SizeTextRole,
        ModifiedRole,
        ModifiedTextRole,
        MimeRole,
        IconRole,
        HiddenRole,
        SuffixRole,
        LocationRole,
        ActivityRole,
        ActivityTextRole,
    };

    explicit FileListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void beginLoad();
    void appendEntries(const QVector<VfsEntry> &entries);
    void syncEntries(const QVector<VfsEntry> &entries);
    void applyStats(const QVector<VfsStat> &stats);

    [[nodiscard]] const VfsEntry *entryAt(int row) const;

    Q_INVOKABLE QString urlAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;
    Q_INVOKABLE QString mimeAt(int row) const;

private:
    QVector<VfsEntry> m_entries;
};
