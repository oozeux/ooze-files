#include "core/Devices.h"

#include <QSignalSpy>
#include <QTest>

class TestDevices : public QObject
{
    Q_OBJECT

private slots:
    void listsMountableFilesystems();
    void readsMountPointsAndDriveFlags();
    void hidesSystemAndInternalVolumes();
    void namesUnlabelledVolumesBySize();
    void ordersByName();
    void monitorWithoutBusIsEmpty();

private:
    static void addVolume(DbusObjects &objects, const QString &name, const QString &label,
                          const QList<QByteArray> &mountPoints, const QString &usage = {},
                          const QString &drive = {}, bool hintIgnore = false,
                          qulonglong size = 0);
    static void addDrive(DbusObjects &objects, const QString &name, bool removable, bool eject,
                         bool powerOff);
};

void TestDevices::addVolume(DbusObjects &objects, const QString &name, const QString &label,
                            const QList<QByteArray> &mountPoints, const QString &usage,
                            const QString &drive, bool hintIgnore, qulonglong size)
{
    QVariantMap block;
    block.insert(QStringLiteral("Device"), QByteArray("/dev/") + name.toUtf8() + '\0');
    block.insert(QStringLiteral("IdLabel"), label);
    block.insert(QStringLiteral("IdType"), QStringLiteral("ext4"));
    block.insert(QStringLiteral("IdUsage"), usage.isEmpty() ? QStringLiteral("filesystem") : usage);
    block.insert(QStringLiteral("HintIgnore"), hintIgnore);
    block.insert(QStringLiteral("Size"), size);
    block.insert(QStringLiteral("Drive"),
                 QVariant::fromValue(QDBusObjectPath(
                     drive.isEmpty() ? QStringLiteral("/") : drive)));

    QVariantMap filesystem;
    filesystem.insert(QStringLiteral("MountPoints"), QVariant::fromValue(mountPoints));

    DbusInterfaces interfaces;
    interfaces.insert(QStringLiteral("org.freedesktop.UDisks2.Block"), block);
    interfaces.insert(QStringLiteral("org.freedesktop.UDisks2.Filesystem"), filesystem);

    objects.insert(QDBusObjectPath(QStringLiteral("/org/freedesktop/UDisks2/block_devices/") + name),
                   interfaces);
}

void TestDevices::addDrive(DbusObjects &objects, const QString &name, bool removable, bool eject,
                           bool powerOff)
{
    QVariantMap drive;
    drive.insert(QStringLiteral("Removable"), removable);
    drive.insert(QStringLiteral("Ejectable"), eject);
    drive.insert(QStringLiteral("CanPowerOff"), powerOff);

    DbusInterfaces interfaces;
    interfaces.insert(QStringLiteral("org.freedesktop.UDisks2.Drive"), drive);
    objects.insert(QDBusObjectPath(QStringLiteral("/org/freedesktop/UDisks2/drives/") + name),
                   interfaces);
}

void TestDevices::listsMountableFilesystems()
{
    DbusObjects objects;
    addVolume(objects, QStringLiteral("sda1"), QStringLiteral("Datos"), {});

    const QVector<DeviceInfo> devices = Devices::parse(objects);
    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.first().device, QStringLiteral("/dev/sda1"));
    QCOMPARE(devices.first().displayName(), QStringLiteral("Datos"));
    QCOMPARE(devices.first().fsType, QStringLiteral("ext4"));
    QVERIFY(!devices.first().mounted());
    QVERIFY(!devices.first().removable);
}

void TestDevices::readsMountPointsAndDriveFlags()
{
    DbusObjects objects;
    addDrive(objects, QStringLiteral("usb"), true, true, true);
    addVolume(objects, QStringLiteral("sdb1"), QStringLiteral("LLAVERO"),
              {QByteArray("/run/media/yo/LLAVERO\0", 22)}, {},
              QStringLiteral("/org/freedesktop/UDisks2/drives/usb"));

    const QVector<DeviceInfo> devices = Devices::parse(objects);
    QCOMPARE(devices.size(), 1);

    const DeviceInfo &usb = devices.first();
    QVERIFY(usb.mounted());
    QCOMPARE(usb.mountPoints, QStringList{QStringLiteral("/run/media/yo/LLAVERO")});
    QVERIFY(usb.removable);
    QVERIFY(usb.canEject);
    QVERIFY(usb.canPowerOff);
    QCOMPARE(usb.driveObject, QStringLiteral("/org/freedesktop/UDisks2/drives/usb"));
}

