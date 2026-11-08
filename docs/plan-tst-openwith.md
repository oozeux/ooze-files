# Plan: reparar `tst_openwith`

Estado: **implementado**. No requiere cambios en código de producción; afecta
solo a `tests/unit/tst_openwith.cpp`.

Resultado: `tst_openwith` pasa 8/8 y `ctest` completo pasa 15/15 sin `-E`.

## Problema

`tst_openwith` pasa 6 de 8 casos. Fallan exactamente los dos que comparan
**listas completas** de aplicaciones:

```
FAIL!  : listsRegisteredApplications() Compared values are not the same
   Actual   (apps.size()): 5
   Expected (2)          : 2
FAIL!  : reportsCurrentDefaultAndCandidates() Compared lists have different sizes.
   Actual   (ids) size: 5
   Expected (editor-a.desktop, editor-b.desktop) size: 2
```

El resto (`setDefault*` y `launch*`) pasa. Por eso el proyecto ejecuta las
pruebas con `ctest -E '^openwith$'`, lo que deja fuera una parte del código.

## Causa raíz

El test aísla `XDG_DATA_HOME`, `XDG_DATA_DIRS`, `XDG_CONFIG_HOME` y
`XDG_CONFIG_DIRS` en `initTestCase` y asume que GIO solo verá las dos
aplicaciones de prueba. En NixOS eso es falso: el paquete `glib` de nixpkgs
aplica `glib-appinfo-watch.patch`, que **añade siempre** estas carpetas a
`GDesktopAppInfo`, sin consultar `XDG_DATA_DIRS`:

- `/nix/var/nix/profiles/system/sw/share`
- `/nix/var/nix/profiles/per-user/$USER/profile/share`
- `/etc/profiles/per-user/$USER/share`

El parche existe para que los perfiles de Nix se detecten al instalar o quitar
paquetes; es comportamiento deseado en producción. Con él, el sistema aporta
aplicaciones reales (`vim.desktop`, `nvim.desktop`, `writer.desktop`…) al
listado de `text/plain`, y las aserciones de cantidad fallan.

Comprobado con un binario mínimo enlazado contra la misma GLib: con
`XDG_DATA_DIRS=/tmp/vacio` siguen apareciendo aplicaciones del sistema. No es
un fallo de Ooze Files ni una caché de Qt.

## Por qué no basta con variables de entorno

El parche no lee ninguna variable: las tres rutas están escritas en el código de
GLib. Aislar con `XDG_*` funciona en otras distribuciones, pero no en NixOS, que
es la plataforma principal del proyecto. Tampoco sirve `G_TEST_OPTION_ISOLATE_DIRS`
(solo aísla directorios de usuario).

## Objetivo

- `ctest` completo en verde en NixOS, sin `-E`.
- Mantener el valor del test: verificar que se listan las aplicaciones del
  fixture, que la predeterminada va primero y que `setDefault`/`launch` siguen
  funcionando.
- No introducir código solo para tests en `src/`.

## Opciones

| Opción | Ventaja | Coste / problema |
| --- | --- | --- |
| **A. Aserciones relativas** (recomendada) | Cambio pequeño y local; sigue probando GIO real | Deja de comprobar que *solo* existen las apps del fixture |
| B. Backend inyectable (`AppInfoBackend` + fake) | Aislamiento total y unitario | Refactor de `OpenWith`; el fake no prueba GIO |
| C. `bwrap` ocultando perfiles Nix | Aislamiento real | Dependencia nueva, no portable, test frágil |
| D. Escanear `mimeinfo.cache` en vez de GIO | Control total de las rutas | Duplica lógica de freedesktop; pierde `setDefault`/`launch` |
| E. `QSKIP` si hay apps del sistema | Trivial | El test no se ejecuta en NixOS, justo donde importa |

## Decisión

**Opción A**, con dos ajustes para que el test siga siendo significativo:

1. Comparar **orden relativo** y presencia, no cantidades exactas.
2. Añadir un fixture de audio para que la categoría «Audio» se compruebe por
   contenido en lugar de por «está vacía» (también dependía del sistema).

No se relaja nada de `setDefault` ni de `launch`: esos casos ya son
deterministas porque escriben y leen su propio `mimeapps.list`.

## Cambios concretos

### 1. Fixture de audio en `initTestCase`

Añadir una cuarta aplicación y su caché:

