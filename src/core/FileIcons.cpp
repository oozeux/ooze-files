#include "core/FileIcons.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMutex>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QUrl>

namespace {

constexpr qint64 kMaxDesktopBytes = 256 * 1024;
constexpr int kIconSide = 512;
constexpr int kProcessTimeoutMs = 20000;

const char kAbsolutePrefix[] = "abs:";

QString cacheDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.cache/ooze-files");
    return base + QStringLiteral("/appimage-icons");
}

QString findUnsquashfs()
{
    const QString fromEnv = qEnvironmentVariable("OOZE_FILES_UNSQUASHFS");
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable())
        return fromEnv;

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("unsquashfs"));
    if (!fromPath.isEmpty())
        return fromPath;

    const QStringList candidates = {
        QStringLiteral("/run/current-system/sw/bin/unsquashfs"),
        QStringLiteral("/usr/bin/unsquashfs"),
        QStringLiteral("/usr/sbin/unsquashfs"),
        QStringLiteral("/usr/local/bin/unsquashfs"),
        QDir::homePath() + QStringLiteral("/.nix-profile/bin/unsquashfs"),
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }

    return {};
}

QString desktopIcon(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray data = file.read(kMaxDesktopBytes);
    bool inDesktopEntry = false;
    QString value;

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.startsWith('[')) {
            inDesktopEntry = line == "[Desktop Entry]";
            continue;
        }
        if (!inDesktopEntry)
            continue;

        if (line.startsWith("Icon=")) {
            value = QString::fromUtf8(line.mid(5)).trimmed();
            break;
        }
    }

    if (value.isEmpty())
        return {};

    if (value.startsWith(QStringLiteral("file://"))) {
        const QUrl url(value);
        if (url.isLocalFile())
            value = url.toLocalFile();
    }

    if (value.startsWith(QLatin1Char('/'))) {
        const QFileInfo iconInfo(value);
        if (iconInfo.isFile())
            return FileIcons::absoluteSpec(iconInfo.absoluteFilePath());
        return {};
    }

    if (value.contains(QLatin1Char('/'))) {
        const QFileInfo relative(QFileInfo(path).absolutePath(), value);
        if (relative.isFile())
            return FileIcons::absoluteSpec(relative.absoluteFilePath());
        return {};
    }

    return value;
}

bool isAppImage(const QString &path)
{
    if (!path.endsWith(QStringLiteral(".appimage"), Qt::CaseInsensitive))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray header = file.read(12);
    return header.size() == 12
        && header.startsWith(QByteArrayLiteral("\x7f" "ELF"))
        && header.at(8) == 'A' && header.at(9) == 'I';
}

qint64 appImageOffset(const QByteArray &header, qint64 fileSize)
{
    if (header.size() < 52 || !header.startsWith(QByteArrayLiteral("\x7f" "ELF")))
        return -1;

    const bool is64 = static_cast<quint8>(header.at(4)) == 2;
    const auto read16 = [&header](int offset) -> qint64 {
        return static_cast<quint8>(header.at(offset))
             | (static_cast<quint8>(header.at(offset + 1)) << 8);
    };
    const auto read32 = [&header](int offset) -> qint64 {
        return static_cast<quint8>(header.at(offset))
             | (static_cast<quint8>(header.at(offset + 1)) << 8)
             | (static_cast<quint8>(header.at(offset + 2)) << 16)
             | (static_cast<quint8>(header.at(offset + 3)) << 24);
    };
    const auto read64 = [&read32](int offset) -> qint64 {
        return read32(offset) | (read32(offset + 4) << 32);
    };

    qint64 sectionOffset = 0;
    int sectionEntrySize = 0;
    int sectionCount = 0;

    if (is64) {
        sectionOffset = read64(0x28);
        sectionEntrySize = int(read16(0x3A));
        sectionCount = int(read16(0x3C));
    } else {
        sectionOffset = read32(0x20);
        sectionEntrySize = int(read16(0x2E));
        sectionCount = int(read16(0x30));
    }

    const qint64 offset = sectionOffset + qint64(sectionEntrySize) * sectionCount;
    if (offset <= 0 || offset >= fileSize)
        return -1;
    return offset;
}

qint64 readAppImageOffset(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    const QByteArray header = file.read(64);
    return appImageOffset(header, file.size());
}

