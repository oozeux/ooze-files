# Configuración

## Archivo

Ruta: `$XDG_CONFIG_HOME/ooze-files/config.json` (normalmente
`~/.config/ooze-files/config.json`).

Se crea con los valores por defecto en el primer arranque y se reescribe de forma
atómica (`QSaveFile`) 300 ms después del último cambio. Es editable a mano: al
iniciar se valida cada clave y cualquier valor desconocido se ignora conservando
el valor por defecto. Un JSON inválido no se sobrescribe: se mantienen los
valores por defecto y el error aparece en la barra de estado y en el registro.

La mayoría de las claves también se editan desde la aplicación: el diálogo
`Configuración…` (menú principal o `Ctrl+,`) expone tema, paleta, tipografía,
densidad, vista, archivos ocultos, apertura con un solo clic, comando de
terminal, restauración de sesión y visibilidad permanente de la barra de
pestañas, y guarda automáticamente en este mismo archivo. El mismo diálogo
muestra una vista previa de la paleta y abre su carpeta.

Estructura completa:

```json
{
    "version": 1,
    "ui": {
        "theme": "system",
        "palette": "builtin",
        "iconTheme": "",
        "fontScale": 1.0,
        "fontFamily": "",
        "density": "normal",
        "language": "system",
        "iconSize": "normal",
        "thumbnails": true,
        "infoPanel": false,
        "lightweight": false,
        "viewMode": "auto",
        "tabBarAlways": false
    },
    "listing": {
        "showHidden": false,
        "sortMode": 0,
        "sortDescending": false,
        "singleClickOpen": false
    },
    "behavior": {
        "terminal": "",
        "singleInstance": true,
        "lastPath": ""
    },
    "session": {
        "restore": true,
        "maxTabs": 16,
        "tabs": [],
        "active": 0
    }
}
```

### `ui.theme`

| Valor | Efecto |
| --- | --- |
| `system` | sigue el esquema claro/oscuro del sistema (por defecto) |
| `light` | fuerza tema claro |
| `dark` | fuerza tema oscuro |

El cambio es inmediato; no requiere reiniciar.

### `ui.palette`

Identificador de la paleta de colores. Valores admitidos:

| Valor | Origen |
| --- | --- |
| `builtin` | par claro/oscuro incorporado (por defecto) |
| `<id>` | `$XDG_CONFIG_HOME/ooze-files/themes/<id>.json`; el id solo admite letras, números, punto, guion y guion bajo |
| `system:ooze` | `~/.cache/oozeshell/colors.json` (OozeShell/Matugen, claro y oscuro) |
| `system:qt6ct` | esquema activo de `~/.config/qt6ct/qt6ct.conf` |
| `system:kde` | esquema activo de `~/.config/kdeglobals` |

Las fuentes `system:*` son de solo lectura y solo aparecen en el selector de
Ajustes si existen en el sistema. Como qt6ct y KDE describen un único modo, su
esquema se usa tal cual y el contraste de la selección se deduce del fondo.

El tema elige qué mitad usar (`light` o `dark`) según `ui.theme`. Si la fuente
no existe, no es JSON válido o declara un `schema` desconocido, se vuelve al
tema incorporado, se registra el motivo y la barra de estado lo muestra.

Formato del archivo, tablas de importación y recarga: [temas.md](temas.md).

### Acento del sistema

Con `ui.palette` en `builtin`, el acento se toma de
`org.freedesktop.portal.Settings` (`org.freedesktop.appearance`,
`accent-color`) y se actualiza en vivo si el escritorio lo cambia. Si el portal
no existe o no implementa la clave se mantiene el acento incorporado.

Un tema propio o una fuente `system:*` ya trae su acento y **no** se sobrescribe
con el del portal. El mismo portal sirve de respaldo para `ui.theme = system`
cuando Qt no informa de un esquema claro/oscuro.

### `ui.iconTheme`

Nombre de un tema de iconos instalado (por ejemplo `Papirus-Dark`). Vacío
significa detección automática: tema de plataforma de Qt, `qt6ct`, `kdeglobals`,
GTK y, por último, Papirus/breeze/Adwaita. Un nombre desconocido se ignora.

### `ui.fontScale`

Multiplicador de la tipografía y de las alturas de fila, cabecera, barra de
estado y barra lateral. Rango `0.75`–`2.0`; por omisión `1.0`. La cuadrícula
mantiene el tamaño de celda.

### `ui.fontFamily`

Familia tipográfica de la interfaz (por ejemplo `Noto Sans`). Vacío usa la
tipografía del sistema (`QGuiApplication::font()`). Se admiten hasta 128
caracteres; se descartan barras y caracteres de control. Un nombre que no exista
se deja tal cual y Qt cae en la fuente predeterminada.

### `ui.language`

| Valor | Efecto |
| --- | --- |
| `system` | Español si el locale del sistema empieza por `es`; inglés en cualquier otro caso. |
| `es` | Español. |
| `en` | Inglés. |

Se aplica al momento, sin reiniciar. Ver [idiomas](i18n.md).

