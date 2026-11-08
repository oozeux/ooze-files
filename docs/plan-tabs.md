# Plan: pestañas en Ooze Files

Estado: **implementado** (fases 0–4). Documentación de uso en
[tabs.md](tabs.md).

Este documento describe cómo añadir pestañas sin repetir el error de las vistas
divididas ni el de las columnas Miller. El objetivo no es solo abrir varias
carpetas: es que cada pestaña tenga su propio historial, selección, filtro y
estado de carga, sin multiplicar hilos ni refrescos en segundo plano.

## Objetivo

- Varias carpetas abiertas en la misma ventana.
- Navegación, historial, selección y filtro independientes por pestaña.
- Cambio de pestaña instantáneo cuando la carpeta no cambió.
- Solo la pestaña activa carga y se refresca; las demás se marcan como sucias.
- Sesión restaurable tras reiniciar.
- Integración con el lanzador: `ooze-files -t <ruta>` abre una pestaña nueva.
- Ningún hilo, temporizador ni repintado nuevo en reposo.

## Fuera de alcance

- Vista dividida y columnas Miller (descartadas).
- Reordenar pestañas arrastrando.
- Pestañas fijadas o agrupadas.
- Arrastrar archivos sobre una pestaña para soltarlos en su carpeta (posible
  fase 4, no necesaria para cerrar la funcionalidad).
- Ventanas múltiples reales. `--new-window` sigue comportándose como hasta
  ahora; es un problema separado de las pestañas.

## Decisiones de diseño

| Decisión | Elección | Motivo |
| --- | --- | --- |
| Estado por pestaña | `TabController` | Aísla modelo, proxy, historial, selección y filtro sin duplicar servicios globales |
| Servicios globales | Siguen en `ApplicationController` | Config, tema, lugares, dispositivos, portapapeles, operaciones y deshacer son de la aplicación, no de una carpeta |
| Carga de directorios | Un único `DirectoryLoader` compartido | `DirectoryLoader` crea 1 hilo de listado + 2 de stat; uno por pestaña serían 3 hilos por pestaña |
| Vigilancia | Un `DirectoryWatcher` por pestaña | inotify no avisa de cambios en pestañas no vigiladas; un solo watcher compartido perdería eventos |
| Refresco | Solo la pestaña activa | Las inactivas solo se marcan sucias; al activarlas se refrescan si hace falta |
| Deshacer | Global | Una operación de archivos puede afectar a varias carpetas; la pila no se parte |
| Portapapeles | Global | Igual que en cualquier gestor de archivos |
| Límite | 16 pestañas por defecto (1–32) | Evita crecimiento de memoria sin límite; configurable |
| Persistencia | `session` en `config.json` | Reutiliza `Config`, `QSaveFile` y el guardado diferido de 300 ms |
| Barra de pestañas | Solo visible con más de una pestaña | Quien usa una sola carpeta no paga altura ni repintado |
| Cierre de la última pestaña | Cierra la ventana | Comportamiento esperado; la pestaña no se borra hasta cerrar, para no dejar `App.files` nulo |

## Arquitectura

```
ApplicationController  (singleton QML "App")
├── Config, VfsRegistry, FileOperations, TerminalLauncher
├── ThemeStore, PortalSettings, DeviceMonitor, Places
├── UndoStack, portapapeles
├── DirectoryLoader compartido   (1 hilo de listado + 2 de stat)
├── TabListModel                 (modelo de la barra)
└── QVector<TabController*>      (pestañas)
        ├── FileListModel
        ├── FileListProxyModel
        ├── DirectoryWatcher
        ├── historial + índice
        ├── selección (URLs + ancla)
        ├── filtro
        ├── estado de carga/suciedad
        └── mensaje de estado y selección pendiente
```

`ApplicationController` conserva **la misma API QML** (`App.files`,
`App.currentUrl`, `App.goTo`, `App.selectAll`, etc.). La diferencia es que esas
propiedades y métodos se delegan a la pestaña activa. `App` sigue siendo el
único singleton que conoce QML; no se añade otro.