```cpp
app(QStringLiteral("sonido.desktop"), QStringLiteral("Sonido"),
    QStringLiteral("audio/mpeg;"));
```

En `mimeinfo.cache`:

```
audio/mpeg=sonido.desktop;
```

En `mimeapps.list` (junto a `text/plain`):

```
audio/mpeg=sonido.desktop;
```

### 2. `listsRegisteredApplications`

Antes:

```cpp
QCOMPARE(apps.size(), 2);
QCOMPARE(apps.first()..., "editor-a.desktop");
QVERIFY(apps.first()...isDefault);
QCOMPARE(apps.last()...name, "Editor B");
QVERIFY(!apps.last()...isDefault);
```

Después:

```cpp
QVERIFY(apps.size() >= 2);
QCOMPARE(apps.at(0).id, "editor-a.desktop");   // predeterminada primero
QVERIFY(apps.at(0).isDefault);
QCOMPARE(apps.at(1).id, "editor-b.desktop");   // datos de usuario antes que el sistema
QCOMPARE(apps.at(1).name, "Editor B");
QVERIFY(!apps.at(1).isDefault);
```

### 3. `reportsCurrentDefaultAndCandidates`

- Texto: `states` sigue exigiendo `>= 8`; `appId`/`appName` no cambian.
  Los ids pasan de igualdad exacta a presencia y orden relativo:

```cpp
QVERIFY(ids.contains("editor-a.desktop"));
QVERIFY(ids.contains("editor-b.desktop"));
QVERIFY(ids.indexOf("editor-a.desktop") < ids.indexOf("editor-b.desktop"));
```

- Imágenes: en vez de `size() == 1`, comprobar que `visor.desktop` está
  presente y que `appId` sigue siendo `visor.desktop`. No se exige que sea el
  primero: `defaultAppStates()` ordena por nombre y las aplicaciones del
  sistema pueden colocarse antes.
- Audio: en vez de `isEmpty()`, comprobar que `sonido.desktop` está presente y
  que `appId` es `sonido.desktop`, con la misma salvedad sobre el orden.

### 4. Comentario de intención

Dejar junto a las aserciones relativas un comentario corto que explique el
parche de NixOS y por qué no se comparan cantidades. Así nadie «arregla» el test
volviendo a contar aplicaciones.

## Validación

1. Compilar:

   ```sh
   nix-shell nix/devshell.nix --run 'cmake --build --preset linux-release --parallel'
   ```

2. Ejecutar el binario aislado y comprobar 8/8:

   ```sh
   QT_QPA_PLATFORM=offscreen ./build/release/tests/tst_openwith
   ```

3. Ejecutar la suite **sin exclusiones**:

   ```sh
   cd build/release && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
   ```

   Debe dar 15/15.

4. Repetir `ctest -R openwith` dos veces seguidas para descartar dependencia
   del orden o de la caché de GIO.

5. Actualizar la nota de estado en `README.md` si menciona la exclusión del test
   (hoy no la menciona; revisar antes de tocar nada).

## Criterios de aceptación

- `tst_openwith` pasa 8/8 en NixOS con aplicaciones reales instaladas.
- `ctest` completo pasa sin `-E`.
- No se modifica `src/core/OpenWith.cpp` ni su API.
- El test sigue fallando si `appsForMime` deja de poner la predeterminada
  primero, si `setDefault` no escribe `mimeapps.list` o si `launch` no ejecuta
  la aplicación.

## Riesgos

- **Orden de GIO**: se asume que los datos de usuario tienen precedencia sobre
  los perfiles del sistema. Es el orden documentado en
  `desktop_file_dirs_lock()` y el que ya usa la aplicación; el test lo fija.
- **Colisión de ids**: otra distribución podría tener un `editor-a.desktop`
  real. Es improbable y, si ocurriera, el fixture debe renombrarse a algo único
  (`ooze-test-editor-a.desktop`); se puede hacer en el mismo cambio.
- **Cobertura**: se deja de detectar que «se cuelan» aplicaciones del sistema.
  No es un defecto: es el comportamiento correcto en NixOS.

## Fuera de alcance

- Backend inyectable para `OpenWith` (opción B). Si en el futuro se quieren
  pruebas unitarias puras de categorías y ordenación, ese es el camino, pero
  requiere separar `OpenWith` en interfaz + implementación GIO.
- Pruebas de interfaz para el submenú «Abrir con»; hoy se cubren a mano.
