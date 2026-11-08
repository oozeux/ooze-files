#pragma once

#include "vfs/VfsTypes.h"

#include <QObject>
#include <QVector>

class LoaderSignals : public QObject
{
    Q_OBJECT
public:
    explicit LoaderSignals(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

signals:
    void batch(quint64 generation, QVector<VfsEntry> entries);
    void stats(quint64 generation, QVector<VfsStat> stats);
    void statsDone(quint64 generation);
    void finished(quint64 generation, VfsError error);
    void refreshed(quint64 generation, QVector<VfsEntry> entries, VfsError error);
};
