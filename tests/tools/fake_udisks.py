import asyncio
import os
import sys
from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method, dbus_property
from dbus_next.errors import DBusError
from dbus_next import PropertyAccess

MEDIA = os.environ.get('OOZE_FAKE_MEDIA', '/tmp/ooze-fake-media')
LOG = open(os.environ.get('OOZE_FAKE_LOG', os.devnull), 'w', buffering=1)
ROOT = '/org/freedesktop/UDisks2'
BLK = ROOT + '/block_devices/'
DRV = ROOT + '/drives/'

state = {
    'sda1': dict(label='Datos', mounts=[], drive='hdd', removable=False, eject=False, off=False, size=500_000_000_000),
    'sda2': dict(label='raiz', mounts=['/'], drive='hdd', removable=False, eject=False, off=False, size=100_000_000_000),
    'sdb1': dict(label='LLAVERO', mounts=[], drive='usb', removable=True, eject=True, off=True, size=16_000_000_000),
}
bus = None
fs_objects = {}

class Block(ServiceInterface):
    def __init__(self, dev):
        super().__init__('org.freedesktop.UDisks2.Block'); self.dev = dev
    @dbus_property(access=PropertyAccess.READ)
    def Device(self) -> 'ay': return b'/dev/' + self.dev.encode() + b'\0'
    @dbus_property(access=PropertyAccess.READ)
    def IdLabel(self) -> 's': return state[self.dev]['label']
    @dbus_property(access=PropertyAccess.READ)
    def IdType(self) -> 's': return 'ext4'
    @dbus_property(access=PropertyAccess.READ)
    def IdUsage(self) -> 's': return 'filesystem'
    @dbus_property(access=PropertyAccess.READ)
    def HintIgnore(self) -> 'b': return False
    @dbus_property(access=PropertyAccess.READ)
    def Size(self) -> 't': return state[self.dev]['size']
    @dbus_property(access=PropertyAccess.READ)
    def Drive(self) -> 'o': return DRV + state[self.dev]['drive']

def mps(dev): return [m.encode() + b'\0' for m in state[dev]['mounts']]

class Filesystem(ServiceInterface):
    def __init__(self, dev):
        super().__init__('org.freedesktop.UDisks2.Filesystem'); self.dev = dev
    @dbus_property(access=PropertyAccess.READ)
    def MountPoints(self) -> 'aay': return mps(self.dev)
    @method()
    def Mount(self, options: 'a{sv}') -> 's':
        d = state[self.dev]
        LOG.write(f'Mount {self.dev}\n')
        if d['mounts']:
            raise DBusError('org.freedesktop.UDisks2.Error.AlreadyMounted', 'ya montado')
        mount_point = os.path.join(MEDIA, d['label'])
        os.makedirs(mount_point, exist_ok=True)
        with open(os.path.join(mount_point, 'hola.txt'), 'w') as handle:
            handle.write('desde ' + d['label'])
        d['mounts'] = [mount_point]
        self.emit_properties_changed({'MountPoints': mps(self.dev)})
        return d['mounts'][0]
    @method()
    def Unmount(self, options: 'a{sv}'):
        LOG.write(f'Unmount {self.dev}\n')
        state[self.dev]['mounts'] = []
        self.emit_properties_changed({'MountPoints': []})

class Drive(ServiceInterface):
    def __init__(self, name):
        super().__init__('org.freedesktop.UDisks2.Drive'); self.dev = name
    def any(self, key):
        return any(d[key] for d in state.values() if d['drive'] == self.dev)
    @dbus_property(access=PropertyAccess.READ)
    def Removable(self) -> 'b': return self.any('removable')
    @dbus_property(access=PropertyAccess.READ)
    def Ejectable(self) -> 'b': return self.any('eject')
    @dbus_property(access=PropertyAccess.READ)
    def CanPowerOff(self) -> 'b': return self.any('off')
    @method()
    def Eject(self, options: 'a{sv}'): LOG.write(f'Eject {self.dev}\n')
    @method()
    def PowerOff(self, options: 'a{sv}'): LOG.write(f'PowerOff {self.dev}\n')

class Fake(ServiceInterface):
    def __init__(self): super().__init__('org.ooze.Fake')
    @method()
    def Remove(self, name: 's'):
        LOG.write(f'Remove {name}\n')
        state.pop(name, None)
        bus.unexport(BLK + name)

async def main(addr):
    global bus
    bus = await MessageBus(bus_address=addr).connect()
    bus.export('/fake', Fake())
    for dev in state:
        bus.export(BLK + dev, Block(dev))
        bus.export(BLK + dev, Filesystem(dev))
    for drv in {d['drive'] for d in state.values()}:
        bus.export(DRV + drv, Drive(drv))
    await bus.request_name('org.freedesktop.UDisks2')
    LOG.write('READY\n')
    await asyncio.get_event_loop().create_future()

asyncio.run(main(sys.argv[1]))