### Por qué no un `ApplicationController` por pestaña

Sería más uniforme, pero obligaría a extraer antes `DeviceMonitor`,
`PortalSettings`, `ThemeStore`, `FileOperations`, la pila de deshacer y el
registro de acciones a un `AppServices` compartido. Es un refactor más grande y
con más riesgo que extraer solo el estado de una carpeta. Si en el futuro hacen
falta ventanas independientes, `TabController` se puede promover a controlador
completo.

### `DirectoryLoader` compartido

Hoy cada `DirectoryLoader` posee dos `QThreadPool` (1 hilo de listado y 2 de
stat) y espera a que terminen en su destructor. Con 16 pestañas serían hasta 48
hilos y 16 esperas. En su lugar:

- `ApplicationController` crea **un** `DirectoryLoader` para toda la ventana.
- Se añaden dos operaciones:

```cpp
void attach(FileListModel *model, const QUrl &directory); // sin cargar
void detach();                                            // cancela e invalida
```

- `detach()` cancela el `CancelToken`, incrementa la generación, suelta el
  modelo y la carpeta, y pone `loading` a `false` (emitiendo
  `loadingChanged(false)` si hacía falta). Cualquier tarea antigua que llegue
  después se descarta por generación.
- `attach()` fija el modelo y la carpeta de la pestaña activa sin borrar filas.
- Al activar una pestaña: `detach()` de la anterior, `attach()` de la nueva y,
  solo si está sucia o a medias, `load()` o `refresh()`.
- El número de hilos de carga de directorios queda fijo en **3** para toda la
  aplicación, coincidiendo con lo que ya documenta `docs/performance.md`. La
  búsqueda recursiva conserva su hilo aparte, como ahora.

Para saber si una pestaña terminó de calcular metadatos se añade una señal
`statsFinished()` al loader, emitida cuando terminan todas las tareas de stat de
la generación actual. Una carpeta vacía se considera terminada en cuanto llega
`loadFinished` con cero filas. Si una pestaña se desactiva antes, se marca sucia
y se refresca al volver.

`refresh()` no emite `loadStarted` ni pone `loading` a `true` (comportamiento
actual); por eso el estado de carga de la pestaña se lleva en `TabController`,
no se deduce del loader.

### Vigilancia por pestaña

Cada `TabController` posee su `DirectoryWatcher`:

- Si la pestaña está activa, `changed()` llama a `loader->refresh()`. Si el
  loader está cargando, se marca `refreshPending` y se refresca al terminar.
- Si está inactiva, solo marca `dirty = true`. No se lee el directorio.
- `directoryGone()` en la activa replica el comportamiento actual: volver atrás
  o subir. En una inactiva se marca `directoryGone` y se resuelve al activarla.

Coste: un descriptor inotify, un `QSocketNotifier` y un temporizador de debounce
de 180 ms por pestaña. Con el límite de 16 es despreciable y evita el problema
de un watcher compartido que no ve cambios mientras mira otra carpeta.

## Estado por pestaña y estado global

| Por pestaña (`TabController`) | Global (`ApplicationController`) |
| --- | --- |
| `FileListModel` y `FileListProxyModel` | `Config` |
| URL actual, migas, espacio libre | Tema, paleta, densidad, tamaño de icono |
| Historial y posición | `viewMode`, `showHidden`, orden, miniaturas, panel de info |
| Selección, ancla y revisión | Lugares, dispositivos, portapapeles |
| Texto de filtro | `FileOperations`, `TerminalLauncher` |
| Selección pendiente | Pila de deshacer/rehacer |
| Mensaje de estado | `VfsRegistry`, `DirectoryLoader` compartido |
| `DirectoryWatcher` | `TabListModel`, pila de pestañas cerradas |
| Banderas `dirty`, `partial`, `settled` | Sesión y límite de pestañas |

