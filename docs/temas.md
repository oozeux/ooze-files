# Temas y ajustes de Qt 6

Documento de análisis y plan. Explica qué ajustes del sistema afectan hoy a
Ooze Files, cuáles no, y cómo añadir temas externos sin rehacer la interfaz.

## Resumen

Ooze Files dibuja toda su interfaz con QML propio: `Rectangle`, `Text`, `Image`
y `MouseArea`, sin QtQuick Controls y sin QtWidgets. Por eso no hay ningún
«estilo» de Qt que pinte la ventana: los colores, radios y tamaños salen de
`qml/theme/Theme.qml`.

Consecuencia directa:

- Los ajustes de **qt6ct**, **Kvantum**, **QtQuick Controls** y los estilos de
  Qt (Fusion, Breeze, Adwaita) **no cambian el color** de la interfaz, porque no
  se usa `QStyle` ni `QQuickStyle`.
- Sí se heredan el **esquema claro/oscuro**, la **tipografía**, el **tema de
  iconos**, la **escala** de la pantalla y el **cursor**.
- Un tema «externo» para Ooze Files tiene que ser un formato propio, pero puede
  **importar** los colores ya generados por OozeShell/Matugen, qt6ct o KDE
  (véase «Fuentes externas»), sin reutilizar Kvantum ni un `QPalette` en vivo.

## Lo que ya se hereda

| Ajuste del escritorio | Cómo llega | Dónde se usa |
| --- | --- | --- |
| Claro / oscuro | `QStyleHints::colorScheme()` | `ApplicationController::effectiveDark`, `Theme.dark` |
| Tipografía (familia) | `QGuiApplication::font().family()` | `App.uiFontFamily` → `Theme.fontFamily` |
| Tema de iconos | `IconTheme::apply()` | `QIcon::setThemeName`, `IconProvider` |
| Escala / HiDPI | Qt escala la superficie de la ventana | Todos los tamaños lógicos en px |
| Cursor | Variables `XCURSOR_*` y el compositor | `MouseArea.cursorShape` |
| Iconos SVG | Módulo `Svg` de Qt | `IconProvider` |

`themeMode` en `~/.config/ooze-files/config.json` (`light`, `dark`, `system`)
tiene la última palabra: con `system` se sigue el esquema del escritorio y se
reacciona a `colorSchemeChanged` sin sondeo.

### Iconos

`IconTheme::apply(dark)` construye la lista de búsqueda con:

1. `~/.local/share/icons`, `~/.icons`, `/run/current-system/sw/share/icons`,
   `/usr/local/share/icons`, `/usr/share/icons` y los directorios XDG de datos.
2. Nombre preferente, por orden: el que fija el **tema de plataforma de Qt**
   (qt6ct/KDE/GNOME), `~/.config/qt6ct/qt6ct.conf` (`Interface/icon_theme`),
   `~/.config/kdeglobals` (`Icons/Theme`), `~/.config/gtk-3.0/settings.ini`
   (`gtk-icon-theme-name`) y, si no hay nada, `Papirus`, `breeze` y `Adwaita`.
3. En modo oscuro se prueba el sufijo `-dark` del tema elegido.

El resultado se cachea en `IconProvider` y el emblema de enlace se compone al
vuelo (`?emblem=link`), también desde el tema activo. Si el tema no trae
`emblem-symbolic-link`, se dibuja un distintivo propio.

### Lo que no se hereda

| Ajuste | Por qué no afecta | ¿Se puede integrar? |
| --- | --- | --- |
| Paleta de qt6ct / Kvantum | Son paletas de `QPalette` para widgets y Controls; aquí no hay ninguno | Sí, leyendo el esquema activo de qt6ct y traduciendo sus colores a tokens |
| Acento de Plasma / GNOME | Solo lo consulta `QPalette`/Controls | Sí, con el portal `org.freedesktop.portal.Settings` |
| Colores del tema GTK | Solo los usan las apps GTK | No tiene sentido, salvo el tema de iconos |
| Tamaño de letra del sistema | `Theme.fontSize` depende de la densidad | Sí, con `ui.fontScale`; la familia, con `ui.fontFamily` |
| Radio de esquinas / bordes | Son decisiones de Ooze Files | Sí, forman parte del tema propio |

## Propuesta: temas externos

