#include "app/IconProvider.h"

#include "core/FileIcons.h"

#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QStringList>

namespace {

const QHash<QString, QStringList> &aliases()
{
    static const QHash<QString, QStringList> map = {
        {QStringLiteral("view-list"),
         {QStringLiteral("view-list"), QStringLiteral("view-list-details"),
          QStringLiteral("view-list-text"), QStringLiteral("view-list-symbolic")}},
        {QStringLiteral("object-select"),
         {QStringLiteral("object-select"), QStringLiteral("checkmark"),
          QStringLiteral("check-symbolic"), QStringLiteral("dialog-ok")}},
        {QStringLiteral("folder-symlink"),
         {QStringLiteral("folder-symlink"), QStringLiteral("folder-link"),
          QStringLiteral("folder")}},
        {QStringLiteral("application-x-special-desktop"),
         {QStringLiteral("application-x-special-desktop"),
          QStringLiteral("application-x-executable"), QStringLiteral("text-x-generic")}},
        {QStringLiteral("drive-harddisk"),
         {QStringLiteral("drive-harddisk"), QStringLiteral("drive-harddisk-symbolic"),
          QStringLiteral("drive-removable-media")}},
        {QStringLiteral("open-menu-symbolic"),
         {QStringLiteral("open-menu-symbolic"), QStringLiteral("open-menu"),
          QStringLiteral("application-menu")}},
    };

    return map;
}

const QStringList &linkEmblemNames()
{
    static const QStringList names = {
        QStringLiteral("emblem-symbolic-link"),
        QStringLiteral("emblem-symbolic-link-symbolic"),
        QStringLiteral("emblem-link"),
        QStringLiteral("link"),
    };

    return names;
}

QIcon resolveIcon(const QString &name)
{
    if (FileIcons::isAbsoluteSpec(name)) {
        const QString path = FileIcons::decodeAbsoluteSpec(name);
        if (path.isEmpty() || !QFileInfo::exists(path))
            return {};
        return QIcon(path);
    }

    const auto it = aliases().constFind(name);
    const QStringList candidates = it == aliases().constEnd()
        ? QStringList{name}
        : it.value();

    for (const QString &candidate : candidates) {
        const QIcon icon = QIcon::fromTheme(candidate);
        if (!icon.isNull())
            return icon;
    }

    return QIcon::fromTheme(QStringLiteral("text-x-generic"));
}

}

IconProvider::IconProvider(int maxCacheEntries)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_maxCacheEntries(maxCacheEntries)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString name = id;
    bool linkEmblem = false;

    const qsizetype query = name.indexOf(QLatin1Char('?'));
    if (query >= 0) {
        const QStringList params = name.mid(query + 1).split(QLatin1Char('&'));
        for (const QString &param : params) {
            if (param == QLatin1String("emblem=link"))
                linkEmblem = true;
        }
        name.truncate(query);
    }

    if (name.isEmpty())
        name = QStringLiteral("text-x-generic");

    const int pixels = (requestedSize.isValid() && requestedSize.width() > 0)
        ? requestedSize.width()
        : 20;

    const QPixmap result = pixmap(name, pixels, linkEmblem);
    if (size)
        *size = result.size();

    return result;
}

QPixmap IconProvider::pixmap(const QString &name, int pixels, bool linkEmblem)
{
    const QString key = name + QLatin1Char('|') + QString::number(pixels)
        + (linkEmblem ? QLatin1String("|link") : QString());

    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd())
        return cached.value();

    const QIcon icon = resolveIcon(name);
    QPixmap result = icon.isNull() ? QPixmap() : icon.pixmap(pixels, pixels);

    if (result.isNull())
        result = fallbackPixmap(pixels);

    if (linkEmblem)
        result = withLinkEmblem(result);

    if (m_cache.size() >= m_maxCacheEntries)
        m_cache.clear();

    m_cache.insert(key, result);
    return result;
}

void IconProvider::clearCache()
{
    m_cache.clear();
}

QPixmap IconProvider::fallbackPixmap(int pixels) const
{
    QPixmap pixmap(pixels, pixels);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(1.5, 1.5, pixels - 3.0, pixels - 3.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(128, 128, 128, 60));
    painter.drawRoundedRect(rect, pixels * 0.18, pixels * 0.18);
    painter.end();

    return pixmap;
}

QPixmap IconProvider::linkEmblemPixmap(int pixels) const
{
    for (const QString &name : linkEmblemNames()) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull())
            return icon.pixmap(pixels, pixels);
    }

    return fallbackLinkEmblem(pixels);
}

QPixmap IconProvider::fallbackLinkEmblem(int pixels) const
{
    QPixmap pixmap(pixels, pixels);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal inset = pixels * 0.12;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(61, 111, 224, 235));
    painter.drawEllipse(QRectF(inset, inset, pixels - 2.0 * inset, pixels - 2.0 * inset));

    QPen pen(Qt::white);
    pen.setWidthF(qMax(1.0, pixels * 0.10));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const qreal low = pixels * 0.32;
    const qreal high = pixels * 0.68;
    painter.drawLine(QPointF(low, high), QPointF(high, low));
    painter.drawLine(QPointF(high, low), QPointF(high - pixels * 0.17, low));
    painter.drawLine(QPointF(high, low), QPointF(high, low + pixels * 0.17));
    painter.end();

    return pixmap;
}

QPixmap IconProvider::withLinkEmblem(const QPixmap &base) const
{
    const int side = qMax(9, qRound(base.width() * 0.42));
    const QPixmap emblem = linkEmblemPixmap(side);
    if (emblem.isNull())
        return base;

    QPixmap combined = base;
    QPainter painter(&combined);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(base.width() - side, base.height() - side, emblem);
    painter.end();

    return combined;
}
