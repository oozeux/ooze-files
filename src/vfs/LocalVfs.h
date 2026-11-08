#pragma once

#include "vfs/VfsBackend.h"

class LocalVfs : public VfsBackend
{
    Q_OBJECT
public:
    explicit LocalVfs(QObject *parent = nullptr);

    [[nodiscard]] QString scheme() const override;
    [[nodiscard]] VfsCapabilities capabilities(const QUrl &url) const override;

    void list(const QUrl &dir, const ListCallback &callback, CancelToken &token) override;
    VfsEntry stat(const QUrl &url, VfsError *error) override;
    bool exists(const QUrl &url) override;
};
