# Búsqueda recursiva

`Ctrl+Mayús+F` (o «Buscar en subcarpetas…» en el menú del fondo y en la paleta de
comandos) abre `qml/components/SearchOverlay.qml`.

## Modelo y trabajador

`src/app/SearchModel.{h,cpp}` es un `QAbstractListModel` expuesto como singleton
`Search`. La búsqueda corre en un `QThread` propio, creado solo la primera vez que
se usa, para que una ventana en reposo no mantenga un hilo extra.

- `QDirIterator` recorre la carpeta actual y todas sus subcarpetas.
- Los resultados se entregan en lotes de 64 con `beginInsertRows`, así que la
  lista se pinta mientras llegan.
- El tope es de 2000 resultados; al alcanzarlo se marca `truncated` y la interfaz
  avisa de que hay más coincidencias.
- Cada búsqueda tiene un token. Al empezar otra o cerrar el panel se sube el
  token y el trabajador abandona en la siguiente entrada del iterador, sin
  bloquear la interfaz.
- Se incluyen archivos ocultos solo si la casilla «Ocultos» está activa.

## Filtros

| Filtro | Qué incluye |
| --- | --- |
| Todo | cualquier entrada |
| Archivos / Carpetas | por tipo |
| Imágenes, Vídeos, Audio | por prefijo MIME |
| Texto | `text/*`, JSON y XML |
| PDF | `application/pdf` |

El filtro se aplica en el trabajador, no en un proxy de la vista principal, de
modo que la carpeta abierta y su filtro instantáneo no se tocan.

## Interfaz

- La consulta se lanza 280 ms después de la última pulsación; `Intro` la lanza de
  inmediato y pasa el foco a la lista.
- `↑`/`↓` recorren los resultados, `Intro` abre el elemento y `Esc` cierra.
- Abrir una carpeta navega a ella; abrir un archivo usa la aplicación
  predeterminada.
- Al cerrar se cancelan el trabajador y se vacía el modelo para liberar memoria.
