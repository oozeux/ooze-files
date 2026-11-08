# Arquitectura

## Capas

```
qml/                    interfaz declarativa (Qt Quick puro)
  Main.qml              ventana, menús, atajos
  panels/               cabecera, barra lateral, barra de estado
  views/                cuadrícula y lista
  components/           botones, iconos, menú, buscador, scroll
  theme/Theme.qml       singleton de tokens visuales (lee Palette)
  components/MenuPanel.qml       panel de menú con scroll y submenús
  components/PopupMenu.qml       menú público (MenuPanel + submenús anidados)
  components/PreviewOverlay.qml  vista previa rápida (Espacio)
  components/SearchOverlay.qml   búsqueda recursiva con filtros (Ctrl+Mayús+F)
  components/BulkRenameDialog.qml renombrado en lote (Mayús+F2)
  components/InfoPanel.qml       panel lateral de información (F11)
  components/PropertiesDialog.qml diálogo de propiedades (Alt+Intro)
  components/DetailsList.qml     lista de propiedades compartida
  pages/SettingsWindow.qml       ventana flotante de configuración con pestañas

src/app/                puente entre Qt y QML
  ApplicationController singleton QML "App"; pestañas y servicios globales
  TabController         estado de una pestaña (modelo, proxy, historial, selección)
  TabListModel          modelo plano que alimenta la barra de pestañas
  ActionRegistry        tabla única de acciones (singleton "Actions")
  Tools                 detección de ffmpeg/ffprobe/unsquashfs/pdftoppm (singleton "Tools")
  SearchModel           búsqueda recursiva en hilo propio (singleton "Search")
  SingleInstance        D-Bus
  IconProvider          image://icons/<nombre>?emblem=link
  DragPreviewProvider   image://dragpreview/<nombre>?icon=…
  ThumbnailProvider     image://thumbnails/<url> (vídeo, caché XDG)
  PdfPreviewProvider    image://previews/<url> (primera página, pdftoppm)
  IconTheme             resolución del tema de iconos
  Logging               patrón y filtros de registro

src/core/               lógica sin QML
  DirectoryLoader       carga en dos fases con generaciones
  FileListModel         modelo de filas
  FileListProxyModel    filtro y orden
  PathUtils             rutas y URL en bytes
  Places                lugares XDG
  OpenWith              aplicaciones por tipo MIME y lanzamiento (GIO)
  FilePreview           datos de vista previa y propiedades de un archivo local
  ThumbnailCache        caché freedesktop ~/.cache/thumbnails
  UndoStack             pila de deshacer/rehacer (datos puros, sin hilos)
  Archive               formatos y comandos de compresión y extracción
  Devices               parser de UDisks2 y DeviceMonitor (D-Bus del sistema)
  TerminalLauncher      detección y lanzamiento de terminal
  CancelToken           cancelación cooperativa

src/vfs/                contrato y backends
  VfsBackend, VfsRegistry, LocalVfs, VfsTypes

src/settings/Config     configuración JSON

src/theme/ThemeStore    paletas incorporadas, temas externos e importadores del sistema (singleton "Palette")
src/theme/PortalSettings acento y esquema claro/oscuro del portal del escritorio (solo lectura)
```

La regla es que `src/core` y `src/vfs` no conocen QML, y `qml/` no ejecuta
operaciones de archivos: solo llama a `App` y dibuja.

## Flujo de navegación

```
QML (App.goTo / breadcrumb / lugares)
  └─ ApplicationController::navigate        delega en la pestaña activa
       └─ TabController::navigate
            ├─ PathUtils::normalized        normaliza la URL en bytes
            ├─ historial propio de la pestaña
            ├─ DirectoryLoader::attach      une el modelo de la pestaña
            │    ├─ cancela la carga anterior (CancelToken)
            │    ├─ incrementa generación
            │    ├─ FileListModel::beginLoad (limpia filas)
            │    └─ ListTask en el pool compartido de listado
            └─ emite currentUrlChanged, lo que repinta migas y barra de estado

ListTask (hilo de listado)
  └─ VfsBackend::list por lotes de 500
       └─ señal batch(generación, entradas) al hilo principal
            ├─ se descarta si la generación no es la actual
            ├─ se añaden filas al modelo
            └─ StatTask (pool de stat) calcula tamaño, fecha, MIME e icono

StatTask (2 hilos)
  └─ señal stats(generación, lote)
       └─ FileListModel::applyStats actualiza filas por índice
```

