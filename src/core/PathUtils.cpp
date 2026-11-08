#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QVariantMap>
#include <QCoreApplication>

namespace {
constexpr char kHexDigits[] = "0123456789ABCDEF";

bool isPathSafe(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'
        || c == '~' || c == '/';
}

QByteArray percentEncode(const QByteArray &bytes)
{
    QByteArray out;
    out.reserve(bytes.size() + 16);

    for (unsigned char c : bytes) {
        if (isPathSafe(c)) {
            out.append(char(c));
        } else {
            out.append('%');
            out.append(kHexDigits[c >> 4]);
            out.append(kHexDigits[c & 0x0F]);
        }
    }

    return out;
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

QByteArray percentDecode(const QByteArray &encoded)
{
    QByteArray out;
    out.reserve(encoded.size());

    for (qsizetype i = 0; i < encoded.size(); ++i) {
        if (encoded.at(i) == '%' && i + 2 < encoded.size()) {
            const int high = hexValue(encoded.at(i + 1));
            const int low = hexValue(encoded.at(i + 2));
            if (high >= 0 && low >= 0) {
                out.append(char((high << 4) | low));
                i += 2;
                continue;
            }
        }
        out.append(encoded.at(i));
    }

    return out;
}
}

namespace PathUtils {
QByteArray toLocalBytes(const QUrl &url)
{
    const QString encoded = url.path(QUrl::FullyEncoded);
    QByteArray path = percentDecode(encoded.toLatin1());

    if (path.isEmpty())
        path = "/";
    if (!path.startsWith('/'))
        path.prepend('/');

    return path;
}

QUrl fromLocalBytes(const QByteArray &bytes)
{
    QByteArray path = bytes;
    if (path.isEmpty())
        path = "/";
    if (!path.startsWith('/'))
        path.prepend('/');

    QByteArray encoded = "file://";
    encoded.append(percentEncode(path));

    return QUrl::fromEncoded(encoded, QUrl::StrictMode);
}

QString displayName(const QByteArray &bytes)
{
    const QString asUtf8 = QString::fromUtf8(bytes);
    if (asUtf8.toUtf8() == bytes)
        return asUtf8;
    return QString::fromLocal8Bit(bytes);
}

QUrl normalized(const QUrl &url)
{
    if (!url.isLocalFile())
        return url;

    QByteArray path = toLocalBytes(url);

    QByteArray out;
    QList<QByteArray> stack;
    const QList<QByteArray> parts = path.split('/');

    for (const QByteArray &part : parts) {
        if (part.isEmpty() || part == ".")
            continue;
        if (part == "..") {
            if (!stack.isEmpty())
                stack.removeLast();
            continue;
        }
        stack.append(part);
    }

    out = '/' + stack.join('/');
    return fromLocalBytes(out);
}

bool isVirtual(const QUrl &url)
{
    const QString scheme = url.scheme();
    return scheme == QLatin1String("recent") || scheme == QLatin1String("trash");
}

QString homeAbbreviated(const QByteArray &folder)
{
    const QByteArray home = QDir::homePath().toUtf8();
    if (home != "/" && !home.isEmpty()) {
        if (folder == home)
            return QStringLiteral("~");
        if (folder.startsWith(home + '/'))
            return QStringLiteral("~") + displayName(folder.mid(home.size()));
    }
    return displayName(folder);
}

QUrl parent(const QUrl &url)
{
    if (url.scheme() == QLatin1String("recent"))
        return url;

    if (url.scheme() == QLatin1String("trash")) {
        QByteArray encoded = url.path(QUrl::FullyEncoded).toLatin1();
        while (encoded.endsWith('/'))
            encoded.chop(1);
        const QList<QByteArray> segments = encoded.split('/');
        if (segments.size() <= 3)
            return QUrl(QStringLiteral("trash:///"));
        QByteArray parentPath;
        for (qsizetype i = 0; i < segments.size() - 1; ++i) {
            if (segments.at(i).isEmpty())
                continue;
            parentPath += "/" + segments.at(i);
        }
        return QUrl::fromEncoded("trash://" + parentPath);
    }

    if (!url.isLocalFile())
        return url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

    QByteArray path = toLocalBytes(normalized(url));
    while (path.size() > 1 && path.endsWith('/'))
        path.chop(1);

    const qsizetype idx = path.lastIndexOf('/');
    if (idx <= 0)
        return fromLocalBytes("/");
    return fromLocalBytes(path.left(idx));
}

QByteArray childBytes(const QByteArray &dir, const QByteArray &name)
{
    QByteArray base = dir;
    if (base.isEmpty())
        base = "/";
    if (!base.endsWith('/'))
        base.append('/');
    base.append(name);
    return base;
}

QUrl child(const QUrl &dir, const QByteArray &name)
{
    if (!dir.isLocalFile()) {
        QString base = dir.path();
        if (!base.endsWith('/'))
            base += '/';
        base += QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(name)));
        QUrl result = dir;
        result.setPath(base, QUrl::DecodedMode);
        return result;
    }