Estado: implementadas las fases 1–4 (tokens, archivos de tema, selector con
vista previa en Ajustes y fuentes del sistema), las claves `ui.iconTheme`,
`ui.fontScale` y `ui.fontFamily`, la importación de fuentes externas
(`system:ooze`, `system:qt6ct`, `system:kde`) y el acento/esquema por portal.
Solo queda la exportación opcional descrita más abajo.

### Formato

Un archivo JSON por tema en `~/.config/ooze-files/themes/<id>.json`. JSON y no
QML a propósito: un archivo de tema es **dato**, no código; así no se ejecuta
JavaScript de terceros y se puede validar y probar.

```json
{
  "schema": 1,
  "name": "Bosque",
  "light": {
    "bg": "#f2f4ef",
    "surface": "#ffffff",
    "surfaceAlt": "#e9ede4",
    "border": "#d8ddd2",
    "fg": "#1d211b",
    "fgMuted": "#67715f",
    "accent": "#3f7d4e",
    "danger": "#b23b3b"
  },
  "dark": {
    "bg": "#12150f",
    "surface": "#1a1e16",
    "surfaceAlt": "#171b13",
    "border": "#2a3025",
    "fg": "#e6eadf",
    "fgMuted": "#98a28d",
    "accent": "#8fca9b",
    "danger": "#f08a8a"
  },
  "metrics": {
    "radius": 10,
    "radiusSmall": 6,
    "borderWidth": 1
  }
}
```

Reglas de validación:

- `schema` debe ser `1`; si no, el archivo se ignora.
- Cada modo (`light`, `dark`) puede ser parcial: los tokens que falten se toman
  del tema incorporado del modo correspondiente.
- Los colores deben ser `#rrggbb` u `#aarrggbb`; un valor inválido descarta ese
  token y deja un aviso en el registro, no rompe la ventana.
- `surfaceHover`, `surfaceActive` y `surfaceAlt` son opcionales: si faltan se
  derivan mezclando `surface` con `fg` (o con `bg`, en el caso de `surfaceAlt`),
  para que un tema no pueda dejar el resaltado invisible.
- `selectionBg` no se pide: se deriva de `accent` con alfa 0,22 en oscuro y 0,12
  en claro, igual que el tema incorporado.
- `metrics` es opcional y acepta enteros entre 0 y 32 (`borderWidth` hasta 8).
- Se ignoran las claves desconocidas. `name` es solo para la interfaz; si falta,
  se usa el id.

### Resolución del tema

Clave de configuración: `ui.palette`.

- `"builtin"` (por omisión) → el par claro/oscuro de siempre.
- `"system:ooze"`, `"system:qt6ct"` o `"system:kde"` → paleta del escritorio
  (solo lectura; véase «Fuentes externas»).
- Cualquier otro valor → `themes/<id>.json`, con un id limitado a letras,
  números, punto, guion y guion bajo (nada de rutas).

`ui.theme` (`light` / `dark` / `system`) sigue eligiendo **qué mitad** del tema
se usa; un tema propio trae las dos, así que la combinación funciona igual. Las
fuentes qt6ct y KDE solo traen el modo que se aplicó por última vez (véase
«Modo único»).
Si la fuente no existe o no valida, se cae al tema incorporado, se registra el
motivo y la barra de estado lo muestra una vez. El selector de Ajustes relee la
carpeta de temas cada vez que se abre el diálogo, así que un archivo nuevo
aparece sin reiniciar.

### Fuentes externas del escritorio

Las paletas que ya genera el resto del escritorio se pueden consumir sin
duplicarlas. Son de **solo lectura**: Ooze Files nunca escribe en las rutas de
qt6ct, KDE ni Matugen (eso mantiene un único dueño de cada archivo).

| `ui.palette` | Archivo leído | Lo escribe | Modos |
| --- | --- | --- | --- |
| `system:ooze` | `~/.cache/oozeshell/colors.json` | OozeShell (`matugen image --json hex`) | claro y oscuro |
| `system:qt6ct` | esquema activo de `~/.config/qt6ct/qt6ct.conf` | qt6ct o Matugen | uno solo |
| `system:kde` | esquema activo de `~/.config/kdeglobals` → `color-schemes/*.colors` | KDE o Matugen | uno solo |

Los tres aparecen en el selector de Ajustes solo si la fuente existe. Si el
archivo desaparece o no valida, se usa el tema incorporado y la barra de estado
explica el motivo; el resto del escritorio no se toca.

