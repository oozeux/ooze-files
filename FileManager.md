# Plan técnico: `ooze-files`, administrador de archivos para Wayland

> **Estado:** plan revisado. Sustituye a la versión anterior (2026-10).
> **Licencia:** GPL-3.0-or-later.
> **Stack decidido:** C++20 + Qt 6 / Qt Quick (QML) + CMake.
> **Objetivo de uso:** reemplazo diario de Nautilus / Dolphin / Thunar en escritorios Wayland minimalistas (Hyprland, Niri, MangoWM) y también en KDE Plasma y GNOME.

---

## 1. Objetivo

Crear un administrador de archivos **minimalista por defecto y completo cuando se necesite**, inspirado en la claridad visual de Nautilus y en las herramientas avanzadas de Dolphin.

Debe funcionar en Wayland sin depender de un compositor concreto. Tendrá integración opcional con OozeShell, pero seguirá siendo plenamente útil de forma independiente.

### 1.1 Principios

- **Minimalismo real, no estético.** La ventana principal no muestra botones permanentes para funciones de uso esporádico; todo lo demás vive en menús, en la paleta de comandos y en atajos. Objetivo: no más de ~6 controles visibles por defecto.
- **Nada de pérdida de datos.** Toda eliminación va a la papelera por defecto. El borrado permanente es una acción explícita y confirmada. Toda operación es deshacible cuando sea posible.
- **Arquitectura modular con contratos explícitos.** VFS, cola de operaciones, dispositivos, compresión, previsualización e integraciones son módulos separados con interfaces definidas; la UI no conoce `QFile` ni `unistd.h`.
- **Seguridad por diseño.** Rutas no confiables, archivos comprimidos hostiles y miniaturas de origen desconocido son entradas no confiables y se tratan como tal.
- **Rendimiento medible.** Presupuestos numéricos (§7) verificados con benchmarks, no impresiones subjetivas.
- **Portabilidad.** Sin APIs de compositor en el núcleo. Sin rutas fijas de distribución.

### 1.2 Alcance del proyecto

Este proyecto se plantea como **gestor principal de archivos del sistema**, no como un experimento. Eso implica que features como papelera, pestañas, miniaturas, dispositivos y "abrir con" son requisitos, no extras.

---

## 2. No objetivos (explícitos)

Escribirlo evita discusiones futuras y alcance creep:

| No objetivo | Motivo |
|---|---|
| Editor de particiones (formatear, redimensionar, crear) | Requiere flujos destructivos y validación que multiplican el riesgo. Se delega a GParted/KDE Partition Manager. |
| Helper privilegiado propio (escritura como root) | Es el diseño que genera CVEs. Se delega a `kio-admin` o al terminal. |
| Indexación global del disco por defecto | Coste de CPU/RAM/disco y privacidad. Búsqueda recursiva acotada y opcional. |
| Servidor de miniaturas propio | Se usa el spec XDG de thumbnailers. |
| Servidor de indexación propio | Fuera de alcance; se puede consumir `plocate`/`localsearch` como fuente opcional. |
| Sincronización en la nube | Es trabajo de clientes dedicados. Solo se navega lo que expone gvfs. |
| Terminal incrustada | Fase futura como mucho; la terminal externa es suficiente y más segura. |
| Soporte de Windows/macOS | Objetivo Linux/Wayland. |

---

## 3. Decisiones de arquitectura (ADR resumidos)

Cada decisión con su alternativa descartada. Formato corto aquí; los ADR completos vivirán en `docs/adr/`.

### ADR-001 — Lenguaje y toolkit: C++20 + Qt 6 / QML

- **Decidido:** C++20, Qt 6 (Qt Quick + Qt Quick Controls con estilo base propio), CMake.
- **Motivo:** integración nativa con QML (coherencia con OozeShell), sistema de modelos/vistas maduro, D-Bus y red en la misma librería, empaquetado Nix sencillo, ecosistema de temas y tipografías resuelto.
- **Descartado:** Rust + cxx-qt (core más seguro, pero ecosistema Qt mixto y más fricción al depurar QML); Slint/iced/GTK4 (rompe coherencia con OozeShell y obliga a reimplementar vistas virtualizadas).

### ADR-002 — Rutas como bytes en el núcleo, `QString` solo para presentar

- **Decidido:** el núcleo y la capa VFS usan `std::filesystem::path` / `QByteArray` (bytes) para identificar entradas. `QString` se genera en la frontera con QML.
- **Motivo:** Linux permite nombres con bytes no UTF-8. Usar `QString` como identificador corrompe nombres al renombrar o copiar (pérdida de datos silenciosa).
- **Descartado:** `QString` en todo el núcleo (simple, pero pierde archivos legítimos).
- **Consecuencia:** los roles expuestos a QML son `QString` "de presentación"; para operar se usa un `EntryId` opaco (índice/`QByteArray`) que la UI nunca interpreta.

### ADR-003 — Capa VFS obligatoria desde el día 1

- **Decidido:** toda la UI y los controladores hablan con `VfsBackend` (§5), nunca con `QFile` directamente.
- **Motivo:** sin esto, gvfs/MTP y la navegación dentro de archivos comprimidos obligan a reescribir vistas, modelos y operaciones.
- **Descartado:** implementación directa con `std::filesystem` y "abstraer después". Es la causa habitual de reescrituras a mitad de proyecto.

### ADR-004 — Híbrido: POSIX para local, GIO para trash/remoto/dispositivos

- **Decidido:**
  - `LocalVfs`: syscalls POSIX (`openat`, `O_NOFOLLOW`, `renameat`, `FICLONE`), máximo control de seguridad y rendimiento.
  - `GvfsVfs`: `GFile`/libgio para esquemas no locales (`smb`, `sftp`, `dav`, `mtp`, `recent`).
  - `TrashService`: `g_file_trash()` de GIO (implementa correctamente el spec freedesktop, incluida papelera por montaje).
  - `MountService`: `GVolumeMonitor` (usa UDisks2 por debajo) para montar/desmontar/expulsar.
  - `MimeService`: `g_content_type_guess` + `GAppInfo` para "abrir con" y MIME.
- **Motivo:** GIO resuelve gratis y correctamente las partes más tediosas (papelera multi-montaje, montaje de volúmenes, MTP, MIME) y aporta gvfs como backend remoto. Implementarlas a mano sería meses de trabajo con más bugs.
- **Descartado:** reimplementar el spec de papelera (error seguro pero muchos casos límite); UDisks2 directo para el MVP (más código y llamadas D-Bus propias para lo que `GVolumeMonitor` ya hace); KIO (duplicaría backend remoto y arrastra medio KDE).
- **Consecuencia:** dependencia de `glib`/`gio` (estable, disponible en todas las distros y en Nixpkgs). Se aísla en `src/system/gio/` para que sea reemplazable.

### ADR-005 — QML empaquetado con `qt_add_qml_module`

- **Decidido:** módulos QML con URI (`Ooze.Files`, `Ooze.Files.Components`, …), compilados y embebidos en recursos vía `qt_add_qml_module`.
- **Motivo:** arranque más rápido, sin `QML2_IMPORT_PATH` roto (típico problema en NixOS), sin dependencia de rutas de instalación.
- **Descartado:** cargar QML de disco en `qml/` (más cómodo para editar en caliente, pero frágil al empaquetar). Se mantiene una opción de CMake `OOZE_FILES_QML_FROM_DISK` solo para desarrollo.

### ADR-006 — Instancia única con D-Bus

