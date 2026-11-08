# Plan: selección por arrastre (marco de selección)

Estado: **implementado**. La geometría, el enrutado de pulsaciones y los modos
se validaron con un banco QML offscreen; queda la verificación manual del gesto
con puntero real en Wayland (ver al final).

## Condición de entrada

Este trabajo comparte el mismo mecanismo de puntero que el arrastre de archivos.
La lista de verificación manual de
[docs/drag-and-drop.md](drag-and-drop.md#verificación) debe pasar completa:

- Arrastrar fuera y soltar dentro funcionan sin abrir archivos por accidente.
- No hay advertencias de QML en la salida estándar al arrastrar.
- No se duplican ni se pierden archivos en ningún caso.

Si algo de eso falla, se corrige primero: un fallo de puntero se multiplicaría al
añadir un segundo gesto sobre las mismas vistas.

## Objetivo

Seleccionar varios elementos dibujando un rectángulo con el botón izquierdo
sobre el fondo de la vista, sin tocar el teclado ni el portapapeles.

Comportamiento deseado:

- Botón izquierdo sobre hueco vacío y arrastrar: marco visible y selección
  continua de todo lo que queda dentro.
- Sin modificadores: la selección anterior se reemplaza.
- `Ctrl`: se alterna cada elemento alcanzado (añadir/quitar).
- `Mayús`: se añade al ancla de la última selección con clic o teclado.
- `Esc` durante el arrastre: cancela y deja la selección como estaba.
- Al soltar: el marco desaparece y la selección queda aplicada.
- El foco de teclado sigue en la vista; `currentIndex` pasa a la primera fila
  seleccionada para continuar con las flechas.
- Un clic normal sobre un elemento, un clic derecho y el arrastre de archivos
  siguen funcionando exactamente igual.

Fuera de alcance: selección por marco dentro de la barra lateral, selección con
dos dedos, y arrastrar el marco más allá de los límites de la ventana.

## Diseño

### 1. API de selección por lotes en el controlador

Hoy `selectRow(row, toggle, range)` emite `selectionChanged` por cada cambio, lo
que con un marco que abarca cientos de filas provocaría repintados en cascada.
Se añade una operación por lotes:

- `Q_INVOKABLE void selectRows(const QVector<int> &rows, int mode)` con
  `mode`: `0` reemplazar, `1` añadir, `2` alternar.
- Traduce filas a URLs con `FileListModel::UrlRole`, actualiza `m_selectedUrls`
  de una vez, fija `m_anchorUrl` y emite `selectionChanged` **una sola vez**.
- Si el conjunto resultante es idéntico al actual, no emite nada.
- Ignora filas fuera de rango y URLs vacías.
- Reutiliza `pruneSelection()` y la lógica de `notifySelection()` existentes.

Para el modo `2`, la vista no envía el rectángulo completo en cada movimiento:
envía solo la diferencia simétrica respecto a la posición anterior del marco.
Un elemento que permanece dentro no se alterna en cada tick; uno que sale del
marco recupera su estado anterior. Así la operación es idempotente por
movimiento y sigue emitiendo una sola señal.

Se añade también `selectedRows()`, que devuelve los índices seleccionados en
orden. La vista lo usa para fotografiar la selección al empezar el gesto y
restaurarla si se cancela con `Esc`.

Pruebas unitarias nuevas en `tests/unit/tst_controller.cpp`:
reemplazar, añadir, alternar, filas duplicadas, filas fuera de rango, conjunto
vacío y comprobación de que solo se emite una señal por llamada, además del
orden de `selectedRows()`.

### 2. Superficie de gestos

El fondo actual de cada vista es un `MouseArea` con `z: -1` que solo atiende al
botón derecho y queda **por debajo** de la vista, por lo que no recibe pulsaciones
dentro del área del listado.

Se añade un componente compartido, `qml/components/MarqueeSelection.qml`, que
cubre la vista completa por encima del contenido:

- En `onPressed` se hace la prueba de impacto; si hay un elemento bajo el
  puntero, se pone `mouse.accepted = false` para que el evento llegue al
  delegado. Esto preserva clics, doble clic y el `DragHandler` de archivos.
- La prueba de impacto usa `indexAt()` de la vista con coordenadas de contenido
  (`mapToItem(contentItem, x, y)`), que es O(1).
- El botón derecho no se toca: el `MouseArea` de fondo existente sigue abriendo
  el menú de la carpeta.
- No se fija `cursorShape`: el puntero de los delegados sigue mandando.
- El gesto empieza tras superar un umbral de 3 px, para que un clic en un hueco
  no dibuje un marco de tamaño cero.

### 3. Geometría del marco

Coordenadas de contenido (se suma `contentY`, y `contentX` solo en la
cuadrícula):

- Cuadrícula:
  - `columnas = max(1, floor(grid.width / grid.cellWidth))`
  - `filaInicial = clamp(floor((marco.top + contentY) / grid.cellHeight), 0, …)`
  - `filaFinal = clamp(floor((marco.bottom + contentY) / grid.cellHeight), …)`
  - Para cada fila se calculan las columnas primera y última que corta el
    rectángulo, y con ellas los índices `fila * columnas + columna`.
  - Solo se generan índices `< count`; las celdas virtualizadas no necesitan
    existir.
- Lista:
  - `primera = floor((marco.top + contentY) / Theme.rowHeight)`
  - `última = ceil((marco.bottom + contentY) / Theme.rowHeight) - 1`
  - Índices contiguos entre ambas, acotados a `count - 1`.
  - La lista añade un pie del alto de una fila para que siempre haya un hueco
    bajo el último elemento donde empezar el gesto.

La lista de índices se pasa a `App.selectRows(rows, mode)` en cada movimiento del
puntero. El coste es proporcional al área del marco (como máximo las celdas
visibles), no al tamaño de la carpeta.

### 4. Dibujo

Un `Rectangle` con borde de 1 px y relleno translúcido, visible solo mientras el
marco está activo. Tokens nuevos en `Theme` (`marqueeFill`, `marqueeBorder`)
derivados de `selectionBg`/`accent` para no introducir colores sueltos ni
dependencias del tema del escritorio. Sin sombras, sin blur y sin animaciones.

### 5. Desplazamiento automático

Cuando el puntero entra en una franja de ~24 px junto al borde superior o
inferior, la vista se desplaza para seguir seleccionando fuera de la pantalla.

Es el único temporizador nuevo:

- Un `Timer` de 30 ms que solo existe mientras el marco está activo y el
  puntero está en la franja; se detiene al salir de la franja, al soltar o al
  cancelar.
- Cada tick desplaza `contentY` 12 px y recalcula la selección.
- Se documenta en `docs/performance.md` como excepción justificada.

Alternativa descartada: `Flickable.flick()` en ráfagas, que produce saltos y
compite con el gesto.

### 6. Interacción con el resto del teclado y el ratón

- `Mayús`+clic y `Ctrl`+clic siguen usando `selectRow(...)`, sin cambios.
- Un clic sin arrastre en un hueco vacío limpia la selección y deja la vista sin
  índice actual. Con `Ctrl` o `Mayús` no cambia nada, para no interferir con la
  intención de añadir a la selección.
- `Ctrl+A` y el resto de atajos no cambian. `Esc` durante el marco lo cancela
  y restaura la selección capturada al pulsar; fuera del marco conserva su
  comportamiento actual.
- Tras el marco, `currentIndex` se fija a la primera fila seleccionada con el
  mismo mecanismo que ya usa `setCurrentRow` (sin seleccionar ni abrir).
- El arrastre de archivos sigue teniendo prioridad sobre los elementos; el marco
  solo empieza en huecos vacíos.

## Fases y orden de trabajo

1. Controlador: `selectRows` + pruebas unitarias. Sin cambios de interfaz.
2. Cuadrícula: `MouseArea` de marco, geometría y dibujo.
3. Lista: reutilizar la misma lógica con geometría de filas.
4. Desplazamiento automático y cancelación con `Esc`.
5. Tokens de `Theme`, documentación y tabla de atajos del `README`.
6. Verificación manual completa.

Cada fase debe dejar `ctest` en verde y la prueba de humo sin advertencias.

## Estado de implementación

Hecho:

- `selectRows(rows, mode)` y `selectedRows()` en el controlador, con pruebas de
  reemplazo, adición, alternancia, duplicados, fuera de rango, conjunto vacío,
  señal única y orden.
- `qml/components/MarqueeSelection.qml`, compartido por cuadrícula y lista.
- Modos: sin modificador reemplaza; `Ctrl` alterna el delta simétrico;
  `Mayús` añade el delta.
- `Esc` cancela y restaura la selección capturada; al soltar, `currentIndex`
  pasa a la primera fila seleccionada.
- Un clic sin arrastre en un hueco vacío limpia la selección y el índice actual
  (el `MouseArea` del marco emite `emptyClicked`).
- Tokens `Theme.marqueeFill` y `Theme.marqueeBorder` derivados de `accent`.
- Desplazamiento automático de 12 px cada 30 ms, solo en la franja de 24 px y
  mientras el gesto está activo.
- Pie de una fila en la lista para garantizar un hueco de inicio.
- `ctest` en verde y prueba de humo sin advertencias.

Pendiente:

- Verificación manual del gesto real en Wayland. En este entorno no hay
  inyección de puntero hacia la ventana; la geometría y el enrutado se
  comprobaron con un banco QML offscreen temporal con pulsaciones sintéticas.

## Verificación manual

Pendiente de ejecutar con puntero real. La lista sirve de guion:

1. Arrastrar en un hueco de la cuadrícula: se dibuja el marco y se seleccionan
   solo los elementos tocados.
2. Arrastrar en el hueco de la lista: selección por rango de filas.
3. `Ctrl` + marco: añade y quita sin perder el resto.
4. `Mayús` + marco: añade al ancla existente.
5. `Esc` a mitad: se cancela y la selección previa queda intacta.
6. Clic normal, doble clic, clic derecho y arrastre de archivos: sin cambios.
7. Carpeta con miles de archivos: el marco no produce tirones ni selecciona
   elementos fuera del rectángulo.
8. Borde superior e inferior: la vista se desplaza y la selección continúa.
9. Cambiar de vista (cuadrícula/lista) y de tema: el marco usa los tokens
   correctos.
10. Teclado después del marco: las flechas continúan desde la primera fila
    seleccionada.

## Riesgos conocidos

- **Conflicto con el desplazamiento táctil/ratón**: el marco ocupa el hueco
  vacío, así que arrastrar ahí deja de desplazar la lista. Es el comportamiento
  esperado en un gestor de archivos.
- **Umbral compartido** con el `DragHandler` de los delegados: se resuelve con la
  prueba de impacto al pulsar, no con temporizadores.
- **Selección masiva**: `selectRows` debe emitir una sola señal y comparar el
  conjunto antes de notificar, o la interfaz se degradará en carpetas grandes.
- **Rebotes del marco** en Wayland: no se esperan, pero la lista de verificación
  incluye el caso de cancelación para detectarlos.
