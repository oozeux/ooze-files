#pragma once

#include <QAbstractListModel>
#include <QThread>
#include <QVector>

#include <atomic>

class SearchWorker;

// Recursive, cancellable search.  Results live in their own model so the
// directory view (and its proxy/filter) stays untouched.
class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY truncatedChanged)
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY rootPathChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        RelativePathRole,
        IsDirRole,
        IconRole,
        SizeTextRole,
        MimeRole,
    };

    explicit SearchModel(QObject *parent = nullptr);
    ~SearchModel() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_entries.size()); }
    [[nodiscard]] bool searching() const { return m_searching; }
    [[nodiscard]] bool truncated() const { return m_truncated; }
    [[nodiscard]] QString rootPath() const { return m_rootPath; }

    Q_INVOKABLE void start(const QString &rootUrl, const QString &query,
                           const QString &type, bool includeHidden);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clear();

    Q_INVOKABLE QString urlAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;

signals:
    void countChanged();
    void searchingChanged();
    void truncatedChanged();
    void rootPathChanged();

private slots:
    void appendBatch(quint64 token, const QVariantList &entries, bool truncated);
    void finishSearch(quint64 token, bool truncated);

private:
    struct Entry
    {
        QString name;
        QString url;
        QString relativePath;
        QString iconName;
        QString sizeText;
        QString mimeType;
        bool isDir = false;
    };

    void setSearching(bool searching);

    QThread m_thread;
    SearchWorker *m_worker = nullptr;
    QVector<Entry> m_entries;
    QString m_rootPath;
    quint64 m_nextToken = 0;
    quint64 m_activeToken = 0;
    bool m_searching = false;
    bool m_truncated = false;
};
