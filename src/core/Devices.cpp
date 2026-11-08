#include "core/Devices.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QLocale>
#include <QUrl>

#include <algorithm>
#include <QCoreApplication>

namespace {

const QString kService = QStringLiteral("org.freedesktop.UDisks2");
const QString kRoot = QStringLiteral("/org/freedesktop/UDisks2");
const QString kBlockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString kFilesystemIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString kDriveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString kObjectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");

QByteArray trimNul(QByteArray bytes)
{
    while (!bytes.isEmpty() && bytes.back() == '\0')
        bytes.chop(1);
    return bytes;
}

QByteArray bytesOf(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QByteArray>())
        return trimNul(value.toByteArray());

    if (value.canConvert<QDBusArgument>())
        return trimNul(qdbus_cast<QByteArray>(value.value<QDBusArgument>()));

    return {};
}

QStringList mountPointsOf(const QVariant &value)
{
    QList<QByteArray> raw;

    if (value.canConvert<QDBusArgument>()) {
        raw = qdbus_cast<QList<QByteArray>>(value.value<QDBusArgument>());
    } else if (value.metaType() == QMetaType::fromType<QList<QByteArray>>()) {
        raw = value.value<QList<QByteArray>>();
    } else if (value.metaType() == QMetaType::fromType<QVariantList>()) {
        const QVariantList list = value.toList();
        for (const QVariant &item : list)
            raw.append(item.toByteArray());
    }

    QStringList result;
    for (const QByteArray &entry : std::as_const(raw)) {
        const QByteArray clean = trimNul(entry);
        if (!clean.isEmpty())
            result.append(QString::fromUtf8(clean));
    }
    return result;
}

QString pathOf(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value.toString();
}

bool isSystemMount(const QString &mountPoint)
{
    static const QStringList prefixes = {
        QStringLiteral("/boot"), QStringLiteral("/nix"), QStringLiteral("/var"),
        QStringLiteral("/usr"),  QStringLiteral("/etc"), QStringLiteral("/sys"),
        QStringLiteral("/proc"), QStringLiteral("/dev"),
    };

    if (mountPoint == QLatin1String("/"))
        return true;

    for (const QString &prefix : prefixes) {
        if (mountPoint == prefix || mountPoint.startsWith(prefix + QLatin1Char('/')))
            return true;
    }
    return false;
}

void registerTypes()
{
    static const bool done = [] {
        qDBusRegisterMetaType<DbusInterfaces>();
        qDBusRegisterMetaType<DbusObjects>();
        return true;
    }();
    Q_UNUSED(done)
}

QString friendlyError(const QDBusError &error)
{
    const QString name = error.name();
    if (name.endsWith(QLatin1String("NotAuthorized"))
        || name.endsWith(QLatin1String("NotAuthorizedCanObtain"))
        || name.endsWith(QLatin1String("NotAuthorizedDismissed"))) {
        return QCoreApplication::translate("Ooze", "No se obtuvo autorización para esta operación");
    }

    if (name.endsWith(QLatin1String("DeviceBusy")))
        return QCoreApplication::translate("Ooze", "El dispositivo está en uso");

    return error.message().isEmpty() ? name : error.message();
}

}

QString DeviceInfo::displayName() const
{
    if (!label.trimmed().isEmpty())
        return label.trimmed();

    if (size > 0) {
        return QCoreApplication::translate("Ooze", "Volumen de %1").arg(QLocale().formattedDataSize(qint64(size), 0));
    }

    return device.isEmpty() ? objectPath.section(QLatin1Char('/'), -1) : device;
}

