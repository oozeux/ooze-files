# Renombrado en lote

`Mayús+F2`, el menú contextual o la paleta de comandos abren
`qml/components/BulkRenameDialog.qml` con la selección actual.

## Modos

| Modo | Campos | Resultado |
| --- | --- | --- |
| Buscar y reemplazar | buscar, reemplazar, expresión regular, distinguir mayúsculas | sustituye en el nombre base |
| Prefijo / sufijo | prefijo, sufijo | envuelve el nombre base |
| Numeración | prefijo, sufijo, desde, incremento, dígitos | `prefijo` + número con ceros + `sufijo` |

La extensión se conserva siempre: `foto.jpg` → `vacaciones.jpg`, no
`vacaciones.jpg.jpg`.

## Vista previa y conflictos

Antes de aplicar se calcula la lista completa de nombres nuevos:

- se marcan en rojo los nombres vacíos, con `/`, repetidos dentro del lote o ya
  existentes en la carpeta y que no forman parte del lote;
- el botón «Renombrar» se deshabilita si no hay cambios o hay conflictos;
- el contador muestra cuántos elementos cambiarán.

## Operación atómica

`FileOperations::renameBatch` ejecuta un único `RenameBatchTask` en el hilo
serializado de operaciones:

1. valida todos los nombres y conflictos antes de tocar el disco;
2. mueve a un nombre temporal los elementos que son destino de otro del lote,
   lo que resuelve cadenas (`a→b, b→c`) y ciclos (`a↔b`);
3. renombra cada temporal (o el original) a su destino;
4. si algo falla, deshace los pasos ya aplicados en orden inverso.

Todo el lote produce una sola entrada de deshacer, así que `Ctrl+Z` revierte el
renombrado completo.
