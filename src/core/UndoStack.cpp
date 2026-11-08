#include "core/UndoStack.h"

void UndoStack::push(const JournalEntry &entry)
{
    if (entry.firsts.isEmpty())
        return;

    m_entries.resize(m_index);
    m_entries.append(entry);

    if (m_entries.size() > kLimit)
        m_entries.remove(0, m_entries.size() - kLimit);

    m_index = m_entries.size();
}

void UndoStack::clear()
{
    m_entries.clear();
    m_index = 0;
}

QString UndoStack::undoLabel() const
{
    return canUndo() ? m_entries.at(m_index - 1).label : QString();
}

QString UndoStack::redoLabel() const
{
    return canRedo() ? m_entries.at(m_index).label : QString();
}

JournalEntry UndoStack::nextUndo() const
{
    return canUndo() ? m_entries.at(m_index - 1) : JournalEntry();
}

JournalEntry UndoStack::nextRedo() const
{
    return canRedo() ? m_entries.at(m_index) : JournalEntry();
}

void UndoStack::commitUndo(const JournalEntry &updated)
{
    if (!canUndo())
        return;

    m_entries[m_index - 1] = updated;
    --m_index;
}

void UndoStack::commitRedo(const JournalEntry &updated)
{
    if (!canRedo())
        return;

    m_entries[m_index] = updated;
    ++m_index;
}