QVector<DeviceInfo> Devices::parse(const DbusObjects &objects)
{
    QVector<DeviceInfo> devices;

    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const DbusInterfaces &interfaces = it.value();
        if (!interfaces.contains(kFilesystemIface) || !interfaces.contains(kBlockIface))
            continue;

        const QVariantMap block = interfaces.value(kBlockIface);
        if (block.value(QStringLiteral("HintIgnore")).toBool())
            continue;

        if (block.value(QStringLiteral("IdUsage")).toString() != QLatin1String("filesystem"))
            continue;

        DeviceInfo info;
        info.objectPath = it.key().path();
        info.device = QString::fromUtf8(bytesOf(block.value(QStringLiteral("Device"))));

        if (info.device.startsWith(QLatin1String("/dev/loop"))
            || info.device.startsWith(QLatin1String("/dev/ram"))
            || info.device.startsWith(QLatin1String("/dev/zram")))
            continue;

        info.label = block.value(QStringLiteral("IdLabel")).toString();
        info.fsType = block.value(QStringLiteral("IdType")).toString();
        info.size = block.value(QStringLiteral("Size")).toULongLong();
        info.mountPoints = mountPointsOf(
            interfaces.value(kFilesystemIface).value(QStringLiteral("MountPoints")));

        const bool system = std::any_of(info.mountPoints.cbegin(), info.mountPoints.cend(),
                                        isSystemMount);
        if (system)
            continue;

        const QString drivePath = pathOf(block.value(QStringLiteral("Drive")));
        if (!drivePath.isEmpty() && drivePath != QLatin1String("/")) {
            info.driveObject = drivePath;
            const auto drive = objects.constFind(QDBusObjectPath(drivePath));
            if (drive != objects.constEnd() && drive->contains(kDriveIface)) {
                const QVariantMap props = drive->value(kDriveIface);
                info.removable = props.value(QStringLiteral("Removable")).toBool()
                    || props.value(QStringLiteral("MediaRemovable")).toBool();
                info.canEject = props.value(QStringLiteral("Ejectable")).toBool();
                info.canPowerOff = props.value(QStringLiteral("CanPowerOff")).toBool();
            }
        }

        devices.append(info);
    }

    std::sort(devices.begin(), devices.end(), [](const DeviceInfo &a, const DeviceInfo &b) {
        const int byName = a.displayName().compare(b.displayName(), Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a.device < b.device;
    });

    return devices;
}

DeviceMonitor::DeviceMonitor(QObject *parent)
    : DeviceMonitor(QDBusConnection::systemBus(), parent)
{
}

DeviceMonitor::DeviceMonitor(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    registerTypes();

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(200);
    connect(&m_refreshTimer, &QTimer::timeout, this, &DeviceMonitor::refresh);
}

void DeviceMonitor::start()
{
    if (m_started)
        return;
    m_started = true;

    if (!m_bus.isConnected())
        return;

    m_bus.connect(kService, kRoot, kObjectManager, QStringLiteral("InterfacesAdded"), this,
                  SLOT(scheduleRefresh()));
    m_bus.connect(kService, kRoot, kObjectManager, QStringLiteral("InterfacesRemoved"), this,
                  SLOT(scheduleRefresh()));
    m_bus.connect(kService, QString(), QStringLiteral("org.freedesktop.DBus.Properties"),
                  QStringLiteral("PropertiesChanged"), this, SLOT(scheduleRefresh()));

    refresh();
}

QVariantList DeviceMonitor::devices() const
{
    QVariantList list;
    for (const DeviceInfo &device : m_devices) {
        QVariantMap entry;
        entry.insert(QStringLiteral("objectPath"), device.objectPath);
        entry.insert(QStringLiteral("name"), device.displayName());
        entry.insert(QStringLiteral("device"), device.device);
        entry.insert(QStringLiteral("fsType"), device.fsType);
        entry.insert(QStringLiteral("mounted"), device.mounted());
        entry.insert(QStringLiteral("mountPoint"),
                     device.mounted() ? device.mountPoints.first() : QString());
        entry.insert(QStringLiteral("mountUrl"),
                     device.mounted() ? QUrl::fromLocalFile(device.mountPoints.first()).toString()
                                      : QString());
        entry.insert(QStringLiteral("removable"), device.removable);
        entry.insert(QStringLiteral("size"), device.size);
        entry.insert(QStringLiteral("icon"),
                     device.removable ? QStringLiteral("drive-removable-media")
                                      : QStringLiteral("drive-harddisk"));
        list.append(entry);
    }
    return list;
}

DeviceInfo DeviceMonitor::find(const QString &objectPath) const
{
    for (const DeviceInfo &device : m_devices) {
        if (device.objectPath == objectPath)
            return device;
    }
    return {};
}

void DeviceMonitor::scheduleRefresh()
{
    m_refreshTimer.start();
}