El modo de vista (`grid`/`list`) y el orden son globales, como ahora. El filtro
es por pestaña porque es una búsqueda dentro de esa carpeta.

## API QML

### Propiedades de `App`

| Propiedad | Tipo | Notas |
| --- | --- | --- |
| `files` | `QAbstractItemModel*` | **Cambia de `CONSTANT` a `NOTIFY filesChanged`**; es el proxy de la pestaña activa |
| `tabs` | `QAbstractItemModel*` | `TabListModel`; `CONSTANT`, el objeto es estable |
| `activeTab` | `int` | Índice activo, `NOTIFY activeTabChanged` |
| `tabCount` | `int` | `NOTIFY tabsChanged` |
| `canReopenTab` | `bool` | `NOTIFY reopenStateChanged` |
| `restoreSession` | `bool` | Lectura/escritura, `NOTIFY` |
| `alwaysShowTabBar` | `bool` | Lectura/escritura, `NOTIFY` |
| `currentUrl`, `currentPath`, `breadcrumb`, `canGoBack`, `canGoForward`, `itemCount`, `freeSpaceText`, `loading`, `statusMessage`, `pendingSelection`, `selectionCount`, `selectionRevision`, `canRenameSelection`, `selectedIsDirectory`, `selectedName`, `filterText` | — | Se mantienen y se delegan a la pestaña activa |

`TabListModel` expone los roles `title`, `url` y `active`. `TabBar.qml` lo usa
directamente como `model`.

### Métodos nuevos

```cpp
// Gestión de pestañas
Q_INVOKABLE void newTab();                       // carpeta actual
Q_INVOKABLE void openInNewTab(const QString &url); // vacío = carpeta actual
Q_INVOKABLE void openRowInNewTab(int row);       // carpeta o archivo + selección
Q_INVOKABLE void activateTab(int index);
Q_INVOKABLE void closeTab(int index);
Q_INVOKABLE void closeOtherTabs(int index);
Q_INVOKABLE void closeTabsToRight(int index);
Q_INVOKABLE void nextTab();
Q_INVOKABLE void previousTab();
Q_INVOKABLE void reopenClosedTab();
Q_INVOKABLE void copyTabPath(int index);
```

Reglas:

- `openInNewTab("")` usa la carpeta de la pestaña activa.
- Si la URL es un archivo, la pestaña nueva abre su carpeta y deja el archivo
  como selección pendiente.
- Si se alcanza `maxTabs`, no se crea y se muestra un mensaje de estado; no se
  cierra ninguna pestaña existente.
- `closeTab` activa primero la vecina (derecha; si no, izquierda) y después
  elimina la pestaña. Así `App.files` nunca queda apuntando a un proxy borrado.
- Cerrar la última pestaña **no** la elimina: emite `closeWindowRequested` y
  QML cierra la ventana. La sesión se guarda antes de salir.
- `reopenClosedTab` guarda las últimas 10 pestañas cerradas con su URL e
  índice. Si la carpeta ya no existe, se descarta y se prueba la siguiente.

### Señales nuevas

```cpp
void tabsChanged();
void activeTabChanged();
void filesChanged();
void reopenStateChanged();
void closeWindowRequested();
```

`filesChanged` se emite al cambiar de pestaña, no en cada navegación: el objeto
proxy de una pestaña no cambia mientras vive.

## Interfaz

### `TabBar.qml`

Componente nuevo en `qml/panels/TabBar.qml`, sin `QtQuick.Controls` (el proyecto
es Qt Quick puro).

- `ListView` horizontal, `clip: true`, `boundsBehavior: StopAtBounds`.
- Delegado por pestaña:
  - icono de carpeta pequeño (o icono según el esquema de la URL);
  - título elidido en el centro;
  - borde inferior de acento de 2 px si está activa;
  - botón de cerrar visible al pasar el ratón o si está activa;
  - clic izquierdo activa, clic central cierra;
  - `WheelHandler` cambia a la pestaña siguiente/anterior;
  - clic derecho pide el menú contextual.
