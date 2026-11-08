#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct JournalEntry
{
    enum Kind {
        Moved = 0,
        Trashed,
        Created,
    };

    Kind kind = Moved;
    QString label;
    QStringList firsts;
    QStringList seconds;
};

class UndoStack
{
public:
    static constexpr int kLimit = 50;

    void push(const JournalEntry &entry);
    void clear();

    [[nodiscard]] bool canUndo() const { return m_index > 0; }
    [[nodiscard]] bool canRedo() const { return m_index < m_entries.size(); }
    [[nodiscard]] QString undoLabel() const;
    [[nodiscard]] QString redoLabel() const;
    [[nodiscard]] JournalEntry nextUndo() const;
    [[nodiscard]] JournalEntry nextRedo() const;

    void commitUndo(const JournalEntry &updated);
    void commitRedo(const JournalEntry &updated);

private:
    QVector<JournalEntry> m_entries;
    int m_index = 0;
};
