# Acciones y paleta de comandos

Documenta `src/app/ActionRegistry.{h,cpp}` y `qml/components/CommandPalette.qml`.

## Idea

Toda acción de la aplicación se declara una vez en una tabla de C++. De esa
tabla salen:

- el menú principal, descrito por `menuLayout()` como un árbol de entradas,
  grupos (submenús) y separadores;
- la paleta de comandos (`Ctrl+K`);
- el estado habilitado/marcado de cada entrada, calculado contra
  `ApplicationController`.

Los menús contextuales de elemento y de fondo siguen escritos en `Main.qml`
porque dependen de la fila bajo el puntero; se migrarán a la misma tabla cuando
acepten un contexto explícito.

El registro se expone a QML como singleton `Actions`
(`qmlRegisterSingletonInstance` en `src/app/main.cpp`) con dos propiedades:

| Propiedad | Contenido |
| --- | --- |
| `Actions.commands` | todas las acciones con `id`, `label`, `hint`, `category`, `keywords`, `enabled` y `checked` |
| `Actions.mainMenu` | solo las acciones del menú principal, con `{ separator: true }` entre secciones y una entrada final deshabilitada con la versión |

`Actions.trigger(id)` ejecuta la acción. Las acciones de interfaz emiten
`uiRequested(id)`, que `Main.qml` traduce a diálogos, prompts o foco.

## Acciones de edición, vista y selección

| Id | Qué hace |
| --- | --- |
| `edit.undo`, `edit.redo` | deshacer y rehacer (`Ctrl+Z`, `Ctrl+Shift+Z`); la etiqueta es dinámica, por ejemplo «Deshacer: Mover a la papelera» |
| `view.thumbnails` | miniaturas de imágenes en la cuadrícula |
| `view.infoPanel` | panel lateral de información (`F11`) |
| `icons.small` … `icons.huge` | tamaño de iconos |
| `selection.preview` | vista previa y propiedades (`Espacio`) |
| `selection.bulkRename` | renombrado en lote con vista previa (`Mayús+F2`) |
| `nav.searchRecursive` | búsqueda recursiva con filtros (`Ctrl+Mayús+F`) |

### Acciones de pestañas

| Id | Qué hace |
| --- | --- |
| `tab.new` | nueva pestaña en la carpeta activa (`Ctrl+T`) |
| `tab.duplicate` | duplica la pestaña activa justo a su derecha |
| `tab.close` | cierra la pestaña activa (`Ctrl+W`); la última cierra la ventana |
| `tab.closeOthers` | cierra todas las pestañas salvo la activa |
| `tab.closeRight` | cierra las pestañas situadas a la derecha de la activa |
| `tab.reopen` | reabre la última pestaña cerrada en su posición (`Ctrl+Mayús+T`) |
| `tab.next` | pestaña siguiente, circular (`Ctrl+Tab`) |
| `tab.previous` | pestaña anterior, circular (`Ctrl+Mayús+Tab`) |
| `tab.copyPath` | copia la ruta de la pestaña activa |

El grupo «Pestañas» del menú principal usa estos ids. El menú contextual de una
pestaña concreta se construye en `Main.qml` porque necesita su índice.

Una acción puede tener una etiqueta dinámica (`dynamicLabel`): una función que
se evalúa contra el controlador cada vez que se reconstruye el menú.

## Añadir una acción

Se añade una entrada a `definitions()` en `ActionRegistry.cpp`:

```cpp
{QStringLiteral("view.example"),            // id estable, nunca se traduce
 tr("Etiqueta visible"),                    // se traduce con el contexto ActionRegistry
 QStringLiteral("Ctrl+E"),                  // texto del atajo; solo se muestra
 tr("Categoría"),                           // agrupación en la paleta
 QStringLiteral("palabras clave"),          // sinónimos para la búsqueda
 [](const ApplicationController &c) {       // enabled (opcional)
     return c.itemCount() > 0;
 },
 [](const ApplicationController &c) {       // checked (opcional, acciones de estado)
     return c.viewMode() == QLatin1String("list");
 },
 [](ApplicationController &c) {             // run (vacío si es de interfaz)
     c.setViewMode(QStringLiteral("list"));
 },
 false}                                     // true = uiRequested
```