- Botón `+` al final para `App.newTab()`.
- Ancho por pestaña: entre 120 y 220 px, repartiendo el ancho disponible; si no
  caben, el `ListView` desplaza horizontalmente.
- Sin animaciones, sin blur, sin sombras, sin temporizadores.
- La barra solo se muestra cuando `App.tabCount > 1 ||
  App.alwaysShowTabBar`; con una pestaña mide 0 y no ocupa espacio.

`Theme.qml` gana `tabHeight` (30 px compacto, 34 normal, 38 cómodo, escalado con
`App.uiFontScale`).

### Encaje en `Main.qml`

Ahora `Sidebar`, `InfoPanel` y `content` empiezan en `header.bottom`. Se añade:

```qml
property Item chromeBottom: tabBar.visible ? tabBar : header
```

y esos tres elementos usan `anchors.top: root.chromeBottom.bottom`. `TabBar`
va anclado a `header.bottom`. Los menús que se abren bajo la cabecera usan
`chromeBottom` para no quedar tapados por la barra.

### Atajos

| Atajo | Acción |
| --- | --- |
| `Ctrl+T` | Pestaña nueva en la carpeta actual |
| `Ctrl+W` | Cerrar pestaña activa (la última cierra la ventana) |
| `Ctrl+Shift+T` | Reabrir última pestaña cerrada |
| `Ctrl+Tab` | Pestaña siguiente (circular) |
| `Ctrl+Shift+Tab` | Pestaña anterior (circular) |
| `Alt+1` … `Alt+8` | Activar pestaña 1–8 |
| `Alt+9` | Última pestaña |
| `Ctrl+Enter` / `Ctrl+Return` | Abrir la selección en pestaña nueva |
| Clic central en una carpeta | Abrir en pestaña nueva |

Los atajos de pestañas se desactivan con un diálogo modal abierto o mientras se
edita la ruta, para que `Ctrl+W` no cierre una pestaña mientras se escribe.
`Ctrl+Tab` y `Alt+n` tampoco deben robar el foco al campo de búsqueda.

### Menús y acciones

`ActionRegistry` gana un grupo «Pestaña» en el menú principal:

- Nueva pestaña (`tab.new`).
- Cerrar pestaña (`tab.close`).
- Reabrir pestaña cerrada (`tab.reopen`).
- Separador.
- Pestaña siguiente (`tab.next`).
- Pestaña anterior (`tab.prev`).

También se añade `tab.openInNewTab` para el menú contextual de una carpeta o
archivo seleccionado. La paleta de comandos los recoge automáticamente porque
salen de la misma tabla de acciones.

Menú contextual de una pestaña (clic derecho):

- Cerrar.
- Cerrar las demás.
- Cerrar las de la derecha.
- Copiar ruta.

### Al cambiar de pestaña

`Main.qml` escucha `App.activeTabChanged` y:

- cierra el menú abierto;
- cierra `preview`, `properties`, `searchOverlay`, `bulkRename` y `prompt`
  (están atados a filas de la pestaña anterior);
- termina la edición de ruta de `HeaderBar`;
- resetea `pendingRow`, `contextRow` y el `currentIndex` de grid/list;
- devuelve el foco a la vista activa.

La paleta de comandos puede quedarse abierta: sus acciones se aplican a la
pestaña nueva.

## Persistencia de sesión

Se añade al JSON existente, sin subir `version` (las claves nuevas son
compatibles hacia atrás):

```json
{
  "version": 1,
  "ui": {
    "tabBarAlways": false
  },
  "session": {
    "restore": true,
    "maxTabs": 16,
    "tabs": [
      "file:///home/oozenix",
      "file:///home/oozenix/Downloads"
    ],
    "active": 1
  }
}
```

`Config` gana:

