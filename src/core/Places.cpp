#include "core/Places.h"

#include "core/TrashService.h"
#include "vfs/RecentVfs.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <QStandardPaths>
#include <QUrl>
#include <QCoreApplication>

namespace Places {
namespace {
void appendPlace(QVariantList &out, const QString &name, const QString &icon,
                 const QString &path, const QString &section)
{
    if (path.isEmpty())
        return;

    QVariantMap place;
    place.insert(QStringLiteral("name"), name);
    place.insert(QStringLiteral("icon"), icon);
    place.insert(QStringLiteral("url"), QUrl::fromLocalFile(path).toString());
    place.insert(QStringLiteral("id"), QDir::cleanPath(path));
    place.insert(QStringLiteral("section"), section);
    out.append(place);
}

void appendVirtualPlace(QVariantList &out, const QString &name, const QString &icon,
                        const QString &section, const QString &url = RecentVfs::rootUrl().toString())
{
    QVariantMap place;
    place.insert(QStringLiteral("name"), name);
    place.insert(QStringLiteral("icon"), icon);
    place.insert(QStringLiteral("url"), url);
    place.insert(QStringLiteral("id"), url);
    place.insert(QStringLiteral("section"), section);
    out.append(place);
}

bool containsId(const QVariantList &places, const QString &id)
{
    for (const QVariant &value : places) {
        if (value.toMap().value(QStringLiteral("id")).toString() == id)
            return true;
    }
    return false;
}

void appendXdgPlace(QVariantList &out, QStandardPaths::StandardLocation location,
                    const QString &name, const QString &icon)
{
    const QString path = QStandardPaths::writableLocation(location);
    if (path.isEmpty() || !QFileInfo(path).isDir())
        return;
    appendPlace(out, name, icon, path, QStringLiteral("places"));
}
}

QVariantList build(const QVariantList &custom, const QStringList &order)
{
    QVariantList places;

    appendPlace(places, QCoreApplication::translate("Ooze", "Inicio"), QStringLiteral("user-home"),
                QDir::homePath(), QStringLiteral("places"));

    appendXdgPlace(places, QStandardPaths::DocumentsLocation,
                   QCoreApplication::translate("Ooze", "Documentos"), QStringLiteral("folder-documents"));
    appendXdgPlace(places, QStandardPaths::DownloadLocation,
                   QCoreApplication::translate("Ooze", "Descargas"), QStringLiteral("folder-download"));
    appendXdgPlace(places, QStandardPaths::PicturesLocation,
                   QCoreApplication::translate("Ooze", "Imágenes"), QStringLiteral("folder-pictures"));
    appendXdgPlace(places, QStandardPaths::MusicLocation,
                   QCoreApplication::translate("Ooze", "Música"), QStringLiteral("folder-music"));
    appendXdgPlace(places, QStandardPaths::MoviesLocation,
                   QCoreApplication::translate("Ooze", "Vídeos"), QStringLiteral("folder-videos"));
    appendXdgPlace(places, QStandardPaths::DesktopLocation,
                   QCoreApplication::translate("Ooze", "Escritorio"), QStringLiteral("user-desktop"));

    for (const QVariant &value : custom) {
        const QVariantMap entry = value.toMap();
        const QString path = QDir::cleanPath(entry.value(QStringLiteral("path")).toString());
        if (!QFileInfo(path).isDir() || containsId(places, path))
            continue;

        QVariantMap place;
        place.insert(QStringLiteral("name"), entry.value(QStringLiteral("name")).toString());
        place.insert(QStringLiteral("icon"), QStringLiteral("folder"));
        place.insert(QStringLiteral("url"), QUrl::fromLocalFile(path).toString());
        place.insert(QStringLiteral("id"), path);
        place.insert(QStringLiteral("section"), QStringLiteral("places"));
        place.insert(QStringLiteral("custom"), true);
        places.append(place);
    }

    if (!order.isEmpty()) {
        const auto rank = [&order](const QVariant &value) {
            const qsizetype index = order.indexOf(value.toMap().value(QStringLiteral("id")).toString());
            return index < 0 ? order.size() : index;
        };
        std::stable_sort(places.begin(), places.end(),
                         [&rank](const QVariant &a, const QVariant &b) { return rank(a) < rank(b); });
    }

    appendVirtualPlace(places, QCoreApplication::translate("Ooze", "Recientes"),
                       QStringLiteral("document-open-recent"), QStringLiteral("recent"));

    appendPlace(places, QCoreApplication::translate("Ooze", "Sistema de archivos"),
                QStringLiteral("drive-harddisk"), QStringLiteral("/"),
                QStringLiteral("system"));

    appendVirtualPlace(places, QCoreApplication::translate("Ooze", "Papelera"),
                       QStringLiteral("user-trash"), QStringLiteral("system"),
                       TrashService::rootUrl().toString());

    return places;
}

QStringList orderedIds(const QVariantList &places)
{
    QStringList ids;
    for (const QVariant &value : places) {
        const QVariantMap place = value.toMap();
        if (place.value(QStringLiteral("section")).toString() == QLatin1String("places"))
            ids.append(place.value(QStringLiteral("id")).toString());
    }
    return ids;
}
}
