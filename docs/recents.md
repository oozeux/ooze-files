# Recientes

Sección «Recientes» de la barra lateral (entre «Lugares» y «Sistema»). Abre la
ubicación virtual `recent:///`, que lista los archivos que el escritorio ha
registrado como usados recientemente.

## Qué significa «reciente»

La única fuente es `recently-used.xbel` (especificación *Recent File Storage* de
freedesktop.org), en `$XDG_DATA_HOME` (por defecto `~/.local/share`). Ooze Files
**no mantiene un historial propio** ni escribe en ese archivo.

Un archivo es reciente si alguna aplicación lo registró allí. Su fecha de
actividad es la mayor entre `added`, `modified` y `visited` del marcador y el
`modified` de sus aplicaciones. Esto implica:

- No es la fecha de modificación del archivo; esa se sigue mostrando en las
  propiedades.
- Solo aparece lo que las aplicaciones registran (GTK y muchas de GNOME y KDE
  lo hacen). Lo abierto con programas que no registran, o con el historial
  desactivado en el escritorio, no aparece, tampoco lo abierto desde Ooze Files.
- Si el archivo no existe o no se puede leer, la lista está vacía. No se inventa
  actividad.

## Filtrado y orden

1. Se leen como máximo 32 MiB del XML con `QXmlStreamReader`. Un XML dañado
   devuelve lo leído hasta el error.
2. Se descartan los marcadores sin `href` `file://` local, y los del grupo
   `private`.
3. Las rutas se decodifican a bytes (nombres no UTF-8 incluidos). Los duplicados
   se unifican conservando la actividad más reciente.
4. Orden: actividad descendente; los de fecha desconocida van al final.
5. El backend comprueba cada ruta con `lstat` (como máximo 1000 candidatas) y
   muestra hasta 200 entradas:
   - `ENOENT` o `ENOTDIR`: la entrada se oculta, sin tocar el `.xbel`.
   - Otros errores (por ejemplo falta de permiso en una carpeta): se muestra
     sin tamaño ni fecha, y al abrirla se informa de que no hay acceso.

Los archivos de la papelera quedan ocultos de forma natural: ya no existen en su
ruta original.

## Interfaz

- Columnas de la vista de lista: nombre, ubicación (carpeta contenedora, con `~`
  para Inicio) y «Último uso». La vista de cuadrícula muestra nombre e icono.
- El orden es siempre por actividad (`FileListProxyModel::setActivityOrder`) y
  no depende del orden elegido para las carpetas.
- Se puede abrir, abrir con, vista previa, copiar, copiar ruta y propiedades. En
  el menú contextual, «Mostrar carpeta contenedora» navega a la carpeta y
  selecciona el archivo.
- Cortar, renombrar, eliminar, crear, pegar, comprimir y extraer están
  desactivados (`App.inVirtualLocation`): la lista no es una carpeta.

## Actualización

`DirectoryWatcher` vigila el directorio de datos con un filtro por nombre y solo
reacciona a `recently-used.xbel` (las aplicaciones lo reescriben con un `rename`).
La recarga está agrupada (180 ms) y, como el resto de pestañas, solo se aplica a
la pestaña activa. Si el directorio de datos desaparece no se navega a ninguna
parte.

## Implementación

| Pieza | Archivo |
|---|---|
| Lectura del `.xbel` | `src/core/RecentFiles.{h,cpp}` |
| Backend `recent:///` | `src/vfs/RecentVfs.{h,cpp}` |
| Entrada de la barra lateral | `Places::build` (sección `recent`) |
| Roles `locationText`, `activity`, `activityText` | `FileListModel` |

Pruebas: `tests/unit/tst_recentfiles.cpp`.
