# Arrastrar y soltar

Documento de comportamiento, implementación y verificación del arrastre y
soltura (punto 1 de la hoja de ruta). No hay comentarios en el código: esta es
la referencia.

## Alcance

1. Sacar archivos y carpetas de Ooze Files hacia otros programas o hacia el
   escritorio.
2. Soltar archivos y carpetas sobre la ventana, vengan de otro programa o de la
   propia aplicación.
3. Mover, copiar o enlazar dentro de la aplicación usando el ratón, sin pasar por
   el portapapeles.

## Arrastrar desde Ooze Files

- Arrastrar un elemento **no seleccionado** arrastra solo ese elemento.
- Arrastrar un elemento **seleccionado** arrastra toda la selección en el orden
  del listado.
- El arrastre empieza al superar el umbral del sistema; un clic normal no inicia
  ningún arrastre.
- Al empezar el arrastre aparece una tira flotante con el icono del elemento y
  su nombre, sin fondo ni resaltado de selección, y sigue al puntero. Si se
  arrastra una selección múltiple, el nombre lleva el sufijo `(+N)` con el resto
  de elementos. La tira la dibuja `DragPreviewProvider` en el momento de
  empezar, una sola vez por arrastre.
- Después de arrastrar, el clic de suelta no abre el archivo (se suprime una
  sola vez, sin temporizadores).
- Datos publicados: `text/uri-list` con una URI por línea, separadas por `CRLF`
  y codificadas con `QUrl` (las rutas se tratan como bytes en el núcleo).
- Acción propuesta: **mover** (se ofrecen `CopyAction | MoveAction` y Qt propone
  mover cuando la acción está permitida). El programa de destino decide la
  acción final; si se quiere copiar sin ambigüedad, usar `Ctrl+C` y `Ctrl+V`.

Destinos habituales: otro gestor de archivos, el escritorio, un editor, un
cliente de correo, un navegador.

## Soltar en Ooze Files

| Zona de soltura | Resultado |
| --- | --- |
| Icono o fila de una **carpeta** | dentro de esa carpeta |
| Icono o fila de un **archivo** | en la carpeta actual |
| Fondo vacío de la vista | en la carpeta actual |
| **Lugar** de la barra lateral | en esa ruta |

Al soltar no se ejecuta nada de inmediato: aparece un menú en el punto de
soltura con el destino y tres acciones.

| Acción | Efecto |
| --- | --- |
| **Mover aquí** | traslada el origen (no queda nada en su carpeta) |
| **Copiar aquí** | duplica el origen y conserva el original |
| **Enlazar aquí** | crea un enlace simbólico que apunta al origen |
| **Cancelar** (`Esc`) | no toca el disco |

El mismo menú sirve para arrastres internos y externos, así que da igual lo que
proponga el origen: la decisión final es siempre del usuario.

Reglas:

- Ante el programa de origen el arrastre se acepta como **copiar** (cuando el
  origen lo permite), de modo que ningún programa externo borre archivos
  mientras se elige la acción. Si se elige «Mover aquí», el borrado del origen lo
  hace Ooze Files.
- Solo se aceptan URLs locales. Si no hay ninguna, la barra de estado muestra
  «Solo se admiten archivos locales» y no se toca el disco.
