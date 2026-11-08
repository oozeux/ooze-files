# Scripts de compilación y ejecución

Documenta `build.sh` y `run.sh`, ambos en la raíz del proyecto.

## `build.sh`

Configura con los presets de `CMakePresets.json` y compila con Ninja.

```
./build.sh [opciones]
```

| Opción | Efecto |
|---|---|
| `-r`, `--release` | `linux-release` (RelWithDebInfo) en `build/release`. Es el modo por defecto. |
| `-d`, `--debug` | `linux-debug` en `build/debug`. |
| `-c`, `--clean` | Borra el directorio de compilación del modo elegido antes de configurar. |
| `-t`, `--test` | Ejecuta `ctest` al terminar con `QT_QPA_PLATFORM=offscreen`. |
| `--no-tests` | Pasa `-DOOZE_FILES_BUILD_TESTS=OFF`. Incompatible con `--test`. |
| `-j N`, `--jobs N` | Trabajos en paralelo. Sin valor usa todos los núcleos. |
| `-h`, `--help` | Muestra la ayuda. |

Bloques del script:

- **raíz**: se coloca en el directorio del script, así funciona desde cualquier
  ruta o mediante un enlace simbólico.
- **argumentos**: lee las opciones y rechaza las desconocidas con código `2`.
- **entorno nix**: si falta `cmake` o `ninja` en el `PATH`, se vuelve a lanzar
  dentro de `nix-shell nix/devshell.nix` (o `nix develop -f nix/devshell.nix`
  si no existe `nix-shell`). La variable `OOZE_IN_NIX` evita que se repita.
- **configuración**: `cmake --preset`, con los tests activados o no.
- **compilación**: `cmake --build --preset`.
- **tests**: `ctest --test-dir build/<modo>` si se pidió `--test`.

El binario queda en `build/<modo>/ooze-files`.

## `run.sh`

Ejecuta el binario compilado, y lo compila antes si no existe.

```
./run.sh [opciones] [-- argumentos de ooze-files]
```

| Opción | Efecto |
|---|---|
| `-r`, `--release` | Usa `build/release/ooze-files`. Es el modo por defecto. |
| `-d`, `--debug` | Usa `build/debug/ooze-files`. |
| `-b`, `--build` | Recompila antes de ejecutar (con `--no-tests`). |
| `-h`, `--help` | Muestra la ayuda. |

Todo lo que va después de `--`, o el primer argumento que no sea una opción, se
entrega a `ooze-files`:

```sh
./run.sh ~/Descargas
./run.sh -d -b -- /tmp
```

Bloques del script:

- **raíz** y **argumentos**: igual que en `build.sh`.
- **compilación**: llama a `build.sh` cuando se pide `--build` o falta el binario.
- **entorno nix**: en NixOS, fuera de un `nix develop`, vuelve a lanzarse dentro
  de la shell del proyecto (con el mismo mecanismo que `build.sh`) para que Qt encuentre sus plugins y módulos QML. Si
  ya hay `QML_IMPORT_PATH`, o no es NixOS, no hace nada.
- **ejecución**: `exec` sobre el binario, de modo que las señales llegan
  directamente a la aplicación.

## `install-desktop.sh`

Instala el lanzador y el icono en `~/.local/share` (o `XDG_DATA_HOME`) para
encontrar Ooze Files desde el launcher. No necesita permisos de administrador.

```
./install-desktop.sh [opciones]
```

| Opción | Efecto |
|---|---|
| `-r`, `--release` | Lanza `build/release` mediante `run.sh`. Es el modo por defecto. |
| `-d`, `--debug` | Lanza `build/debug` mediante `run.sh --debug`. |
| `--binary RUTA` | Lanza ese binario directamente (paquete de nix, `/usr/bin`…). |
| `--reuse` | El lanzador reutiliza la ventana abierta. Sin esta opción, cada clic abre una ventana nueva (`--new-window`). |
| `--set-default` | Fija Ooze Files como gestor de carpetas (`inode/directory`) con `xdg-mime` o `gio`. |
| `-u`, `--uninstall` | Elimina lanzador e icono. |

- En NixOS conviene dejar la opción por defecto: `run.sh` prepara el entorno de
  Qt. `--binary` solo sirve con un binario ya envuelto (por ejemplo el del paquete).
- El `.desktop` declara `SingleMainWindow=false` y la acción «Nueva ventana». Que
  el shell o launcher respete esa clave depende de él.
- Otras aplicaciones (por ejemplo kitty con `kitty-open.desktop`) también declaran
  `inode/directory`. Si al abrir una carpeta se lanza otra aplicación, comprueba
  `gio mime inode/directory` y usa `--set-default`.
- Hay que compilar antes con `build.sh`; si falta el binario el script avisa.
- La ruta queda fija en el `.desktop`: si mueves el proyecto, vuelve a ejecutarlo.
- La línea `Exec` se escribe sin comillas siempre que la ruta no tenga caracteres
  especiales. Es lo que necesita `xdg-open` para resolver el binario; el detalle
  está en [Integración con el escritorio](desktop-integration.md).

## Flujo habitual

```sh
./build.sh -d -t
./run.sh -d
```