Para `system:qt6ct` se lee primero `Appearance/color_scheme_path` (ruta
absoluta) y, si no está, `Appearance/color_scheme` como nombre dentro de
`qt6ct/colors`. Para `system:kde` se lee `[KDE] ColorScheme` (Plasma 6) o
`[General] ColorScheme` (versiones anteriores) y se busca
`<datadir>/color-schemes/<nombre>.colors`. Los nombres se validan
(`[A-Za-z0-9][A-Za-z0-9 ._-]*`) y el archivo debe existir; no se aceptan rutas
arbitrarias en la configuración de Ooze Files.

#### OozeShell / Matugen

`~/.cache/oozeshell/colors.json` es la salida de `matugen image … --json hex`:
cada rol Material 3 trae `dark`, `light` y `default`. OozeShell ya lo vigila,
así que es la fuente más completa: un cambio de fondo o de paleta en el shell
llega aquí con las dos mitades.

| Token de Ooze Files | Rol de Matugen |
| --- | --- |
| `bg` | `surface` |
| `surface` | `surface_container` |
| `surfaceAlt` | `surface_container_low` |
| `surfaceHover` | `surface_container_high` |
| `surfaceActive` | `surface_container_highest` |
| `border` | `outline_variant` |
| `fg` | `on_surface` |
| `fgMuted` | `on_surface_variant` |
| `accent` | `primary` |
| `danger` | `error` |
| `selectionBg` | `primary` con alfa 0.22 (oscuro) / 0.12 (claro) |

OozeShell aplica la misma cadena (fondo → Matugen → `palette-apply.py` →
plantillas de `~/.config/matugen/config.toml`), de modo que `colors.json`,
`OozeSoft.conf` y `OozeSoft.colors` cuentan la misma historia. Si falta un rol
se deriva mezclando, como en un tema propio incompleto.

#### qt6ct

`active_colors` es una lista fija de 22 colores en el orden de
`QPalette::ColorRole` que usa qt6ct 0.11 (el último, `Accent`, solo existe en
Qt ≥ 6.6; los esquemas antiguos traen 21 y se aceptan igual).

| # | Rol de qt6ct | Token |
| --- | --- | --- |
| 1 | WindowText | `fg` |
| 2 | Button | `surfaceAlt` |
| 6 | Mid | `fgMuted` si PlaceholderText es translúcido |
| 10 | Base | `surface` |
| 11 | Window | `bg` |
| 13 | Highlight | `accent` |
| 21 | PlaceholderText | `fgMuted` si es opaco |
| 22 | Accent | `accent` (tiene prioridad sobre Highlight) |

`Light`, `Midlight`, `Dark`, `Mid`, `Shadow`, `Link`, `LinkVisited`,
`AlternateBase` y `NoRole` no se usan como tokens: `border`, `surfaceHover` y
`surfaceActive` se derivan mezclando para que la interfaz mantenga la misma
jerarquía visual con cualquier esquema. `danger` cae al incorporado porque
qt6ct no define un rol de error.

#### KDE

| Token | Grupo / clave |
| --- | --- |
| `bg` | `Colors:Window/BackgroundNormal` |
| `surface` | `Colors:View/BackgroundNormal` |
| `surfaceAlt` | `Colors:Button/BackgroundNormal` |
| `fg` | `Colors:Window/ForegroundNormal` |
| `fgMuted` | `Colors:Window/ForegroundInactive` |
| `accent` | `Colors:Selection/BackgroundNormal` |
| `danger` | `Colors:View/ForegroundNegative` (opcional) |

Los valores son `R,G,B` (o `R,G,B,A`). QSettings los entrega como lista porque
la coma separa elementos; el importador los reconstruye antes de convertirlos.
Igual que en qt6ct, el borde y los estados se derivan.

#### Modo único y recarga

qt6ct y KDE guardan **solo el modo activo**: si OozeShell está en claro y luego
en oscuro, `palette-apply.py` reescribe el mismo archivo. Para no mezclar un
esquema oscuro con el alfa claro, el importador deduce la luminosidad de `bg` y
usa esa mitad para el alfa de selección; el resto de tokens es el esquema tal
cual. `system:ooze` es la única fuente que trae claro y oscuro a la vez.

El archivo de la fuente activa (y su carpeta, salvo la raíz `~/.config`) se
vigila con `QFileSystemWatcher`. Un cambio real dispara un `QTimer` de un solo
golpe de 200 ms que vuelve a aplicar la paleta; no hay sondeo y un guardado
normal no reinicia nada.

