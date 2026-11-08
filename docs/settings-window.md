# Ventana de configuración

Documenta `qml/pages/SettingsWindow.qml`.

Es una ventana propia (`Window` con `flags: Qt.Dialog` y la principal como
`transientParent`), no una capa dentro de la ventana principal. Así un compositor
con mosaico la trata como un diálogo y la deja flotando, que se puede mover y
redimensionar (820×600, mínimo 580×400). Se abre con `Ctrl+,`, desde el menú
principal o desde la paleta; `Esc` o «Cerrar» la cierran.

## Pestañas

| Pestaña | Contenido |
|---|---|
| Apariencia | tema, paleta (con muestras y carpeta de paletas), tipografía |
| Vista | vista, densidad, tamaño de iconos, miniaturas, panel de información |
| Comportamiento | mostrar ocultos, abrir con un clic, terminal |
| Aplicaciones | aplicación predeterminada por tipo de archivo |

Los cambios se aplican al momento y se guardan con el retardo habitual de la
configuración; no hay botones de aceptar o aplicar.

## Aplicaciones predeterminadas

Cada fila es una categoría de tipos MIME:

| Categoría | Tipos |
|---|---|
| Carpetas | `inode/directory` |
| Texto | `text/plain`, `text/markdown` |
| Imágenes | PNG, JPEG, GIF, WebP, SVG, BMP, TIFF, AVIF |
| Vídeo | MP4, Matroska, WebM, QuickTime, AVI, MPEG |
| Audio | MP3, FLAC, Ogg, WAV, MP4, Opus |
| Documentos PDF y libros | `application/pdf`, EPUB |
| Archivos comprimidos | zip, tar, gzip, 7z, rar |
| Navegador web | `text/html` y los esquemas `http` y `https` |

El botón muestra la aplicación actual y abre una lista con las aplicaciones
registradas para esa categoría (la unión de las de todos sus tipos). Al elegir
una se llama a `g_app_info_set_as_default_for_type` para cada tipo de la
categoría, lo que escribe `~/.config/mimeapps.list`. Esa preferencia es la
estándar de FreeDesktop, así que la comparten el resto de aplicaciones y «Abrir
con» la respeta de inmediato.

Una categoría sin aplicaciones registradas aparece deshabilitada como «Sin
aplicaciones». Cuando no hay una predeterminada explícita, GIO considera la
primera registrada y eso es lo que se muestra.

## Hyprland y otros compositores con mosaico

La ventana se declara como diálogo con ventana padre, y Hyprland suele dejarla
flotante. Si en tu configuración aparece en mosaico, fuerza la regla:

```
windowrulev2 = float, class:^(ooze-files)$, title:^(Configuración)$
windowrulev2 = size 820 600, class:^(ooze-files)$, title:^(Configuración)$
windowrulev2 = center, class:^(ooze-files)$, title:^(Configuración)$
```

`ooze-files` es el nombre de archivo de escritorio que fija `main.cpp`.
