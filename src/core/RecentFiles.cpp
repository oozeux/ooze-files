#include "core/RecentFiles.h"

#include <QFile>
#include <QHash>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include <algorithm>

namespace {
QDateTime parseDate(const QString &text)
{
    if (text.isEmpty())
        return {};
    return QDateTime::fromString(text, Qt::ISODate);
}

QByteArray pathFromHref(const QString &href)
{
    static const QString prefix = QStringLiteral("file://");
    if (!href.startsWith(prefix))
        return {};

    const QString rest = href.mid(prefix.size());
    if (!rest.startsWith(QLatin1Char('/')))
        return {};

    const QByteArray path = QByteArray::fromPercentEncoding(rest.toUtf8());
    if (path.isEmpty() || path.contains('\0') || !path.startsWith('/'))
        return {};
    return path;
}
}

namespace RecentFiles {
QString defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/recently-used.xbel");
}

QVector<RecentItem> parse(const QByteArray &xml)
{
    QHash<QByteArray, RecentItem> byPath;

    QXmlStreamReader reader(xml);
    bool inBookmark = false;
    bool isPrivate = false;
    RecentItem item;
    QDateTime activity;

    const auto consider = [&activity](const QDateTime &candidate) {
        if (candidate.isValid() && (!activity.isValid() || candidate > activity))
            activity = candidate;
    };

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            const QStringView name = reader.name();

            if (name == QLatin1String("bookmark")) {
                inBookmark = true;
                isPrivate = false;
                activity = QDateTime();
                item = RecentItem();
                item.path = pathFromHref(reader.attributes().value(QStringLiteral("href")).toString());
                consider(parseDate(reader.attributes().value(QStringLiteral("added")).toString()));
                consider(parseDate(reader.attributes().value(QStringLiteral("modified")).toString()));
                consider(parseDate(reader.attributes().value(QStringLiteral("visited")).toString()));
            } else if (inBookmark && name == QLatin1String("mime-type")) {
                item.mimeType = reader.attributes().value(QStringLiteral("type")).toString();
            } else if (inBookmark && name == QLatin1String("application")) {
                consider(parseDate(reader.attributes().value(QStringLiteral("modified")).toString()));
            } else if (inBookmark && name == QLatin1String("group")) {
                const QString group = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (group == QLatin1String("private"))
                    isPrivate = true;
            }
        } else if (reader.isEndElement() && inBookmark && reader.name() == QLatin1String("bookmark")) {
            inBookmark = false;
            if (item.path.isEmpty() || isPrivate)
                continue;

            item.activity = activity;
            const auto existing = byPath.constFind(item.path);
            if (existing == byPath.constEnd()
                || (item.activity.isValid()
                    && (!existing->activity.isValid() || item.activity > existing->activity))) {
                byPath.insert(item.path, item);
            }
        }
    }

    QVector<RecentItem> items;
    items.reserve(byPath.size());
    for (auto it = byPath.cbegin(); it != byPath.cend(); ++it)
        items.append(it.value());

    std::sort(items.begin(), items.end(), [](const RecentItem &a, const RecentItem &b) {
        if (a.activity.isValid() != b.activity.isValid())
            return a.activity.isValid();
        if (a.activity != b.activity)
            return a.activity > b.activity;
        return a.path < b.path;
    });
    return items;
}

QVector<RecentItem> load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parse(file.read(kMaxFileBytes));
}
}
