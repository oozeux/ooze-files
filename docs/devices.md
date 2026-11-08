# Dispositivos y lugares propios

Documenta `src/core/Devices.{h,cpp}`, `src/core/Places.cpp` y la barra lateral.

## Dispositivos (UDisks2)

`DeviceMonitor` habla con `org.freedesktop.UDisks2` en el bus del sistema usando
QtDBus (ya es dependencia por la instancia única). No usa `gvfs` ni GIO para
esto, de modo que funciona igual con o sin un entorno de escritorio.

| Función | Llamada D-Bus |
|---|---|
| Listar | `ObjectManager.GetManagedObjects` |
| Montar | `Filesystem.Mount` |
| Desmontar | `Filesystem.Unmount` |
| Expulsar | desmontar y después `Drive.Eject`, o `Drive.PowerOff` si la unidad lo admite y no se puede expulsar |

La lista se reconstruye, con un retardo de 200 ms, cuando llegan
`InterfacesAdded`, `InterfacesRemoved` o `PropertiesChanged`, y solo se notifica
a QML si algo ha cambiado.

`Devices::parse` es una función pura sobre los datos de `GetManagedObjects` y
tiene sus propios tests. Un volumen aparece si:

- tiene las interfaces `Block` y `Filesystem` y `IdUsage` es `filesystem`;
- no tiene `HintIgnore`;
- no es un dispositivo `loop`, `ram` o `zram`;
- no está montado en el sistema (`/`, `/boot*`, `/nix*`, `/var*`, `/usr*`,
  `/etc*`, `/sys*`, `/proc*`, `/dev*`).

Se ordenan por nombre sin distinguir mayúsculas. Sin etiqueta se muestra
«Volumen de 32 GB» o, en su defecto, el nombre del dispositivo.

## Barra lateral

La sección «DISPOSITIVOS» aparece solo si hay alguno.

- Clic: monta si hace falta y navega al punto de montaje.
- Botón de expulsar (solo montados): expulsa las unidades extraíbles y
  desmonta las demás.
- Clic derecho: Montar y abrir, Abrir, Desmontar, Expulsar.
- Se puede soltar archivos sobre un dispositivo montado.
- Si no hay bus del sistema o UDisks2 no está activo, la sección no aparece y no
  se muestra ningún error.

La autorización de operaciones que lo requieran la gestiona polkit; si se
rechaza se muestra «No se obtuvo autorización para esta operación».

## Estado visual

Los dispositivos sin montar se dibujan con icono y nombre atenuados
(`Theme.inactiveOpacity`) y el nombre en cursiva, para distinguirlos de los
montados, que además muestran el botón de expulsar.

## Lugares propios

Se guardan en la configuración (`places`, ver [configuration.md](configuration.md))
y se muestran en «LUGARES» tras los de XDG.

- «Añadir a Lugares» en el menú contextual de una carpeta.
- «Añadir esta carpeta a Lugares» en el menú del fondo.
- Clic derecho en un lugar propio: «Quitar de Lugares».

## Probar sin discos reales

`tests/tools/fake_udisks.py` es un UDisks2 de pega con tres volúmenes (`Datos`,
`raiz` —oculto por ser del sistema— y `LLAVERO`, extraíble). Requiere `dbus-next`
y un bus privado:

```sh
ADDR=$(dbus-daemon --session --fork --print-address)
OOZE_FAKE_MEDIA=/tmp/ooze-media python3 tests/tools/fake_udisks.py "$ADDR" &
DBUS_SYSTEM_BUS_ADDRESS="$ADDR" ./run.sh
```

Montar `LLAVERO` crea `/tmp/ooze-media/LLAVERO` con un archivo de ejemplo.
`OOZE_FAKE_LOG` guarda en un fichero las llamadas recibidas.