void TestDevices::hidesSystemAndInternalVolumes()
{
    DbusObjects objects;
    addVolume(objects, QStringLiteral("sda2"), QStringLiteral("raiz"), {QByteArray("/")});
    addVolume(objects, QStringLiteral("sda1"), QStringLiteral("EFI"), {QByteArray("/boot/efi")});
    addVolume(objects, QStringLiteral("sda3"), QStringLiteral("store"), {QByteArray("/nix/store")});
    addVolume(objects, QStringLiteral("sdc1"), QStringLiteral("oculto"), {}, {}, {}, true);
    addVolume(objects, QStringLiteral("sdd1"), QStringLiteral("cifrado"), {}, QStringLiteral("crypto"));
    addVolume(objects, QStringLiteral("loop0"), QStringLiteral("snap"), {});
    addVolume(objects, QStringLiteral("zram0"), QStringLiteral("zram"), {});
    addVolume(objects, QStringLiteral("sde1"), QStringLiteral("visible"), {});

    QVariantMap swap;
    swap.insert(QStringLiteral("Device"), QByteArray("/dev/sda4\0", 10));
    swap.insert(QStringLiteral("IdUsage"), QStringLiteral("other"));
    DbusInterfaces onlyBlock;
    onlyBlock.insert(QStringLiteral("org.freedesktop.UDisks2.Block"), swap);
    objects.insert(QDBusObjectPath(QStringLiteral("/org/freedesktop/UDisks2/block_devices/sda4")),
                   onlyBlock);

    const QVector<DeviceInfo> devices = Devices::parse(objects);
    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.first().displayName(), QStringLiteral("visible"));
}

void TestDevices::namesUnlabelledVolumesBySize()
{
    DbusObjects objects;
    addVolume(objects, QStringLiteral("sdf1"), QString(), {}, {}, {}, false, 32ULL * 1000 * 1000 * 1000);
    addVolume(objects, QStringLiteral("sdg1"), QStringLiteral("   "), {});

    const QVector<DeviceInfo> devices = Devices::parse(objects);
    QCOMPARE(devices.size(), 2);

    QStringList names;
    for (const DeviceInfo &device : devices)
        names.append(device.displayName());

    QVERIFY(names.contains(QStringLiteral("/dev/sdg1")));
    QVERIFY(std::any_of(names.cbegin(), names.cend(), [](const QString &name) {
        return name.startsWith(QStringLiteral("Volumen de "));
    }));
}

void TestDevices::ordersByName()
{
    DbusObjects objects;
    addVolume(objects, QStringLiteral("sda1"), QStringLiteral("Zeta"), {});
    addVolume(objects, QStringLiteral("sdb1"), QStringLiteral("alfa"), {});
    addVolume(objects, QStringLiteral("sdc1"), QStringLiteral("Beta"), {});

    const QVector<DeviceInfo> devices = Devices::parse(objects);
    QCOMPARE(devices.size(), 3);
    QCOMPARE(devices.at(0).displayName(), QStringLiteral("alfa"));
    QCOMPARE(devices.at(1).displayName(), QStringLiteral("Beta"));
    QCOMPARE(devices.at(2).displayName(), QStringLiteral("Zeta"));
}

void TestDevices::monitorWithoutBusIsEmpty()
{
    DeviceMonitor monitor(QDBusConnection(QStringLiteral("ooze-test-no-connection")));
    monitor.start();
    QVERIFY(!monitor.available());
    QVERIFY(monitor.devices().isEmpty());

    QSignalSpy errors(&monitor, &DeviceMonitor::errorOccurred);
    monitor.mount(QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdz1"));
    QCOMPARE(errors.count(), 1);
}

QTEST_GUILESS_MAIN(TestDevices)
#include "tst_devices.moc"