### `ui.density`

| Valor | Altura de fila | Espaciado | Tamaño de fuente |
| --- | --- | --- | --- |
| `compact` | 28 px | 6 px | 12 px |
| `normal` | 34 px | 10 px | 13 px |
| `comfortable` | 40 px | 14 px | 14 px |

Afecta a cabecera, barra lateral, barra de estado y vistas.

### `ui.viewMode`

| Valor | Efecto |
| --- | --- |
| `auto` | cuadrícula (la elección por número de elementos llegará más adelante) |
| `grid` | fuerza cuadrícula |
| `list` | fuerza lista |

### `ui.iconSize`

Tamaño de los iconos de archivos, independiente de la densidad. No afecta a los
iconos de la interfaz (cabecera, barra lateral).

| Valor | Cuadrícula | Lista |
| --- | --- | --- |
| `small` | 40 px | 16 px |
| `normal` | 56 px (por defecto) | 18 px (16 px en densidad compacta) |
| `large` | 80 px | 28 px |
| `huge` | 112 px | 40 px |

En lista la altura de fila crece lo necesario para que el icono quepa. Un valor
desconocido vuelve a `normal`.

### `ui.thumbnails`

`true` (por defecto) muestra miniaturas reales de las imágenes en la vista de
cuadrícula. Detalles en [preview.md](preview.md). Con `false` se usan siempre los
iconos del tema.

### `ui.infoPanel`

`true` muestra el panel lateral de información (también con `F11`). Por defecto
`false`. Ver [preview.md](preview.md).

### `ui.lightweight`

`true` usa el backend de Qt Quick por software (`QT_QUICK_BACKEND=software`) para
reducir la memoria a costa de más CPU al desplazar. En un equipo con NVIDIA el
proceso baja de ~190 MB a ~125 MB de RSS en reposo. Se lee antes de crear
`QGuiApplication`, así que **requiere reiniciar**; la variable de entorno
`OOZE_FILES_LIGHTWEIGHT=1` fuerza el modo sin tocar el archivo. Ver
[performance.md](performance.md).

### `places`

Lista de carpetas propias que aparecen en «Lugares»:

```json
"places": [
    { "name": "Proyectos", "path": "/home/yo/Proyectos" }
]
```

Reglas: la ruta debe ser absoluta y no puede ser `/`; se normaliza y se
descartan duplicados y entradas inválidas; sin nombre se usa el de la carpeta;
máximo 64 entradas. Un lugar propio que coincide con uno integrado (por ejemplo
`~/Documentos`) no se muestra dos veces: prevalece el integrado. Las carpetas que
ya no existen se ocultan sin borrarse del fichero. Se gestiona desde la interfaz con «Añadir a Lugares» y «Quitar de
Lugares».

### `placesOrder`

Orden personalizado de la sección «Lugares», como lista de rutas absolutas
normalizadas. Incluye tanto los lugares integrados (Inicio y carpetas XDG) como
los propios. Se identifica cada lugar por su ruta y no por su nombre, así que el
orden sobrevive a un cambio de idioma.

```json
"placesOrder": ["/home/yo", "/home/yo/Proyectos", "/home/yo/Descargas"]
```

Reglas: rutas absolutas, sin duplicados, máximo 160. Los lugares que no figuran
en la lista (por ejemplo una carpeta XDG nueva) se colocan al final en su orden
por defecto. Las rutas que ya no existen se conservan en la lista para que el
lugar recupere su posición si vuelve, salvo las de lugares propios quitados con
«Quitar de Lugares», que se eliminan. Se escribe desde la interfaz al arrastrar
o al usar «Subir» y «Bajar».

### `listing.showHidden`

`true` muestra los archivos que empiezan por punto. Atajo: `Ctrl+H`.

### `listing.sortMode`

| Valor | Criterio |
| --- | --- |
| `0` | nombre (`QCollator`, orden numérico, sin distinguir mayúsculas) |
| `1` | tamaño |
| `2` | fecha de modificación |
| `3` | tipo (carpeta/archivo y extensión) |

Las carpetas van siempre primero, también en orden descendente.

### `listing.sortDescending`

`true` invierte el criterio manteniendo las carpetas delante.

### `listing.singleClickOpen`

`true` abre con un solo clic. Desactivado por defecto para evitar aperturas
accidentales; con él activo, el clic deja de servir para seleccionar.

### `behavior.terminal`

Comando o ruta del emulador de terminal. Vacío significa detección automática:

1. este valor (si el ejecutable existe);
2. `$TERMINAL`;
3. candidatos conocidos: `foot`, `kitty`, `ghostty`, `alacritty`, `wezterm`,
   `konsole`, `kgx`, `gnome-terminal`, `cosmic-term`, `xterm`.

Se pasan los argumentos adecuados a cada terminal para abrir en la carpeta
actual y el proceso se lanza sin shell intermedio.

### `behavior.lastPath`

Última carpeta visitada, como URL (`file:///...`). Se actualiza al navegar y se
usa al arrancar sin argumentos.