#### Exportar (pendiente)

Sería posible escribir `OozeSoft.conf` / `OozeSoft.colors` desde un tema de
Ooze Files, pero esas rutas ya tienen dueño (Matugen/OozeShell). Dos
generadores sobre el mismo archivo producen carreras y resultados confusos, así
que queda como opción explícita de línea de comandos, nunca como
comportamiento por omisión. Mientras tanto, la dirección útil es la inversa:
Ooze Files lee lo que el escritorio ya genera.

### Acento del sistema

El acento se lee del portal, sin sondeo:

- `org.freedesktop.portal.Settings.Read("org.freedesktop.appearance",
  "accent-color")` devuelve tres dobles (0–1) desde GNOME 47 y Plasma 6.
- Se escucha `SettingChanged` para el mismo espacio y clave: si el usuario
  cambia el acento, se actualiza `Theme.accent` y con él `selectionBg`.
- Si el portal no existe, no implementa la clave (Hyprland, por ejemplo) o
  devuelve un valor raro, se usa el acento del tema sin avisar.
- También sirve de respaldo para el modo: si `QStyleHints::colorScheme()`
  devuelve `Unknown`, se usa `color-scheme` del mismo portal (0 sin
  preferencia, 1 oscuro, 2 claro).

El acento del portal solo completa el tema **incorporado**. Un tema propio o una
fuente del sistema (`system:ooze`, `system:qt6ct`, `system:kde`) ya trae su
acento y no se pisa, para no romper la coherencia con el resto del escritorio.

Todo es una lectura asíncrona al arrancar (`asyncCall` con 3 s de límite) y una
señal: nada de temporizadores ni de bloquear la interfaz si el portal no
responde.

### Ajustes de Qt expuestos

Tres claves opcionales, con «automático» por omisión:

- `ui.iconTheme`: nombre de tema de iconos; vacío = descubrimiento actual.
- `ui.fontScale`: 1.0 por omisión; multiplica tipografía y alturas de fila,
  cabecera, barra de estado y barra lateral.
- `ui.fontFamily`: familia tipográfica; vacío = la del sistema. Se valida
  (máximo 128 caracteres, sin barras ni caracteres de control) y se aplica en
  caliente desde la página de Configuración.

Con esto, quien use qt6ct para el resto del escritorio puede fijar los mismos
iconos, la misma escala de letra, la misma familia y, con `system:qt6ct`, la
misma paleta sin tocar el código.

## Puntos de la interfaz

- `qml/theme/Theme.qml`: los literales de color pasaron a `Palette.*`; los usos
  de `Theme.*` no cambiaron de nombre. `fontSize` y las alturas se multiplican
  por `App.uiFontScale`.
- `qml/pages/SettingsPage.qml`: filas «Tema», «Paleta» y «Tipografía»,
  vista previa de la paleta y botón para abrir la carpeta de paletas.
- `src/settings/Config.{h,cpp}`: claves `ui.palette`, `ui.iconTheme`,
  `ui.fontScale` y `ui.fontFamily`, con validación y el guardado agrupado de
  300 ms.
- `src/theme/PortalSettings.{h,cpp}` (nuevo): lectura asíncrona del acento y del
  esquema claro/oscuro del portal, sin bloquear el arranque.
- `src/theme/ThemeStore.{h,cpp}` (nuevo): lectura, validación y señales del
  tema; sin QML, registrado como singleton `Palette` para que `Theme.qml` lo
  consuma. Incluye los importadores de OozeShell, qt6ct y KDE y la recarga por
  `QFileSystemWatcher` cuando la fuente es del sistema.
- `src/app/ApplicationController.cpp`: aplica el tema al arrancar y en cada
  cambio de `ui.palette` o de esquema, y avisa a `IconTheme` cuando cambia el
  tema de iconos.
- Barra de estado y registro: aviso si un tema no carga.

Los delegados, menús y diálogos **no** se tocan: ya consumen tokens. El
resaltado de selección sale de `accent`, así que un tema nuevo lo hereda sin
trabajo extra; la tira de arrastre recibe `Theme.fg` y `Theme.fontFamily` en su
URL, de modo que también sigue al tema.

## Fases

1. **Tokens sin cambio visual.** Hecho: `ThemeStore` con el par incorporado y
   `Theme.qml` leyendo `Palette.*`; los colores son idénticos a los anteriores.
