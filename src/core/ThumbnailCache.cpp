#include "core/ThumbnailCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString bucketFor(const QSize &size)
{
    const int side = qMax(size.width(), size.height());
    if (side <= 128)
        return QStringLiteral("normal");
    if (side <= 256)
        return QStringLiteral("large");
    if (side <= 512)
        return QStringLiteral("x-large");
    return QStringLiteral("xx-large");
}

QString cacheRoot()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.cache");
    return base + QStringLiteral("/thumbnails");
}

} // namespace

namespace ThumbnailCache {

QString directoryFor(const QSize &size)
{
    return cacheRoot() + QLatin1Char('/') + bucketFor(size);
}

QString pathFor(const QString &localPath, const QSize &size)
{
    const QString uri = QUrl::fromLocalFile(QFileInfo(localPath).absoluteFilePath())
                            .toString(QUrl::FullyEncoded);
    // The specification requires MD5 of the encoded URI.
    const QByteArray hash = QCryptographicHash::hash(uri.toUtf8(),
                                                     QCryptographicHash::Md5)
                                .toHex();
    return directoryFor(size) + QLatin1Char('/') + QString::fromLatin1(hash)
        + QStringLiteral(".png");
}

QImage load(const QString &localPath, const QString &cachePath)
{
    const QFileInfo source(localPath);
    if (!source.exists() || !QFileInfo::exists(cachePath))
        return {};

    QImage image;
    if (!image.load(cachePath, "PNG") || image.isNull())
        return {};

    const QString storedMtime = image.text(QStringLiteral("Thumb::MTime"));
    if (storedMtime.isEmpty())
        return {};
    if (storedMtime.toLongLong() != source.lastModified().toSecsSinceEpoch())
        return {};

    return image;
}

bool store(const QString &localPath, const QString &cachePath, const QImage &image)
{
    if (image.isNull())
        return false;

    const QFileInfo source(localPath);
    if (!source.exists())
        return false;

    if (!QDir().mkpath(QFileInfo(cachePath).absolutePath()))
        return false;

    QImage tagged = image;
    tagged.setText(QStringLiteral("Thumb::URI"),
                   QUrl::fromLocalFile(source.absoluteFilePath())
                       .toString(QUrl::FullyEncoded));
    tagged.setText(QStringLiteral("Thumb::MTime"),
                   QString::number(source.lastModified().toSecsSinceEpoch()));
    tagged.setText(QStringLiteral("Software"), QStringLiteral("Ooze Files"));

    QSaveFile saver(cachePath);
    if (!saver.open(QIODevice::WriteOnly))
        return false;
    if (!tagged.save(&saver, "PNG")) {
        saver.cancelWriting();
        return false;
    }
    return saver.commit();
}

} // namespace ThumbnailCache
