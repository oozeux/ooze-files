# Deshacer y rehacer

Documenta `src/core/UndoStack.{h,cpp}` y el diario de `FileOperations`.

## Qué se deshace

| Operación | Entrada del diario | Deshacer | Rehacer |
|---|---|---|---|
| Mover, pegar tras cortar, arrastrar para mover | `Moved` | devuelve cada elemento a su sitio | lo mueve otra vez |
| Renombrar | `Moved` | recupera el nombre anterior | aplica el nuevo |
| Mover a la papelera | `Trashed` | restaura desde la papelera | la vuelve a enviar |
| Copiar, crear carpeta o archivo, enlazar, comprimir, extraer | `Created` | envía lo creado a la papelera | lo restaura de la papelera |

Lo creado se deshace enviándolo a la papelera, no borrándolo: si el archivo ya
tenía contenido o la carpeta ya tenía elementos, no se pierde nada, y rehacer
recupera exactamente lo mismo.

Las operaciones en las que se eligió **sobrescribir** o **escribir dentro** no se
registran: el original reemplazado va a la papelera (si es posible) y se puede
recuperar a mano, pero deshacer no lo restaura automáticamente. Una copia o un
movimiento en los que solo se eligió otro nombre sí se registran con normalidad.

**No se registra** la eliminación permanente (`Shift+Supr`): no hay forma de
volver atrás. Tampoco se registran los elementos cuyo nombre no es UTF-8 válido
ni los que la papelera del sistema no devuelve con su ruta.

## Cómo funciona

1. Cada tarea de `FileOperations` anota en un `JournalEntry` lo que ha hecho
   con éxito (`note()`), también si la operación falla a medias.
2. Al terminar, `finish()` emite `changeRecorded(kind, label, firsts, seconds)`.
   Las rutas viajan como `QStringList` para cruzar hilos sin registrar tipos.
3. `ApplicationController` guarda la entrada en un `UndoStack` (máximo 50; al
   registrar algo nuevo se descarta lo que se podía rehacer).
4. `undo()`/`redo()` envían la entrada con `FileOperations::applyJournal`, que
   ejecuta un `JournalTask` en el pool y emite `journalApplied` con las rutas
   actualizadas; el controlador mueve entonces la posición de la pila.

La papelera se usa con `QFile::moveToTrash`, que devuelve la ruta dentro de la
papelera. Al restaurar se elimina también el `.trashinfo` correspondiente.

## Reglas de seguridad

- Si el destino original ya existe o su carpeta ya no está, deshacer falla con
  un mensaje y la pila no cambia.
- Si el elemento ya no está en la papelera (por ejemplo, se vació), deshacer o
  rehacer fallan con un mensaje.
- No se inicia un deshacer mientras hay otra operación en curso; el pool de
  operaciones tiene un único hilo, de modo que el orden de las señales coincide
  con el de ejecución.
- Una operación entre dispositivos distintos usa copiar y borrar en lugar de
  `rename`, igual que mover.

## Interfaz

`Ctrl+Z` deshace, `Ctrl+Shift+Z` o `Ctrl+Y` rehacen. El menú principal muestra
las dos entradas con la operación afectada («Deshacer: Mover a la papelera»);
están deshabilitadas cuando no hay nada que hacer.
