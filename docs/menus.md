# Menús emergentes

Documenta `qml/components/MenuPanel.qml`, `qml/components/PopupMenu.qml` y su uso
en `qml/Main.qml`.

## Problema que resuelve

Un menú dibujado a su altura natural se corta cuando la ventana es más pequeña
que el menú. Pasa sobre todo en compositores con mosaico (Hyprland en modo
`dwindle`, Niri, Sway), donde el tamaño de la ventana lo decide el compositor y
puede quedar por debajo de `minimumWidth`/`minimumHeight`.

Con la corrección, un menú nunca sale del área de la ventana: se encoge, se
recoloca y, si aún no cabe, se desplaza con la rueda.

## Dos tipos: `PopupMenu` y `MenuPanel`

QML no permite que un tipo se instancie a sí mismo, ni siquiera dentro de un
`Component`. Por eso el panel y el menú público son archivos distintos:

| Tipo | Papel |
|---|---|
| `MenuPanel` | Todo el comportamiento: filas, scroll, límites, hover y submenús. Su propiedad `submenuComponent` dice qué componente usar para un submenú. |
| `PopupMenu` | `MenuPanel` con `submenuComponent` apuntando a un `Component` interno que es otro `MenuPanel` que se referencia a sí mismo por `id`. Es el que se usa en `Main.qml`. |

Una referencia por `id` no es una instanciación recursiva, así que los submenús
pueden anidarse a cualquier profundidad.

## Submenús

Una entrada con `submenu: [ ... ]` (lista de entradas con el mismo formato) se
dibuja con un «›» y no ejecuta nada por sí misma:

- el submenú se abre al pasar el puntero por la fila o al hacer clic en ella, y
  se cierra al pasar a otra fila, al desplazar el menú o al cerrarlo;
- se coloca a la derecha de la fila, alineado con ella; si no cabe en la ventana
  se abre a la izquierda, y siempre se ajusta a los límites verticales (con
  scroll si hace falta);
- solo emite `triggeredItem(item)` (no `triggered(index)`), y la señal sube por
  la cadena de menús hasta el consumidor, que además recibe `closed()`;
- `parentMenu` enlaza cada submenú con su padre; `windowX`/`windowY` dan su
  posición en coordenadas de ventana para aplicar los mismos límites.

Las entradas de submenú de «Abrir con» llevan `appId` en lugar de `id`.

## Estructura de `MenuPanel.qml`

```
PopupMenu (Rectangle)
├── MouseArea            absorbe hover, clics y rueda en huecos y bordes
├── Flickable            área desplazable con el contenido
│   └── Column
│       └── Repeater     una fila por entrada de `items`
│           └── delegate Item
│               ├── separador (línea de 1 px)
│               ├── fondo con resalte al pasar el puntero
│               ├── marca de check (FileIcon)
│               ├── etiqueta (Text, se recorta con «…»)
│               ├── atajo o «›» si tiene submenú (Text)
│               ├── HoverHandler
│               └── MouseArea de la fila
├── Loader               submenú activo (otro MenuPanel)
└── ScrollIndicator      barra fina, solo visible si hay desbordamiento
```

## Propiedades

| Propiedad | Tipo | Para qué sirve |
|---|---|---|
| `items` | `var` | Lista de entradas. Cada una admite `label`, `hint`, `enabled`, `checked`, `separator` e `id`. |
| `menuWidth` | `int` | Ancho preferido (236 por defecto). |
| `boundsWidth` | `real` | Ancho del área disponible. `0` desactiva el límite. |
| `boundsHeight` | `real` | Alto del área disponible. `0` desactiva el límite. |
| `edgeMargin` | `int` | Margen mínimo contra el borde de la ventana (8). |
| `maxWidth` | solo lectura | `boundsWidth - 2 * edgeMargin`, con mínimo de 120. |
| `parentMenu` | `var` | Menú padre; `null` en los menús raíz. |
| `submenuComponent` | `Component` | Componente usado para abrir submenús. |
| `openSubIndex` | `int` | Fila con submenú abierto; `-1` si ninguno. |
| `maxHeight` | solo lectura | `boundsHeight - 2 * edgeMargin`, con mínimo de una fila. |
| `fullHeight` | solo lectura | Alto que necesitaría el contenido completo. |
| `scrollable` | solo lectura | `true` cuando `fullHeight` supera el alto real. |

Señales: `triggered(index)`, `triggeredItem(item)` y `closed()`.

## Menús de Main.qml

| Menú | Identificación de entradas |
|---|---|
| `mainMenu` | `id` de acción, vía `triggeredItem`; la estructura sale de `ActionRegistry` (`menuLayout()`). |
| `contextMenu` | Campo `id` propio de cada entrada (`handleMenuItem`), `appId` para «Abrir con» y `format` para «Comprimir». Se construye como binding, así `enabled` sigue al estado de `App`. |
| `placeMenu` | Menú de la barra lateral: «Quitar de Lugares» para carpetas propias; «Montar y abrir», «Abrir», «Desmontar» y «Expulsar» para dispositivos (`handlePlaceMenu`). |
| `tabMenu` | Menú contextual de una pestaña: nueva, duplicar, cerrar, cerrar las demás, cerrar las de la derecha, reabrir y copiar ruta (`handleTabMenu`). Guarda el índice en `tabMenuIndex`. |
| `sortMenu`, `dropMenu` | Índice (`triggered`), sin submenús. |

## Menú principal

