#include "app/IconTheme.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {

QStringList iconRoots()
{
    QStringList roots;

    roots << QDir::homePath() + QStringLiteral("/.local/share/icons")
          << QDir::homePath() + QStringLiteral("/.icons")
          << QStringLiteral("/run/current-system/sw/share/icons")
          << QStringLiteral("/usr/local/share/icons")
          << QStringLiteral("/usr/share/icons");

    const QStringList dataDirs =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dir : dataDirs)
        roots << dir + QStringLiteral("/icons");

    return roots;
}

bool themeExists(const QStringList &searchPaths, const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral("hicolor"))
        return false;

    for (const QString &root : searchPaths) {
        if (QFileInfo::exists(root + QLatin1Char('/') + name + QStringLiteral("/index.theme")))
            return true;
    }

    return false;
}

QString kdeThemeName()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (config.isEmpty())
        return {};

    QSettings settings(config + QStringLiteral("/kdeglobals"), QSettings::IniFormat);
    return settings.value(QStringLiteral("Icons/Theme")).toString();
}

QString gtkThemeName()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (config.isEmpty())
        return {};

    QSettings settings(config + QStringLiteral("/gtk-3.0/settings.ini"), QSettings::IniFormat);
    return settings.value(QStringLiteral("Settings/gtk-icon-theme-name")).toString();
}

QString qt6ctThemeName()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (config.isEmpty())
        return {};

    QSettings settings(config + QStringLiteral("/qt6ct/qt6ct.conf"), QSettings::IniFormat);
    return settings.value(QStringLiteral("Interface/icon_theme")).toString();
}

QString platformThemeName()
{
    static const QString cached = [] {
        const QString name = QIcon::themeName();
        return name == QLatin1String("hicolor") ? QString() : name;
    }();

    return cached;
}

QString darkVariant(const QStringList &searchPaths, const QString &name)
{
    if (name.endsWith(QStringLiteral("-dark")) || name.endsWith(QStringLiteral("-Dark")))
        return {};

    for (const QString &suffix : {QStringLiteral("-dark"), QStringLiteral("-Dark")}) {
        const QString candidate = name + suffix;
        if (themeExists(searchPaths, candidate))
            return candidate;
    }

    return {};
}

QString pickTheme(const QStringList &searchPaths, const QStringList &preferred, bool dark)
{
    for (const QString &name : preferred) {
        if (!themeExists(searchPaths, name))
            continue;

        if (dark) {
            const QString variant = darkVariant(searchPaths, name);
            if (!variant.isEmpty())
                return variant;
        }

        return name;
    }

    return {};
}

}

namespace IconTheme {

void apply(bool dark, const QString &requested)
{
    QStringList paths = QIcon::themeSearchPaths();
    const QStringList extraRoots = iconRoots();
    for (qsizetype i = extraRoots.size() - 1; i >= 0; --i) {
        const QString &root = extraRoots.at(i);
        if (QFileInfo(root).isDir() && !paths.contains(root))
            paths.prepend(root);
    }
    QIcon::setThemeSearchPaths(paths);

    if (!requested.isEmpty() && themeExists(paths, requested)) {
        QIcon::setThemeName(requested);
        return;
    }

    QStringList preferred;
    const QString platformName = platformThemeName();
    if (!platformName.isEmpty())
        preferred << platformName;

    for (const QString &name : {qt6ctThemeName(), kdeThemeName(), gtkThemeName()}) {
        if (!name.isEmpty() && !preferred.contains(name))
            preferred << name;
    }
    preferred << QStringLiteral("Papirus") << QStringLiteral("breeze")
              << QStringLiteral("Adwaita") << QStringLiteral("Papirus-Dark")
              << QStringLiteral("breeze-dark") << QStringLiteral("Adwaita-dark");

    const QString chosen = pickTheme(paths, preferred, dark);
    QIcon::setThemeName(chosen.isEmpty() ? QStringLiteral("hicolor") : chosen);
}

}