- `restoreSession()` / `setRestoreSession(bool)` (`session.restore`).
- `maxTabs()` / `setMaxTabs(int)` (`session.maxTabs`, limitado a 1–32).
- `alwaysShowTabBar()` / `setAlwaysShowTabBar(bool)` (`ui.tabBarAlways`).
- `sessionTabs()` y `sessionActiveTab()`.
- `setSession(const QVariantList &tabs, int active)` con una sola señal
  `sessionChanged()`.

Reglas de guardado:

- Se guarda al abrir, cerrar, activar o navegar una pestaña, y al salir.
- `Config::scheduleSave()` ya agrupa escrituras cada 300 ms; no se añade otro
  temporizador.
- Al salir se fuerza `save()` para no perder el último cambio.
- No se guardan filtro, selección, scroll ni pestañas cerradas.

Reglas de restauración:

- Solo se restauran URLs locales que sigan siendo directorios existentes.
- Se descartan las inválidas, se recorta a `maxTabs` y se ajusta el índice
  activo.
- Si no queda ninguna válida, se usa `lastPath` y, si falla, `$HOME`.
- Si se lanzó con una ruta explícita (`ooze-files /ruta`, `--select`), no se
  restaura la sesión: se abre esa ruta en la primera pestaña.
- Si `session.restore` es `false`, se ignora la sesión guardada y se usa
  `lastPath`.

`lastPath` se sigue actualizando con la carpeta de la pestaña activa, para que
siga sirviendo de respaldo.

## Lanzador y D-Bus

- `SingleInstance::newTabRequested` se conecta a `openInNewTab`, no a
  `handleExternalOpen`.
- `ooze-files -t /ruta` con una instancia abierta crea y activa una pestaña.
- `ooze-files -t` sin ruta abre una pestaña en la carpeta activa.
- `ooze-files /ruta` con instancia abierta sigue navegando la pestaña activa,
  como ahora.
- En arranque en frío, `-t` no tiene efecto especial: se abre una sola pestaña
  con la ruta. Se documenta así para no complicar la restauración.
- `--new-window` no cambia en este plan; hoy reutiliza la ventana. Es una
  limitación conocida y separada.

## Casos límite

| Caso | Comportamiento |
| --- | --- |
| Cerrar la última pestaña | Emite `closeWindowRequested`; QML cierra la ventana; la pestaña sigue viva hasta salir |
| Cerrar una pestaña inactiva | Se elimina sin tocar el loader ni `App.files` |
| Cerrar la pestaña activa | Se activa la vecina, se emite `filesChanged`, luego se borra la antigua |
| Pestaña desactivada a mitad de carga | `detach()` cancela; se marca `partial`; al volver se hace `load()` completo |
| Pestaña desactivada antes de terminar los stat | Se marca `dirty`; al volver se hace `refresh()` |
| Carpeta borrada en pestaña inactiva | Se marca `directoryGone`; al activarla se vuelve atrás o se sube |
| Operación de archivos en curso | Se captura la pestaña de origen; al terminar se refresca esa pestaña y se marcan las demás sucias |
| Deshacer/rehacer | Igual: se refresca la pestaña afectada y se marcan las demás sucias |
| Copiar a una carpeta abierta en otra pestaña | Esa pestaña se marca sucia y se refresca al activarla |
| URL duplicada | Permitida; dos pestañas pueden mirar la misma carpeta con filtros distintos |
| Límite alcanzado | Mensaje de estado; no se crea la pestaña |
| URL remota o VFS sin watcher | Se abre si el backend existe; no se vigila; `refresh` manual con `F5` |
| Restauración con rutas inexistentes | Se descartan; si no queda ninguna, `lastPath`/`$HOME` |
| Cambio rápido de pestañas | `detach()` incrementa generación; resultados viejos se descartan |
| `Ctrl+W` editando la ruta | Desactivado por el guardián de atajos |

## Rendimiento

