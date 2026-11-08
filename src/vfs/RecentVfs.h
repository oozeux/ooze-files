#pragma once

#include "vfs/VfsBackend.h"

class RecentVfs : public VfsBackend
{
    Q_OBJECT
public:
    static constexpr int kMaxEntries = 200;
    static constexpr int kMaxCandidates = 1000;

    explicit RecentVfs(QString filePath, QObject *parent = nullptr);

    [[nodiscard]] QString scheme() const override;
    [[nodiscard]] VfsCapabilities capabilities(const QUrl &url) const override;

    void list(const QUrl &dir, const ListCallback &callback, CancelToken &token) override;
    VfsEntry stat(const QUrl &url, VfsError *error) override;
    bool exists(const QUrl &url) override;

    static QUrl rootUrl();

private:
    QString m_filePath;
};