La lista se pinta con nombres e iconos aproximados desde el primer lote. Los
metadatos llegan después sin reordenar filas, así que los índices siguen siendo
válidos. El proxy aplica filtro y orden sobre el modelo.

Una carga nueva cancela la anterior y sube la generación; cualquier resultado de
una generación vieja se descarta. Eso hace segura la navegación rápida (entrar y
salir de carpetas antes de que terminen de cargar).

## Pestañas

`ApplicationController` conserva los servicios globales (configuración,
portapapeles, deshacer, operaciones de archivo, buscador, D-Bus) y delega todo lo
que depende de la carpeta activa en un `TabController`. Cada pestaña tiene su
propio `FileListModel`, `FileListProxyModel`, historial, selección, filtro,
mensaje de estado, `DirectoryWatcher` y marcas de carga.

```
ApplicationController
  ├─ DirectoryLoader compartido   (3 hilos en total)
  ├─ TabListModel                 títulos, URLs y pestaña activa
  ├─ TabController × N            estado independiente por carpeta
  └─ pila de pestañas cerradas    reabrir en su posición
```

Solo la pestaña activa está unida al `DirectoryLoader`. Al cambiar de pestaña:

1. la anterior llama a `deactivate()`, que cancela su carga y suelta el modelo;
2. la nueva llama a `activate()`, que vuelve a unir su modelo y carga solo si
   nunca se cargó, si quedó sucia o si su carpeta cambió;
3. si ya estaba cargada y limpia, el cambio es inmediato y no toca el disco.

Una pestaña inactiva nunca refresca por su cuenta: su `DirectoryWatcher` marca la
pestaña como sucia y el refresco se pospone hasta que vuelva a activarse. Así no
hay `readdir` ni `stat` en segundo plano para carpetas que no se están viendo.

`TabListModel` guarda valores planos (título, URL, activa) en lugar de punteros a
`TabController`. Borrar una pestaña nunca deja una referencia colgante en QML; la
barra se reconstruye desde el controlador. `App.tabs` es estable y emite
`tabsChanged`/`activeTabChanged`; `App.files` apunta al modelo de la pestaña
activa y emite `filesChanged` al cambiar.

Cerrar la última pestaña emite `closeWindowRequested` y cierra la ventana. Las
pestañas cerradas se apilan con su índice original para que `Ctrl+Mayús+T` las
reabra donde estaban. El límite (`session.maxTabs`, 1–32, por defecto 16) se
comprueba antes de crear una pestaña y produce un aviso en la barra de estado.

La sesión se guarda en `Config` de forma diferida al abrir, cerrar, activar o
navegar, y se fuerza en `aboutToQuit`. Al arrancar sin ruta explícita se
restauran las URLs que sigan siendo carpetas locales; el resto se omite sin
impedir el arranque. El índice activo se captura antes de restaurar porque
arrancar la primera pestaña ya dispara un guardado.

## Hilos

- Hilo principal: QML, modelos, proxies, D-Bus, configuración.
- `ooze-files-list` (1 hilo): lectura de directorios. Un solo listado a la vez
  evita competir por la caché del sistema y mantiene el orden de llegada. El pool
  es único y lo comparten todas las pestañas; cambiar de pestaña no crea hilos.
- `ooze-files-stat` (2 hilos): `lstat`, MIME e iconos por lotes de 256.

Las tareas son `QRunnable` con un `LoaderSignals` que vive en el hilo principal;
las señales cruzan el hilo automáticamente por conexión en cola. `CancelToken`
es un `std::shared_ptr` con un `std::atomic<bool>`, comprobado cada 64 entradas
en el listado y en cada elemento del stat.

`ApplicationController` destruye explícitamente el `DirectoryLoader` antes que el
`VfsRegistry`: así las tareas en vuelo se cancelan y terminan mientras los
backends siguen vivos.

## Rutas como bytes

Linux no garantiza que los nombres de archivo sean UTF-8. Un `QString` no puede
representar `0xE9` suelto sin pérdida, así que la fuente de verdad son los bytes:

- `VfsEntry::path` es la ruta absoluta en `QByteArray`.
- `VfsEntry::id` es el nombre en bytes relativo a la carpeta.
- `displayName` es solo para pintar y puede ser lossy.
- Todas las operaciones (`stat`, `open`, futuras copias) usan `path`.

