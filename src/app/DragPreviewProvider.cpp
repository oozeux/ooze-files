#include "app/DragPreviewProvider.h"

#include "app/IconProvider.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int kMaxLabelWidth = 260;
constexpr qreal kEmblemRatio = 2.0;
}

DragPreviewProvider::DragPreviewProvider(IconProvider *icons)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_icons(icons)
{
}

QPixmap DragPreviewProvider::requestPixmap(const QString &id, QSize *size,
                                           const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)

    QString label = id;
    QUrlQuery query;

    const qsizetype divider = label.indexOf(QLatin1Char('?'));
    if (divider >= 0) {
        query.setQuery(label.mid(divider + 1));
        label.truncate(divider);
    }

    label = QUrl::fromPercentEncoding(label.toUtf8());

    const QString iconName =
        query.queryItemValue(QStringLiteral("icon"), QUrl::FullyDecoded);
    const bool linkEmblem =
        query.queryItemValue(QStringLiteral("emblem"), QUrl::FullyDecoded)
            == QLatin1String("link");
    const int count = query.queryItemValue(QStringLiteral("count")).toInt();
    const QColor foreground(
        query.queryItemValue(QStringLiteral("fg"), QUrl::FullyDecoded));
    const QString family =
        query.queryItemValue(QStringLiteral("family"), QUrl::FullyDecoded);
    const int fontSize =
        qBound(9, query.queryItemValue(QStringLiteral("size")).toInt(), 40);

    QFont font(family.isEmpty() ? QGuiApplication::font().family() : family);
    font.setPixelSize(fontSize);

    const QFontMetrics metrics(font);
    QString text = metrics.elidedText(label, Qt::ElideMiddle, kMaxLabelWidth);
    if (count > 1)
        text += QStringLiteral(" (+%1)").arg(count - 1);

    const int iconPixels = qRound(fontSize * kEmblemRatio);
    const int gap = qRound(fontSize * 0.55);
    const int padding = qRound(fontSize * 0.45);
    const int textWidth = metrics.horizontalAdvance(text);

    const int width = padding * 2 + iconPixels + gap + textWidth;
    const int height = qMax(iconPixels, metrics.height()) + padding * 2;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPixmap icon =
        m_icons ? m_icons->pixmap(iconName, iconPixels, linkEmblem) : QPixmap();
    if (!icon.isNull()) {
        painter.drawPixmap(padding + (iconPixels - icon.width()) / 2,
                           (height - icon.height()) / 2, icon);
    }

    const QRect textRect(padding + iconPixels + gap, 0, textWidth, height);
    painter.setFont(font);
    painter.setPen(QColor(0, 0, 0, 90));
    painter.drawText(textRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.setPen(foreground.isValid()
                       ? foreground
                       : QGuiApplication::palette().color(QPalette::Text));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.end();

    if (size)
        *size = QSize(width, height);

    return QPixmap::fromImage(image);
}