bool runUnsquashfs(const QString &program, const QStringList &arguments,
                   QByteArray *output)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(3000))
        return false;

    if (!process.waitForFinished(kProcessTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;

    if (output)
        *output = process.readAllStandardOutput();
    return true;
}

QString rootLevelIcon(const QString &program, qint64 offset, const QString &image)
{
    QByteArray listing;
    if (!runUnsquashfs(program,
                       {QStringLiteral("-o"), QString::number(offset),
                        QStringLiteral("-ls"), image},
                       &listing)) {
        return {};
    }

    const QByteArray prefix = QByteArrayLiteral("squashfs-root/");
    QString png;
    QString svg;

    const QList<QByteArray> lines = listing.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (!line.startsWith(prefix))
            continue;

        const QString relative = QString::fromUtf8(line.mid(prefix.size()));
        if (relative.contains(QLatin1Char('/')) || relative.isEmpty())
            continue;

        if (relative.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
            png = relative;
            break;
        }
        if (svg.isEmpty()
            && relative.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
            svg = relative;
        }
    }

    return png.isEmpty() ? svg : png;
}

QImage imageFromPayload(const QByteArray &payload)
{
    if (payload.isEmpty())
        return {};

    if (payload.startsWith(QByteArrayLiteral("<?xml"))
        || payload.startsWith(QByteArrayLiteral("<svg"))
        || payload.contains(QByteArrayLiteral("<svg"))) {
        QSvgRenderer renderer(payload);
        if (!renderer.isValid())
            return {};

        QImage image(kIconSide, kIconSide, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.end();
        return image;
    }

    QImage image = QImage::fromData(payload);
    if (!image.isNull()
        && (image.width() > kIconSide || image.height() > kIconSide)) {
        image = image.scaled(kIconSide, kIconSide, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image;
}

QString cacheKey(const QString &path, const QFileInfo &info)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(path.toUtf8());
    hash.addData(QByteArray::number(info.size()));
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    return QString::fromLatin1(hash.result().toHex());
}

QString appImageIcon(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};

    const QString directory = cacheDirectory();
    const QString key = cacheKey(path, info);
    const QString cachePath = directory + QLatin1Char('/') + key + QStringLiteral(".png");

    if (QFileInfo(cachePath).isFile() && QFileInfo(cachePath).size() > 0)
        return FileIcons::absoluteSpec(cachePath);

    static QMutex mutex;
    const QMutexLocker locker(&mutex);

    // Another worker may have produced it while we were waiting.
    if (QFileInfo(cachePath).isFile() && QFileInfo(cachePath).size() > 0)
        return FileIcons::absoluteSpec(cachePath);

    const QString program = findUnsquashfs();
    if (program.isEmpty())
        return {};

    const qint64 offset = readAppImageOffset(path);
    if (offset <= 0)
        return {};

    QByteArray payload;
    bool extracted = runUnsquashfs(program,
                                   {QStringLiteral("-o"), QString::number(offset),
                                    QStringLiteral("-cat"), path,
                                    QStringLiteral(".DirIcon")},
                                   &payload);

    if (!extracted || payload.isEmpty()) {
        const QString relative = rootLevelIcon(program, offset, path);
        if (relative.isEmpty())
            return {};

        payload.clear();
        extracted = runUnsquashfs(program,
                                  {QStringLiteral("-o"), QString::number(offset),
                                   QStringLiteral("-cat"), path, relative},
                                  &payload);
    }

    if (!extracted)
        return {};

    const QImage image = imageFromPayload(payload);
    if (image.isNull())
        return {};

    if (!QDir().mkpath(directory))
        return {};

    if (!image.save(cachePath, "PNG"))
        return {};

    return FileIcons::absoluteSpec(cachePath);
}

} // namespace

namespace FileIcons {

QString absoluteSpec(const QString &path)
{
    const QByteArray encoded = path.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(kAbsolutePrefix) + QString::fromLatin1(encoded);
}

QString decodeAbsoluteSpec(const QString &spec)
{
    if (!isAbsoluteSpec(spec))
        return {};

    const QByteArray encoded = spec.mid(int(qstrlen(kAbsolutePrefix))).toLatin1();
    return QString::fromUtf8(QByteArray::fromBase64(encoded,
                                                    QByteArray::Base64UrlEncoding));
}

bool isAbsoluteSpec(const QString &spec)
{
    return spec.startsWith(QLatin1String(kAbsolutePrefix));
}

QString iconForFile(const QString &path, const QString &fallback)
{
    if (path.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        const QString icon = desktopIcon(path);
        if (!icon.isEmpty())
            return icon;
    }

    if (isAppImage(path)) {
        const QString icon = appImageIcon(path);
        if (!icon.isEmpty())
            return icon;
    }

    return fallback;
}

} // namespace FileIcons
