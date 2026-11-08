# Pestañas y sesión

Documenta `src/app/TabController.{h,cpp}`, `src/app/TabListModel.{h,cpp}`,
`qml/panels/TabBar.qml` y la integración en `qml/Main.qml`.

## Idea

Cada pestaña es una carpeta con su propio estado. No es una vista dividida ni
columnas Miller: hay una sola vista de carpeta y varias pestañas que se turnan.

Estado independiente por pestaña:

- historial atrás/adelante;
- selección y fila actual;
- filtro de nombre;
- orden y visibilidad de ocultos (se comparten como preferencia global, pero el
  modelo de cada pestaña se actualiza al activarse);
- mensaje de estado y marca de carga;
- `DirectoryWatcher`.

Los servicios globales siguen en `ApplicationController`: configuración,
portapapeles, pila de deshacer, operaciones de archivo, buscador, D-Bus y
proveedores de imágenes.

## Comportamiento

- La barra solo aparece con dos o más pestañas. `ui.tabBarAlways` la muestra
  también con una sola.
- Cerrar la última pestaña cierra la ventana.
- `Ctrl+W` cierra la pestaña activa; si hay una sola, cierra la ventana.
- Las pestañas cerradas se recuerdan en una pila y `Ctrl+Shift+T` las reabre en
  su posición original.
- El límite es `session.maxTabs` (1–32, por defecto 16). Al alcanzarlo se muestra
  un aviso en la barra de estado.
- Al cambiar de pestaña se cierran menús, vista previa, búsqueda, propiedades y
  renombrado en lote; la vista vuelve al principio y sin fila actual (v1).
- Las pestañas inactivas no refrescan: su watcher las marca sucias y se
  actualizan al volver.

## Atajos

| Atajo | Acción |
| --- | --- |
| `Ctrl+T` | Nueva pestaña en la carpeta actual |
| `Ctrl+W` | Cerrar la pestaña activa |
| `Ctrl+Shift+T` | Reabrir la última pestaña cerrada |
| `Ctrl+Tab` | Pestaña siguiente (circular) |
| `Ctrl+Shift+Tab` | Pestaña anterior (circular) |
| `Alt+1` … `Alt+8` | Activar la pestaña 1–8 |
| `Alt+9` | Última pestaña |
| `Ctrl+Enter` | Abrir la selección en una pestaña nueva |

Los atajos de pestaña se desactivan mientras se edita la ruta, hay un campo de
texto con foco o hay un diálogo/menú abierto. Así `Ctrl+W` no cierra una pestaña
mientras se escribe.

El clic central sobre una carpeta o sobre un elemento abre una pestaña nueva; el
clic central sobre una pestaña la cierra. El menú contextual de pestaña ofrece
nueva, duplicar, cerrar, cerrar las demás, cerrar las de la derecha, reabrir y
copiar la ruta.

## Sesión

La sesión se guarda en `~/.config/ooze-files/config.json`:

```json
"session": {
    "restore": true,
    "maxTabs": 16,
    "tabs": ["file:///home/ana", "file:///home/ana/Descargas"],
    "active": 1
}
```

- Se escribe de forma diferida al abrir, cerrar, activar o navegar, y se fuerza
  al salir.
- `restore: true` restaura las pestañas al arrancar sin ruta explícita.
- Las rutas que ya no existen, no son locales o no son carpetas se descartan sin
  impedir el arranque.
- `active` se recorta al rango válido.
- Los toggles están en Ajustes → Comportamiento → Pestañas. `maxTabs` solo se
  edita en el JSON.

Detalle de implementación: el índice activo se captura antes de restaurar porque
arrancar la primera pestaña ya dispara un guardado de sesión.

## Línea de comandos

```sh
ooze-files -t ~/Descargas
```

- Con una instancia abierta, `-t` añade y activa una pestaña.
- En arranque en frío, `-t` no tiene efecto especial: se abre una sola pestaña.
- Sin `-t`, `ooze-files /ruta` navega la pestaña activa de la instancia existente.

## Implementación

```
ApplicationController
  ├─ DirectoryLoader compartido   1 hilo de listado + 2 de stat
  ├─ TabListModel                 títulos, URLs y activa para QML
  ├─ TabController × N            modelo, proxy, historial, selección, watcher
  └─ pila de pestañas cerradas
```

- `DirectoryLoader::attach()` une el modelo de la pestaña activa; `detach()`
  cancela la generación y lo suelta. Cambiar de pestaña no crea hilos.
- `TabListModel` guarda valores planos, no punteros, para que borrar un
  `TabController` no deje referencias colgantes en QML.
- `App.tabs` es estable y emite `tabsChanged`/`activeTabChanged`; `App.files`
  apunta al modelo activo y emite `filesChanged`.
- `ApplicationController` destruye el `DirectoryLoader` antes que el
  `VfsRegistry` para que las tareas en vuelo terminen con los backends vivos.

Ver [architecture.md](architecture.md) y [performance.md](performance.md).

## Pruebas

`tests/unit/tst_tabs.cpp` cubre:

- pestaña nueva en la carpeta actual;
- independencia de selección e historial;
- cierre con activación de la vecina y cierre de la última;
- duplicado y reapertura en la posición correcta;
- límite de pestañas;
- modelo de la barra (título y activa);
- apertura de un archivo en pestaña nueva con selección;
- persistencia y restauración de sesión;
- omisión de rutas inexistentes y de la restauración con ruta explícita.

`tests/unit/tst_config.cpp` valida las claves `session` y sus límites. La prueba
`smoke-ui` carga `Main.qml` con `TabBar`, de modo que un error de QML se detecta
sin abrir una sesión gráfica.
