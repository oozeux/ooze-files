#include "theme/PortalSettings.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QVariantList>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPortal, "ooze.files.portal")

namespace {

constexpr auto kService = "org.freedesktop.portal.Desktop";
constexpr auto kPath = "/org/freedesktop/portal/desktop";
constexpr auto kInterface = "org.freedesktop.portal.Settings";
constexpr auto kNamespace = "org.freedesktop.appearance";

QVariant unwrap(QVariant value)
{
    while (value.metaType().id() == qMetaTypeId<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

bool isUnit(double value)
{
    return value >= 0.0 && value <= 1.0;
}

bool parseAccent(const QVariant &raw, QColor *color)
{
    const QVariant value = unwrap(raw);

    if (value.metaType().id() == QMetaType::QColor) {
        const QColor parsed = value.value<QColor>();
        if (!parsed.isValid())
            return false;
        *color = parsed;
        return true;
    }

    if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        QDBusArgument argument = value.value<QDBusArgument>();
        if (argument.currentSignature() != QLatin1String("(ddd)"))
            return false;

        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        argument.beginStructure();
        argument >> red >> green >> blue;
        argument.endStructure();

        if (!isUnit(red) || !isUnit(green) || !isUnit(blue))
            return false;

        *color = QColor::fromRgbF(red, green, blue);
        return true;
    }

    if (value.canConvert<QVariantList>()) {
        const QVariantList list = value.toList();
        if (list.size() != 3)
            return false;

        const double red = list.at(0).toDouble();
        const double green = list.at(1).toDouble();
        const double blue = list.at(2).toDouble();
        if (!isUnit(red) || !isUnit(green) || !isUnit(blue))
            return false;

        *color = QColor::fromRgbF(red, green, blue);
        return true;
    }

    return false;
}

} // namespace

PortalSettings::PortalSettings(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    const bool subscribed = bus.connect(QString::fromLatin1(kService),
                                        QString::fromLatin1(kPath),
                                        QString::fromLatin1(kInterface),
                                        QStringLiteral("SettingChanged"), this,
                                        SLOT(onSettingChanged(QString, QString, QDBusVariant)));
    if (!subscribed)
        qCDebug(lcPortal) << "No se pudo escuchar SettingChanged del portal";

    read(QStringLiteral("accent-color"));
    read(QStringLiteral("color-scheme"));
}

void PortalSettings::read(const QString &key)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kService), QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface), QStringLiteral("Read"));
    message << QString::fromLatin1(kNamespace) << key;

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 3000), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, key](QDBusPendingCallWatcher *call) {
                call->deleteLater();

                const QDBusMessage reply = call->reply();
                if (reply.type() != QDBusMessage::ReplyMessage) {
                    qCDebug(lcPortal) << "El portal no ofrece" << key << ":"
                                      << reply.errorMessage();
                    return;
                }

                const QVariantList arguments = reply.arguments();
                if (arguments.isEmpty())
                    return;

                applyValue(key, arguments.constFirst());
            });
}

void PortalSettings::onSettingChanged(const QString &nameSpace, const QString &key,
                                      const QDBusVariant &value)
{
    if (nameSpace != QLatin1String(kNamespace))
        return;

    applyValue(key, value.variant());
}

void PortalSettings::applyValue(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("accent-color")) {
        QColor accent;
        if (!parseAccent(value, &accent) || accent == m_accent)
            return;

        m_accent = accent;
        emit accentChanged(accent);
        return;
    }

    if (key == QLatin1String("color-scheme")) {
        const int scheme = int(unwrap(value).toUInt());
        if (scheme < 0 || scheme > 2 || scheme == m_colorScheme)
            return;

        m_colorScheme = scheme;
        emit colorSchemeChanged(scheme);
    }
}