- **Hilos**: 3 en total (1 listado + 2 stat), no 3 por pestaña.
- **Carga**: solo la pestaña activa llama a `load()` o `refresh()`.
- **Cambio de pestaña limpio**: no lee el directorio; solo reengancha el modelo.
- **Cambio de pestaña sucio**: un `refresh()` asíncrono; las filas antiguas
  siguen visibles hasta que llega `syncEntries`, sin parpadeo.
- **Reposo**: ningún temporizador nuevo salvo los debounces de inotify de cada
  pestaña, que solo se activan con eventos reales.
- **Memoria**: cada pestaña conserva sus filas. Con 10 000 entradas y 16
  pestañas el peor caso puede ser alto; por eso hay límite. La medición no
  justificó descargar modelos inactivos, así que la LRU se descartó: introduciría
  recargas al volver a una pestaña.
- **Barra**: oculta con una pestaña; sin animaciones ni blur.
- **Sesión**: escritura diferida de `Config`, no en cada pulsación.

Medición prevista:

1. 10 pestañas con carpetas de 10 000 archivos.
2. RSS con `ps -o rss= -C ooze-files`.
3. CPU en reposo durante 60 s con 5 pestañas.
4. Descriptores con `ls /proc/$(pgrep -x ooze-files)/fd | wc -l`.
5. Comparar con la línea base de una pestaña.

## Pruebas

### Nuevo `tests/unit/tst_tabs.cpp`

| Caso | Qué comprueba |
| --- | --- |
| `startCreatesOneTab` | Arranque normal, `tabCount == 1`, `files()` no nulo |
| `newTabHasIndependentHistory` | Navegar A→B en la pestaña 1 y C en la 2; `goBack` en la 2 no toca la 1 |
| `selectionIsPerTab` | Seleccionar en 1, cambiar a 2, `selectionCount == 0`; volver y seguir seleccionado |
| `filterIsPerTab` | Filtro en 1, vacío en 2, se conserva al volver |
| `closeActivatesNeighbor` | Cerrar la del medio activa la derecha; cerrar la última activa la izquierda |
| `lastTabRequestsWindowClose` | `QSignalSpy` sobre `closeWindowRequested`; la pestaña no se borra |
| `reopenRestoresUrlAndIndex` | Cerrar y reabrir conserva URL e índice |
| `openInNewTabSelectsFile` | URL de archivo abre su carpeta y deja `pendingSelection` |
| `tabLimitIsEnforced` | `maxTabs = 2`; la tercera no se crea y hay mensaje |
| `sessionRoundTrip` | Guardar 3 pestañas, cargar otra `Config`, mismo orden e índice |
| `sessionSkipsMissingDirectories` | Una ruta borrada se descarta |
| `inactiveTabRefreshesOnActivation` | Crear archivo en pestaña inactiva; su modelo no cambia hasta activarla |

Los tests de inotify usan `QTRY_VERIFY_WITH_TIMEOUT` y `QTemporaryDir`, como
`tst_watcher`. Si inotify no está disponible, se saltan como ya hace ese test.

### Otros tests

- `tst_config`: valores por defecto de `session`, parseo, límite 1–32 y claves
  desconocidas.
- `tst_controller`: sigue pasando sin cambios; se añaden comprobaciones de que
  las acciones `tab.*` existen y de que `tab.openInNewTab` está habilitada con
  una carpeta seleccionada.
- `smoke-ui`: sigue cargando `Main.qml`; el componente `TabBar` se instancia
  aunque esté oculto, así que un error de QML se detecta en CI.

### Validación manual