- Conflicto de nombres al copiar o mover (también al pegar): se pregunta qué
  hacer (ver [Conflictos de nombres](#conflictos-de-nombres)). Nunca se
  sobrescribe nada sin que el usuario lo elija. Los enlaces conservan siempre un
  nombre único (`archivo (1).txt`).
- Mover a la misma carpeta no hace nada (ni error ni duplicado).
- Mover una carpeta dentro de sí misma se rechaza con un mensaje explícito.
- Enlazar acepta archivos y carpetas; el enlace puede quedar roto si luego se
  borra el origen, como en cualquier enlace simbólico. El enlace se muestra con
  el icono de su tipo: `folder-symlink` si apunta a una carpeta (y se puede
  abrir como carpeta) y el icono de la extensión si apunta a un archivo. En
  ambos casos lleva un pequeño emblema de enlace en la esquina inferior derecha.
- Todo el trabajo lo hace `FileOperations` en su hilo serializado: `FICLONE`
  cuando el sistema de archivos lo permite, `rename` atómico dentro del mismo
  sistema de archivos y copia segura con nombre temporal en el resto de casos.

Mientras se arrastra sobre un destino válido, el elemento destino se resalta con
el color de selección. No hay animaciones ni capas adicionales.

## Implementación

Núcleo y controlador:

- `ApplicationController::dragUriList(int row)` construye el `text/uri-list` de
  la selección (o de la fila indicada si no forma parte de la selección).
- `Theme.dragImageUrl(nombre, icono, emblema, total)` compone la URL
  `image://dragpreview/…` con el nombre, el icono, el emblema de enlace, el
  contador de la selección, el color de texto, la tipografía y la revisión de
  iconos (para invalidar la caché al cambiar de tema).
- `ApplicationController::dropUrls(urls, action, targetUrl)` filtra las URLs
  locales, resuelve el destino vacío como la carpeta actual y lanza la operación
  según `Qt::MoveAction`, `Qt::LinkAction` o, por omisión, copia.
- `FileOperations::link()` crea enlaces simbólicos con nombre único en el
  destino y publica el resultado por la misma señal de operaciones.

Interfaz:

- `qml/views/FileGridView.qml` y `qml/views/FileListView.qml`
  - `Drag` sobre el delegado: `Drag.Automatic`, acciones copiar/mover y punto
    activo sobre el centro del icono de la tira (constante `Theme.dragHotSpot`,
    así que no cambia con el puntero y no hay que reasignarlo en `onPressed`).
  - `DragHandler` con `target: null` dentro del `MouseArea` del delegado: al
    activarse toma el `text/uri-list`, asigna `Theme.dragImageUrl(...)` como
    `Drag.imageSource` y activa el arrastre en el mismo evento. No hay captura
    asíncrona: la imagen ya está lista y el arrastre no espera un fotograma.
  - `DropArea` por delegado: carpeta → esa carpeta; archivo → carpeta actual.
  - `DropArea` de fondo (`z: -1`): huecos vacíos → carpeta actual.
  - Los `DropArea` no ejecutan la operación: aceptan el arrastre como copia
    (para que el origen no borre nada), emiten `dropRequested(urls, destino,
    x, y)` con coordenadas de escena y el menú decide en `qml/Main.qml`.
  - `dragSuppressClick` evita abrir el elemento con el clic de suelta.
- `qml/panels/Sidebar.qml`: un `DropArea` por lugar, que emite el mismo
  `dropRequested` con la ruta del lugar.
- `qml/Main.qml`: `requestDrop()` abre el menú de soltura (`PopupMenu`) en las
  coordenadas recibidas y `handleDropMenu()` traduce la entrada elegida a
  `Qt::MoveAction`, `Qt::CopyAction` o `Qt::LinkAction`.
- `src/app/IconProvider.cpp`: resuelve el icono del tema y, cuando la URL pide
  `emblem=link`, compone el emblema del enlace sobre la esquina inferior derecha
  (`emblem-symbolic-link` y alias; si el tema no lo trae, se dibuja un distintivo
  propio). El resultado se cachea por nombre, tamaño y emblema.
- `src/app/DragPreviewProvider.cpp`: dibuja la tira de arrastre con `QPainter`
  sobre un lienzo transparente: icono (con emblema si procede), nombre recortado
  con puntos suspensivos y `(+N)` cuando hay más elementos. Mismo tamaño que la
  tipografía de la interfaz y sombra suave en el texto para que se lea sobre
  cualquier fondo. No captura el delegado ni el resaltado de selección.

Rendimiento: todo el trabajo ocurre dentro de eventos de arrastre (entrar,
mover, soltar) y al abrir el menú. La imagen de arrastre se dibuja una vez al
empezar, en el hilo de la interfaz, con una sola pasada de `QPainter` y un icono
ya cacheado; no hay `grabToImage()`, ni lectura de disco, ni espera asíncrona. No
se añaden temporizadores, sondeos, animaciones ni texturas permanentes.

## Conflictos de nombres

Cuando el destino ya contiene un elemento con el mismo nombre, `TransferTask`
pide una decisión a la interfaz y espera la respuesta. El diálogo
(`ConflictDialog.qml`) muestra tamaño y fecha de ambos elementos.

| Caso | Opciones |
|---|---|
| archivo contra archivo | otro nombre, sobrescribir, cancelar |
| carpeta contra carpeta | otro nombre, combinar reemplazando repetidos, combinar preguntando, reemplazar la carpeta entera, cancelar |
| archivo contra carpeta (o al revés) | otro nombre, cancelar |

Reglas:

- **Otro nombre**: se elige un nombre libre (`nombre (1).ext`; `.tar.gz` se
  trata como una sola extensión). La colocación final usa
  `renameat2(RENAME_NOREPLACE)`, así que una carrera con otro proceso nunca pisa
  datos.
- **Sobrescribir** (archivos) o **Reemplazar la carpeta entera**: el nuevo elemento se prepara primero con un nombre temporal
  `.ooze-tmp-<pid>-<n>` en la carpeta destino, de modo que un fallo deja intacto
  el original. Después el original se envía a la papelera y el nuevo toma su
  nombre. Si la papelera no está disponible (por ejemplo en un medio extraíble),
  un archivo se reemplaza con `rename(2)` atómico y una carpeta se aparta, se
  sustituye y se borra: en ese caso el original no es recuperable. Una carpeta no
  vacía no se puede reemplazar con una sola llamada al sistema, por eso no se
  trata como un archivo.
- **Combinar y reemplazar los archivos repetidos**: fusión recursiva que conserva
  lo que solo existe en el destino (`b.txt`, `c.txt`…) y reemplaza únicamente los
  archivos con el mismo nombre, sin volver a preguntar. Cada archivo reemplazado
  se prepara con un temporal y el anterior se envía a la papelera si es posible.
  Es la opción para volcar sobre la carpeta principal solo los archivos
  modificados de una copia.
- **Combinar y preguntar por cada archivo repetido**: la misma fusión, pero cada
  nombre repetido dentro de las carpetas vuelve a mostrar el diálogo.
- **Reemplazar la carpeta entera**: descarta el contenido anterior. Al mover, cada elemento se mueve por
  separado y la carpeta de origen solo se elimina si queda vacía.
- **Aplicar a todos**: recuerda la decisión por tipo de conflicto (archivos,
  carpetas, tipos distintos) durante el resto de la operación.
- **Cancelar** detiene la operación; lo ya completado se conserva y se registra.
- Entre sistemas de archivos (`EXDEV`) mover es copiar a un temporal y borrar el
  origen solo cuando la copia está completa. Si el borrado falla, quedan ambas
  copias y se informa del error.
- Copiar en la misma carpeta sigue creando un nombre único sin preguntar, porque
  el conflicto sería con el propio origen. Un elemento que es el mismo inodo que
  el destino nunca se sobrescribe.
- Sin ningún receptor de `conflictRequested` (pruebas, uso sin interfaz) se
  aplica «otro nombre».
- Un error de permisos, un destino que desaparece o un disco lleno aborta la
  operación con el mensaje correspondiente y elimina los temporales.
- Si el proceso muere a mitad de una operación pueden quedar archivos
  `.ooze-tmp-*` ocultos en la carpeta destino; son seguros de borrar.

## Portapapeles entre ventanas

Copiar y cortar publican la selección en el portapapeles del sistema:
`text/uri-list`, `text/plain` (rutas), `x-special/gnome-copied-files` y
`application/x-kde-cutselection` (marca de «cortar»). Pegar lee de ahí, de modo
que otra ventana de Ooze Files (otro proceso), Nautilus o Dolphin pueden pegar lo
copiado y viceversa, con la semántica de mover para «cortar». Solo el pegado de
«cortar» vacía el portapapeles; otras operaciones (borrar, mover por arrastre,
renombrar) no lo afectan. Si el portapapeles pasa a contener otra cosa (por
ejemplo texto), «Pegar» se desactiva.

Los elementos del portapapeles se marcan en las vistas de cuadrícula y lista
mediante `App.clipboardMark(url)`: `1` copiado (borde de acento), `2` cortado
(contenido atenuado, `Theme.cutOpacity`) y `0` sin marca.

En Wayland, el contenido del portapapeles desaparece si se cierra la ventana que
lo copió, salvo que haya un gestor de portapapeles.

## Reordenar Lugares

Los lugares integrados y los propios de la sección «Lugares» se pueden
reordenar arrastrándolos. «Sistema», «Dispositivos» y el resto de secciones no.

- Es un gesto interno de la barra lateral: un `DragHandler` por fila, sin sesión
  de arrastre de Qt ni tipos MIME. Por eso no cruza con el arrastre de archivos:
  los `DropArea` de los lugares nunca lo ven, no se abre el menú de soltura y no
  se toca el sistema de archivos.
- Al superar el umbral, una tira con icono y nombre sigue al puntero, la fila
  original queda atenuada y una línea de inserción marca dónde caerá. Soltar
  fuera de la barra lateral cancela el gesto.
- Cerca del borde superior o inferior de la lista se desplaza el contenido en
  cada movimiento del puntero (sin temporizadores).
- El clic de suelta no navega: se suprime hasta la siguiente pulsación.
- «Subir» y «Bajar» del menú contextual de un lugar hacen lo mismo con teclado o
  sin arrastre.
- La posición se guarda en `placesOrder` (ver [configuration.md](configuration.md)).

Implementación: `Places::build` ordena según `Config::placesOrder`;
`ApplicationController::movePlace` y `movePlaceBy` calculan el nuevo orden,
conservan las rutas ausentes y lo guardan, y `placesChanged` reconstruye la lista.
Cada lugar lleva un campo `id` con su ruta normalizada.

## Límites conocidos

- Las acciones se eligen en el menú de soltura; en Wayland no se leen `Ctrl` ni
  `Shift` al soltar, así que no hay atajos de modificador para la acción.
- Arrastrar en vertical dentro del listado puede iniciar el desplazamiento del
  listado antes que el arrastre, porque `ListView`/`GridView` y el
  `DragHandler` comparten umbral. Los desplazamientos horizontales y el arrastre
  desde la cuadrícula no tienen este problema.
- La imagen de arrastre es una tira con icono y nombre, no una miniatura real del
  contenido: un archivo de imagen se representa con el icono de su tipo.
- La tira se dibuja a escala 1×; en pantallas HiDPI (factor > 1) puede verse algo
  menos nítida hasta que se añada la escala del dispositivo a la URL.
- Las migas de pan no se pueden arrastrar. Los lugares de la barra lateral solo
  se arrastran para reordenarlos (ver más abajo), no como origen de archivos.
- Solo hay una ventana, así que no hay arrastre entre ventanas de la aplicación.
- Solo se publica `text/uri-list`. Los formatos de interoperabilidad de
  Nautilus (`x-special/gnome-copied-files`) y Dolphin
  (`application/x-kde-cutselection`) y el arrastre entre Wayland y XWayland
  quedan para más adelante (ver `FileManager.md`, §11.9).
- Soltar sobre «Papelera» de la barra lateral envía los archivos a la papelera
  directamente, sin menú de mover/copiar/enlazar (ver [trash.md](trash.md)).

## Verificación

Automática (incluida en `ctest`):

- `tests/unit/tst_controller.cpp`
  - `dragUriList` usa la selección completa y separa las URIs con `CRLF`.
  - `dragUriList` cae en la fila indicada cuando no hay selección.
  - Soltar con `Qt::MoveAction` mueve el archivo y borra el origen.
  - Soltar con `Qt::CopyAction` deja origen y destino.
  - Soltar con `Qt::LinkAction` crea un enlace simbólico al origen.
  - Soltar con mover en la misma carpeta no genera copias.
  - Una URL remota no se toca y deja el mensaje esperado.
- `smoke-ui` carga la interfaz completa en offscreen (los delegados y el menú de
  soltura se crean sin errores de QML).

Manual (el arrastre de puntero no se puede automatizar con las herramientas
disponibles):

1. Arrastrar un archivo de la cuadrícula al escritorio: aparece el archivo y una
   tira con el icono y el nombre sigue al puntero.
2. Arrastrar un archivo a otra aplicación que acepte `text/uri-list`.
3. Seleccionar tres elementos y arrastrar uno de ellos a una carpeta: la tira
   muestra `(+2)` y el menú permite mover los tres.
4. Arrastrar un archivo a una carpeta de la barra lateral y elegir «Mover aquí».
5. Arrastrar un archivo desde otra aplicación a la ventana y elegir «Copiar
   aquí».
6. Elegir «Enlazar aquí»: aparece un enlace simbólico que abre el archivo
   original.
7. Soltar en el fondo de la vista: el menú apunta a la carpeta actual.
8. Un clic normal sigue abriendo archivos y no inicia arrastres.
9. Arrastrar y cancelar con `Esc`: no se mueve nada y no se abre nada.
