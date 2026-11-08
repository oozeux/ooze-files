#include <gio/gio.h>

#include "core/OpenWith.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QVariantMap>

#include <algorithm>
#include <memory>
#include <QCoreApplication>

namespace {

struct Category
{
    const char *id;
    const char *label;
    QStringList mimes;
};

const QVector<Category> &categories()
{
    static const QVector<Category> list = {
        {"folders", "Carpetas", {QStringLiteral("inode/directory")}},
        {"text", "Texto", {QStringLiteral("text/plain"), QStringLiteral("text/markdown")}},
        {"images", "Imágenes",
         {QStringLiteral("image/png"), QStringLiteral("image/jpeg"), QStringLiteral("image/gif"),
          QStringLiteral("image/webp"), QStringLiteral("image/svg+xml"),
          QStringLiteral("image/bmp"), QStringLiteral("image/tiff"),
          QStringLiteral("image/avif")}},
        {"video", "Vídeo",
         {QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"),
          QStringLiteral("video/webm"), QStringLiteral("video/quicktime"),
          QStringLiteral("video/x-msvideo"), QStringLiteral("video/mpeg")}},
        {"audio", "Audio",
         {QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"), QStringLiteral("audio/ogg"),
          QStringLiteral("audio/x-wav"), QStringLiteral("audio/mp4"),
          QStringLiteral("audio/x-opus+ogg")}},
        {"documents", "Documentos PDF y libros",
         {QStringLiteral("application/pdf"), QStringLiteral("application/epub+zip")}},
        {"archives", "Archivos comprimidos",
         {QStringLiteral("application/zip"), QStringLiteral("application/x-tar"),
          QStringLiteral("application/gzip"), QStringLiteral("application/x-7z-compressed"),
          QStringLiteral("application/vnd.rar")}},
        {"web", "Navegador web",
         {QStringLiteral("text/html"), QStringLiteral("x-scheme-handler/http"),
          QStringLiteral("x-scheme-handler/https")}},
    };
    return list;
}

struct ListDeleter
{
    void operator()(GList *list) const { g_list_free_full(list, g_object_unref); }
};

using AppList = std::unique_ptr<GList, ListDeleter>;

QStringList mimeChain(const QString &mime)
{
    QStringList chain{mime};
    const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
    if (type.isValid()) {
        for (const QString &ancestor : type.allAncestors()) {
            if (ancestor != QLatin1String("application/octet-stream") && !chain.contains(ancestor))
                chain.append(ancestor);
        }
    }
    if (mime.startsWith(QLatin1String("text/")) && !chain.contains(QLatin1String("text/plain")))
        chain.append(QStringLiteral("text/plain"));
    return chain;
}

QString defaultIdFor(const QByteArray &type)
{
    QString id;
    if (GAppInfo *info = g_app_info_get_default_for_type(type.constData(), FALSE)) {
        if (const char *raw = g_app_info_get_id(info))
            id = QString::fromUtf8(raw);
        g_object_unref(info);
    }
    return id;
}

void appendApps(GList *apps, QSet<QString> &seen, QList<QVariantMap> &out)
{
    for (GList *node = apps; node; node = node->next) {
        GAppInfo *info = G_APP_INFO(node->data);
        const char *id = g_app_info_get_id(info);
        if (!id || !g_app_info_should_show(info))
            continue;

        const QString appId = QString::fromUtf8(id);
        if (seen.contains(appId))
            continue;
        seen.insert(appId);

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), appId);
        entry.insert(QStringLiteral("name"), QString::fromUtf8(g_app_info_get_display_name(info)));
        out.append(entry);
    }
}

GAppInfo *findApp(const AppList &all, const QString &appId)
{
    for (GList *node = all.get(); node; node = node->next) {
        GAppInfo *info = G_APP_INFO(node->data);
        const char *id = g_app_info_get_id(info);
        if (id && appId == QString::fromUtf8(id))
            return info;
    }
    return nullptr;
}

}

QVariantList OpenWith::defaultAppStates()
{
    QVariantList states;

    for (const Category &category : categories()) {
        QVariantList apps;
        QSet<QString> seen;

        for (const QString &mime : category.mimes) {
            const QVariantList forMime = appsForMime(mime);
            for (const QVariant &value : forMime) {
                const QVariantMap app = value.toMap();
                const QString id = app.value(QStringLiteral("id")).toString();
                if (seen.contains(id))
                    continue;
                seen.insert(id);

                QVariantMap entry;
                entry.insert(QStringLiteral("id"), id);
                entry.insert(QStringLiteral("name"), app.value(QStringLiteral("name")));
                apps.append(entry);
            }
        }

        std::sort(apps.begin(), apps.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString().compare(
                       b.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive)
                < 0;
        });

        QString currentId;
        QString currentName;
        const QByteArray first = category.mimes.first().toUtf8();
        if (GAppInfo *current = g_app_info_get_default_for_type(first.constData(), FALSE)) {
            if (const char *id = g_app_info_get_id(current))
                currentId = QString::fromUtf8(id);
            currentName = QString::fromUtf8(g_app_info_get_display_name(current));
            g_object_unref(current);
        }

        QVariantMap state;
        state.insert(QStringLiteral("id"), QString::fromLatin1(category.id));
        state.insert(QStringLiteral("label"), QString::fromUtf8(category.label));
        state.insert(QStringLiteral("appId"), currentId);
        state.insert(QStringLiteral("appName"), currentName);
        state.insert(QStringLiteral("apps"), apps);
        states.append(state);
    }

    return states;
}