1. Abrir dos pestañas con `Ctrl+T`, navegar distinto en cada una.
2. Comprobar historial (`Alt+Left`), selección y filtro independientes.
3. Cerrar la pestaña activa y la inactiva; comprobar la vecina activada.
4. `Ctrl+Shift+T` reabre en la posición correcta.
5. Reiniciar y comprobar que se restauran URL e índice activo.
6. Borrar una carpeta de la sesión y reiniciar; se descarta sin fallar.
7. `ooze-files -t /tmp/ooze-tabs/a` con la app abierta; se añade pestaña.
8. Clic central en una carpeta y `Ctrl+Enter`; abren pestaña nueva.
9. Abrir 16 pestañas; la 17 muestra el límite.
10. Comprobar CPU en reposo, RSS y descriptores.
11. Comprobar que `Ctrl+W` no cierra pestañas mientras se edita la ruta.
12. Comprobar que los overlays se cierran al cambiar de pestaña.

## Fases de implementación

### Fase 0 — Base verde (completada)

El plan `docs/plan-tst-openwith.md` ya está implementado: `ctest` pasa 15/15
sin `-E '^openwith$'`. Sin esta base, cualquier regresión del refactor se
confundiría con el fallo ambiental de GLib.

### Fase 1 — Extraer `TabController` (completada)

- Crear `TabController` y mover modelo, proxy, historial, selección, filtro,
  estado y watcher.
- `ApplicationController` conserva servicios globales y delega en la pestaña
  activa.
- `DirectoryLoader` gana `attach()`/`detach()` y `statsFinished()`.
- `files` pasa a `NOTIFY`, pero solo hay una pestaña.
- `CMakeLists.txt` y `tests/CMakeLists.txt` incluyen los archivos nuevos.

Aceptación: comportamiento idéntico con una pestaña, `ctest` 15/15, sin
advertencias QML nuevas, mismos hilos (3) y misma memoria en reposo.

### Fase 2 — Pestañas visibles (completada)

- `TabListModel`, `TabBar.qml`, `Theme.tabHeight`.
- Abrir, cerrar, activar, siguiente/anterior.
- Atajos, acciones y menú «Pestaña».
- Reset de vistas y cierre de overlays al cambiar.
- `tst_tabs` básico.

Aceptación: varias pestañas independientes, sin sesión todavía; `ctest` 16/16;
sin CPU en reposo.

### Fase 3 — Sesión y lanzador (completada)

- Claves `session` en `Config` y toggles en Ajustes.
- Restauración, guardado al salir, límite.
- `SingleInstance::newTabRequested` → `openInNewTab`.
- Reabrir pestaña cerrada.
- «Abrir en pestaña nueva» en menú contextual, clic central y `Ctrl+Enter`.
- Cierre de la última pestaña.

Aceptación: sesión exacta tras reiniciar, `-t` funciona con instancia abierta,
rutas inválidas no rompen el arranque.

### Fase 4 — Pulido (completada)

- Menú contextual de pestaña (cerrar las demás / a la derecha).
- Desbordamiento horizontal y rueda.
- Toggle «Mostrar siempre la barra de pestañas».
- Medición de memoria; descarga LRU descartada: el límite de 16 pestañas y el
  consumo medido no la justifican.
- Scroll y fila actual se reinician al cambiar de pestaña (v1); se documenta el
  comportamiento y queda como mejora opcional.
- Documentación final.

## Riesgos y mitigaciones

| Riesgo | Mitigación |
| --- | --- |
| El refactor de fase 1 rompe navegación o selección | Fase 1 sin cambios de UI; tests existentes; revisar `git diff` por partes |
| `files` con `NOTIFY` provoca rebind y pérdida de scroll | Aceptado en v1; reset explícito de `currentIndex`; conservar posición queda como mejora opcional |
| Cambiar de pestaña a mitad de carga mezcla filas | `detach()` cancela y sube generación; `load()` completo si `partial` |
| Stat incompletos al desactivar | `statsFinished()` + marca `dirty` + `refresh()` al volver |
| Demasiados descriptores inotify | Límite de 16 pestañas; el watcher tolera fallos de `inotify_add_watch` |
| Cierre de pestaña activa deja `App.files` colgando | Activar vecina y emitir `filesChanged` antes de borrar |
| Sesión con rutas muertas | Validar, recortar, fallback a `lastPath`/`$HOME`; nunca fallar el arranque |
| Deshacer afecta a una pestaña inactiva | Marcar todas sucias y refrescar al activar |
| `Ctrl+W` cierra mientras se escribe | Guardián `textInputFocused` y diálogos |
| Memoria con muchas carpetas grandes | Límite 16; LRU descartada tras medir |
| `--new-window` sigue mal | Documentado como fuera de alcance; tarea separada |

