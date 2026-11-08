# Ooze Files 0.1.46

Administrador de archivos minimalista para Wayland, escrito en C++20 y Qt 6/QML.
Pensado como gestor de archivos diario: rápido, sin dependencias de escritorio y
con una interfaz que respeta el tema del sistema.

Estado: **alfa**. La navegación local es funcional y estable; las operaciones de
archivos y los backends remotos están en fases posteriores (ver
[Estado y hoja de ruta](#estado-y-hoja-de-ruta)).

## Características actuales

- Navegación local con listado asíncrono en dos fases (nombres primero, metadatos
  después) e índices estables durante la carga.
- Pestañas con estado independiente (historial, selección, filtro, orden y carga),
  barra que solo aparece con más de una pestaña, reapertura de pestañas cerradas y
  restauración de sesión al iniciar.
- Historial atrás/adelante, subir carpeta, edición de ruta con autocompletado
  visual por migas de pan.
- Barra lateral de lugares XDG y del sistema.
- Vista de cuadrícula y de lista, con densidad compacta/normal/cómoda.
- Ordenación por nombre, tamaño, fecha o tipo, con carpetas siempre primero y
  orden ascendente/descendente.
- Filtro instantáneo por nombre dentro de la carpeta actual.
- Archivos ocultos visibles u ocultos (Ctrl+H).
- Apertura de archivos con la aplicación predeterminada y de la carpeta actual
  en la terminal.
- Temas claro, oscuro y automático según el esquema del sistema, con paletas
  en `~/.config/ooze-files/themes/*.json` o importadas de OozeShell/Matugen,
  qt6ct y KDE (solo lectura), acento del sistema por portal y tipografía
  configurable.
- Instancia única por D-Bus: abrir una carpeta desde otra aplicación reutiliza la
  ventana existente.
- Iconos del tema del sistema con caché en memoria y emblema en los enlaces
  simbólicos.
- Arrastrar y soltar: tira flotante con el icono y el nombre (sin fondo, con
  `(+N)` en selecciones múltiples), menú al soltar (mover, copiar o enlazar),
  destinos en carpetas y lugares, y exportación de la selección como
  `text/uri-list` a otros programas.
- «Abrir con» en el menú contextual: lista las aplicaciones registradas para el
  tipo del archivo (GIO), con la predeterminada primero.
- Vista previa rápida con `Espacio`: imágenes, texto, Markdown renderizado,
  póster de vídeo, primera página de PDF y carpetas, con panel de propiedades
  (tamaño, dimensiones, fechas, permisos, propietario) y navegación con las
  flechas. Las vistas de vídeo/PDF usan `ffmpeg`/`pdftoppm` si están instalados y
  lo explican si faltan.
- Miniaturas de imágenes y pósteres de vídeo en la cuadrícula (opcionales), con
  caché compartida en `~/.cache/thumbnails` y tamaño de iconos ajustable
  (pequeño, normal, grande, muy grande).
- Búsqueda recursiva con filtros por tipo (`Ctrl+Mayús+F`), con resultados en
  vivo, cancelación y tope de 2000 entradas.
- Renombrado en lote (`Mayús+F2`): buscar/reemplazar (texto o regex),
  prefijo/sufijo y numeración, con vista previa y detección de conflictos.
- Modo ligero opcional para reducir el consumo de memoria en reposo.
- Menú principal compacto con submenús (Vista, Tamaño de iconos, Densidad,
  Ordenar por, Tema, Nuevo, Herramientas) y configuración como página completa.
- Deshacer y rehacer (`Ctrl+Z`) para mover, copiar, renombrar, crear, comprimir y
  enviar a la papelera.
- Comprimir (zip, tar.gz, tar.xz, tar.zst, 7z) y extraer desde el menú
  contextual, con las herramientas del sistema.
- Dispositivos en la barra lateral con montar, desmontar y expulsar (UDisks2) y
  carpetas propias en «Lugares».
- Panel de información (`F11`) y diálogo de propiedades (`Alt+Intro`).
- Configuración en una ventana flotante con pestañas, incluido el selector de
  aplicaciones predeterminadas por tipo de archivo.
- Configuración legible y editable a mano en JSON.
- Paleta de comandos (`Ctrl+K`): busca por nombre, identificador o palabras clave
  y ejecuta cualquiera de las acciones registradas sin pasar por los menús.
- Selección por marco: arrastrar con el botón izquierdo en un hueco de la vista
  selecciona lo que queda dentro; `Ctrl` alterna, `Mayús` añade, `Esc` cancela,
  el borde superior/inferior desplaza la vista y un clic en un hueco limpia la
  selección.

## Requisitos

- Linux con Wayland (probado en Niri; X11 debería funcionar sin garantías).
- Qt 6.5 o superior: `Core`, `Gui`, `Qml`, `Quick`, `DBus`, `Svg`, `Test`.
  `Svg` es necesario para los iconos de los temas actuales.
- CMake 3.21+, Ninja (o Make) y un compilador con C++20.
- Un tema de iconos instalado (por ejemplo Breeze, Papirus o Adwaita).

Dependencias opcionales (la aplicación funciona sin ellas y avisa en
Configuración → Comportamiento):

- `ffmpeg` y `ffprobe`: pósteres y miniaturas de vídeo;
- `poppler-utils` (`pdftoppm`): primera página de los PDF;
- `squashfs-tools` (`unsquashfs`): iconos incrustados en AppImage.

## Compilación

Con Nix (entorno reproducible incluido):

```sh
nix develop -f nix/devshell.nix
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Con los scripts incluidos (ver [docs/scripts.md](docs/scripts.md)):

```sh
./build.sh -d -t
./run.sh -d
```

Compilación directa, con Qt 6 en el sistema:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Instalación:

```sh
cmake --install build --prefix ~/.local
```

El paquete de Nix está en `nix/package.nix` y el flake queda fijado en
`flake.lock` (rama `nixos-26.05`, Qt 6.11):

```sh
nix build .#default
```

## Uso

```sh
ooze-files                 # restaura la sesión o abre la última carpeta / $HOME
ooze-files ~/Descargas     # carpeta concreta en la pestaña activa
ooze-files ~/nota.txt      # abre la carpeta y selecciona el archivo
ooze-files --select ~/a.txt
ooze-files --new-tab ~/tmp # pestaña nueva si ya hay una instancia abierta
ooze-files --new-window ~/tmp
ooze-files --debug         # registro detallado
```

`--new-tab` (`-t`) añade una pestaña a la instancia existente; en un arranque en
frío simplemente abre la primera pestaña. `--new-window` (`-n`) o **Ctrl+N**
abren una ventana independiente (otro proceso). Esas ventanas no guardan cambios
de configuración ni de sesión, para no pisar los de la ventana principal. Al arrancar sin ruta explícita se restauran las pestañas de la
sesión anterior (configurable en Ajustes → Comportamiento → Pestañas).

### Atajos

| Atajo | Acción |
| --- | --- |
| `Ctrl+L` | Editar la ruta actual |
| `Ctrl+F` | Enfocar el filtro |
| `Ctrl+K` | Paleta de comandos |
| `Ctrl+H` | Mostrar u ocultar archivos ocultos |
| `Ctrl+,` | Abrir la configuración |
| `Ctrl+T` | Nueva pestaña en la carpeta actual |
| `Ctrl+W` | Cerrar la pestaña activa (la última cierra la ventana) |
| `Ctrl+Shift+T` | Reabrir la última pestaña cerrada |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Pestaña siguiente / anterior |
| `Alt+1` … `Alt+8` | Activar la pestaña 1–8 |
| `Alt+9` | Última pestaña |
| `Ctrl+Enter` | Abrir la selección en una pestaña nueva |
| `Ctrl+Shift+N` | Nueva carpeta |
| `Ctrl+A` | Seleccionar todo |
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copiar / cortar / pegar |
| `Ctrl+Q` | Salir |
| `Shift+F4` | Abrir una terminal en la carpeta actual |
| `F2` | Renombrar la selección |
| `Supr` | Enviar a la papelera |
| `Shift+Supr` | Eliminar permanentemente |
| `F5` | Recargar la carpeta |
| `Alt+←` / `Alt+→` | Atrás / adelante |
| `Alt+↑` | Subir de carpeta |
| `Retroceso` | Subir de carpeta (con el listado enfocado) |
| `Enter` | Abrir la selección |
| `Esc` | Cerrar menús y diálogos; limpiar el filtro |
| `Mayús+F10` / `Menú` | Menú contextual de la selección |
| Flechas | Moverse por el listado |
| `Ctrl`/`Mayús` + clic | Selección múltiple |
| Arrastrar en un hueco | Marco de selección: `Ctrl` alterna, `Mayús` añade, `Esc` cancela |
| Clic en un hueco | Cancela la selección actual |
| Arrastrar cerca del borde | Desplaza la vista mientras el marco sigue activo |

El clic derecho sobre un elemento abre su menú contextual; sobre el fondo de la
carpeta abre el menú de la carpeta (nueva carpeta, pegar, recargar, …). El clic
central o `Ctrl+Enter` sobre una carpeta la abre en una pestaña nueva, y el clic
central sobre una pestaña la cierra.

Arrastrar un elemento publica `text/uri-list` para otros programas y una tira con
su icono y su nombre sigue al puntero. Al soltar sobre una carpeta, un lugar de
la barra lateral o la carpeta actual aparece un menú para elegir **mover**,
**copiar** o **enlazar** (enlace simbólico); `Esc` cancela. Reglas completas en
[docs/drag-and-drop.md](docs/drag-and-drop.md).

### Integración con el escritorio

`packaging/ooze-files.desktop` declara `MimeType=inode/directory`, así que puede
registrarse como manejador de carpetas. La instancia única expone
`org.ooze.Files` en `/org/ooze/Files`; la API está documentada en
[docs/architecture.md](docs/architecture.md#instancia-única).

## Configuración

La configuración vive en `~/.config/ooze-files/config.json` y se guarda sola tras
cada cambio. Todas las claves, valores válidos y ejemplos están en
[docs/configuration.md](docs/configuration.md).

## Documentación

- [Arquitectura](docs/architecture.md): capas, hilos, pestañas, rutas en bytes,
  D-Bus.
- [Pestañas y sesión](docs/tabs.md): atajos, estado por pestaña, barra y
  restauración.
- [Capa VFS](docs/vfs.md): contrato de backends y cómo añadir uno.
- [Arrastrar y soltar](docs/drag-and-drop.md): comportamiento, límites y
  verificación.
- [Plan: selección por arrastre](docs/plan-seleccion-por-arrastre.md): marco de
  selección, geometría, modos y verificación.
- [Acciones y paleta de comandos](docs/actions.md): tabla única de acciones,
  menú principal y búsqueda con `Ctrl+K`.
- [Configuración](docs/configuration.md): archivo JSON, CLI y opciones de CMake.
- [Temas y ajustes de Qt 6](docs/temas.md): qué se hereda del escritorio, qué no
  y plan de temas externos.
- [Menús emergentes](docs/menus.md): tamaño, posición, scroll y submenús.
- [Vista previa, Abrir con y miniaturas](docs/preview.md).
- [Deshacer y rehacer](docs/undo.md).
- [Recientes](docs/recents.md): origen de datos y qué significa «reciente».
- [Papelera](docs/trash.md): integración con la papelera de freedesktop.
- [Comprimir y extraer](docs/archives.md).
- [Dispositivos y lugares propios](docs/devices.md).
- [Ventana de configuración](docs/settings-window.md).
- [Scripts](docs/scripts.md): `build.sh`, `run.sh` e `install-desktop.sh`.
- [Integración con el escritorio](docs/desktop-integration.md): lanzador, gestor de carpetas por defecto y `xdg-open`.
- [Idiomas](docs/i18n.md): español e inglés.
- [Rendimiento](docs/performance.md): presupuestos y decisiones de coste.
- [Seguridad](docs/security.md): modelo de amenazas y límites.
- [FileManager.md](FileManager.md): plan de arquitectura completo y fases.

## Estructura

```
src/core/        modelo, listado, rutas, lugares, terminal
src/vfs/         contrato VFS, registro y backend local POSIX
src/settings/    configuración JSON
src/app/         controlador QML, instancia única, iconos, main
qml/             interfaz (Qt Quick sin QtQuick Controls)
tests/unit/      pruebas de rutas, configuración, VFS y modelo
nix/             devshell y paquete
packaging/       archivo .desktop e iconos
```

## Pruebas

```sh
ctest --test-dir build --output-on-failure
```

Incluye pruebas de rutas en bytes, configuración, backend local, modelo proxy,
controlador (incluido el arrastre y la soltura), cargador de temas y una prueba
de humo que carga la interfaz completa en modo offscreen.

## Estado y hoja de ruta

Implementado:

- Navegación, historial, lugares, vistas, ordenación, filtro, temas, instancia
  única, terminal, configuración, iconos.
- Paletas en JSON (`ui.palette`), importación de solo lectura de OozeShell,
  qt6ct y KDE, tema de iconos, escala y familia tipográficas configurables;
  selector de paleta con vista previa y acceso a su carpeta en Ajustes.
- Operaciones de archivos: copiar/mover con `FICLONE` cuando sea posible,
  renombrar atómico, papelera vía GIO, creación de carpetas y archivos.
- Selección múltiple, menú contextual (elemento y fondo), atajos de teclado,
  diálogo de configuración.
- Vigilancia de directorios con `inotify` y refresco incremental.
- Arrastrar y soltar: salida como `text/uri-list`, tira flotante con icono y
  nombre, soltura en carpetas, lugares y fondo de la vista, con menú para mover,
  copiar o enlazar.
- Registro único de acciones (`ActionRegistry`): el menú principal y la paleta
  de comandos se generan de la misma tabla, con estado habilitado/marcado en
  vivo. Ver [docs/actions.md](docs/actions.md).
- Selección por marco en cuadrícula y lista, con modos reemplazar/añadir/alternar,
  restauración con `Esc` y desplazamiento automático en los bordes. Ver
  [docs/plan-seleccion-por-arrastre.md](docs/plan-seleccion-por-arrastre.md).
- Pestañas completas: estado independiente por carpeta, barra con desbordamiento,
  menú contextual, reapertura, límite configurable, clic central/`Ctrl+Enter`,
  restauración de sesión y `--new-tab` con instancia única. Ver
  [docs/tabs.md](docs/tabs.md) y [docs/plan-tabs.md](docs/plan-tabs.md).

Pendiente, en orden:

1. Temas externos: queda la exportación opcional a formatos qt6ct/KDE. Plan y
   estado en [docs/temas.md](docs/temas.md).
2. Ventanas múltiples reales.
3. Backend gvfs/MTP y dispositivos extraíbles.
4. Archivos comprimidos con libarchive y búsqueda recursiva.

## Licencia

GPL-3.0-or-later. Ver [LICENSE](LICENSE).
