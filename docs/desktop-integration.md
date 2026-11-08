# Integración con el escritorio

Documenta `packaging/ooze-files.desktop` y `install-desktop.sh`: cómo se
registra Ooze Files en el sistema y qué ocurre cuando se usa como gestor de
carpetas por defecto.

## Dos caminos para abrir la aplicación

| Camino | Quién lo usa | Qué hace con el `Exec` |
|---|---|---|
| Launcher o shell | Quickshell, rofi, menús de aplicaciones | Interpreta la línea `Exec` completa, con comillas y códigos de campo. |
| Gestor por defecto | `xdg-open`, navegadores, otras aplicaciones al abrir una carpeta | Busca la aplicación asociada a `inode/directory` y lanza su `.desktop`. |

El launcher puede funcionar y el gestor por defecto no: los dos leen el mismo
archivo pero con intérpretes distintos.

## Fallo corregido: carpeta abierta en el navegador

Síntoma: con Ooze Files como aplicación por defecto de `inode/directory`, abrir
una carpeta lanzaba el navegador u otra aplicación, mientras que desde el
launcher se abría con normalidad.

Causa: `install-desktop.sh` escribía la ruta entre comillas.

```
Exec="/ruta/run.sh" -- --new-window %U
```

`xdg-open` obtiene el binario cogiendo la primera palabra del `Exec` sin quitar
las comillas y se la pasa a `which`. `which "\"/ruta/run.sh\""` no encuentra
nada, `xdg-open` considera que no hay aplicación asociada y cae a su último
recurso, `$BROWSER`.

Corrección: el script solo entrecomilla cuando la ruta contiene caracteres que
lo exigen.

```
Exec=/ruta/run.sh -- --new-window %U
```

## Estructura de `install-desktop.sh`

```
install-desktop.sh
├── argumentos      lee --debug, --release, --binary, --reuse, --set-default, --uninstall
├── rutas           calcula dónde van el .desktop y el icono dentro de XDG_DATA_HOME
├── exec_path()     devuelve la ruta tal cual o entrecomillada según los caracteres
├── desinstalacion  borra .desktop e icono y termina
├── binario         comprueba que el binario existe
├── lanzador        elige run.sh o el binario y arma command_line
├── generación      copia la plantilla línea a línea sustituyendo Exec e Icon
└── set-default     registra inode/directory con xdg-mime o gio
```

### `exec_path()`

| Ruta | Resultado |
|---|---|
| Solo letras, números y `_ . / + : @ -` | Se escribe sin comillas. |
| Cualquier otro carácter (espacios, `$`, `"`, `` ` ``, `\`) | Se escribe entre comillas dobles, con los caracteres reservados escapados según la especificación de Desktop Entry. |

Con una ruta que necesite comillas, `xdg-open` seguirá sin resolverla. La
ruta del proyecto no puede tener espacios ni paréntesis (lo exige `CMakeLists.txt`),
así que el caso afecta solo a `--binary` con rutas poco habituales.

### Generación del `.desktop`

La plantilla `packaging/ooze-files.desktop` tiene dos líneas `Exec`. El script
las reconoce por su texto exacto y escribe el resto sin cambios:

| Línea de la plantilla | Resultado |
|---|---|
| `Exec=ooze-files %U` | `command_line` más `-- --new-window` (o nada con `--reuse`) y `%U`. |
| `Exec=ooze-files --new-window %U` | `command_line` más `--new-window` y `%U`, para la acción «Nueva ventana». |
| `Icon=...` | `Icon=ooze-files`. |

El separador `--` solo se añade cuando se lanza `run.sh`, para que sus propias
opciones no se confundan con las de `ooze-files`.

## Comprobar que funciona

```sh
./install-desktop.sh --set-default
gio mime inode/directory
xdg-open ~/Descargas
```

`gio mime inode/directory` debe mostrar `ooze-files.desktop` como aplicación
registrada. Después de reinstalar, si el shell sigue usando la entrada anterior,
reinicia la sesión o el propio shell para que vuelva a leer las aplicaciones.