bool OpenWith::setDefault(const QString &categoryId, const QString &appId, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    const Category *category = nullptr;
    for (const Category &candidate : categories()) {
        if (categoryId == QLatin1String(candidate.id))
            category = &candidate;
    }

    if (!category)
        return fail(QCoreApplication::translate("Ooze", "Categoría desconocida"));

    const AppList all(g_app_info_get_all());
    GAppInfo *target = nullptr;
    for (GList *node = all.get(); node; node = node->next) {
        GAppInfo *info = G_APP_INFO(node->data);
        const char *id = g_app_info_get_id(info);
        if (id && appId == QString::fromUtf8(id)) {
            target = info;
            break;
        }
    }

    if (!target)
        return fail(QCoreApplication::translate("Ooze", "La aplicación ya no está disponible"));

    bool any = false;
    QString firstError;
    for (const QString &mime : category->mimes) {
        const QByteArray type = mime.toUtf8();
        GError *gerror = nullptr;
        if (g_app_info_set_as_default_for_type(target, type.constData(), &gerror)) {
            any = true;
        } else if (gerror) {
            if (firstError.isEmpty())
                firstError = QString::fromUtf8(gerror->message);
            g_error_free(gerror);
        }
    }

    if (!any)
        return fail(firstError.isEmpty() ? QCoreApplication::translate("Ooze", "No se pudo guardar la preferencia")
                                         : firstError);

    return true;
}

QVariantList OpenWith::appsForMime(const QString &mime)
{
    QVariantList result;
    if (mime.isEmpty())
        return result;

    QSet<QString> seen;
    QList<QVariantMap> entries;
    for (const QString &candidate : mimeChain(mime)) {
        const QByteArray type = candidate.toUtf8();
        const AppList exact(g_app_info_get_all_for_type(type.constData()));
        appendApps(exact.get(), seen, entries);
        const AppList fallback(g_app_info_get_fallback_for_type(type.constData()));
        appendApps(fallback.get(), seen, entries);
    }

    const QString defaultId = defaultIdFor(mime.toUtf8());
    for (QVariantMap entry : entries) {
        const bool isDefault = entry.value(QStringLiteral("id")).toString() == defaultId;
        entry.insert(QStringLiteral("isDefault"), isDefault);
        if (isDefault)
            result.prepend(entry);
        else
            result.append(entry);
    }

    return result;
}

QVariantList OpenWith::otherApps(const QString &mime)
{
    QSet<QString> seen;
    for (const QVariant &value : appsForMime(mime))
        seen.insert(value.toMap().value(QStringLiteral("id")).toString());

    const AppList all(g_app_info_get_all());
    QList<QVariantMap> entries;
    appendApps(all.get(), seen, entries);

    std::sort(entries.begin(), entries.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("name")).toString().compare(
                   b.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive)
            < 0;
    });

    QVariantList result;
    for (const QVariantMap &entry : entries) {
        QVariantMap item = entry;
        item.insert(QStringLiteral("isDefault"), false);
        result.append(item);
    }
    return result;
}

bool OpenWith::launch(const QString &appId, const QString &, const QList<QUrl> &urls,
                      QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (appId.isEmpty() || urls.isEmpty())
        return fail(QCoreApplication::translate("Ooze", "No hay aplicación o archivos que abrir"));

    const AppList all(g_app_info_get_all());
    GAppInfo *target = findApp(all, appId);

    if (!target)
        return fail(QCoreApplication::translate("Ooze", "La aplicación ya no está disponible"));

    QList<QByteArray> encoded;
    encoded.reserve(urls.size());
    GList *uris = nullptr;
    for (const QUrl &url : urls) {
        encoded.append(url.toEncoded());
        uris = g_list_append(uris, const_cast<char *>(encoded.last().constData()));
    }

    GError *gerror = nullptr;
    const gboolean ok = g_app_info_launch_uris(target, uris, nullptr, &gerror);
    g_list_free(uris);

    if (!ok) {
        const QString message = gerror ? QString::fromUtf8(gerror->message)
                                       : QCoreApplication::translate("Ooze", "No se pudo lanzar la aplicación");
        if (gerror)
            g_error_free(gerror);
        return fail(message);
    }

    return true;
}
