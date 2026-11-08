#pragma once

#include "vfs/VfsTypes.h"

#include <QObject>
#include <QString>

#include <functional>

class CancelToken;

class VfsBackend : public QObject
{
    Q_OBJECT
public:
    using ListCallback = std::function<void(QVector<VfsEntry> &&entries, const VfsError &error)>;

    explicit VfsBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~VfsBackend() override = default;

    [[nodiscard]] virtual QString scheme() const = 0;
    [[nodiscard]] virtual VfsCapabilities capabilities(const QUrl &url) const = 0;

    virtual void list(const QUrl &dir, const ListCallback &callback, CancelToken &token) = 0;

    virtual VfsEntry stat(const QUrl &url, VfsError *error) = 0;

    virtual bool exists(const QUrl &url) = 0;

    [[nodiscard]] virtual bool canHandle(const QUrl &url) const
    {
        return url.scheme() == scheme();
    }
};

inline void vfsEmitError(const VfsBackend::ListCallback &callback, const QUrl &url, int err)
{
    callback({}, VfsError::fromErrno(err, url));
}