### `ui.tabBarAlways`

`true` muestra la barra de pestañas incluso con una sola pestaña. Por defecto
`false`: la barra solo aparece cuando hay dos o más, de modo que quien usa una
única carpeta no paga altura ni repintado.

### `behavior.singleInstance`

`true` (por defecto): un segundo lanzamiento reenvía la ruta a la ventana abierta.
`false`: cada lanzamiento abre una ventana independiente. Lo mismo hacen `-n` y
Ctrl+N. Solo la primera ventana (la que tiene el nombre D-Bus) guarda
configuración y sesión; las demás trabajan en modo solo lectura
(`Config::setPersistent(false)`).

### `session.restore`

`true` (por defecto) restaura las pestañas de la sesión anterior al arrancar sin
una ruta explícita. Con una ruta en la línea de comandos, o con `--select`, no se
restaura nada. Las rutas que ya no existen o no son carpetas locales se
descartan sin impedir el arranque.

### `session.maxTabs`

Número máximo de pestañas, entre `1` y `32`. Por defecto `16`. Al alcanzarlo,
`Ctrl+T` y las acciones de pestaña nueva muestran un aviso en la barra de estado
en lugar de crear otra pestaña. No se expone en Ajustes; se edita en el JSON.

### `session.tabs`

Lista ordenada de URLs de las pestañas. Se reescribe de forma diferida al abrir,
cerrar, activar o navegar, y se fuerza al salir. Solo se guardan URLs con
esquema; las entradas vacías o sin esquema se ignoran al leer.

### `session.active`

Índice de la pestaña activa dentro de `session.tabs`. Se recorta al rango válido
al cargar y al restaurar.

### `version`

Versión del formato. Si el archivo declara una versión mayor que la soportada se
avisa por el registro y se intenta leer lo conocido. No se degrada ni se
reescribe automáticamente.

## Línea de comandos

```
Uso: ooze-files [opciones] [ruta]

  -h, --help            Ayuda
  -v, --version         Versión
  -t, --new-tab         Abrir en una pestaña nueva
  -n, --new-window      Abrir en una ventana independiente
  -s, --select <ruta>   Seleccionar un archivo en su carpeta
      --debug           Registro detallado (categorías ooze.files.*)
      --smoke-test      Cargar la interfaz y salir (CI)
```

Notas:

- `-v` es la versión de Qt; el registro detallado es `--debug`, sin atajo, para
  no colisionar.
- Si la ruta es un archivo, se abre su carpeta y se selecciona.
- Si la ruta no existe o no es local, se usa `behavior.lastPath` o `$HOME`.
- Con una instancia ya en marcha, la petición se reenvía por D-Bus y el proceso
  termina con 0.
- `-t` añade una pestaña solo si ya hay una instancia abierta; en arranque en
  frío abre la primera pestaña. `-n` todavía navega en la ventana existente.
- Al arrancar sin ruta ni `--select`, se restaura la sesión si
  `session.restore` está activo.

## Variables de entorno

| Variable | Uso |
| --- | --- |
| `TERMINAL` | terminal preferida si la configuración está vacía |
| `XDG_CONFIG_HOME` | ubicación de `config.json` |
| `XDG_DOCUMENTS_DIR`, `XDG_DOWNLOAD_DIR`, … | lugares de la barra lateral |
| `QT_QPA_PLATFORM` | `offscreen` en las pruebas de humo |
| `QT_QUICK_BACKEND` | `software` en CI o sin GPU |
| `QT_FORCE_STDERR_LOGGING` | `1` fuerza el registro a stderr cuando se redirige la salida |

El registro se controla con `QT_LOGGING_RULES`, por ejemplo
`QT_LOGGING_RULES="ooze.files.*=true"`.

## Opciones de compilación

| Opción | Por defecto | Estado |
| --- | --- | --- |
| `OOZE_FILES_BUILD_TESTS` | `ON` | compila `tests/` |
| `OOZE_FILES_WITH_ARCHIVES` | `OFF` | reservada para libarchive; activarla aborta la configuración |
| `OOZE_FILES_WITH_REMOTE` | `OFF` | reservada para gvfs/MTP |
| `OOZE_FILES_WITH_UDISKS` | `OFF` | reservada para detalles avanzados de UDisks2 |
| `OOZE_FILES_WITH_XDG_THUMBNAILERS` | `OFF` | reservada para miniaturas externas |

Las opciones reservadas existen para que el paquete pueda declararlas sin
sorpresas, pero no hay código detrás: activarlas falla de forma explícita en vez
de producir un binario silenciosamente incompleto.

## Integración D-Bus

Servicio `org.ooze.Files`, objeto `/org/ooze/Files`, interfaz `org.ooze.Files`.
Ver [architecture.md](architecture.md#instancia-única) para los métodos.

## Archivo .desktop

`packaging/ooze-files.desktop` declara `inode/directory` y
`StartupWMClass=ooze-files`. Para registrarlo como manejador por defecto:

```sh
xdg-mime default ooze-files.desktop inode/directory
```
