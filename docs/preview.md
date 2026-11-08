# Vista previa, «Abrir con» y miniaturas

## Abrir con

`src/core/OpenWith.{h,cpp}` usa GIO para listar las aplicaciones registradas
para un tipo MIME y lanzarlas con un archivo:

| Función | Qué hace |
|---|---|
| `appsForMime(mime)` | Lista `{id, name, isDefault}` sin duplicados y filtrando las que no deben mostrarse (`NoDisplay`). Recorre el tipo, sus tipos padre (`QMimeType::allAncestors`, más `text/plain` para cualquier `text/*`) y las aplicaciones de reserva de GIO. La predeterminada va la primera. |
| `otherApps(mime)` | Resto de aplicaciones visibles del sistema que no están en `appsForMime`, ordenadas por nombre. |
| `launch(appId, mime, urls, &error)` | Busca la aplicación por id entre todas las instaladas y la lanza con `g_app_info_launch_uris`. Devuelve `false` con un mensaje si ya no existe. |

El controlador lo expone como `App.openWithApps(row)`,
`App.openWithOtherApps(row)` y `App.openRowWith(row, appId)`. Las carpetas usan
el tipo `inode/directory`. El menú contextual consulta la lista solo cuando se
abre sobre un único elemento local (nunca selecciones múltiples) y la muestra
como submenú «Abrir con», con la predeterminada marcada y, tras un separador,
el submenú «Otras aplicaciones» con el resto del sistema, de modo que un editor
que no declara el tipo del archivo (VSCodium, por ejemplo) siempre se puede
elegir.

`OpenWith.cpp` incluye `<gio/gio.h>` antes que cualquier cabecera de Qt: el
macro `signals` de Qt choca con un campo de GIO.

## Vista previa y propiedades

`Espacio` abre una vista previa sobre la fila actual (también desde el menú
contextual o la paleta de comandos, acción `selection.preview`).

`src/core/FilePreview.{h,cpp}` devuelve un `QVariantMap` con el tipo de
contenido (`kind`) y las propiedades:

| `kind` | Cuándo | Contenido |
|---|---|---|
| `image` | `QImageReader` puede leer el archivo y no supera 120 Mpx | se muestra con `Image` (asíncrono, `sourceSize` máx. 2048) |
| `video` | el tipo MIME empieza por `video/` | póster generado con `ffmpeg`/`ffprobe` y botón de reproducción |
| `pdf` | el tipo MIME es `application/pdf` | primera página renderizada con `pdftoppm` (poppler-utils) |
| `markdown` | el tipo MIME es `text/markdown` o la extensión es `.md`/`.markdown` | texto con `Text.MarkdownText` |
| `text` | el tipo MIME hereda de `text/plain`, no está vacío y no contiene bytes nulos | primeros 64 KiB y 400 líneas |
| `directory` | carpetas | número de elementos (tope 5000, se muestra «5000+») |
| `none` | todo lo demás, archivos remotos y errores | icono grande y propiedades |

Propiedades: tipo, tamaño, dimensiones (solo cabecera de la imagen), fechas,
permisos `rwxr-xr--`, propietario y grupo, destino del enlace y ubicación. Los
elementos remotos no se inspeccionan.

La lectura de texto e imagen es local y acotada, así que se hace en el hilo de
la interfaz sin bloquearla. Los pósteres de vídeo y las páginas PDF se generan
en hilos de trabajo (`ThumbnailProvider` y `PdfPreviewProvider`) y se guardan en
la caché de miniaturas XDG.

`qml/components/PreviewOverlay.qml`:

- `←`/`↑` y `→`/`↓` cambian de elemento y la selección de la vista los sigue;
- `Espacio` o `Esc` cierran, `Intro` abre el elemento;
- el panel de propiedades se oculta si la tarjeta mide menos de 620 px, para que
  funcione en ventanas pequeñas de un compositor con mosaico;
- mientras está abierta se desactivan los atajos de edición (`F2`, `Supr`,
  `Ctrl+A`…).

Si falta `ffmpeg`/`ffprobe`, la vista de vídeo lo explica en lugar de quedarse en
blanco. Lo mismo ocurre con `pdftoppm` y los PDF. La pestaña «Comportamiento» de
Configuración lista las dependencias opcionales y si están instaladas.

## Panel de información y propiedades

`F11` (o el menú principal) muestra `InfoPanel.qml` a la derecha: una vista previa
pequeña (imagen, póster de vídeo, primera página de PDF, Markdown renderizado,
primeras líneas de texto o icono) más las mismas propiedades que la vista previa.
Muestra la carpeta actual si no hay selección, el elemento si hay uno, y «N
elementos seleccionados» si hay varios. Se actualiza con un retardo de 120 ms
para no leer archivos mientras se arrastra una selección, y se oculta si la
ventana es demasiado estrecha para dejar sitio a la vista.

`Alt+Intro` o «Propiedades» en el menú contextual abren `PropertiesDialog.qml`,
una tarjeta flotante con las propiedades del elemento (o de la carpeta actual
desde el menú del fondo). El tamaño de una carpeta se calcula de forma acotada y
se muestra en el diálogo.

Las tres vistas (panel, diálogo y vista previa) comparten `DetailsList.qml`, que
construye la lista a partir del mapa de `FilePreview::describe`.

## Miniaturas

En la cuadrícula, las imágenes se dibujan con una `Image` sobre el icono:

- solo si `ui.thumbnails` está activo, el tipo MIME empieza por `image/` y el
  archivo pesa entre 1 byte y 30 MiB;
- `asynchronous: true` y `sourceSize` al doble del tamaño del icono, de modo que
  Qt decodifica ya reducido y el pixmap en memoria es pequeño;
- el delegado solo existe para las filas visibles (más el búfer de la vista), así
  que no se decodifica nada fuera de pantalla;
- si la carga falla o aún no ha terminado, se ve el icono normal;
- la lista no usa miniaturas: con 16-32 px no aportan y costarían más.

Los vídeos usan `image://thumbnails/<url>`:

- `ffprobe` obtiene la duración y `ffmpeg` extrae un fotograma al ~10 % (con
  reserva al segundo 1 y luego al 0), que evita los fundidos a negro iniciales;
- el trabajo se ejecuta en un `QThreadPool` propio con cancelación segura: la
  respuesta de QML y el `QRunnable` no comparten el mismo objeto, así que el
  pool nunca borra la respuesta mientras el lector de pixmaps la usa;
- los pósteres de PDF usan `pdftoppm` y un proveedor `image://previews`.

### Caché XDG compartida

`src/core/ThumbnailCache.{h,cpp}` implementa el estándar freedesktop:

- `~/.cache/thumbnails/{normal,large,x-large,xx-large}/<md5(uri)>.png`;
- solo se reutiliza una miniatura si `Thumb::MTime` coincide con la fecha del
  archivo de origen;
- se escribe con `QSaveFile` y se etiqueta con `Thumb::URI`, `Thumb::MTime` y
  `Software`, de modo que Dolphin, Nautilus y otros gestores comparten la caché.

La generación es perezosa: solo se pide para delegados visibles o para la vista
previa abierta. No hay sondeos ni regeneración periódica.