    return fromLocalBytes(childBytes(toLocalBytes(normalized(dir)), name));
}

bool isSameOrAncestor(const QUrl &ancestor, const QUrl &url)
{
    if (ancestor.scheme() != url.scheme())
        return false;

    const QByteArray a = toLocalBytes(normalized(ancestor));
    const QByteArray u = toLocalBytes(normalized(url));

    if (a == u)
        return true;
    if (a == "/")
        return true;
    if (!u.startsWith(a))
        return false;

    return u.size() > a.size() && u.at(a.size()) == '/';
}

QVariantList breadcrumb(const QUrl &url)
{
    QVariantList result;

    if (url.scheme() == QLatin1String("recent")) {
        result.append(QVariantMap{{QStringLiteral("name"), QCoreApplication::translate("Ooze", "Recientes")},
                                  {QStringLiteral("url"), url.toString()}});
        return result;
    }

    if (url.scheme() == QLatin1String("trash")) {
        result.append(QVariantMap{{QStringLiteral("name"), QCoreApplication::translate("Ooze", "Papelera")},
                                  {QStringLiteral("url"), QStringLiteral("trash:///")}});

        const QList<QByteArray> parts = url.path(QUrl::FullyEncoded).toLatin1().split('/');
        QByteArray accumulated;
        int index = 0;
        for (const QByteArray &part : parts) {
            if (part.isEmpty())
                continue;
            accumulated += "/" + part;
            if (index++ == 0)
                continue;
            result.append(QVariantMap{
                {QStringLiteral("name"), displayName(QByteArray::fromPercentEncoding(part))},
                {QStringLiteral("url"), QUrl::fromEncoded("trash://" + accumulated).toString()}});
        }
        return result;
    }

    if (!url.isLocalFile()) {
        result.append(QVariantMap{{QStringLiteral("name"), url.toDisplayString()},
                                  {QStringLiteral("url"), url.toString()}});
        return result;
    }

    const QByteArray home = QDir::homePath().toUtf8();
    const QByteArray path = toLocalBytes(normalized(url));

    QByteArray prefix;
    QByteArray remainder;

    if (home != "/" && (path == home || path.startsWith(home + '/'))) {
        result.append(QVariantMap{{QStringLiteral("name"), QCoreApplication::translate("Ooze", "Inicio")},
                                  {QStringLiteral("url"), fromLocalBytes(home).toString()}});
        prefix = home;
        remainder = path == home ? QByteArray() : path.mid(home.size() + 1);
    } else {
        result.append(QVariantMap{{QStringLiteral("name"), QStringLiteral("/")},
                                  {QStringLiteral("url"), fromLocalBytes("/").toString()}});
        remainder = path;
    }

    const QList<QByteArray> parts = remainder.split('/');
    for (const QByteArray &part : parts) {
        if (part.isEmpty())
            continue;
        prefix = childBytes(prefix, part);
        result.append(QVariantMap{{QStringLiteral("name"), displayName(part)},
                                  {QStringLiteral("url"), fromLocalBytes(prefix).toString()}});
    }

    return result;
}

QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("\u2014");
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString formatDate(const QDateTime &dt)
{
    if (!dt.isValid())
        return QStringLiteral("\u2014");
    return QLocale().toString(dt, QLocale::ShortFormat);
}
}