- **Decidido:** `org.ooze.Files` en el bus de sesión, con métodos `Open(QUrl)`, `OpenAndSelect(QUrl)`, `NewWindow(QUrl)`, `NewTab(QUrl)`, `Toggle()`.
- **Motivo:** arranque instantáneo desde OozeShell/atajos, una sola ventana reutilizable, y API estable para integraciones.
- **Descartado:** siempre proceso nuevo (300 ms de arranque y 150 MB por invocación).

### ADR-007 — Registro de acciones único

- **Decidido:** toda acción (menú contextual, barra, paleta de comandos, atajos) se declara una vez en un `ActionRegistry`; los menús se generan a partir de él y el usuario puede ocultar/reordenar vía configuración (§10).
- **Motivo:** consistencia automática entre atajos, menús y paleta; una sola implementación por acción; personalización sin código.
- **Descartado:** menús escritos a mano en QML (deriva: el menú y el atajo dejan de coincidir).

### ADR-008 — Configuración en JSON, estado de sesión separado

- **Decidido:**
  - `~/.config/ooze-files/config.json` — editable a mano, con `"version"`, validado y con errores claros.
  - `~/.config/ooze-files/state.json` — estado de sesión (pestañas, orden, geometría, modo de vista por carpeta). No pensado para edición manual.
  - `~/.config/ooze-files/theme.json` — opcional, tokens de tema.
- **Motivo:** sin dependencias extra (Qt lee JSON nativo), recarga en caliente, y evita que el estado de sesión sobrescriba la configuración del usuario.
- **Descartado:** TOML (más agradable, pero añade dependencia y no aporta nada aquí); `QSettings` (formato opaco, malo para dotfiles y para Nix).

### ADR-009 — Licencia GPL-3.0-or-later

- **Decidido:** GPL-3.0-or-later.
- **Motivo:** Qt 6 bajo LGPLv3 es compatible; mantiene abierta la puerta a enlazar KIO en el futuro (GPL) si se reconsidera; alinea con el ecosistema de file managers libres.
- **Consecuencia:** distribución de binarios requiere ofrecer el código fuente. Se documentará en `README` y en el paquete Nix.

---

## 4. Stack y dependencias

| Componente | Uso | Obligatorio |
|---|---|---|
| **C++20** | Núcleo, operaciones, integraciones | Sí |
| **Qt 6 Base + Declarative (+ Quick Controls)** | UI, modelos, eventos, hilos | Sí |
| **CMake ≥ 3.21** | Build; `qt_add_qml_module`, `qt_add_translations` | Sí |
| **glib / gio 2.x** | Papelera, montajes, gvfs, MIME, `GAppInfo` | Sí |
| **Qt Test + qmltestrunner** | Tests unitarios, de integración y de QML | Sí (dev) |
| **libarchive** | Comprimir/descomprimir, listado de archivos | Feature `archives` |
| **gvfs (+ gvfs-mtp)** | Backends remotos y MTP | Feature `remote` (runtime) |
| **udisks2** | Solo detalles avanzados (SMART, expulsión fina) | Feature `udisks` (fase 4) |
| **Polkit (+ agente)** | Autorización de montaje | Runtime del sistema |
| **Nix / Nixpkgs** | Dev shell y empaquetado | Sí |

### 4.1 Flags de CMake

```
OOZE_FILES_WITH_ARCHIVES   (ON)   — libarchive
OOZE_FILES_WITH_REMOTE     (ON)   — gvfs/GIO no local
OOZE_FILES_WITH_UDISKS     (OFF)  — detalles avanzados vía UDisks2
OOZE_FILES_WITH_XDG_THUMBNAILERS (ON) — thumbnailers externos
OOZE_FILES_QML_FROM_DISK   (OFF)  — solo desarrollo
OOZE_FILES_ENABLE_BENCH    (OFF)
```

Cada flag debe producir un binario funcional: la ausencia de libarchive no rompe la navegación, solo deshabilita el menú correspondiente (con aviso de "no disponible en esta compilación"). Esto se verifica en CI.

### 4.2 Requisitos de runtime (documentar, no asumir)

- **Agente Polkit.** Sin agente de autenticación, montar/expulsar falla con `org.freedesktop.PolicyKit1.Error.NotAuthorized`. Es una causa habitual de "no funciona" en setups Wayland minimalistas. La app debe **detectar** la ausencia de agente y mostrar un mensaje accionable, no un error D-Bus crudo.
- **gvfs daemon.** En NixOS: `services.gvfs.enable = true` (y `services.udisks2.enable`, que ya viene activo por defecto). Sin `gvfsd` no hay `mtp://`, `smb://` ni `sftp://`.
- **thumbnailers.** Se detectan leyendo `/usr/share/thumbnailers/*.thumbnailer`; su ausencia degrada miniaturas de vídeo/PDF/Office a icono genérico.

---

## 5. Capa VFS (contrato central)

Todo acceso a archivos pasa por aquí. La UI recibe URLs con esquema, nunca rutas absolutas sueltas.

```cpp
struct VfsEntry {
    QByteArray  id;        // identificador opaco y estable dentro del backend
    QString     displayName;
    QUrl        url;
    EntryKind   kind;      // File, Directory, Symlink, Device, Socket, ...
    QString     mimeType;  // perezoso, puede estar vacío en el primer lote
    qint64      size;      // -1 si desconocido
    QDateTime   modified;  // inválido si desconocido
    FileFlags   flags;     // hidden, readable, writable, executable, isMountPoint, ...
};

struct VfsCapabilities {
    bool canList, canRead, canWrite, canDelete, canRename, canMove;
    bool canCreateDirectory, canSetPermissions, canWatch, canSearch;
    bool supportsTrash, supportsPartialRange;   // streaming parcial
};

class VfsBackend : public QObject {
public:
    virtual QString scheme() const = 0;                       // "file", "smb", "mtp", "archive"
    virtual VfsCapabilities capabilities(const QUrl&) const = 0;

    virtual void list(const QUrl& dir, ListSink& sink, CancelToken&) = 0;
    virtual void stat(const QUrl&, StatSink&, CancelToken&) = 0;
    virtual std::unique_ptr<QIODevice> openRead(const QUrl&, CancelToken&) = 0;
    virtual std::unique_ptr<QIODevice> openWrite(const QUrl&, CancelToken&) = 0;

    virtual void copy(const QUrl& from, const QUrl& to, OpContext&) = 0;
    virtual void move(const QUrl& from, const QUrl& to, OpContext&) = 0;
    virtual void remove(const QUrl&, OpContext&) = 0;   // borrado definitivo del backend
    virtual void createDirectory(const QUrl&, OpContext&) = 0;

    virtual bool watch(const QUrl& dir, WatchCallback) = 0;   // false si no soportado
    virtual QUrl  childOf(const QUrl& dir, const QByteArray& name) const = 0;
    virtual QUrl  parentOf(const QUrl&) const = 0;
};
```

Implementaciones:

| Backend | Esquemas | Notas |
|---|---|---|
| `LocalVfs` | `file` | POSIX directo. Reflink, `openat`/`O_NOFOLLOW`, `*at`. La ruta rápida. |
| `GvfsVfs` | `smb`, `sftp`, `dav`, `davs`, `mtp`, `gphoto2`, `recent`, `trash` | Envuelve `GFile` + `GFileMonitor`. Capacidades parciales (p. ej. MTP no soporta renombrar/permisos). |
| `ArchiveVfs` | `archive` | Solo lectura. Entrada: URL `file://` + ruta interna. Vive en memoria perezosamente, sin extraer. |
| `SearchVfs` | `search` | Vista virtual de resultados (nombre, ruta real). Solo lectura. |

