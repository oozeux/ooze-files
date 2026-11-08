# Rendimiento

Ooze Files se usa a diario, así que el presupuesto es "abrir una carpeta grande
se siente instantáneo" y "en reposo no consume CPU". Estas son las decisiones que
sostienen ese presupuesto.

## Objetivos

| Escenario | Objetivo |
| --- | --- |
| Carpeta con 10 000 entradas | primer pintado < 100 ms; metadatos completos < 1 s |
| Navegar con el teclado | < 16 ms por pulsación |
| Reposo con la ventana visible | 0 % de CPU, sin repintados |
| Reposo con 5 pestañas inactivas | 0 % de CPU, sin lecturas de disco |
| Cambiar a una pestaña ya cargada | inmediato, sin volver a listar |
| Memoria de una carpeta de 10 000 entradas | < 50 MB de datos del modelo |

## Listado en dos fases

La fase A recorre el directorio y entrega nombre, tipo aproximado e icono por
extensión en lotes de 500. La fase B calcula tamaño, fecha, MIME e icono exacto
en lotes de 256, en un pool aparte.

Consecuencias:

- la lista aparece sin esperar 10 000 `lstat`;
- los `stat` no bloquean el `readdir` ni compiten con él;
- el modelo no se reordena al llegar metadatos, así que no hay saltos ni
  invalidaciones de índices;
- el hilo de listado es único y los de stat son dos: más hilos no mejoran en
  discos giratorios y sí empeoran la latencia del primer lote.

## Cancelación y generaciones

Cada navegación cancela la anterior con un `atomic<bool>` y sube un contador de
generación. Los resultados viejos se descartan antes de tocar el modelo. Sin
esto, entrar y salir rápido de carpetas encolaría trabajo inútil y podría
mezclar filas de dos directorios.

## Pestañas

Las pestañas no multiplican hilos ni trabajo en segundo plano:

- un único `DirectoryLoader` con los mismos tres hilos (1 listado + 2 stat)
  atiende a todas las pestañas; solo la activa está unida a él;
- al cambiar de pestaña, la anterior cancela y suelta su modelo y la nueva se
  une solo si hace falta;
- una pestaña ya cargada y limpia se muestra al instante, sin `readdir` ni
  `stat`;
- los `DirectoryWatcher` de pestañas inactivas no refrescan: marcan la pestaña
  como sucia y el refresco ocurre al activarla;
- la barra de pestañas solo se instancia cuando hay más de una pestaña o cuando
  `ui.tabBarAlways` está activo, así que el caso de una sola carpeta no paga
  altura ni delegados;
- `TabListModel` guarda valores planos, no punteros, y solo emite cambios
  cuando cambia el título, la URL o la pestaña activa.

El coste por pestaña es su `FileListModel`, su proxy y su historial. No se añadió
descarga LRU de modelos inactivos: con el límite por defecto de 16 pestañas el
consumo medido no lo justifica, y descargar modelos introduciría recargas al
volver a una pestaña.

## Sin polling

La interfaz se actualiza solo por eventos: navegación, filtro, orden, cambio de
tema o `F5`. La vigilancia de la carpeta visible usa `inotify` con `QSocketNotifier`
y coalescencia de eventos (180 ms) para refrescar de forma incremental, sin
temporizadores de sondeo. Las pestañas inactivas no refrescan: su watcher solo
las marca sucias.

El guardado de configuración usa un `QTimer` de 300 ms para no escribir el disco
en cada pulsación de densidad o cambio de tema. El único otro temporizador es el
desplazamiento automático del marco de selección (30 ms), que solo corre mientras
el puntero está en el borde de la vista durante el gesto; al soltar o cancelar se
detiene. No hay sondeos ni animaciones continuas.

## Arrastrar y soltar

Todo el trabajo ocurre dentro de eventos de arrastre: dibujar la tira flotante al
empezar (una sola pasada de `QPainter` con un icono cacheado, sin `grabToImage()`
ni espera asíncrona), componer el `text/uri-list`, resolver el destino al soltar,
abrir el menú de acciones y lanzar la operación en el hilo serializado de
`FileOperations`. No hay temporizadores, ni seguimiento continuo del puntero, ni
texturas permanentes. La tira se cachea por URL en `QQuickPixmap` mientras vive
la aplicación, así que arrastrar el mismo archivo dos veces no vuelve a dibujar.

El resaltado del destino usa `containsDrag` de `DropArea`, que solo cambia al
entrar o salir del área, y el clic posterior a un arrastre se suprime con una
bandera de un solo uso (sin `Timer`).

## Selección por marco

El marco calcula sus filas con aritmética de índices sobre `cellWidth`,
`cellHeight` o `rowHeight`, no recorriendo el modelo. El coste por movimiento es
proporcional al área del rectángulo y, como mucho, al número de celdas visibles.