Se dibuja desde `menuLayout()` en `src/app/ActionRegistry.cpp`. Orden de las
secciones:

1. Nueva ventana, Abrir terminal aquí (`Shift+F4`)
2. Deshacer, Rehacer
3. Pestañas ▸, Vista ▸, Tamaño de iconos ▸, Densidad ▸, Ordenar por ▸, Tema ▸
4. Mostrar ocultos, Abrir con un clic, Miniaturas, Panel de información
5. Nuevo ▸, Pegar, Seleccionar todo, Herramientas ▸
6. Configuración, versión

«Abrir terminal aquí» es la acción `tools.terminal`: abre el emulador
configurado en la carpeta de la pestaña activa (`ApplicationController::openCurrentInTerminal`).
Su atajo se declara en la tabla de acciones, que es lo que muestra el menú y la
paleta, y se activa con un `Shortcut` en `Main.qml` que queda deshabilitado
mientras haya un diálogo, una superposición, un menú abierto o un campo de texto
con foco.

## Menú contextual de un elemento

Orden de las entradas (las marcadas con * solo aparecen si procede):

1. Abrir, Abrir con ▸ *, Abrir en terminal, Vista previa
2. Cortar, Copiar, Pegar
3. Renombrar, Eliminar, Eliminar permanentemente
4. Nueva carpeta, Nuevo archivo
5. Comprimir ▸ *, Extraer aquí *, Añadir a Lugares *, Copiar ruta
6. Propiedades

«Abrir con» solo se calcula con un único elemento local seleccionado (archivo o carpeta).
«Comprimir» lista únicamente los formatos cuya herramienta está instalada.
«Extraer aquí» aparece con un único archivo comprimido, y «Añadir a Lugares»
con una única carpeta. El menú del fondo ofrece «Añadir esta carpeta a
Lugares» y «Propiedades» de la carpeta actual.

## Menú contextual de una pestaña

La barra de pestañas (`qml/panels/TabBar.qml`) emite `tabContextRequested` con
el índice y la posición. `Main.qml` la convierte a coordenadas de ventana y
abre `tabMenu`:

1. Nueva pestaña, Duplicar pestaña
2. Cerrar pestaña, Cerrar las demás, Cerrar las de la derecha
3. Reabrir pestaña cerrada, Copiar ruta de la pestaña

«Cerrar las demás» solo se habilita con más de una pestaña y «Cerrar las de la
derecha» solo si hay pestañas a la derecha. El clic central sobre una pestaña
la cierra sin abrir el menú.

## Cómo se calcula el tamaño

```
width  = min(menuWidth, maxWidth)
height = min(fullHeight, maxHeight)
```

Cuando `scrollable` es `true`, el `Flickable` activa `clip` e `interactive`, de
modo que la rueda y el arrastre táctil desplazan las filas. Si el menú cabe, el
`Flickable` no recorta ni captura la rueda. El contenido vuelve arriba cada vez
que el menú se abre o cambia `items`.

## Cómo se posiciona (en `Main.qml`)

Cada instancia recibe `boundsWidth: root.width` y `boundsHeight: root.height`, y
calcula `x`/`y` con el mismo patrón, que se evalúa después de conocer
`width`/`height`:

```
x = max(edgeMargin, min(root.width  - width  - edgeMargin, xPreferida))
y = max(edgeMargin, min(root.height - height - edgeMargin, yPreferida))
```

| Menú | Posición preferida |
|---|---|
| `mainMenu` | Esquina superior derecha, bajo la cabecera. |
| `sortMenu` | A la izquierda del botón del menú principal, bajo la cabecera. |
| `contextMenu` | Donde se hizo clic o donde está el elemento con foco. |
| `tabMenu` | Donde se hizo clic derecho sobre una pestaña. |
| `dropMenu` | Donde se soltó el arrastre. |

Como el tamaño no depende de la posición, no hay bucles de enlace.

## Bloqueo de entrada hacia el fondo

Mientras hay un menú abierto, nada de lo que hay detrás debe reaccionar al
puntero. Hay dos capas que lo garantizan, y las dos necesitan `hoverEnabled`
porque un `MouseArea` sin él deja pasar el hover aunque absorba los clics:

| Capa | Dónde | Qué bloquea |
|---|---|---|
| Capa transparente | `MouseArea` con `z: 150` en `Main.qml` | Hover, clics (izquierdo, derecho y central) y rueda en toda la ventana; un clic cierra el menú. |
| Fondo del menú | `MouseArea` dentro de `PopupMenu.qml` | Hover, clics y rueda en los huecos entre filas, separadores y bordes del menú. |

El `Flickable` del menú queda por encima del fondo, así que la rueda sigue
desplazando el menú cuando `scrollable` es `true`.

## Coste

Sin animaciones, temporizadores ni consultas al sistema. Los enlaces se
reevalúan solo cuando cambia el tamaño de la ventana o el contenido del menú.
`ScrollIndicator` es un único `Rectangle` y no existe mientras el menú cabe
entero (`visible: false`).

## Añadir un menú nuevo

1. Declarar una instancia de `PopupMenu` en `Main.qml` con `z: 151`.
2. Pasar `boundsWidth` y `boundsHeight`.
3. Calcular `x` e `y` con el patrón anterior.
4. Para agrupar entradas, añadir `submenu: [...]` a una entrada.
5. Activarlo con `visible: root.openMenu === "nombre"` y cerrarlo con
   `onClosed: root.openMenu = ""`.