**Reglas:**

1. La UI **consulta `capabilities()` antes de mostrar acciones**. Un backend de solo lectura oculta/deshabilita lo que no puede hacer. Nada de asumir que todo es local.
2. Toda operación larga acepta `CancelToken&` y reporta progreso vía `OpContext`.
3. **La papelera no es responsabilidad del backend**: `OperationQueue` enruta `DeleteMode::Trash` a `TrashService` (GIO) y `DeleteMode::Permanent` a `VfsBackend::remove`. Así la misma UI borra a papelera en local y en `smb` sin duplicar lógica.
4. Las URLs se construyen con `QUrl::fromLocalFile` y se comparan ya normalizadas; nunca con concatenación de cadenas.
5. Un backend que falla no tumba la app: error tipado (`VfsError` con `code`, `url`, mensaje traducible, `errno` si aplica).

---

## 6. Concurrencia, poblado y watchers

### 6.1 Pools separados

| Pool | Hilos | Uso |
|---|---|---|
| `opsPool` | 1–2 | Copiar/mover/eliminar/comprimir. Serializado por destino para evitar carreras. |
| `statPool` | 1–2 | `stat` por lotes para tamaño/fecha/MIME. |
| `thumbPool` | 2–4 | Miniaturas. |
| `listPool` | 1 por pestaña | `list()` del backend activo. |

Si comparten pool, una carpeta llena de vídeos congela una copia en curso. Los límites son configurables pero acotados.

### 6.2 Poblado de directorios en dos fases

Es la decisión de rendimiento más importante del proyecto.

1. **Fase A — nombres.** `readdir()` (o `GFile` enumeración) devuelve nombre + `d_type`. Cero `stat`. Se emiten al modelo en lotes de ~500, con icono por tipo inferido. La ventana ya responde en milisegundos.
2. **Fase B — metadatos.** Un trabajo de `statPool` recorre las entradas por lotes, **priorizando el rango visible** y pausándose cuando la pestaña está oculta o el usuario está haciendo scroll rápido. Los roles `size`, `modified`, `mimeType` se rellenan conforme llegan (con `dataChanged` por rango, no por fila).
3. **Cancelación.** Cambiar de carpeta cancela el listado y el `stat` pendientes. Nunca se debe dejar trabajo huérfano compitiendo por I/O.

Consecuencia de diseño: la vista de detalle debe tolerar valores "—" durante unos milisegundos. Preferible a un `stat` de 50.000 entradas bloqueando la UI.

### 6.3 Watchers

`QFileSystemWatcher` no es suficiente: en Linux = un inotify watch por directorio, no recursivo, no informa qué cambió, y `max_user_watches` se agota con pocas pestañas. Plan:

- Implementación propia sobre **inotify** (`IN_MODIFY|IN_CREATE|IN_DELETE|IN_MOVED_*|IN_ATTRIB|IN_DELETE_SELF|IN_MOVE_SELF`).
- **Debounce** de 150–250 ms y coalescencia: un evento en la carpeta visible dispara `readdir` + diff contra el modelo (no recarga completa si solo cambió un atributo).
- Errores manejados explícitamente: `ENOSPC` (límite de watches) → desactivar watchers y pasar a "recargar al recuperar foco"; `ENOSYS`/FS de red → no vigilar.
- Solo se vigila el directorio **visible**. Las pestañas de fondo se refrescan al activarse.
- Los `.part` y temporales propios se filtran para no generar eventos en bucle.

### 6.4 Operaciones transaccionales

- Escribir siempre a `<destino>.ooze-part` y `rename()` atómico al finalizar. Un fallo no deja un archivo a medias indistinguible del bueno.
- Copia entre sistemas de archivos distintos → `EXDEV` detectado y convertido en copiar+borrar con progreso.
- **Reflink** (`FICLONE`) en btrfs/XFS cuando origen y destino comparten FS: copia instantánea. Es la diferencia de rendimiento más visible frente a otros gestores.
- Borrado recursivo sin seguir symlinks, con `openat(dirfd, name, O_NOFOLLOW)` y `unlinkat` para evitar TOCTOU.
- Preservar metadatos (mtime, permisos) es opcional y configurable; por defecto sí para copias locales, no para extracciones de archivos comprimidos.
- Cada operación registra un `UndoEntry` (origen, destino, tipo) mientras sea deshacible.

---

## 7. Presupuestos de rendimiento

Se miden con `tests/bench/` sobre árboles sintéticos (10k y 100k entradas, locales y en `smb`). Regresión = superar el umbral en > 20 %.

| Métrica | Objetivo |
|---|---|
| Arranque en frío hasta primer frame (instancia nueva) | < 300 ms |
| Instancia única: `Open()` desde D-Bus hasta ventana interactiva | < 80 ms |
| RSS con 1 pestaña y 1.000 archivos | < 150 MB |
| RSS adicional por pestaña | < 20 MB |
| 10.000 entradas: primer lote visible | < 100 ms |
| 10.000 entradas: modelo completo con metadatos | < 800 ms |
| Scroll en vista de lista | 60 fps, sin hitch > 16 ms |
| Miniaturas | ≤ 4 concurrentes, caché ≤ 256 MB, cancelables |
| Copia local | ≥ 80 % del throughput de `cp`; reflink instantáneo en el mismo FS |
| Watcher idle | 0 % CPU; CPU < 1 % al recibir un evento |
| Memoria con 10 pestañas abiertas | < 300 MB |

Reglas derivadas, obligatorias:

- `ListView`/`GridView` con `reuseItems: true`. **Prohibido `Repeater`** en listas de archivos.
- Ningún `delegate` ejecuta `stat`, `exists()`, `iconProvider` síncrono ni expresiones de coste O(n).
- Nada de timers periódicos para refrescar; todo basado en eventos (watcher, señal, foco).
- Las animaciones se ejecutan solo al cambiar estado real, y respetan la preferencia de animaciones reducidas.

---

## 8. Seguridad: modelo de amenazas

### 8.1 Extracción de archivos comprimidos (vector principal)

Un archivo comprimido es **entrada no confiable**. Defensas obligatorias en `ArchiveVfs` y en `ArchiveManager`:

1. **Zip Slip / path traversal**: normalizar cada ruta de entrada y **rechazar** las que salgan del destino (`..`, rutas absolutas, `C:\`, nombres vacíos). Nunca confiar en el nombre que declara el archivo.
2. **Symlinks y hardlinks** dentro del archivo: rechazados por defecto o extraídos como enlaces si el usuario lo habilita explícitamente, verificando que el destino resuelto permanezca dentro del directorio de extracción.
3. **Bits setuid/setgid/sticky**: eliminados siempre al extraer. Nunca restaurar permisos que no estén en una máscara segura (`0777 & ~S_ISUID & ~S_ISGID`).
4. **Bombas de descompresión**: límite de tamaño total expandido, de número de entradas, de profundidad de anidamiento y de ratio de compresión. Se aplica **también al listar** (algunos formatos listan leyendo el stream completo).
5. **Nombres duplicados / colisiones** dentro del mismo archivo: se resuelven de forma determinista, nunca sobrescribiendo silenciosamente.
6. **Rutas largas y caracteres de control**: truncado seguro y saneado para display; el nombre real se conserva.

### 8.2 Miniaturas y decodificadores

- Decodificación de imágenes en **proceso separado** (helper `ooze-thumb`) con timeout y límites de memoria/píxeles, o vía `*.thumbnailer` del sistema.
- `QImageReader::setAllocationLimit()` activo como última barrera.
- Nada de decodificar en el proceso principal un archivo que venga de red/MTP/extraíble sin pasar por el helper.
- Caché en `~/.cache/thumbnails/...` según spec XDG, con escritura atómica y validación de tamaño.

### 8.3 Sistema de archivos

- Rutas no confiables (vienen del disco): usar `*at` syscalls con `dirfd` y `O_NOFOLLOW` en operaciones recursivas.
- No expandir variables de entorno ni `~` en rutas recibidas por CLI/D-Bus sin una regla explícita (solo en la barra de entrada del usuario).
- Comparación de rutas tras normalización; nunca por prefijo de cadena (`/home/ana` vs `/home/ana2`).

### 8.4 Ejecución de procesos externos

- `QProcess::start(program, args)` **siempre con argumentos separados**. Prohibido `sh -c`.
- Terminal: resolver por `$TERMINAL` → config → lista de candidatos conocidos → error explicativo.
- Acciones personalizadas: sustituciones de variables escapadas; nunca interpolación en un string de shell. Si el usuario quiere shell, lo escribe él y asume el riesgo, pero la app no lo construye por él.

### 8.5 Privilegios

- La app corre como usuario normal. Ninguna operación propia requiere root.
- Montar/expulsar delega en UDisks2 vía Polkit (`GVolumeMonitor`).
- Operaciones que requieran root se delegan a herramientas externas (`kio-admin`, terminal). No hay helper setuid ni servicio D-Bus privilegiado propio.

### 8.6 Credenciales remotas

- Las contraseñas de `smb`/`sftp` las gestiona gvfs (keyring del sistema). `ooze-files` **no** guarda credenciales en su configuración ni las muestra en logs.
- Logs con rutas: se registran a nivel `debug` solo, sin contenido de archivos.

---

## 9. Diseño de interfaz

### 9.1 Estructura base

```
┌──────────────────────────────────────────────────────────────┐
│ ‹ › ↑ │  Inicio ▸ Descargas ▸ Proyectos      🔍  ⋮  (+ tabs) │  header
├──────────┬───────────────────────────────────────────────────┤
│ Inicio   │                                                   │
│ Recientes│              contenido (grid | lista | columnas)  │
│ Favoritos│                                                   │
│ Papelera │                                                   │
│ ──────── │                                                   │
│ Discos   │                                                   │
│ Red      │                                                   │
├──────────┴───────────────────────────────────────────────────┤
│ 3 seleccionados · 1,2 GB           ·  ·  · 42,1 GB libres   │  status
└──────────────────────────────────────────────────────────────┘
```

Todo lo demás (panel de información, panel dual, barra de herramientas extendida) es **opcional y oculto por defecto**.

### 9.2 Elementos minimalistas imprescindibles

- **Breadcrumb** con segmentos clicables y **ruta editable** (`Ctrl+L`), con soporte de `~`, `$VAR` y autocompletado de rutas.
- **Filtro instantáneo** en la carpeta actual (fuzzy, sin indexar) separado de la **búsqueda recursiva**.
- **Paleta de comandos** (`Ctrl+K`): la forma minimalista de exponer 80 acciones sin 80 botones.
- **Quick Look** (barra espaciadora): previsualización flotante, sin abrir app externa.
- **Barra de estado** con selección y espacio libre del FS actual.
- **Acciones al hover** discretas (abrir, renombrar, menú) en lugar de barra permanente.
- **Toast de deshacer** tras operaciones destructivas o movimientos.
- **Panel de operaciones** desplegable y no modal (no bloquea la navegación).

### 9.3 Modos de vista

1. **Cuadrícula** con miniaturas (por defecto en carpetas de imágenes/vídeo; heurística simple y configurable).
2. **Lista detallada** con columnas configurables (nombre, tamaño, tipo, modificado, permisos) y orden por clic.
3. **Columnas tipo Miller** (Finder): petición habitual de quien cambia de gestor y encaja con la filosofía minimalista.

El modo se recuerda **por carpeta** y se puede fijar por configuración.

### 9.4 Orden, filtros y detalles

- Orden: nombre (con `QCollator` en modo numérico y locale-aware), tamaño, fecha, tipo; carpetas primero configurable.
- Ocultos: toggle persistente (`Ctrl+H`), sin parpadeo de recarga completa.
- Filtro rápido por patrón glob además del fuzzy.
- Selección por patrón (`Ctrl+S`) y selección por rango como en cualquier gestor.
- "Expandir todo" en árbol lateral solo si se implementa el árbol (fase 3).

### 9.5 Interacción

- Atajos configurables con detección de conflictos (los del sistema/ compositor ganan; se avisa).
- DnD: dentro de la app, hacia otras apps y **desde** otras apps (ver §11.9).
- Doble clic configurable (abrir / seleccionar) para usuarios de un solo clic.
- Accesibilidad: navegación por teclado completa, foco visible, nombres accesibles en botones. Se documenta que el soporte AT-SPI de Qt Quick es limitado y se prueba con lo básico.

### 9.6 Wayland: comportamiento de ventana

- Ventana normal con **decoraciones del servidor** vía `xdg-decoration` cuando el compositor las ofrezca (niri, mango, KWin, GNOME). No se implementa barra de título propia en tiling WMs.
- **`setPosition()` no funciona en Wayland**: solo se persiste tamaño, maximizado y pestañas. Nunca se promete restaurar la posición exacta.
- Sin menús globales, sin `xdg-activation` obligatorio: la activación se solicita vía portal/D-Bus.
- Se prueba explícitamente en sesión Wayland nativa y con `QT_QPA_PLATFORM=xcb` como diagnóstico.
- DnD entre app Wayland y app XWayland debe verificarse (es un punto históricamente frágil).

---

## 10. Personalización y configuración

El objetivo no es un sistema de plugins, sino **un gestor que se adapta sin tocar código**: tema, densidad, qué se ve y qué no. La extensibilidad por scripts queda como fase tardía y opcional.

### 10.1 Tema (Qt 6)

- `Theme.qml` singleton con **tokens**, no colores sueltos: `bg`, `bgElevated`, `fg`, `fgMuted`, `accent`, `border`, `danger`, además de `radius`, `spacing`, `fontSize`, `elevation`.
- Paletas: claro, oscuro, **seguir al sistema** (portales/preferencia del escritorio) y **Matugen** (leer JSON generado).
- Implementación: componentes propios sobre primitivas `Item`/`Rectangle`/`Text`, con `Qt Quick Controls` en estilo `Basic` como base mínima. Evita pelear con los estilos nativos y permite recarga de tema en caliente.
- Cambio de tema en vivo sin reiniciar y sin recargar QML de disco.
- Densidad (compacta / normal / cómoda) y tamaño de fuente escalable.
- Iconos: tema del sistema (`QIcon::themeName`) con fallback propio embebido para tipos comunes; nunca depender de un tema concreto.

### 10.2 Registro de acciones (ADR-007)

Toda acción se declara una vez:

```jsonc
// conceptual
{
  "id": "file.open-terminal",
  "labelKey": "action.open_terminal",
  "icon": "utilities-terminal",
  "contexts": ["dir", "dir-background"],
  "capabilities": ["canRead"],
  "shortcut": "Ctrl+Alt+T",
  "target": "TerminalLauncher"
}
```

De ahí salen automáticamente: entrada de menú contextual, entrada en la paleta, atajo, y elemento opcional de barra. Añadir una acción es añadir una entrada; no hay tres sitios que mantener sincronizados.

### 10.3 Qué puede ocultar o reordenar el usuario

En `config.json`, sin recompilar:

- Elementos del menú contextual por contexto (`file`, `dir`, `dir-background`, `selection`, `sidebar`, `device`, `archive`).
- Elementos de la barra superior y de la barra lateral.
- Columnas visibles en la vista de lista.
- Botones de hover.
- Secciones del panel de información.
- Entradas del menú "Nuevo documento" y de "Abrir con".
- Visibilidad de "Copiar ruta", "Abrir terminal", "Propiedades", etc.

```jsonc
// config.json (extracto)
{
  "version": 1,
  "ui": {
    "density": "normal",
    "sidebar": ["home", "recent", "favorites", "trash", "devices", "network"],
    "statusBar": ["selection", "freeSpace"],
    "columns": ["name", "size", "type", "modified"]
  },
  "menus": {
    "file":    { "hide": ["file.properties"], "order": ["file.open", "file.openWith"] },
    "dir":     { "hide": [] },
    "selection": { "hide": ["selection.compress"] }
  },
  "theme": { "mode": "system", "density": "normal", "matugen": false },
  "behavior": {
    "singleClickOpen": false,
    "deleteToTrash": true,
    "confirmPermanentDelete": true,
    "followSymlinksInRecursiveOps": false,
    "defaultView": "auto"
  }
}
```

Validación: esquema con `version`, tolerancia a claves desconocidas (aviso, no fallo), y errores mostrados en un diálogo con el número de línea si es JSON inválido. Nunca arrancar con configuración corrupta en silencio: usar valores por defecto y avisar.

### 10.4 Acciones personalizadas (fase 5, opcional)

Formato declarativo en `~/.config/ooze-files/actions/*.json` (mismo parser que `config.json`): nombre, icono, tipos MIME o patrones, comando + argumentos tipados (`%f`, `%F`, `%d`), condición de selección, y `confirm` para las destructivas. Sin shell implícito. Es lo que hace extensible a Thunar/Dolphin sin código, a coste bajo.

---

## 11. Integraciones del sistema

### 11.1 Sistema de archivos local

- Navegar, listar, ordenar, filtrar, mostrar ocultos y symlinks (con marca visual y destino en tooltip).
- Copiar, mover, renombrar (individual y múltiple), crear carpeta/archivo, borrar a papelera.
- Tamaño, fechas, permisos, propietario/grupo (si es consultable), tipo MIME, enlaces duros.
- Cola de operaciones con progreso, pausa, cancelación segura y errores por archivo.
- Resolución de conflictos: reemplazar / omitir / renombrar automático (`nombre (2).ext`) / conservar ambos / comparar tamaño y fecha, con **"aplicar a todo"**.
- Casos límite soportados y probados: nombres no UTF-8, nombres con saltos de línea, espacios finales, colisiones case-insensitive (FAT/NTFS/exFAT), archivos dispersos, enlaces rotos, permisos denegados.

### 11.2 Papelera

Implementación completa del spec freedesktop, vía `g_file_trash()` más código propio para restaurar:

- Papelera de usuario (`~/.local/share/Trash`) y **papelera por montaje** (`.Trash-$UID` o `.Trash/$UID` en el punto de montaje) cuando el archivo está en otro FS.
- `.trashinfo` con ruta original y fecha (se lee para restaurar; nunca se reescribe a mano si GIO puede hacerlo).
- Restaurar con resolución de colisiones y validación de que la ruta original sigue existiendo/accesible.
- Vaciar papelera con progreso y confirmación; vaciado selectivo.
- Restaurar entradas de otras apps (Nautilus, Dolphin) que compartan el mismo spec.
- Caso especial: archivos en unidades extraíbles desconectadas → mostrar como "no disponible" sin perder la entrada.

### 11.3 «Abrir terminal aquí»

- Resolución del emulador: `$TERMINAL` → `config.behavior.terminal` → candidatos (`foot`, `kitty`, `alacritty`, `wezterm`, `ghostty`, `konsole`, `kgx`, `xterm`).
- Lanzamiento con `QProcess` y `--working-directory` o `-d` según el emulador (tabla de adaptadores, no concatenación de shell).
- Disponible sobre carpeta, sobre el fondo de la carpeta actual, y sobre un archivo (usando su directorio, con el archivo preseleccionado si el emulador lo permite).
- Rutas con espacios/Unicode: pasan como argumento único, siempre.
- Copiar ruta absoluta, relativa, nombre y URI.

### 11.4 Discos, volúmenes y USB

**Vía principal: `GVolumeMonitor`** (que usa UDisks2 + Polkit por debajo).

- Detección de conexión/desconexión y actualización de la barra lateral sin polling.
- Discos, particiones, sistemas de archivos, punto de montaje, capacidad y espacio libre cuando estén disponibles.
- Montar, desmontar, expulsar. Errores traducidos a mensajes accionables (incluido "no hay agente Polkit").
- Detección de desconexión durante una operación: abortar limpiamente y explicar qué archivos quedaron incompletos (los `.ooze-part` se limpian).
- Aviso antes de expulsar si hay operaciones en curso sobre ese volumen.

**Fase avanzada (opcional, `OOZE_FILES_WITH_UDISKS`):** SMART vía `smartctl`, detalles técnicos, y enlace a GParted/KDE Partition Manager. Sin operaciones destructivas propias.

### 11.5 Archivos comprimidos

**Backend:** libarchive (feature `archives`).

- Listar contenido **sin extraer** (vista virtual `ArchiveVfs`, con todas las defensas de §8.1).
- Extraer aquí / extraer en carpeta nueva / extraer selección.
- Crear: `.zip` (deflate), `.tar`, `.tar.gz`, `.tar.xz`, `.tar.zst`.
- Leer: zip, tar y variantes, 7z (lectura), iso (lectura), y lo que libarchive soporte en la compilación. **RAR y formatos con códecs no libres no se prometen**; si no están, el mensaje debe decirlo.
- Contraseñas: soportadas al leer cuando el formato lo permite; **crear ZIP cifrado no está soportado por libarchive** y se documenta como limitación (no se simula).
- Progreso, cancelación y errores por entrada. Nunca extracción parcial silenciosa: al cancelar se informa de lo extraído.
- Navegación interna: solo lectura y sin prometer edición de archivos dentro del contenedor.

### 11.6 Aplicaciones, MIME y «Abrir con»

- Detección de MIME con `g_content_type_guess` (contenido + extensión), caché por extensión y por inodo.
- Apertura con la aplicación predeterminada vía `GAppInfo` (`g_app_info_launch`), respetando `mimeapps.list` y los `.desktop` del escritorio.
- Menú "Abrir con": aplicaciones que declaran el MIME, más "otra aplicación…" con selector.
- Nunca rutas fijas del tipo `/usr/bin/...`; `GAppInfo` resuelve `Exec` correctamente (incluido NixOS).
- Multi-apertura de varios archivos: agrupar por aplicación compatible cuando sea posible; si no, abrir en la predeterminada uno a uno con aviso.
- "Establecer como predeterminada" solo si se implementa escritura de `mimeapps.list` con copia de seguridad previa; si no, se enlaza a la herramienta del escritorio.

### 11.7 gvfs: red y dispositivos móviles (MTP)

- Backend `GvfsVfs` sobre `GFile`, con `GFileMonitor` para refresco.
- Esquemas soportados según lo instalado: `smb`, `sftp`, `dav`/`davs`, `mtp`, `gphoto2`, además de `recent` y `trash`.
- Entrada de conexión en la barra lateral: "Conectar a servidor…" con validación y sin guardar contraseñas (las gestiona gvfs/keyring).
- Operaciones parcialmente soportadas según capacidades del backend: en MTP, renombrar/permisos pueden no estar disponibles y la UI lo refleja.
- Timeouts, reconexión y errores de red claros; nunca bloquear la UI esperando un servidor caído.
- Documentado el requisito `services.gvfs.enable` en NixOS.

**KIO queda descartado** (ADR-004). La licencia GPL-3.0 deja la puerta abierta si en el futuro se quisiera, pero no se mantienen dos backends remotos.

### 11.8 Marcadores y recientes compartidos con el escritorio

Para que el gestor se sienta parte del sistema y no una isla:

- **Favoritos**: leer y escribir `~/.config/gtk-3.0/bookmarks` (formato `file://ruta etiqueta`) para compartirlos con GTK/Nautilus; importación opcional de `~/.local/share/user-places.xbel` (KDE).
- **Recientes**: leer `~/.local/share/recently-used.xbel` (parser XBEL propio, lectura tolerante a XML corrupto). Se puede además registrar aperturas con `gtk_recent_manager`-compatible vía el mismo fichero, con escritura atómica y bloqueo.
- **Plantillas**: "Nuevo documento" lee `~/Templates` (y `XDG_TEMPLATES_DIR`).
- **Escritorio y carpetas XDG**: resolver `XDG_DESKTOP_DIR`, `XDG_DOWNLOAD_DIR`, etc. vía `~/.config/user-dirs.dirs`, con soporte de `user-dirs.locale`.

### 11.9 Portapapeles y drag & drop interoperables

Un gestor que no puede pegar desde Nautilus no está "completo en integraciones". Soportar en **lectura y escritura**:

| Formato | Origen |
|---|---|
| `text/uri-list` | Estándar, todos |
| `x-special/gnome-copied-files` | Nautilus |
| `application/x-kde-cutselection` | Dolphin |
| `text/plain` con rutas | Fallback de lectura |

- Distinguir copiar vs cortar (el "cut" debe indicar visualmente el origen y limpiarse tras pegar o al cancelar).
- Pegar dentro de la misma carpeta → renombrado automático, no sobrescritura.
- DnD a la papelera y a la barra lateral como destinos válidos.
- DnD entre app Wayland ↔ XWayland verificado y documentado.

### 11.10 Búsqueda

Dos funciones distintas que no deben confundirse:

1. **Filtro instantáneo** en la carpeta actual: fuzzy sobre el modelo ya cargado, sin I/O. Instantáneo.
2. **Búsqueda recursiva**: recorrido acotado con límites explícitos (profundidad, número de resultados, tiempo), cancelable, con opciones de nombre/glob/contenido (contenido solo si se pide y con límite de tamaño). Se ejecuta en `SearchVfs` y muestra resultados con su ruta real.
3. **Búsqueda indexada (opcional)**: consumir `plocate` o `localsearch` si están instalados, sin implementar indexación propia. Si no están, la UI no los ofrece.

### 11.11 Notificaciones y segundo plano

- Fin de operación larga cuando la ventana no tiene foco: notificación D-Bus (`org.freedesktop.Notifications`) con acción "Mostrar".
- Errores agrupados: una notificación con recuento, no N notificaciones.
- Sin bandeja del sistema ni daemon permanente: si no hay ventanas, la app sale (configurable).

### 11.12 OozeShell y Matugen

Separado del núcleo, siempre opcional:

- Leer paleta de Matugen (`~/.config/matugen/...json` o la ruta que indique `config.json`) y aplicarla en caliente al cambiar el fichero.
- API D-Bus `org.ooze.Files` para que OozeShell abra carpeta/archivo, nueva pestaña o ventana.
- CLI equivalente para atajos del compositor.
- Todo funciona sin OozeShell y sin Matugen.

### 11.13 i18n

- `qt_add_translations`, español e inglés desde el inicio; cadenas en QML vía `qsTr` y en C++ vía `tr()`.
- Formatos de fecha/tamaño localizados (`QLocale`), incluido separador decimal y unidades.
- Sin concatenación de frases para construir mensajes (rompe la traducción).

---

## 12. Estructura del repositorio

```text
ooze-files/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE                      # GPL-3.0-or-later
├── src/
│   ├── app/
│   │   ├── main.cpp
│   │   ├── ApplicationController.cpp
│   │   ├── SingleInstance.cpp          # D-Bus org.ooze.Files
│   │   └── Logging.cpp                 # Q_LOGGING_CATEGORY
│   ├── vfs/
│   │   ├── VfsBackend.h
│   │   ├── VfsRegistry.cpp
│   │   ├── LocalVfs.cpp
│   │   ├── GvfsVfs.cpp
│   │   └── SearchVfs.cpp
│   ├── core/
│   │   ├── DirectoryLoader.cpp         # poblado en dos fases
│   │   ├── DirectoryWatcher.cpp        # inotify + debounce
│   │   ├── OperationQueue.cpp
│   │   ├── FileOperations.cpp
│   │   ├── TrashService.cpp
│   │   ├── UndoStack.cpp
│   │   ├── CancelToken.h
│   │   └── PathUtils.cpp               # bytes, normalización, EXDEV
│   ├── models/
│   │   ├── FileListModel.cpp
│   │   ├── SelectionModel.cpp
│   │   ├── PlacesModel.cpp
│   │   └── OperationListModel.cpp
│   ├── system/
│   │   ├── gio/                        # aislado y reemplazable
│   │   │   ├── MountService.cpp
│   │   │   ├── MimeService.cpp
│   │   │   ├── AppsService.cpp
│   │   │   └── BookmarksService.cpp
│   │   ├── terminals/
│   │   └── Notifications.cpp
│   ├── modules/
│   │   ├── archives/
│   │   ├── previews/
│   │   ├── search/
│   │   └── actions/                    # registro + acciones personalizadas
│   └── settings/
│       ├── Config.cpp                  # config.json validado
│       ├── SessionState.cpp            # state.json
│       └── Theme.cpp
├── tools/
│   └── ooze-thumb/                     # helper de miniaturas aislado
├── qml/
│   ├── Main.qml
│   ├── components/                     # Button, Menu, Dialog, Toast...
│   ├── views/                          # Grid, List, Columns
│   ├── panels/                         # Sidebar, Info, Operations
│   ├── dialogs/
│   └── theme/                          # Theme.qml + tokens + matugen
├── resources/                          # iconos fallback, plantillas
├── tests/
│   ├── unit/                           # Qt Test
│   ├── integration/                    # ops sobre temp dirs
│   ├── qml/                            # qmltestrunner
│   ├── fixtures/                       # ZIPs maliciosos, árboles sintéticos
│   └── bench/
├── nix/
│   ├── default.nix                     # paquete
│   ├── devshell.nix
│   └── module.nix                      # opcional, no obligatorio
├── packaging/
│   ├── ooze-files.desktop
│   └── icons/
└── docs/
    ├── architecture.md
    ├── vfs.md
    ├── integrations.md
    ├── security.md
    ├── performance.md
    └── adr/
```

Regla: cada módulo se crea cuando tiene funcionalidad real. La estructura es objetivo, no una checklist del día 1. Lo que **sí** se crea el día 1: `vfs/`, `core/CancelToken`, `app/SingleInstance`, `settings/Config` y el esqueleto QML empaquetado.

---

## 13. Fases de desarrollo

Cada fase tiene criterio de salida verificable. No se empieza la siguiente sin cumplirlo.

### Fase 0 — Esqueleto ejecutable

- CMake + `qt_add_qml_module` + dev shell Nix + CI.
- Ventana que arranca en < 300 ms, tema claro/oscuro conmutables en vivo.
- Instancia única D-Bus + CLI (`ooze-files [ruta]`, `--new-tab`, `--new-window`, `--select`).
- `Config` con JSON validado + `state.json` separado.
- `VfsBackend` local + `CancelToken` + `OperationQueue` vacía + logging por categorías.
- Licencia, `.desktop`, icono, esqueleto de docs.

**Salida:** binario que abre una carpeta y lista nombres. Nada más. Pero los cimientos ya son los definitivos.

### Fase 1 — Navegador usable (solo lectura)

- Poblado en dos fases, grid/lista, breadcrumb + `Ctrl+L`, filtro instantáneo, orden con `QCollator`, ocultos.
- `DirectoryWatcher` con debounce y degradación.
- Favoritos (leyendo bookmarks GTK), recientes, papelera visible.
- Abrir, "abrir con", "abrir terminal", copiar ruta.
- Quick Look básico (imagen/texto/PDF si hay poppler).
- Barra lateral con discos (montajes vía `GVolumeMonitor`) en modo lectura.

**Salida:** abrir `/usr/bin`, una carpeta de 20.000 fotos y `/nix/store` sin tirones de scroll ni bloqueos, con presupuestos de §7 medidos.

### Fase 2 — Operaciones seguras (el núcleo de valor)

- Papelera completa (§11.2), restaurar, vaciar.
- Copiar/mover/renombrar/borrar con cola, pausa, cancelación y errores por archivo.
- Conflictos con "aplicar a todo"; `.ooze-part` + rename atómico; `EXDEV`; reflink.
- **Deshacer** con `UndoStack` + toast.
- Renombrado múltiple por patrón.
- Portapapeles interoperable (3 formatos) y DnD interno/externo.
- Política de symlinks y rutas no UTF-8 cubierta por tests.

**Salida:** uso diario real sustituyendo a otro gestor sin miedo a perder datos. Suite de integración con fixtures (ZIP malicioso pendiente de fase 4, pero árboles con nombres hostiles desde ya).

### Fase 3 — Productividad

- Pestañas, panel dual, vista de columnas Miller.
- Miniaturas con caché XDG + thumbnailers + helper aislado; límites y cancelación.
- Panel de información/previsualización, propiedades, permisos.
- Modo de vista recordado por carpeta; estado de sesión completo.
- Paleta de comandos y registro de acciones con personalización de menús (§10).
- Búsqueda recursiva acotada.
- Barra de estado con espacio libre.

**Salida:** paridad funcional con Nautilus/Dolphin para trabajo diario, con personalización de menús ya operativa.

### Fase 4 — Sistema

- libarchive: navegar, extraer, crear, con todas las defensas de §8.1 y tests de ZIP malicioso/bomba.
- gvfs: `smb`/`sftp`/`dav`/`mtp` con capacidades parciales, errores de red y timeouts.
- MTP probado con un Android real (transferencia, desconexión durante operación).
- UDisks2 opcional para SMART y detalles; enlaces a GParted.
- Detección de agente Polkit ausente con mensaje accionable.

**Salida:** integración de sistema completa y segura. Ningún módulo opcional rompe el arranque si falta.

### Fase 5 — Extensión y pulido

- Acciones personalizadas declarativas (§10.4).
- Integración Matugen + API D-Bus final + integración con OozeShell.
- Notificaciones de fin de operación.
- Accesibilidad, atajos configurables con detección de conflictos, i18n completa.
- Benchmarks en CI, perfilado de RAM/VRAM, revisión de texturas y blur.

**Salida:** release 1.0 candidata.

### Fase 6 — Distribución

- Paquete Nix reproducible (`nix build`), dev shell, y `module.nix` opcional que **no** cambia asociaciones globales.
- Metadatos `.desktop`, iconos, capturas, README con requisitos de runtime (Polkit, gvfs).
- Revisión de dependencias y licencias; verificación de que el binario funciona con `OOZE_FILES_WITH_ARCHIVES=OFF` y `WITH_REMOTE=OFF`.
- Documentación de compilación fuera de Nix (dependencias por distro).

**Salida:** instalable con un comando en NixOS y documentado para otras distros.

---

## 14. Pruebas y validación

### 14.1 Unitarias (Qt Test)

- `PathUtils`: normalización, bytes no UTF-8, symlinks, comparación sin colisión por prefijo.
- `Config`: JSON válido/ inválido/ versión futura/ claves desconocidas.
- `DirectoryLoader`: orden de lotes, cancelación a mitad, reutilización del pool.
- `UndoStack`: deshacer tras copiar/mover/renombrar/papelera.
- `ArchiveVfs` (fase 4): rechazo de Zip Slip, límites de bomba, permisos saneados.
- `TerminalLauncher`: tabla de adaptadores, rutas con espacios.

### 14.2 Integración

- Operaciones reales sobre directorios temporales (`QTest::addColumn` por escenario).
- Fixtures: nombres con `\n`, emoji, bytes no UTF-8, 300 caracteres, espacios finales; árboles con symlinks circulares, permisos denegados, archivo de 0 bytes, archivo disperso.
- Watcher: crear/renombrar/borrar mientras la vista está abierta; verificar que no hay bucles de eventos propios.
- Papelera: borrar en el mismo FS y en otro (loop device), restaurar con colisión.

### 14.3 QML (`qmltestrunner`)

- Componentes del tema, menús generados desde el registro de acciones, visibilidad según `capabilities()`.
- Que ocultar una acción en `config.json` la quita de menú, paleta y barra a la vez.

### 14.4 Rendimiento

- `tests/bench/` genera árboles de 10k/100k y mide arranque, poblado, scroll y copia.
- Umbrales de §7 como `ctest` con etiqueta `bench` (no en cada commit, sí antes de cada fase/release).
- Perfilado con `perf` y `heaptrack` en las release candidates.

### 14.5 Verificación manual por entorno

Matriz mínima antes de release: **Niri**, **MangoWM**, **Hyprland**, **KDE Plasma (Wayland)**, **GNOME (Wayland)**. En cada uno: decoraciones, DnD externo, montaje USB, papelera, atajos, apertura desde el gestor del escritorio.

---

## 15. Empaquetado para NixOS

- Paquete independiente; sin cambios globales en la configuración del sistema.
- `qt_add_qml_module` evita el clásico `QML2_IMPORT_PATH` roto. Aun así, se usa `qt6.wrapQtAppsHook` y se declaran explícitamente `qt6.qtdeclarative` y los módulos QML usados.
- Dependencias por feature flag: libarchive, gvfs, udisks2 solo cuando la feature está activa (verificado en CI).
- `services.gvfs.enable = true` documentado como requisito para red/MTP; el paquete **no** lo activa.
- Preferencias en `XDG_CONFIG_HOME`; nada en el store ni en el home del sistema.
- `.desktop` con `MimeType=inode/directory;` pero **sin** imponerse como predeterminado; se documenta cómo elegirlo.
- `module.nix` opcional (solo `environment.systemPackages`), sin tocar asociaciones.

---

## 16. Criterios de aceptación (release 1.0)

### Funcional

- [ ] Navegación fluida sin bloqueos en carpetas de 50.000 entradas y en `/nix/store`.
- [ ] Papelera: borrar, restaurar, vaciar, y compartir papelera con otros gestores.
- [ ] Deshacer funciona para mover, renombrar y borrar a papelera.
- [ ] Operaciones con progreso, pausa, cancelación y errores por archivo; nunca archivos a medias sin marcar.
- [ ] Conflictos resolubles con "aplicar a todo".
- [ ] Portapapeles: copiar en Nautilus y pegar aquí, y viceversa.
- [ ] Nombres con espacios, Unicode, saltos de línea y bytes no UTF-8 se muestran y se operan sin corromperse.
- [ ] La terminal se abre en la ruta correcta con emuladores distintos.
- [ ] Montaje/desmontaje/expulsión USB con mensajes claros, incluido el caso "sin agente Polkit".
- [ ] Extracción de archivos comprimidos rechaza Zip Slip, symlinks fuera del destino y bombas de descompresión.
- [ ] MTP: transferir a/desde un Android y manejar la desconexión a mitad.
- [ ] "Abrir con" respeta `mimeapps.list` y funciona en NixOS sin rutas fijas.
- [ ] Ocultar acciones de menú desde `config.json` afecta menú, paleta, atajos y barra de forma coherente.
- [ ] Cambio de tema (incluido Matugen) en vivo, sin reiniciar.

### Rendimiento

- [ ] Presupuestos de §7 cumplidos y registrados en `docs/performance.md`.
- [ ] Cada flag de CMake produce un binario funcional.
- [ ] Watchers: sin consumo de CPU en reposo y sin bucles de eventos propios.
- [ ] Caché de miniaturas limitada y con invalidación correcta al cambiar el archivo.

### Portabilidad y calidad

- [ ] Funciona sin OozeShell, sin Matugen y sin gvfs instalado.
- [ ] Probado en al menos tres compositores Wayland distintos y en Plasma/GNOME.
- [ ] `nix build` reproducible; sin rutas del store filtradas en binarios o config.
- [ ] Traducciones completas es/en; textos sin concatenar.
- [ ] Navegación por teclado completa y foco visible en todos los diálogos.

---

## 17. Riesgos y mitigaciones

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Las operaciones de archivos son la parte más delicada | Pérdida de datos | Fase 2 dedicada, escritura atómica, deshacer, tests con fixtures hostiles, papelera por defecto. |
| gvfs/GIO añade dependencia de GLib al proyecto Qt | Complejidad de build | Aislado en `src/system/gio/`, flags de CMake, sin GLib en la UI ni en los modelos. |
| QML + modelos grandes degrada scroll | Mala experiencia | Poblado en dos fases, `reuseItems`, benchmarks obligatorios por fase. |
| Sin agente Polkit el montaje "no funciona" | Mala primera impresión | Detección explícita y mensaje con el paquete a instalar. |
| libarchive no cubre RAR/ZIP cifrado de escritura | Expectativas | Documentar limitaciones en la UI, no prometer formatos. |
| Accesibilidad de Qt Quick es limitada | Exclusión | Navegación por teclado y foco cuidados desde el inicio; documentar el estado real. |
| Dos backends remotos (KIO + gvfs) | Doble superficie de bugs | KIO descartado por ADR-004. |
| Personalización excesiva convierte la app en un rompecabezas | Complejidad de UI | Personalización limitada a tema, densidad, visibilidad y orden; sin motor de layouts. |

---

## 18. Documentación técnica de referencia

- [Qt Quick y QML](https://doc.qt.io/qt-6/qtquick-index.html) — UI, modelos, vistas virtualizadas.
- [Qt for Wayland](https://doc.qt.io/qt-6/wayland-and-qt.html) — plataforma, decoraciones, limitaciones.
- [qt_add_qml_module](https://doc.qt.io/qt-6/qt-cmake-qml-module.html) — empaquetado de QML.
- [GIO / GFile](https://docs.gtk.org/gio/) — operaciones, papelera, montajes, MIME, gvfs.
- [gvfs](https://wiki.gnome.org/Projects/gvfs) — backends remotos y MTP.
- [Desktop Trash spec](https://specifications.freedesktop.org/trash-spec/latest/) — papelera.
- [Thumbnail Managing Standard](https://specifications.freedesktop.org/thumbnail-spec/latest/) — caché de miniaturas y directorio `thumbnailers`.
- [UDisks2 D-Bus API](https://storaged.org/doc/udisks2-api/latest/) — detalles avanzados de dispositivos.
- [Polkit](https://polkit.pages.freedesktop.org/polkit/) — autorización.
- [libarchive](https://libarchive.org/) — compresión y formatos.
- [CMake](https://cmake.org/cmake/help/latest/) — build.
- [Nixpkgs manual](https://nixos.org/manual/nixpkgs/stable/) — empaquetado reproducible.

---

## 19. Resumen de decisiones

1. **Lenguaje:** C++20.
2. **UI:** Qt 6 / Qt Quick + QML empaquetado con `qt_add_qml_module`, componentes propios sobre estilo `Basic`.
3. **Build:** CMake ≥ 3.21 con `CMakePresets.json` y flags por feature.
4. **Abstracción:** capa `VfsBackend` obligatoria desde el día 1; URLs con esquema en toda la app.
5. **Local:** POSIX (`openat`, `FICLONE`, rename atómico) para rendimiento y seguridad.
6. **Papelera, montajes, MIME, apps:** GIO/GLib (`g_file_trash`, `GVolumeMonitor`, `GAppInfo`).
7. **Remoto y MTP:** gvfs. **KIO descartado.**
8. **Compresión:** libarchive con defensas anti Zip Slip y anti bomba.
9. **Miniaturas:** caché XDG + thumbnailers externos + helper aislado; nunca decodificar entrada no confiable en el proceso principal.
10. **Concurrencia:** pools separados, poblado en dos fases, cancelación obligatoria.
11. **Instancia única:** D-Bus `org.ooze.Files` con CLI equivalente.
12. **Personalización:** tema por tokens, densidad, y visibilidad/orden de acciones desde `config.json` sobre un `ActionRegistry` único.
13. **Privilegios:** ninguno propio; delegación en Polkit/UDisks2 y herramientas externas.
14. **Licencia:** GPL-3.0-or-later.
15. **Primera entrega útil:** fase 1 (navegador solo lectura); **primera entrega diaria:** fase 2 (operaciones seguras con papelera y deshacer).

---

## Conclusión

La prioridad es un núcleo pequeño, correcto y medible antes de acumular funciones. La diferencia con la versión anterior del plan es que ahora existen contratos explícitos donde antes había intenciones: **una capa VFS que hace posible gvfs y los archivos comprimidos sin reescribir la UI**, un modelo de concurrencia que evita los bloqueos en carpetas grandes, un modelo de amenazas para archivos comprimidos y miniaturas, y presupuestos de rendimiento verificables.

El orden de entrega se invierte respecto al instinto natural: **primero se aprende a navegar rápido, después se aprende a no perder datos**. La papelera y el deshacer no son "extras de fase 2" decorativos: son la razón por la que alguien se atreve a reemplazar su gestor de archivos de siempre por este.
