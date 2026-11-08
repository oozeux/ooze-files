#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// Freedesktop thumbnail cache helpers:
// https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html
//
// Sharing the standard cache lets Ooze Files reuse thumbnails produced by
// other file managers and makes its own thumbnails visible to them.
namespace ThumbnailCache {

[[nodiscard]] QString directoryFor(const QSize &size);
[[nodiscard]] QString pathFor(const QString &localPath, const QSize &size);

// Only returns images whose Thumb::MTime still matches the source file.
[[nodiscard]] QImage load(const QString &localPath, const QString &cachePath);
[[nodiscard]] bool store(const QString &localPath, const QString &cachePath,
                         const QImage &image);

[[nodiscard]] inline QImage loadFor(const QString &localPath, const QSize &size)
{
    return load(localPath, pathFor(localPath, size));
}

[[nodiscard]] inline bool storeFor(const QString &localPath, const QSize &size,
                                   const QImage &image)
{
    return store(localPath, pathFor(localPath, size), image);
}

} // namespace ThumbnailCache