Reglas:

- No repetir el id: es la clave de la configuración futura y de la paleta.
- `enabled` vacío significa siempre habilitada; `checked` vacío significa que no
  es una acción de estado.
- Si la acción abre un diálogo, un prompt o necesita el `currentIndex` de la
  vista, marcar `ui = true`, dejar `run` vacío y atender el id en el bloque
  `Connections { target: Actions }` de `Main.qml`.
- El `hint` es solo para pintar. Los atajos reales son los `Shortcut` de
  `Main.qml`; al añadir una acción con atajo hay que declarar ambos (ver
  «Pendiente»). `Ctrl+Z`, `Ctrl+Shift+Z`/`Ctrl+Y`, `F11`, `Alt+Intro` y
  `Espacio` ya están declarados así en `Main.qml`.
- Para que aparezca en el menú principal, añadir el id a `menuLayout()`.
  El orden del menú es el de esa tabla, no el de `definitions()`.
- El coste de una acción debe ser O(1) o trabajo ya existente en el
  controlador: la paleta y el menú se reconstruyen al cambiar selección,
  portapapeles, historial, número de elementos, tema, vista, densidad u orden.

## Paleta de comandos

`CommandPalette.qml` es una capa a pantalla completa con z=250 (por debajo de
los diálogos, z=300). Sin animaciones, sin sombras y sin blur.

- `Ctrl+K` abre y cierra; `Esc` cierra; clic fuera cierra.
- El campo tiene el foco al abrir. `↑`/`↓` recorren los resultados, `Enter`
  ejecuta el resaltado y el puntero también puede seleccionar.
- La búsqueda es por tokens separados por espacios. Cada token debe aparecer en
  la etiqueta, el id, las palabras clave o la categoría (sin distinguir
  mayúsculas ni acentos de forma deliberada: se comparan las cadenas tal cual).
- Orden: primero las coincidencias al inicio de la etiqueta, después las de
  inicio de palabra y luego el resto; a igualdad se conserva el orden de la
  tabla. Con la consulta vacía se muestra la tabla completa.
- Las acciones deshabilitadas se ven atenuadas y no se ejecutan.
- Las acciones de estado muestran una marca; por ejemplo el tema activo o la
  densidad actual.
- Al ejecutar se cierra la paleta antes de lanzar la acción para que el diálogo
  o prompt de destino reciba el foco. Al cerrar sin ejecutar, el foco vuelve a
  la vista activa.

La lista se filtra en JavaScript sobre las entradas de la tabla; es trabajo
acotado por pulsación y no hay temporizadores ni sondeos.

## Señales de refresco

`ActionRegistry` se reconstruye y compara la lista nueva con la anterior antes
de emitir `entriesChanged`. Solo emite si cambió algo, así que seleccionar
filas o cambiar de tema no provoca repintados innecesarios del menú o de la
paleta cuando el estado relevante no cambió.

Se escuchan: `selectionChanged`, `clipboardChanged`, `historyChanged`,
`itemCountChanged`, `themeModeChanged`, `showHiddenChanged`,
`singleClickOpenChanged`, `viewModeChanged`, `densityChanged`,
`sortModeChanged`, `sortDescendingChanged`, `tabsChanged`, `activeTabChanged`
y `reopenStateChanged`.

## Pendiente

- Ocultar y reordenar entradas por configuración (`menus.*.hide` y
  `menus.*.order`, §10.3 del diseño). Hoy el orden está en `menuSections()` y no
  se puede personalizar.
- Generar los `Shortcut` de QML desde la tabla para que `hint` y atajo real no
  puedan divergir. Requiere separar secuencia Qt (`Delete`) del texto visible
  (`Supr`) y resolver el contexto de foco de cada atajo.
- Migrar los menús contextuales cuando el registro acepte un contexto de fila.
