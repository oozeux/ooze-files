#pragma once

#include <QHash>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>

class IconProvider : public QQuickImageProvider
{
public:
    explicit IconProvider(int maxCacheEntries = 512);

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    QPixmap pixmap(const QString &name, int pixels, bool linkEmblem);

    void clearCache();

private:
    QPixmap fallbackPixmap(int pixels) const;
    QPixmap linkEmblemPixmap(int pixels) const;
    QPixmap fallbackLinkEmblem(int pixels) const;
    QPixmap withLinkEmblem(const QPixmap &base) const;

    QHash<QString, QPixmap> m_cache;
    int m_maxCacheEntries = 512;
};