## Criterios de aceptación

- `ctest` completo en verde (16/16: 15 actuales + `tabs`).
- Con una pestaña, la aplicación se comporta exactamente como antes.
- Historial, selección, filtro y estado de carga son independientes por pestaña.
- Cambiar a una pestaña limpia no lee el directorio ni repinta de más.
- Cerrar, reabrir y restaurar pestañas conserva URL e índice.
- `ooze-files -t` con instancia abierta añade pestaña.
- La última pestaña cierra la ventana y guarda la sesión.
- En reposo con 5 pestañas: 0 % de CPU y sin repintados.
- Sin advertencias QML nuevas.
- Documentación actualizada: `tabs.md`, `architecture.md`, `configuration.md`,
  `actions.md`, `performance.md`, `menus.md`, `README.md`.

## Archivos previstos

| Archivo | Cambio |
| --- | --- |
| `src/app/TabController.h/.cpp` | Nuevo: estado y lógica de una pestaña |
| `src/app/TabListModel.h/.cpp` | Nuevo: modelo de la barra |
| `src/app/ApplicationController.h/.cpp` | Delegación a la pestaña activa, gestión de pestañas y sesión |
| `src/core/DirectoryLoader.h/.cpp` | `attach()`, `detach()`, `statsFinished()` |
| `src/settings/Config.h/.cpp` | Claves `session` y `ui.tabBarAlways` |
| `src/app/main.cpp` | `start` con ruta explícita, `NewTab`, guardado al salir |
| `src/app/ActionRegistry.cpp` | Acciones y menú de pestañas |
| `qml/panels/TabBar.qml` | Nuevo componente |
| `qml/Main.qml` | Encaje, atajos, menús, overlays |
| `qml/theme/Theme.qml` | `tabHeight` |
| `qml/pages/SettingsWindow.qml` | Toggles de sesión y barra |
| `qml/views/FileGridView.qml`, `FileListView.qml` | Clic central y reset al cambiar de pestaña |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | Fuentes y test nuevos |
| `tests/unit/tst_tabs.cpp` | Nuevo |
| `tests/unit/tst_config.cpp` | Casos de sesión |
| `docs/*` | Documentación |

## Decisiones adoptadas

Valores confirmados e implementados:

1. Cerrar la última pestaña cierra la ventana.
2. Restaurar la sesión al iniciar, activado por defecto.
3. Barra visible solo con más de una pestaña, con opción «mostrar siempre».
4. Límite de 16 pestañas, configurable en JSON entre 1 y 32.
5. `ooze-files /ruta` con instancia abierta navega la pestaña activa; para una
   pestaña nueva se usa `-t`.
6. El scroll y la fila actual se reinician al cambiar de pestaña en v1; queda
   como mejora opcional.
7. El menú contextual de pestaña está implementado en `Main.qml`.

## Resultado

La implementación final cumple el plan con estas notas:

- un solo `DirectoryLoader` compartido mantiene los 3 hilos originales;
- las pestañas inactivas se marcan sucias y solo refrescan al activarse;
- la sesión se guarda en `Config` y se fuerza en `aboutToQuit`;
- `tst_tabs` cubre creación, independencia de estado, cierre, reapertura,
  duplicado, límite, modelo de la barra y restauración de sesión;
- `ctest` pasa 16/16 y el smoke test sin interfaz arranca con dos pestañas
  restauradas sin advertencias QML.
