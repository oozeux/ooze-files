#pragma once

#include "core/UndoStack.h"

#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <QVector>
#include <QWaitCondition>

class FileOperations : public QObject
{
    Q_OBJECT
public:
    enum Action {
        Copy = 0,
        Move,
        Link,
        Trash,
        Remove,
        Rename,
        CreateDirectory,
        CreateFile,
        Undo,
        Redo,
        Compress,
        Extract,
        Restore,
        EmptyTrash,
    };
    Q_ENUM(Action)

    enum ConflictDecision {
        ConflictRename = 0,
        ConflictOverwrite,
        ConflictMerge,
        ConflictCancel,
        ConflictRecreate,
        ConflictRestoreHome,
        ConflictMergeReplace,
    };
    Q_ENUM(ConflictDecision)

    explicit FileOperations(QObject *parent = nullptr);
    ~FileOperations() override;

    void copy(const QVector<QUrl> &sources, const QUrl &destination);
    void move(const QVector<QUrl> &sources, const QUrl &destination);
    void link(const QVector<QUrl> &sources, const QUrl &destination);
    void trash(const QVector<QUrl> &sources);
    void remove(const QVector<QUrl> &sources);
    void rename(const QUrl &source, const QString &newName);
    void renameBatch(const QVector<QUrl> &sources, const QStringList &newNames);
    void createDirectory(const QUrl &directory, const QString &name);
    void createFile(const QUrl &directory, const QString &name);
    void compress(const QVector<QUrl> &sources, const QString &formatId,
                  const QString &baseName = QString());
    void extract(const QUrl &archive);
    void restoreTrashItems(const QVector<QUrl> &items);
    void deleteTrashItems(const QVector<QUrl> &items);
    void emptyTrash();
    void applyJournal(const JournalEntry &entry, bool undo);
    [[nodiscard]] bool busy() const;

    void resolveConflict(int id, int decision, bool applyToAll);
    int awaitConflictDecision(const QVariantMap &info, bool *applyToAll);
    [[nodiscard]] bool hasConflictHandler() const;

    [[nodiscard]] static QString suggestedName(const QUrl &directory, const QString &base,
                                               bool directoryEntry);
    [[nodiscard]] static QString suggestedArchiveBase(const QVector<QUrl> &sources);
    [[nodiscard]] static QString archiveTargetName(const QVector<QUrl> &sources,
                                                   const QString &baseName,
                                                   const QString &formatId,
                                                   QString *error = nullptr);

signals:
    void operationStarted(const QString &description);
    void operationProgress(const QString &description);
    void operationFinished(int action, bool success, const QString &message,
                           const QString &createdName, bool createdIsDirectory);
    void errorOccurred(const QString &message);
    void conflictRequested(const QVariantMap &info);
    void changeRecorded(int kind, const QString &label, const QStringList &firsts,
                        const QStringList &seconds);
    void journalApplied(bool undo, bool success, const QString &message,
                        const QStringList &firsts, const QStringList &seconds);

private:
    QThreadPool m_pool;
    QMutex m_conflictMutex;
    QWaitCondition m_conflictWait;
    int m_conflictSequence = 0;
    int m_conflictPending = 0;
    int m_conflictDecision = ConflictCancel;
    bool m_conflictApplyAll = false;
    bool m_shutdown = false;
};
