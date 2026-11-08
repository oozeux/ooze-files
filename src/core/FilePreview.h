#pragma once

#include <QUrl>
#include <QVariantMap>

namespace FilePreview {

constexpr qint64 kMaxTextBytes = 64 * 1024;
constexpr int kMaxTextLines = 400;
constexpr int kMaxDirectoryCount = 5000;
constexpr int kMaxDirectorySizeEntries = 20000;
constexpr qint64 kMaxImagePixels = 120'000'000;

QVariantMap describe(const QUrl &url, bool includeDirectorySize = false);

}