`ApplicationController::selectRows` actualiza el conjunto de seleccionadas de
una vez y compara el resultado con el anterior antes de emitir
`selectionChanged`; un movimiento que no cambia la selección no repinta. El modo
`Ctrl` recibe únicamente la diferencia simétrica entre dos posiciones del marco,
así que los elementos que permanecen dentro no se alternan repetidamente.

El rectángulo es un `Rectangle` sin sombra, blur ni animación. Los colores
(`Theme.marqueeFill` y `Theme.marqueeBorder`) se derivan del acento del tema
activo.

El desplazamiento automático es la excepción de temporizador ya citada: 12 px
cada 30 ms, solo con el gesto activo y el puntero en la franja de 24 px. Si el
contenido no puede desplazarse en esa dirección, el temporizador se detiene.

## QML ligero

- Sin QtQuick Controls: cada control es una composición mínima de `Item`,
  `Rectangle`, `Text` y `MouseArea`.
- Sin animaciones continuas ni `Behavior` en bucles de pintado. Los cambios de
  color de hover son instantáneos; el coste de una animación no aportaba nada.
- Sin `layer.enabled`, sin `ShaderEffect`, sin blur: menos texturas y menos VRAM.
- `ListView`/`GridView` reciclan delegados por defecto. Los delegados usan
  `required property` para enlazar roles sin objetos `model` intermedios.
- `cacheBuffer: 400` en la cuadrícula: precarga una pantalla escasa para que el
  desplazamiento no cree delegados a ciegas.
- Imágenes de icono con `sourceSize` fijo y proveedor con caché por
  nombre+tamaño+emblema; no se decodifica el mismo icono dos veces.
- El texto se elide en el delegado (`elide`, `maximumLineCount`) para evitar
  cálculos de layout de párrafos largos.

## Rutas y cadenas

`PathUtils::toLocalBytes`/`fromLocalBytes` trabajan sobre ASCII percent-codificado
y `QByteArray`: sin conversiones a `QString` en el camino caliente. El formateo
de tamaño y fecha se hace una vez por entrada, al aplicar el lote de stat, no en
cada repintado.

## Filtro y orden

`FileListProxyModel` filtra por subcadena sobre el nombre ya decodificado y
ordena con `QCollator` (una sola instancia reutilizada, `mutable`). El proxy es
dinámico, así que insertar un lote no reordena manualmente: Qt aplica el orden
de forma incremental.

El filtro se limpia al navegar para no ocultar contenido de forma sorpresiva y
para no arrastrar trabajo de filtrado a la carpeta nueva.

## Memoria en reposo

Medición con la ventana visible, una carpeta pequeña y sin operaciones en curso:

| Backend | RSS | CPU en reposo |
| --- | --- | --- |
| GPU (predeterminado, NVIDIA + Qt Quick) | ~190 MB | 0 % |
| Software (`ui.lightweight`) | ~125 MB | 0 % |

La diferencia es el coste de cargar el controlador y las librerías de la GPU.
El modo ligero se elige en Configuración → Vista → Rendimiento o con
`OOZE_FILES_LIGHTWEIGHT=1`, y se aplica antes de crear `QGuiApplication`, así que
requiere reiniciar. No hay que activarlo en equipos donde la GPU ya está cargada
por el escritorio: en ese caso las páginas compartidas no son coste adicional.

Otras decisiones que mantienen el reposo bajo:

- la caché de iconos está acotada (512 entradas) y se vacía al cambiar de tema;
- las miniaturas se generan solo para delegados visibles o la vista previa
  abierta, y se guardan en disco para no repetir `ffmpeg`/`pdftoppm`;
- el hilo de búsqueda se crea en la primera búsqueda, no al arrancar;
- los proveedores de imágenes usan pools propios y se destruyen con la ventana;
- las pestañas inactivas no mantienen cargas, `stat` ni refrescos en curso: sus
  modelos ya cargados son datos en memoria y su watcher solo marca suciedad.

## Cómo medir

```sh
# uso de CPU y memoria en una carpeta grande
/usr/bin/time -v ./build/ooze-files /usr/lib

# tiempos de escena y pintado (requiere compilación con Qt de desarrollo)
QSG_RENDER_TIMING=1 ./build/ooze-files /usr/lib

# registro de depuración de la aplicación
./build/ooze-files --debug /usr/lib
```

Para perfilar el listado sin interfaz, las pruebas de `tests/unit` permiten
ejecutar `LocalVfs` sobre un directorio temporal grande.

## Lo que no se debe añadir sin medir

- Un `Timer` que refresque la carpeta.
- Animaciones en delegados que se reciclan.
- `Canvas`, `ShaderEffect` o blur en paneles permanentes.
- `stat` síncrono en el hilo principal (por ejemplo, al pintar un delegado).
- Cargar miniaturas antes de que la fila sea visible.
