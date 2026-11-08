#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

class FileListProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum SortMode {
        SortByName = 0,
        SortBySize,
        SortByDate,
        SortByType,
    };
    Q_ENUM(SortMode)

    explicit FileListProxyModel(QObject *parent = nullptr);

    [[nodiscard]] bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);

    [[nodiscard]] QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    [[nodiscard]] SortMode sortMode() const { return m_sortMode; }
    void setSortMode(SortMode mode);

    [[nodiscard]] bool descending() const { return m_descending; }
    void setDescending(bool descending);

    [[nodiscard]] bool foldersFirst() const { return m_foldersFirst; }
    void setFoldersFirst(bool foldersFirst);

    [[nodiscard]] bool activityOrder() const { return m_activityOrder; }
    void setActivityOrder(bool enabled);

    [[nodiscard]] int visibleCount() const;

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow,
                                        const QModelIndex &sourceParent) const override;
    [[nodiscard]] bool lessThan(const QModelIndex &left,
                                const QModelIndex &right) const override;

private:
    void resort();
    void updateFilter();

    bool m_showHidden = false;
    QString m_filterText;
    SortMode m_sortMode = SortByName;
    bool m_descending = false;
    bool m_foldersFirst = true;
    bool m_activityOrder = false;
    mutable QCollator m_collator;
};
