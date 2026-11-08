# Seguridad

Modelo de amenazas: archivos y nombres controlados por terceros (descargas,
USB, archivos compartidos), contenido de la carpeta actual y procesos de la
misma sesión que puedan hablar por D-Bus. No se asume que los nombres sean
UTF-8 válido, cortos, ni que no contengan saltos de línea o comillas.

## Principios

1. **Nunca se ejecuta un shell.** Las rutas no se interpolan en líneas de
   comandos. El terminal se lanza con `QProcess` y una lista de argumentos; los
   archivos se abren con `QDesktopServices::openUrl`.
2. **Sin privilegios.** La aplicación no pide `sudo`, no se instala setuid y no
   escribe fuera de las rutas que el usuario abre o de su configuración.
3. **Rutas en bytes.** Un nombre no UTF-8 no se "repara" ni se trunca: se
   conserva y se percent-codifica para cruzar a QML. Eso evita que `%00`, `%2F`
   o bytes inválidos se conviertan en rutas distintas de las que el usuario ve.
4. **Sin rutas mágicas.** No hay expansión de `~`, variables ni comodines dentro
   de la aplicación; solo el argumento de la línea de comandos expande `~/`.

## Rutas y URL

`PathUtils::toLocalBytes` decodifica el path de `QUrl::FullyEncoded` y comprueba
que empiece por `/`; `fromLocalBytes` percent-codifica byte a byte todo lo que no
sea `A-Z a-z 0-9 - . _ ~ /`. El resultado es una URL `file:///...` opaca para el
resto de la interfaz.

`PathUtils::normalized` elimina `.` y resuelve `..` **léxicamente**, sin tocar el
disco, así que no puede escapar de la raíz: la pila nunca retrocede por debajo de
`/`.

`isSameOrAncestor` compara por componentes completos (`/home/ana` no es ancestro
de `/home/ana2`), pensado para futuras operaciones de copia y pegado.

## Enlaces simbólicos

- El listado usa `readdir` con `d_type`; si no es fiable, `lstat`, nunca `stat`
  del destino para decidir el tipo.
- Un enlace se muestra como enlace (`folder-symlink`) y se marca
  `FlagSymlinkBroken` si el destino no existe.
- Navegar a un enlace lo resuelve el sistema de archivos al abrir el
  directorio: no hay seguimiento manual que pueda desincronizarse.
- Las futuras operaciones de copia deben usar `openat`/`O_NOFOLLOW` cuando el
  destino esté en una ruta no confiable y comprobar que origen y destino siguen
  dentro de lo esperado antes de renombrar.

## Apertura de archivos

`QDesktopServices::openUrl` delega en el mecanismo del escritorio, que resuelve
el MIME con los archivos `.desktop` instalados. Ooze Files no interpreta el
contenido ni decide qué aplicación ejecutar. Si no hay handler, se informa en la
barra de estado; no se intenta ejecutar el archivo.

El terminal solo se lanza para abrir una **carpeta**, con el directorio como
`workingDirectory` y sin comandos adicionales. La lista de terminales admitidos
es cerrada y cada uno recibe argumentos fijos; no se acepta una línea de
comandos arbitraria desde la configuración: `behavior.terminal` debe ser un
ejecutable que exista en `PATH` o una ruta válida.

## D-Bus

El servicio `org.ooze.Files` vive en el bus de sesión, así que cualquier proceso
del mismo usuario puede invocarlo. Por eso:

- los métodos solo aceptan una URL y una intención de navegación;
- no hay métodos que ejecuten comandos, lean archivos arbitrarios ni cambien la
  configuración;
- una URL con esquema no soportado se rechaza en el registro de backends y se
  muestra como error, sin abrir nada;
- `NewWindow`/`NewTab` no crean procesos nuevos desde el bus.

No se usa D-Bus del sistema ni se solicita ningún nombre privilegiado.

## Configuración

`config.json` solo contiene preferencias y la última ruta; no guarda credenciales
ni historial de navegación. Se escribe con `QSaveFile` (archivo temporal +
`rename` atómico) y se lee con validación de tipos y valores cerrados, de modo
que un archivo manipulado no puede inyectar rutas ni comandos.

Un JSON inválido no se sobrescribe: se conservan los valores por defecto y se
avisa. Así un error de edición no destruye la configuración del usuario.

## Temas

Los temas externos son **datos**, no código: `themes/<id>.json` se lee con
`QJsonDocument`, se valida el `schema`, los colores y los rangos numéricos, y se
ignoran las claves desconocidas. El id de la paleta se limita a letras, números,
punto, guion y guion bajo, así que no puede salir de la carpeta de temas ni
apuntar a otra ruta. Un tema no puede definir opacidades de selección ni
propiedades de la interfaz: solo colores y tres métricas.

Un archivo inválido se descarta y se vuelve al tema incorporado con un aviso en
el registro y la barra de estado; nunca deja la ventana sin colores.

Las fuentes `system:ooze`, `system:qt6ct` y `system:kde` son **solo lectura**:
se leen de rutas conocidas (caché de OozeShell, config de qt6ct y de KDE) y
nunca se escriben. Los nombres de esquema se validan antes de construir la ruta,
de modo que `ui.palette` no puede convertirse en un lector de archivos
arbitrarios. Son formatos del escritorio, así que un esquema de terceros puede
traer colores de bajo contraste; se acepta y se avisa igual que con un tema
propio.

El portal (`org.freedesktop.portal.Settings`) solo se consulta para leer
`accent-color` y `color-scheme`; nunca se escriben ajustes del escritorio. Si no
está disponible, la aplicación sigue con el tema y el esquema de Qt.

## Registro

El registro detallado (`--debug`) puede incluir rutas. No se registran contenidos
de archivos ni variables de entorno. En producción el nivel por defecto solo
muestra avisos y errores.

## Límites conocidos

- No hay sandbox: la aplicación puede leer todo lo que el usuario puede leer.
- La papelera y las copias (fases futuras) deben validar cruces de sistemas de
  archivos: `rename` solo es atómico dentro del mismo montaje.
- La vista previa y las miniaturas (fase 5) serán la primera superficie que
  interpreta contenido de terceros; deberán ejecutarse fuera del hilo principal
  y con límites de tamaño y tiempo.
- El bus de sesión no autentica más allá del usuario: cualquier proceso suyo
  puede pedir navegar. Es el mismo nivel de confianza que abrir el gestor a mano.

## Reportar problemas

Los fallos de seguridad deben reportarse en privado al mantenedor antes de
abrir un issue público. Incluir versión, distribución, sistema de archivos y una
descripción reproducible sin datos personales.
