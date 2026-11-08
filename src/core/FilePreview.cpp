#include "core/FilePreview.h"

#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QCoreApplication>

namespace {

QString permissionsText(QFileDevice::Permissions permissions)
{
    struct Bit
    {
        QFileDevice::Permission flag;
        QChar letter;
    };

    static const Bit bits[] = {
        {QFileDevice::ReadOwner, QLatin1Char('r')},  {QFileDevice::WriteOwner, QLatin1Char('w')},
        {QFileDevice::ExeOwner, QLatin1Char('x')},   {QFileDevice::ReadGroup, QLatin1Char('r')},
        {QFileDevice::WriteGroup, QLatin1Char('w')}, {QFileDevice::ExeGroup, QLatin1Char('x')},
        {QFileDevice::ReadOther, QLatin1Char('r')},  {QFileDevice::WriteOther, QLatin1Char('w')},
        {QFileDevice::ExeOther, QLatin1Char('x')},
    };

    QString text;
    for (const Bit &bit : bits)
        text.append(permissions.testFlag(bit.flag) ? bit.letter : QLatin1Char('-'));
    return text;
}

QString dateText(const QDateTime &value)
{
    return value.isValid() ? QLocale().toString(value, QLocale::ShortFormat) : QString();
}

int countEntries(const QString &path, bool *truncated)
{
    int count = 0;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    while (it.hasNext()) {
        it.next();
        if (++count >= FilePreview::kMaxDirectoryCount) {
            *truncated = true;
            break;
        }
    }
    return count;
}

qint64 directorySize(const QString &path, bool *truncated)
{
    qint64 total = 0;
    int visited = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!info.isSymLink())
            total += info.size();
        if (++visited >= FilePreview::kMaxDirectorySizeEntries) {
            *truncated = true;
            break;
        }
    }
    return total;
}

bool readText(const QString &path, QString *text, bool *truncated)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray bytes = file.read(FilePreview::kMaxTextBytes + 1);
    if (bytes.contains('\0'))
        return false;

    *truncated = bytes.size() > FilePreview::kMaxTextBytes;
    if (*truncated)
        bytes.chop(1);

    int lines = 0;
    int cut = -1;
    for (int i = 0; i < bytes.size(); ++i) {
        if (bytes.at(i) == '\n' && ++lines >= FilePreview::kMaxTextLines) {
            cut = i;
            break;
        }
    }

    if (cut >= 0) {
        bytes.truncate(cut);
        *truncated = true;
    }

    QString decoded = QString::fromUtf8(bytes);
    if (*truncated)
        while (decoded.endsWith(QChar::ReplacementCharacter))
            decoded.chop(1);

    *text = decoded;
    return true;
}

}

QVariantMap FilePreview::describe(const QUrl &url, bool includeDirectorySize)
{
    QVariantMap out;
    out.insert(QStringLiteral("kind"), QStringLiteral("none"));
    out.insert(QStringLiteral("url"), url.toString());

    if (!url.isLocalFile()) {
        out.insert(QStringLiteral("name"), url.fileName());
        return out;
    }

    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    out.insert(QStringLiteral("path"), path);
    out.insert(QStringLiteral("name"), info.fileName().isEmpty() ? path : info.fileName());

    if (!info.exists() && !info.isSymLink()) {
        out.insert(QStringLiteral("error"), QCoreApplication::translate("Ooze", "El archivo ya no existe"));
        return out;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path);
    out.insert(QStringLiteral("mime"), mime.name());
    out.insert(QStringLiteral("mimeDescription"),
               info.isDir() ? QCoreApplication::translate("Ooze", "Carpeta") : mime.comment());
    out.insert(QStringLiteral("modifiedText"), dateText(info.lastModified()));
    out.insert(QStringLiteral("createdText"), dateText(info.birthTime()));
    out.insert(QStringLiteral("permissions"), permissionsText(info.permissions()));
    out.insert(QStringLiteral("owner"), info.owner());
    out.insert(QStringLiteral("group"), info.group());

    if (info.isSymLink())
        out.insert(QStringLiteral("symlinkTarget"), info.symLinkTarget());

    if (info.isDir()) {
        bool truncated = false;
        const int count = countEntries(path, &truncated);
        out.insert(QStringLiteral("kind"), QStringLiteral("directory"));
        out.insert(QStringLiteral("itemCountText"),
                   truncated ? QStringLiteral("%1+").arg(count) : QString::number(count));

        if (includeDirectorySize) {
            bool sizeTruncated = false;
            const qint64 bytes = directorySize(path, &sizeTruncated);
            QString sizeText = QLocale().formattedDataSize(bytes);
            if (sizeTruncated)
                sizeText.prepend(QStringLiteral("≥ "));
            out.insert(QStringLiteral("sizeText"), sizeText);
        }
        return out;
    }

    out.insert(QStringLiteral("sizeText"), QLocale().formattedDataSize(info.size()));

    if (mime.name().startsWith(QLatin1String("video/"))) {
        out.insert(QStringLiteral("kind"), QStringLiteral("video"));
        return out;
    }

    if (mime.name() == QLatin1String("application/pdf")) {
        out.insert(QStringLiteral("kind"), QStringLiteral("pdf"));
        return out;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (reader.canRead()) {
        const QSize size = reader.size();
        const qint64 pixels = size.isValid() ? qint64(size.width()) * size.height() : 0;
        if (size.isValid())
            out.insert(QStringLiteral("dimensions"),
                       QStringLiteral("%1 × %2").arg(size.width()).arg(size.height()));
        if (pixels <= kMaxImagePixels) {
            out.insert(QStringLiteral("kind"), QStringLiteral("image"));
            return out;
        }
    }

    if (info.size() > 0 && mime.inherits(QStringLiteral("text/plain"))) {
        QString text;
        bool truncated = false;
        if (readText(path, &text, &truncated)) {
            const bool markdown = mime.inherits(QStringLiteral("text/markdown"))
                || info.suffix().compare(QLatin1String("md"), Qt::CaseInsensitive) == 0
                || info.suffix().compare(QLatin1String("markdown"), Qt::CaseInsensitive) == 0;
            out.insert(QStringLiteral("kind"),
                       markdown ? QStringLiteral("markdown") : QStringLiteral("text"));
            out.insert(QStringLiteral("text"), text);
            out.insert(QStringLiteral("truncated"), truncated);
        }
    }

    return out;
}