void DeviceMonitor::refresh()
{
    if (!m_bus.isConnected())
        return;

    QDBusMessage call = QDBusMessage::createMethodCall(kService, kRoot, kObjectManager,
                                                       QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 10000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *self) {
                const QDBusPendingReply<DbusObjects> reply = *self;
                self->deleteLater();

                if (reply.isError()) {
                    if (m_available) {
                        m_available = false;
                        m_devices.clear();
                        emit devicesChanged();
                    }
                    return;
                }

                m_available = true;
                const QVector<DeviceInfo> parsed = Devices::parse(reply.value());

                bool same = parsed.size() == m_devices.size();
                for (int i = 0; same && i < parsed.size(); ++i) {
                    same = parsed.at(i).objectPath == m_devices.at(i).objectPath
                        && parsed.at(i).mountPoints == m_devices.at(i).mountPoints
                        && parsed.at(i).label == m_devices.at(i).label;
                }

                if (same)
                    return;

                m_devices = parsed;
                emit devicesChanged();
            });
}

void DeviceMonitor::mount(const QString &objectPath)
{
    const DeviceInfo device = find(objectPath);
    if (device.objectPath.isEmpty()) {
        emit errorOccurred(QCoreApplication::translate("Ooze", "El dispositivo ya no está disponible"));
        return;
    }

    if (device.mounted()) {
        emit mounted(objectPath, device.mountPoints.first());
        return;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(kService, objectPath, kFilesystemIface,
                                                       QStringLiteral("Mount"));
    call << QVariantMap();

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 60000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, objectPath](QDBusPendingCallWatcher *self) {
                const QDBusPendingReply<QString> reply = *self;
                self->deleteLater();

                if (reply.isError()) {
                    if (reply.error().name().endsWith(QLatin1String("AlreadyMounted"))) {
                        refresh();
                        return;
                    }
                    emit errorOccurred(friendlyError(reply.error()));
                    return;
                }

                refresh();
                emit mounted(objectPath, reply.value());
            });
}

void DeviceMonitor::unmount(const QString &objectPath)
{
    const DeviceInfo device = find(objectPath);
    if (device.objectPath.isEmpty() || !device.mounted())
        return;

    callUnmount(device, false);
}

void DeviceMonitor::eject(const QString &objectPath)
{
    const DeviceInfo device = find(objectPath);
    if (device.objectPath.isEmpty())
        return;

    if (device.mounted())
        callUnmount(device, true);
    else
        callDriveAction(device);
}

void DeviceMonitor::callUnmount(const DeviceInfo &device, bool thenEject)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kService, device.objectPath, kFilesystemIface,
                                                       QStringLiteral("Unmount"));
    call << QVariantMap();

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 60000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, device, thenEject](QDBusPendingCallWatcher *self) {
                const QDBusPendingReply<> reply = *self;
                self->deleteLater();

                if (reply.isError() && !reply.error().name().endsWith(QLatin1String("NotMounted"))) {
                    emit errorOccurred(friendlyError(reply.error()));
                    return;
                }

                refresh();
                if (thenEject)
                    callDriveAction(device);
                else
                    emit finished(QCoreApplication::translate("Ooze", "«%1» desmontado").arg(device.displayName()));
            });
}

void DeviceMonitor::callDriveAction(const DeviceInfo &device)
{
    if (device.driveObject.isEmpty() || (!device.canEject && !device.canPowerOff)) {
        emit finished(QCoreApplication::translate("Ooze", "«%1» desmontado").arg(device.displayName()));
        return;
    }

    const QString method = device.canEject ? QStringLiteral("Eject") : QStringLiteral("PowerOff");
    QDBusMessage call = QDBusMessage::createMethodCall(kService, device.driveObject, kDriveIface,
                                                       method);
    call << QVariantMap();

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 60000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, device](QDBusPendingCallWatcher *self) {
                const QDBusPendingReply<> reply = *self;
                self->deleteLater();

                refresh();
                if (reply.isError()) {
                    emit errorOccurred(friendlyError(reply.error()));
                    return;
                }

                emit finished(QCoreApplication::translate("Ooze", "«%1» se puede retirar con seguridad")
                                  .arg(device.displayName()));
            });
}
