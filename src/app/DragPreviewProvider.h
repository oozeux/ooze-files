#pragma once

#include <QPixmap>
#include <QQuickImageProvider>

class IconProvider;

class DragPreviewProvider : public QQuickImageProvider
{
public:
    explicit DragPreviewProvider(IconProvider *icons);

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    IconProvider *m_icons = nullptr;
};
