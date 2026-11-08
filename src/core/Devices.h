#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct DeviceInfo
{
    QString objectPath;
    QString driveObject;
    QString device;
    QString label;
    QString fsType;
    QStringList mountPoints;
    qulonglong size = 0;
    bool removable = false;
    bool canEject = false;
    bool canPowerOff = false;

    [[nodiscard]] bool mounted() const { return !mountPoints.isEmpty(); }
    [[nodiscard]] QString displayName() const;
};

using DbusInterfaces = QMap<QString, QVariantMap>;
using DbusObjects = QMap<QDBusObjectPath, DbusInterfaces>;

Q_DECLARE_METATYPE(DbusInterfaces)
Q_DECLARE_METATYPE(DbusObjects)

namespace Devices {

QVector<DeviceInfo> parse(const DbusObjects &objects);

}

class DeviceMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DeviceMonitor(const QDBusConnection &bus, QObject *parent = nullptr);
    explicit DeviceMonitor(QObject *parent = nullptr);

    void start();

    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] QVariantList devices() const;

    void mount(const QString &objectPath);
    void unmount(const QString &objectPath);
    void eject(const QString &objectPath);

signals:
    void devicesChanged();
    void mounted(const QString &objectPath, const QString &mountPoint);
    void finished(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void scheduleRefresh();

private:
    void refresh();
    void callUnmount(const DeviceInfo &device, bool thenEject);
    void callDriveAction(const DeviceInfo &device);
    [[nodiscard]] DeviceInfo find(const QString &objectPath) const;

    QDBusConnection m_bus;
    QVector<DeviceInfo> m_devices;
    QTimer m_refreshTimer;
    bool m_available = false;
    bool m_started = false;
};
