#include "app/SingleInstance.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QUrl>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcInstance, "ooze.files.instance")

namespace {
const QString kService = QStringLiteral("org.ooze.Files");
const QString kPath = QStringLiteral("/org/ooze/Files");
const QString kInterface = QStringLiteral("org.ooze.Files");
}

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

QString SingleInstance::serviceName()
{
    return kService;
}

QString SingleInstance::objectPath()
{
    return kPath;
}

bool SingleInstance::tryAcquire()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcInstance) << "No hay bus de sesión; se ejecuta sin instancia única";
        return true;
    }

    if (!bus.registerService(kService)) {
        return false;
    }

    if (!bus.registerObject(kPath, this, QDBusConnection::ExportAllSlots)) {
        qCWarning(lcInstance) << "No se pudo exportar" << kPath;
        bus.unregisterService(kService);
        return false;
    }

    m_owner = true;
    return true;
}

bool SingleInstance::notifyExisting(const QUrl &url, const QString &selectName, Mode mode)
{
    QDBusInterface iface(kService, kPath, kInterface, QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        qCWarning(lcInstance) << "Instancia existente no accesible:" << iface.lastError().message();
        return false;
    }

    const QString target = url.toString();
    QString method = QStringLiteral("Open");

    if (!selectName.isEmpty() && mode == Mode::Open)
        method = QStringLiteral("OpenAndSelect");
    else if (mode == Mode::NewTab)
        method = QStringLiteral("NewTab");
    else if (mode == Mode::NewWindow)
        method = QStringLiteral("NewWindow");

    const QDBusMessage reply = iface.call(method, target);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(lcInstance) << "Fallo al reenviar petición:" << reply.errorMessage();
        return false;
    }

    return true;
}

void SingleInstance::Open(const QString &url)
{
    emit openRequested(QUrl(url), false);
}

void SingleInstance::OpenAndSelect(const QString &url)
{
    emit openRequested(QUrl(url), true);
}

void SingleInstance::NewTab(const QString &url)
{
    emit newTabRequested(QUrl(url));
}

void SingleInstance::NewWindow(const QString &url)
{
    emit newWindowRequested(QUrl(url));
}

void SingleInstance::Toggle()
{
    emit toggleRequested();
}
