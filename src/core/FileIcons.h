#pragma once

#include <QString>

namespace FileIcons {

// Resolves the icon for a regular file.  .desktop launchers expose the icon
// declared in their [Desktop Entry] group and AppImages expose the icon stored
// in their squashfs payload.  Any other file keeps the supplied fallback.
QString iconForFile(const QString &path, const QString &fallback);

// Absolute icon paths travel through image://icons/ URLs, so they are encoded
// as unpadded base64url to avoid clashing with the query string.
QString absoluteSpec(const QString &path);
QString decodeAbsoluteSpec(const QString &spec);
bool isAbsoluteSpec(const QString &spec);

} // namespace FileIcons
