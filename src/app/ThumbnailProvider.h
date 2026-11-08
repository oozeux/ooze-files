#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    explicit ThumbnailProvider(int maxThreads = 2);
    ~ThumbnailProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;

private:
    QThreadPool m_pool;
};
