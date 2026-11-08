# Comprimir y extraer

Documenta `src/core/Archive.{h,cpp}` y `ArchiveTask` en `FileOperations`.

No se enlaza ninguna biblioteca de archivos: se usan las herramientas del
sistema, y los formatos que no tienen herramienta instalada no se ofrecen.

## Comprimir

«Comprimir ▸» del menú contextual lista los formatos disponibles:

| Formato | Herramientas (por orden de preferencia) |
|---|---|
| `.zip` | `zip`, `7z`/`7zz`/`7za`, `bsdtar` |
| `.tar.gz` | `tar` (o `bsdtar`) y `gzip` |
| `.tar.xz` | `tar` (o `bsdtar`) y `xz` |
| `.tar.zst` | `tar` (o `bsdtar`) y `zstd` |
| `.7z` | `7z`/`7zz`/`7za` |

- El archivo se crea junto a los elementos seleccionados, que deben estar en la
  misma carpeta.
- Nombre: el de la carpeta si es una; el nombre sin extensión si es un archivo;
  el de la carpeta contenedora si son varios. Si ya existe se usa
  `nombre (2).tar.gz`, `nombre (3).tar.gz`…
- Los nombres que empiezan por `-` se pasan como `./-nombre` (y `tar` recibe
  `--`) para que no se interpreten como opciones.
- Si la herramienta falla, se borra el archivo parcial y se muestra la primera
  línea de su error.

### Comprimir como…

La última entrada del submenú abre `CompressDialog.qml`: nombre editable, formato
(chips con la extensión) y una vista previa del nombre final. Si ya existe un
archivo con ese nombre se avisa y se numera igual que arriba.

- `App.suggestedArchiveName()` propone el nombre por defecto.
- `App.archiveTarget(nombre, formato)` devuelve `fileName`, `error` y `exists`; la
  vista previa y la tarea usan el mismo código (`FileOperations::archiveTargetName`),
  así lo que se ve es lo que se crea.
- `App.compressSelectionAs(formato, nombre)` lanza la tarea. Si el nombre ya trae la
  extensión del formato elegido no se duplica; `/`, `.`, `..`, vacío o más de 240
  bytes se rechazan.

Vigilar: `archiveTarget` consulta el disco en cada tecla (barato, pero es el único
acceso a disco lanzado desde QML). `cleanArchiveBase` concentra las reglas del
nombre; cualquier cambio ahí afecta a vista previa y tarea a la vez.

## Extraer

«Extraer aquí» aparece con un único archivo `.zip`, `.tar`, `.tar.gz`, `.tgz`,
`.tar.xz`, `.txz`, `.tar.zst`, `.tzst`, `.tar.bz2`, `.tbz2`, `.7z` o `.rar`.

- Siempre se extrae en una carpeta nueva con el nombre del archivo sin su
  extensión (con sufijo numérico si ya existe). Así nunca se mezcla el contenido
  con el de la carpeta actual ni se pisan archivos.
- Herramientas: `tar` para los tar; `unzip`, `bsdtar` o `7z` para zip; `7z` o
  `bsdtar` para 7z; `unrar`, `7z` o `bsdtar` para rar.
- Las rutas peligrosas (`..`, absolutas) las rechazan las propias herramientas
  por defecto; además se extrae siempre en una carpeta recién creada.
- Si falla, se elimina la carpeta creada.

## Límites

- Se ejecuta como un proceso hijo en el hilo de operaciones y no se puede
  cancelar; mientras dura, el resto de operaciones esperan.
- `unzip` y `tar` devuelven `1` para avisos sin importancia; se aceptan.
- Solo archivos locales.
- Comprimir y extraer se pueden deshacer: lo creado se envía a la papelera
  (ver [undo.md](undo.md)).
