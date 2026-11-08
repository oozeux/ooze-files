# Novedades por versión

## 0.1.46

### Características nuevas

- Marca visual del portapapeles en las vistas de cuadrícula y lista: lo copiado
  se resalta con un borde de acento y lo cortado se atenúa.
- `App.clipboardMark(url)` y `Theme.cutOpacity` para consultar y estilar esas
  marcas.

### Correcciones

- Cortar o copiar ya no se pierde al borrar un archivo. Solo el pegado de «cortar»
  vacía el portapapeles; borrar, mover por arrastre o renombrar no lo afectan.

### Pruebas y documentación

- Pruebas de supervivencia del portapapeles y de las marcas de copiado/cortado.
- Documentación y README actualizados.

## Hasta 0.1.45 (acumulado)

### Navegación y vistas

- Listado asíncrono en dos fases (nombres primero, metadatos después) con
  cancelación por generaciones y vigilancia de directorios con `inotify`.
- Vista de cuadrícula y de lista; densidad, tamaño de iconos y miniaturas de
  imagen y vídeo configurables.
- Ordenación por nombre, tamaño, fecha o tipo con carpetas primero; filtro
  instantáneo; archivos ocultos conmutables.
- Migas de pan editables, historial atrás/adelante y subir de carpeta.
- Barra lateral con lugares XDG, lugares propios reordenables y dispositivos
  (montar, desmontar, expulsar vía UDisks2).
- Panel de información (`F11`) y diálogo de propiedades (`Alt+Intro`).

### Pestañas y ventanas

- Pestañas con estado independiente, reapertura, límite configurable, clic
  central y `Ctrl+Enter`.
- Restauración de sesión, instancia única por D-Bus y opciones `--new-tab`,
  `--new-window` y `--select`.

### Operaciones de archivos

- Copiar, mover y renombrar con `FICLONE` cuando es posible y renombrado atómico.
- Diálogo de conflictos con combinar carpetas y «Aplicar a todos».
- Deshacer y rehacer para mover, copiar, renombrar, crear, comprimir y papelera.
- Papelera de freedesktop (ver, restaurar, eliminar, vaciar) y vista de Recientes.
- Renombrado en lote (`Mayús+F2`) con vista previa y detección de conflictos.
- Comprimir (zip, tar.gz, tar.xz, tar.zst, 7z) y extraer con las herramientas del
  sistema.
- Portapapeles interoperable con Nautilus y Dolphin.

### Arrastrar y soltar

- Salida como `text/uri-list`, tira flotante con icono y nombre, y menú al soltar
  para mover, copiar o enlazar.
- Selección por marco con modos reemplazar, añadir y alternar, y desplazamiento
  automático en los bordes.

### Búsqueda, vista previa y comandos

- Búsqueda recursiva con filtros por tipo (`Ctrl+Mayús+F`).
- Vista previa rápida con `Espacio` (imagen, texto, Markdown, vídeo, PDF y
  carpetas) y «Abrir con» basado en GIO.
- Paleta de comandos (`Ctrl+K`) y registro único de acciones compartido con el
  menú principal.

### Apariencia y configuración

- Temas claro, oscuro y automático; paletas JSON externas e importación de
  OozeShell/Matugen, qt6ct y KDE; acento del sistema por el portal.
- Tema de iconos, escala y tipografía configurables; modo ligero.
- Ventana de configuración con pestañas y aplicaciones predeterminadas por tipo.
- Interfaz en español e inglés.