Las URL que cruzan a QML se construyen percent-codificando los bytes
manualmente (`PathUtils::fromLocalBytes`), y se decodifican con
`PathUtils::toLocalBytes` leyendo `QUrl::FullyEncoded`. Así `%E9` va y vuelve
sin convertirse en `U+FFFD`, algo que `QUrl::fromLocalFile(QString)` no puede
garantizar. Las APIs que solo aceptan `QString` (por ejemplo el portapapeles o
el título de la ventana) usan la versión lossy y se documentan como tales.

## Modelo de datos

`FileListModel` expone estos roles:

| Rol | Tipo | Contenido |
| --- | --- | --- |
| `NameRole` | `QString` | nombre visible |
| `UrlRole` | `QString` | URL percent-codificada exacta |
| `IsDirRole` | `bool` | es carpeta |
| `SizeRole` | `qint64` | bytes, `-1` si no aplica |
| `ModifiedRole` | `QDateTime` | mtime |
| `IconNameRole` | `QString` | nombre de icono del tema |
| `MimeRole` | `QString` | MIME detectado |
| `HiddenRole` | `bool` | empieza por punto |
| `SizeTextRole` | `QString` | tamaño formateado |
| `ModifiedTextRole` | `QString` | fecha formateada |
| `KindRole` | `int` | `EntryKind` |

El modelo no ordena ni filtra. `FileListProxyModel` aplica:

- filtro por subcadena sin distinguir mayúsculas sobre `NameRole`;
- orden por nombre (con `QCollator` numérico), tamaño, fecha o tipo;
- carpetas primero en ambos sentidos de orden.

## Interfaz QML

El módulo `Ooze.Files` se compila con `qt_add_qml_module` y se embebe en el
binario bajo `:/qt/qml/Ooze/Files`. No se usan QtQuick Controls: solo primitivas
(`Item`, `Rectangle`, `Text`, `ListView`, `GridView`, `TextInput`, `PopupMenu`
propio) y tokens del singleton `Theme`.

`Theme` deriva colores, radios, tamaños y espaciados de `App.effectiveDark` y
`App.density`, de modo que cambiar de tema o densidad repinta sin recargar QML.
Los iconos se piden a `image://icons/<nombre>`; el proveedor cachea por
nombre+tamaño y usa el icono del tema con reserva genérica.

`IconTheme` elige el tema de iconos al arrancar y cuando cambia el esquema
claro/oscuro:

1. añade a las rutas de búsqueda `~/.local/share/icons`, `~/.icons`,
   `/run/current-system/sw/share/icons`, `/usr/share/icons` y los directorios
   XDG, útil en NixOS y en entornos sin portal;
2. respeta el tema que haya fijado el tema de plataforma de Qt y, si no hay,
   lee `qt6ct/qt6ct.conf`, `kdeglobals` (`[Icons] Theme`) y
   `gtk-3.0/settings.ini` (`gtk-icon-theme-name`);
3. verifica que el tema exista de verdad (`index.theme`) antes de usarlo, así
   que un tema configurado pero no instalado —por ejemplo Papirus— no deja la
   interfaz sin iconos;
4. en modo oscuro prefiere la variante `-dark`/`-Dark` cuando existe.

`IconProvider` traduce nombres estándar a alternativas por tema (`view-list` →
`view-list-details`, `object-select` → `checkmark`, …) y devuelve un icono
genérico si ninguno existe. Los iconos actuales son SVG: el paquete enlaza
`Qt6::Svg` y la compilación manual necesita ese módulo.

Los elementos ocultos (rol `hidden` de `FileListModel`) se dibujan con
`Theme.hiddenOpacity` en el icono, la miniatura y el nombre de ambas vistas, para
distinguirlos cuando «Mostrar ocultos» está activo.

Los menús son instancias de `PopupMenu` posicionadas en `Main.qml`, con una
capa transparente que los cierra al hacer clic fuera. Evita depender de
QtQuick Controls y mantiene el control del foco y del pintado.

Un menú nunca sale de la ventana: `PopupMenu` limita su tamaño al área
disponible y se desplaza con la rueda si no cabe. Ver [menus.md](menus.md).

