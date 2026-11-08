# Papelera

Entrada «Papelera» al final de la sección «SISTEMA» de la barra lateral. Abre la
ubicación virtual `trash:///`.

## Integración con el escritorio

No hay una papelera propia. Se lee y se modifica la que define la
[especificación de papelera de freedesktop.org](https://specifications.freedesktop.org/trash-spec/latest/),
la misma que usan Nautilus, Dolphin y `gio trash`, y **no depende de gvfs**
(Qt 6 solo sabe enviar a la papelera con `QFile::moveToTrash`; no lista, restaura
ni vacía). Enviar a la papelera sigue haciéndolo `QFile::moveToTrash` con
`g_file_trash` como respaldo.

`TrashService` (`src/core/TrashService.{h,cpp}`) descubre estas papeleras:

- La del usuario: `$XDG_DATA_HOME/Trash` (por defecto `~/.local/share/Trash`).
- Las de cada volumen montado: `$topdir/.Trash/$uid` (solo si `.Trash` es un
  directorio real, no un enlace, con el bit *sticky*) y `$topdir/.Trash-$uid`.
  Solo se consideran las que ya existen, nunca se crean, y solo en volúmenes
  escribibles con dispositivo `/dev/…`, para no bloquear con montajes de red.

Cada papelera contiene `files/` (los elementos), `info/` (un `.trashinfo` por
elemento con `Path` y `DeletionDate`) y `directorysizes`.

La variable de entorno `OOZE_FILES_TRASH_HOME_ONLY=1` limita la búsqueda a la
papelera del usuario. Las pruebas la activan para no tocar las papeleras de los
volúmenes reales.

## Lectura de los metadatos

- `Path` se decodifica de porcentaje. Una ruta absoluta es válida en cualquier
  papelera. Una relativa solo en las de volumen y se resuelve contra `$topdir`.
- Se rechazan rutas con segmentos `.` o `..` o con bytes nulos. Ese elemento
  queda sin ubicación original.
- Un elemento sin `.trashinfo` válido se lista con su nombre interno y solo se
  puede eliminar, no restaurar. Un `.trashinfo` sin archivo se limpia al vaciar.
- `DeletionDate` es hora local, como indica la especificación.

## Interfaz

- Columnas de la vista de lista: nombre, **ubicación original** y **eliminado**.
  El orden en la raíz es por fecha de eliminación descendente.
- Se puede navegar por las carpetas eliminadas (`trash:///<papelera>/<ruta>`).
  Dentro de ellas la vista es de solo lectura y solo muestra el contenido real.
  `<papelera>` es la ruta de la papelera en hexadecimal; `TrashService::resolve`
  solo acepta papeleras descubiertas y rechaza `..`, de modo que una URL
  manipulada no puede apuntar fuera.
- Las entradas llevan la ruta real del archivo, así que abrir, vista previa,
  copiar, copiar ruta y propiedades funcionan sobre el archivo verdadero.
- Menú contextual: Abrir, Restaurar, Eliminar permanentemente, Copiar, Copiar
  ruta y Propiedades. Menú de fondo: Vaciar la papelera. También están en la paleta
  de comandos («Restaurar de la papelera», «Vaciar la papelera»).
- Cortar, renombrar, crear, pegar, comprimir y extraer están desactivados.
- `Supr` y `Shift+Supr` eliminan permanentemente, siempre con confirmación, igual
  que vaciar. Soltar archivos sobre «Papelera» en la barra lateral los envía a la
  papelera sin preguntar el modo (no hay menú de mover/copiar).
- Estados: «La papelera está vacía» y los errores de lectura del VFS habituales.

## Restaurar

`RestoreTask` mueve cada elemento de primer nivel a su ubicación original con la
misma lógica de conflictos que copiar y mover (ver
[drag-and-drop.md](drag-and-drop.md)): un nombre ya existente abre el diálogo de
conflicto (otro nombre, sobrescribir o escribir dentro).

- Si la carpeta original ya no existe se pregunta: **Recrear la carpeta**
  (`mkdir -p`, modo 0755), **Restaurar en Inicio** o Cancelar. Sin interfaz se
  recrea.
- Si la papelera y el destino están en dispositivos distintos se copia y se borra
  el original solo tras copiar con éxito (`EXDEV`).
- Tras restaurar se borran el `.trashinfo` y la línea de `directorysizes`. Las
  restauraciones no se registran en deshacer.
- Solo se restauran elementos de primer nivel; lo de dentro de una carpeta
  eliminada se restaura con ella.

## Eliminar y vaciar

`TrashDeleteTask` elimina el elemento y sus metadatos. Antes de borrar una carpeta
se le añaden permisos de escritura y acceso, porque un directorio de solo lectura
no se puede vaciar. Vaciar recorre todas las papeleras descubiertas y limpia los
`.trashinfo` huérfanos. Nunca se elimina el directorio `Trash` en sí.

## Actualización

`DirectoryWatcher` vigila con inotify `files/` e `info/` de cada papelera
descubierta (sin timers) y recarga con la agrupación habitual. Limitaciones:

- Si la papelera del usuario aún no existe, no hay nada que vigilar hasta volver a
  abrir la vista.
- Las papeleras de volúmenes montados después de abrir la vista se detectan al
  recargar.
- Los volúmenes desmontados no aparecen: no se puede saber qué contienen.
- El icono de la barra lateral no cambia entre vacía y llena.

## Implementación

| Pieza | Archivo |
|---|---|
| Lectura y borrado según la especificación | `src/core/TrashService.{h,cpp}` |
| Backend `trash:///` | `src/vfs/TrashVfs.{h,cpp}` |
| Restaurar, eliminar y vaciar | `RestoreTask`, `TrashDeleteTask` en `FileOperations.cpp` |
| Confirmaciones | `qml/components/ConfirmDialog.qml` |

Pruebas: `tests/unit/tst_trash.cpp`.