2. **Tema desde archivo.** Hecho: `ThemeStore`, esquema, validación, `ui.palette`
   y `tests/unit/tst_theme.cpp` (incorporado, válido, parcial, JSON roto,
   `schema` 2, id inseguro, derivación de tonos y listado de temas).
3. **Sistema.** Hecho: `ui.iconTheme`, `ui.fontScale`, las fuentes externas
   `system:ooze`, `system:qt6ct` y `system:kde` con recarga por
   `QFileSystemWatcher`, y el acento/esquema por portal como respaldo.
4. **Ajustes.** Hecho lo esencial: selector de paleta, vista previa de colores,
   botón para abrir la carpeta de paletas y `ui.fontFamily` editable. La
   exportación a formatos qt6ct/KDE queda como opción explícita pendiente.
5. **Documentación.** Hecho: esta página, `docs/configuration.md` y el README.

## Rendimiento

- Cargar un tema es leer un JSON pequeño al arrancar y al cambiar la clave; no
  hay vigilancia de archivos ni temporizadores para los temas propios.
- Con una fuente externa se vigila solo el archivo activo (y su carpeta) con
  `QFileSystemWatcher`; la recarga se agrupa en un temporizador de un solo golpe
  de 200 ms. El resto de fuentes no se tocan.
- La carpeta de temas se relee solo al abrir el diálogo de configuración.
- El portal se consulta una vez al arrancar (llamadas asíncronas con límite de
  3 s) y luego solo por señal; si no existe, no hay sondeo ni aviso.
- Los tokens son propiedades planas: cambiar de tema es una asignación, no una
  animación ni una recarga de la vista.
- El único coste nuevo en pintado sería volver a componer iconos con el emblema
  si cambia el tema de iconos, algo que ya ocurre hoy al cambiar claro/oscuro.

## Riesgos y decisiones

- **Temas como código**: descartado (QML ejecutable). JSON validado.
- **Contraste**: un tema externo puede dejar texto ilegible. Se puede avisar si
  `fg` y `bg` tienen contraste menor que 3:1, sin bloquear la carga.
- **Explosión de tokens**: la lista canónica vive en `ThemeStore`; el esquema y
  las pruebas son la garantía de que la interfaz no pide un token inexistente.
- **Escala**: los `metrics` son píxeles lógicos; en HiDPI Qt los multiplica como
  hoy, sin cambios.
- **Propiedad de archivos**: los importadores solo leen. Escribir en
  `~/.config/qt6ct/colors`, `~/.local/share/color-schemes` o
  `~/.cache/oozeshell` queda fuera de alcance para no competir con Matugen.
- **Modo único**: qt6ct y KDE solo describen el modo que se aplicó por última
  vez; el alfa de selección se deduce del `bg` importado. Para claro y oscuro
  simultáneos hay que usar `system:ooze`.
- **Rutas**: `color_scheme_path` es una ruta absoluta del archivo del usuario;
  se exige que exista y que termine en `.conf`. Los nombres de esquema se
  validan antes de construir la ruta.
- **Portal ausente**: en compositores sin `org.freedesktop.appearance`
  (Hyprland) se ignora el acento y el esquema; no se bloquea ni se avisa. El
  acento del portal nunca sobrescribe un tema propio ni una fuente `system:*`.

## Archivos de referencia

- `qml/theme/Theme.qml`: tokens de la interfaz y `dragImageUrl()`.
- `src/theme/ThemeStore.{h,cpp}`: paletas incorporadas, lectura de archivos,
  validación, derivación, importadores de OozeShell/qt6ct/KDE, acento del
  portal, recarga y listado de temas.
- `src/theme/PortalSettings.{h,cpp}`: acento y esquema claro/oscuro del portal
  (`org.freedesktop.appearance`).
- `src/app/IconTheme.cpp`: descubrimiento del tema de iconos.
- `src/app/IconProvider.cpp`: caché de iconos y emblema de enlace.
- `src/app/ApplicationController.cpp`: `effectiveDark`, `paletteId`,
  `uiFontScale`, `uiFontFamily`, `colorSchemeChanged` y carpeta de paletas.
- `src/settings/Config.{h,cpp}`: configuración JSON.
- `qml/pages/SettingsPage.qml`: selector y vista previa de paleta,
  tipografía y carpeta de paletas.
- `tests/unit/tst_theme.cpp`: pruebas del cargador de temas y del acento.