El menú principal se genera desde `Actions.mainMenu` y la paleta de comandos
(`Ctrl+K`, `qml/components/CommandPalette.qml`) desde `Actions.commands`. Ambas
listas vienen de la misma tabla de `ActionRegistry`, que calcula `enabled` y
`checked` contra el controlador y emite `entriesChanged` solo cuando el
resultado cambia. Las acciones que necesitan interfaz emiten `uiRequested` y
`Main.qml` las traduce a diálogos, prompts o foco; el resto se ejecuta en C++.
Ver [actions.md](actions.md).

La selección por marco es un componente compartido,
`qml/components/MarqueeSelection.qml`, superpuesto a cada vista. Rechaza la
pulsación cuando hay un elemento debajo (para no romper clics ni el arrastre de
archivos), calcula las filas con la geometría de la vista y llama a
`App.selectRows` en lote. El controlador emite una sola señal por actualización
y la vista restaura la selección previa si el gesto se cancela con `Esc`. Ver
[plan-seleccion-por-arrastre.md](plan-seleccion-por-arrastre.md).

## Instancia única

Al arrancar se registra el nombre `org.ooze.Files` en el bus de sesión:

- Si el nombre está libre, se exporta `/org/ooze/Files` con la interfaz
  `org.ooze.Files` y esa instancia abre la ventana.
- Si ya está ocupado, el segundo proceso reenvía la petición y termina con 0.
- Si el bus no está disponible, la aplicación arranca igualmente sin instancia
  única.

Métodos exportados:

| Método | Efecto |
| --- | --- |
| `Open(s url)` | navega a `url` en la ventana existente |
| `OpenAndSelect(s url)` | navega al padre y selecciona el archivo |
| `NewTab(s url)` | reenvía la intención de pestaña |
| `NewWindow(s url)` | reenvía la intención de ventana |
| `Toggle()` | muestra u oculta la ventana |

Ejemplo:

```sh
gdbus call --session --dest org.ooze.Files \
  --object-path /org/ooze/Files \
  --method org.ooze.Files.OpenAndSelect "file:///home/ana/nota.txt"
```

`NewTab` abre una pestaña nueva en la instancia existente; si el arranque en frío
recibe `-t`, simplemente abre la primera pestaña, porque `-t` solo añade pestañas
cuando ya hay una instancia en marcha.

## Terminal

`TerminalLauncher` resuelve el emulador en este orden:

1. `behavior.terminal` de la configuración;
2. la variable de entorno `TERMINAL`;
3. una lista de candidatos conocidos (`foot`, `kitty`, `ghostty`, `alacritty`,
   `wezterm`, `konsole`, `kgx`, `gnome-terminal`, `cosmic-term`, `xterm`).

Cada terminal recibe su opción de directorio de trabajo (`--working-directory`,
`--directory`, `--workdir`, `start --cwd`, …). El proceso se lanza con
`QProcess::startDetached` y el directorio como `workingDirectory`, sin pasar por
un shell.

## Configuración

`Config` carga `~/.config/ooze-files/config.json`, valida cada valor contra una
lista cerrada y escribe con `QSaveFile` (escritura atómica). Los cambios se
agrupan con un `QTimer` de 300 ms para no golpear el disco al arrastrar un
deslizador de densidad o cambiar de tema varias veces seguidas.

Si el JSON es inválido se conservan los valores por defecto, se emite
`errorOccurred` y la barra de estado muestra el problema; nunca se sobrescribe
el archivo roto automáticamente.

## Decisiones descartadas

- **KIO**: añade dependencia de KDE Frameworks, procesos auxiliares y un modelo
  de hilos distinto al de Qt. GIO cubre papelera y gvfs con menos superficie.
- **Quickshell**: excelente para widgets, pero el proyecto necesita un
  controlador C++ propio y empaquetado de escritorio.
- **Rutas en `QString`**: pierde nombres no UTF-8 (ver arriba).
- **`QFileSystemModel`**: no permite controlar el orden de llegada de metadatos,
  no expone bytes y su refresco es difícil de presupuestar.

## Límites conocidos

- Sin vigilancia de pestañas inactivas en tiempo real: se marcan sucias y se
  refrescan al activarlas.
- Una sola ventana; varias carpetas solo mediante pestañas.
- `FileListProxyModel` usa la API de filtro disponible en cada versión de Qt;
  en Qt ≥ 6.9 usa `beginFilterChange`/`endFilterChange`.
