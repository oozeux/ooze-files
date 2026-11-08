# Capa VFS

Todo acceso a archivos pasa por `VfsBackend`. La interfaz no sabe si una ruta es
local, un montaje MTP o un archivo comprimido: pide un backend al registro y usa
el contrato común.

## Backends incluidos

| Esquema | Backend | Uso |
|---|---|---|
| `file` | `LocalVfs` | carpetas locales |
| `recent` | `RecentVfs` | lista de recientes, solo lectura (ver [recents.md](recents.md)) |
| `trash` | `TrashVfs` | papelera freedesktop, solo lectura (ver [trash.md](trash.md)) |

Una ubicación virtual como `recent:///` no es un directorio: sus entradas llevan
la ruta real del archivo en `path` y `url`, de modo que abrir, copiar, arrastrar
y las propiedades funcionan sobre el archivo verdadero. Por eso `id` es la ruta
completa (el nombre puede repetirse entre carpetas) y los campos `location` y
`activity` de `VfsEntry` transportan la carpeta contenedora y la fecha de
actividad.

## Contrato

```cpp
class VfsBackend : public QObject
{
public:
    using ListCallback =
        std::function<void(QVector<VfsEntry> &&entries, const VfsError &error)>;

    virtual QString scheme() const = 0;
    virtual VfsCapabilities capabilities(const QUrl &url) const = 0;

    virtual void list(const QUrl &dir, const ListCallback &callback,
                      CancelToken &token) = 0;

    virtual VfsEntry stat(const QUrl &url, VfsError *error) = 0;
    virtual bool exists(const QUrl &url) = 0;
    virtual bool canHandle(const QUrl &url) const;
};
```

Reglas que todo backend debe cumplir:

1. **No lanzar excepciones** ni llamar a la UI: los errores viajan en
   `VfsError`.
2. **`list` es por lotes**. Cada llamada entrega hasta 500 entradas con
   `error.code == None`. El final se marca con una última llamada con vector
   vacío y el error final (`None` si fue bien, `Cancelled` si se canceló).
3. **`list` corre en un hilo de trabajo**. El callback se invoca desde ese
   hilo; `DirectoryLoader` reemite por señal al hilo principal. Un backend no
   debe tocar `QObject` de la UI desde `list`.
4. **Comprobar `CancelToken`**. Como mínimo cada 64 entradas; si se cancela, hay
   que parar cuanto antes y terminar con `Cancelled`.
5. **Rutas en bytes**. `VfsEntry::path` e `id` son `QByteArray`. `url` es la
   representación percent-codificada para cruzar a QML; `displayName` es solo
   para pintar.
6. **`stat` es síncrono y ligero**. Se llama desde el pool de stat, nunca desde
   el hilo principal.
7. **Capacidades honestas**: si `canWrite` es falso, la UI no debe ofrecer
   escritura. `LocalVfs` declara `canWatch = false` y `supportsTrash = false`
   porque la vigilancia y la papelera llegarán por GIO.

## Tipos

### `VfsEntry`

| Campo | Descripción |
| --- | --- |
| `id` | nombre relativo en bytes |
| `path` | ruta absoluta en bytes |
| `url` | URL exacta para QML y para D-Bus |
| `displayName` | nombre para pintar (puede ser lossy) |
| `kind` | `File`, `Directory`, `Symlink`, `Socket`, `Fifo`, `BlockDevice`, `CharDevice` |
| `size` | bytes; `-1` en carpetas o desconocido |
| `modified` | mtime |
| `mimeType` | MIME detectado |
| `iconName` | nombre de icono freedesktop |
| `suffix` | extensión en minúsculas |
| `flags` | máscara de `VfsFlag` |

### `VfsFlag`

`FlagHidden`, `FlagReadable`, `FlagWritable`, `FlagExecutable`,
`FlagSymlinkBroken`, `FlagMountPoint`, `FlagSymlinkDir`.

### `VfsError`

Códigos: `None`, `NotFound`, `PermissionDenied`, `NotADirectory`, `IoError`,
`Unsupported`, `Cancelled`, `Unknown`. `fromErrno()` traduce `ENOENT`, `EACCES`,
`EPERM` y `ENOTDIR`; el resto se convierte en `IoError` con `strerror`.

### `VfsCapabilities`

`canList`, `canRead`, `canWrite`, `canDelete`, `canRename`,
`canCreateDirectory`, `canWatch`, `canSearch`, `supportsTrash`.

## Registro

`VfsRegistry` mapea esquema → backend. Al construirse registra `LocalVfs` para
`file`. Una URL sin esquema se trata como local.

```cpp
VfsBackend *backendFor(const QUrl &url) const;  // nullptr si no hay backend
bool isAvailable(const QString &scheme) const;
QStringList schemes() const;
void registerBackend(VfsBackend *backend);      // toma la propiedad
```

Si `backendFor` devuelve `nullptr`, `DirectoryLoader` termina con
`Unsupported` y la barra de estado lo muestra.

## Backend local

`LocalVfs` usa POSIX directamente:

- `opendir`/`readdir` con `d_type`; si `d_type` es `DT_UNKNOWN`, se resuelve con
  `lstat`.
- Lotes de 500 entradas, cancelación cada 64.
- Icono aproximado por tipo y extensión en el listado, sin abrir el archivo. En
  los enlaces simbólicos se resuelve el destino con `stat`: si apunta a una
  carpeta se marca `FlagSymlinkDir` y usa el icono `folder-symlink`; si apunta a
  un archivo (o está roto) usa el icono de la extensión del propio nombre. En
  ambos casos la interfaz añade el emblema de enlace sobre el icono.
- `stat` con `lstat`, `access` para permisos, `stat` del destino para enlaces
  rotos o a carpeta y `QMimeDatabase` (solo por extensión, sin leer contenido)
  para MIME e icono.
- `exists` con `lstat`.

El backend no abre archivos ni sigue enlaces: la navegación a un enlace
simbólico a carpeta la resuelve el sistema al listar la ruta. Para el modelo de
datos, un enlace a carpeta cuenta como carpeta (`isDir`), de modo que se navega
dentro de él.

## Añadir un backend

1. Crear `src/vfs/MiBackend.{h,cpp}` heredando de `VfsBackend`.
2. Implementar `scheme`, `capabilities`, `list`, `stat`, `exists` y, si hace
   falta, `canHandle`.
3. Registrar en `VfsRegistry` (o exponer una función `registerBackends`).
4. Añadir la fuente a `ooze_core` en `CMakeLists.txt`.
5. Añadir pruebas en `tests/unit` con un directorio temporal o un mock.
6. Documentar sus límites en este archivo.

## Backends previstos

| Esquema | Origen | Notas |
| --- | --- | --- |
| `file` | POSIX | implementado |
| `mtp`/`gphoto2` | gvfs | fase 5; listado y copia vía GIO |
| `smb`/`sftp`/`dav` | gvfs | fase 5 |
| `archive` | libarchive | fase 6; montaje virtual de solo lectura |

La papelera (`trash://`) y los montajes se harán con GIO (`GFile`, `GVolumeMonitor`)
en lugar de reimplementar `freedesktop.org` a mano.
