#include "app/ActionRegistry.h"

#include "app/ApplicationController.h"
#include "app/I18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QVector>

#include <functional>

namespace {

struct ActionDefinition
{
    QString id;
    QString label;
    QString hint;
    QString category;
    QString keywords;
    std::function<bool(const ApplicationController &)> enabled;
    std::function<bool(const ApplicationController &)> checked;
    std::function<void(ApplicationController &)> run;
    bool ui = false;
    std::function<QString(const ApplicationController &)> dynamicLabel;
};

QVector<ActionDefinition> buildDefinitions()
{
    QVector<ActionDefinition> table = {
        {QStringLiteral("window.new"), QCoreApplication::translate("ActionRegistry", "Nueva ventana"), QStringLiteral("Ctrl+N"),
         QCoreApplication::translate("ActionRegistry", "Aplicación"), QCoreApplication::translate("ActionRegistry", "abrir ventana independiente"),
         {}, {},
         [](ApplicationController &controller) { controller.openNewWindow(); }, false},

        {QStringLiteral("tab.new"), QCoreApplication::translate("ActionRegistry", "Nueva pestaña"), QStringLiteral("Ctrl+T"),
         QCoreApplication::translate("ActionRegistry", "Pestañas"), QCoreApplication::translate("ActionRegistry", "abrir carpeta pestaña"),
         {}, {},
         [](ApplicationController &controller) { controller.newTab(); }, false},

        {QStringLiteral("tab.duplicate"), QCoreApplication::translate("ActionRegistry", "Duplicar pestaña"), {}, QCoreApplication::translate("ActionRegistry", "Pestañas"),
         QCoreApplication::translate("ActionRegistry", "copiar clonar pestaña"),
         [](const ApplicationController &controller) { return controller.tabCount() > 0; }, {},
         [](ApplicationController &controller) {
             controller.duplicateTab(controller.activeTab());
         }, false},

        {QStringLiteral("tab.close"), QCoreApplication::translate("ActionRegistry", "Cerrar pestaña"), QStringLiteral("Ctrl+W"),
         QCoreApplication::translate("ActionRegistry", "Pestañas"), QCoreApplication::translate("ActionRegistry", "cerrar pestaña"),
         [](const ApplicationController &controller) { return controller.tabCount() > 0; }, {},
         [](ApplicationController &controller) {
             controller.closeTab(controller.activeTab());
         }, false},

        {QStringLiteral("tab.closeOthers"), QCoreApplication::translate("ActionRegistry", "Cerrar las demás pestañas"), {}, QCoreApplication::translate("ActionRegistry", "Pestañas"),
         QCoreApplication::translate("ActionRegistry", "cerrar otras pestañas"),
         [](const ApplicationController &controller) { return controller.tabCount() > 1; }, {},
         [](ApplicationController &controller) {
             controller.closeOtherTabs(controller.activeTab());
         }, false},

        {QStringLiteral("tab.closeRight"), QCoreApplication::translate("ActionRegistry", "Cerrar las pestañas de la derecha"), {},
         QCoreApplication::translate("ActionRegistry", "Pestañas"), QCoreApplication::translate("ActionRegistry", "cerrar derecha pestañas"),
         [](const ApplicationController &controller) {
             return controller.activeTab() >= 0
                 && controller.activeTab() < controller.tabCount() - 1;
         }, {},
         [](ApplicationController &controller) {
             controller.closeTabsToRight(controller.activeTab());
         }, false},

        {QStringLiteral("tab.reopen"), QCoreApplication::translate("ActionRegistry", "Reabrir pestaña cerrada"),
         QStringLiteral("Ctrl+Shift+T"), QCoreApplication::translate("ActionRegistry", "Pestañas"),
         QCoreApplication::translate("ActionRegistry", "restaurar recuperar pestaña"),
         [](const ApplicationController &controller) { return controller.canReopenTab(); }, {},
         [](ApplicationController &controller) { controller.reopenClosedTab(); }, false},

        {QStringLiteral("tab.next"), QCoreApplication::translate("ActionRegistry", "Pestaña siguiente"), QStringLiteral("Ctrl+Tab"),
         QCoreApplication::translate("ActionRegistry", "Pestañas"), QCoreApplication::translate("ActionRegistry", "avanzar siguiente"),
         [](const ApplicationController &controller) { return controller.tabCount() > 1; }, {},
         [](ApplicationController &controller) { controller.nextTab(); }, false},

        {QStringLiteral("tab.previous"), QCoreApplication::translate("ActionRegistry", "Pestaña anterior"),
         QStringLiteral("Ctrl+Shift+Tab"), QCoreApplication::translate("ActionRegistry", "Pestañas"),
         QCoreApplication::translate("ActionRegistry", "retroceder anterior"),
         [](const ApplicationController &controller) { return controller.tabCount() > 1; }, {},
         [](ApplicationController &controller) { controller.previousTab(); }, false},

        {QStringLiteral("tab.copyPath"), QCoreApplication::translate("ActionRegistry", "Copiar ruta de la pestaña"), {}, QCoreApplication::translate("ActionRegistry", "Pestañas"),
         QCoreApplication::translate("ActionRegistry", "portapapeles ubicación pestaña"),
         [](const ApplicationController &controller) { return controller.tabCount() > 0; }, {},
         [](ApplicationController &controller) {
             controller.copyTabPath(controller.activeTab());
         }, false},

        {QStringLiteral("nav.back"), QCoreApplication::translate("ActionRegistry", "Atrás"), QStringLiteral("Alt+Left"),
         QCoreApplication::translate("ActionRegistry", "Navegación"), QCoreApplication::translate("ActionRegistry", "volver anterior historial"),
         [](const ApplicationController &controller) { return controller.canGoBack(); }, {},
         [](ApplicationController &controller) { controller.goBack(); }, false},

        {QStringLiteral("nav.forward"), QCoreApplication::translate("ActionRegistry", "Adelante"), QStringLiteral("Alt+Right"),
         QCoreApplication::translate("ActionRegistry", "Navegación"), QCoreApplication::translate("ActionRegistry", "siguiente historial"),
         [](const ApplicationController &controller) { return controller.canGoForward(); }, {},
         [](ApplicationController &controller) { controller.goForward(); }, false},

        {QStringLiteral("nav.up"), QCoreApplication::translate("ActionRegistry", "Subir un nivel"), QStringLiteral("Alt+Up"),
         QCoreApplication::translate("ActionRegistry", "Navegación"), QCoreApplication::translate("ActionRegistry", "padre carpeta"), {}, {},
         [](ApplicationController &controller) { controller.goUp(); }, false},

        {QStringLiteral("nav.home"), QCoreApplication::translate("ActionRegistry", "Ir a la carpeta personal"), {}, QCoreApplication::translate("ActionRegistry", "Navegación"),
         QCoreApplication::translate("ActionRegistry", "inicio home"), {}, {},
         [](ApplicationController &controller) {
             controller.goToUrl(QUrl::fromLocalFile(QDir::homePath()));
         }, false},

        {QStringLiteral("nav.reload"), QCoreApplication::translate("ActionRegistry", "Recargar"), QStringLiteral("F5"), QCoreApplication::translate("ActionRegistry", "Navegación"),
         QCoreApplication::translate("ActionRegistry", "actualizar refrescar"), {}, {},
         [](ApplicationController &controller) { controller.reload(); }, false},

        {QStringLiteral("nav.path"), QCoreApplication::translate("ActionRegistry", "Editar la ruta"), QStringLiteral("Ctrl+L"),
         QCoreApplication::translate("ActionRegistry", "Navegación"), QCoreApplication::translate("ActionRegistry", "ubicación barra"), {}, {}, {}, true},

        {QStringLiteral("nav.search"), QCoreApplication::translate("ActionRegistry", "Filtrar en esta carpeta"), QStringLiteral("Ctrl+F"),
         QCoreApplication::translate("ActionRegistry", "Navegación"), QCoreApplication::translate("ActionRegistry", "buscar filtro"), {}, {}, {}, true},

        {QStringLiteral("palette.open"), QCoreApplication::translate("ActionRegistry", "Paleta de comandos"), QStringLiteral("Ctrl+K"),
         QCoreApplication::translate("ActionRegistry", "Aplicación"), QCoreApplication::translate("ActionRegistry", "comandos buscar acciones"), {}, {}, {}, true},

        {QStringLiteral("view.settings"), QCoreApplication::translate("ActionRegistry", "Configuración…"), QStringLiteral("Ctrl+,"),
         QCoreApplication::translate("ActionRegistry", "Aplicación"), QCoreApplication::translate("ActionRegistry", "ajustes preferencias opciones"), {}, {}, {}, true},

        {QStringLiteral("tools.themes"), QCoreApplication::translate("ActionRegistry", "Abrir la carpeta de paletas"), {},
         QCoreApplication::translate("ActionRegistry", "Aplicación"), QCoreApplication::translate("ActionRegistry", "temas colores carpeta"), {}, {},
         [](ApplicationController &controller) { controller.openThemesFolder(); }, false},

        {QStringLiteral("app.quit"), QCoreApplication::translate("ActionRegistry", "Salir"), QStringLiteral("Ctrl+Q"), QCoreApplication::translate("ActionRegistry", "Aplicación"),
         QCoreApplication::translate("ActionRegistry", "cerrar terminar"), {}, {},
         [](ApplicationController &) { QCoreApplication::quit(); }, false},

        {QStringLiteral("theme.light"), QCoreApplication::translate("ActionRegistry", "Tema claro"), {}, QCoreApplication::translate("ActionRegistry", "Tema"),
         QCoreApplication::translate("ActionRegistry", "colores apariencia"), {},
         [](const ApplicationController &controller) {
             return controller.themeMode() == QLatin1String("light");
         },
         [](ApplicationController &controller) {
             controller.setThemeMode(QStringLiteral("light"));
         }, false},

        {QStringLiteral("theme.dark"), QCoreApplication::translate("ActionRegistry", "Tema oscuro"), {}, QCoreApplication::translate("ActionRegistry", "Tema"),
         QCoreApplication::translate("ActionRegistry", "colores apariencia"), {},
         [](const ApplicationController &controller) {
             return controller.themeMode() == QLatin1String("dark");
         },
         [](ApplicationController &controller) {
             controller.setThemeMode(QStringLiteral("dark"));
         }, false},

        {QStringLiteral("theme.system"), QCoreApplication::translate("ActionRegistry", "Seguir el tema del sistema"), {}, QCoreApplication::translate("ActionRegistry", "Tema"),
         QCoreApplication::translate("ActionRegistry", "colores apariencia automático"), {},
         [](const ApplicationController &controller) {
             return controller.themeMode() == QLatin1String("system");
         },
         [](ApplicationController &controller) {
             controller.setThemeMode(QStringLiteral("system"));
         }, false},

        {QStringLiteral("view.grid"), QCoreApplication::translate("ActionRegistry", "Vista de cuadrícula"), {}, QCoreApplication::translate("ActionRegistry", "Vista"),
         QCoreApplication::translate("ActionRegistry", "iconos mosaico"), {},
         [](const ApplicationController &controller) {
             return controller.viewMode() == QLatin1String("grid");
         },
         [](ApplicationController &controller) {
             controller.setViewMode(QStringLiteral("grid"));
         }, false},

        {QStringLiteral("view.list"), QCoreApplication::translate("ActionRegistry", "Vista de lista"), {}, QCoreApplication::translate("ActionRegistry", "Vista"),
         QCoreApplication::translate("ActionRegistry", "detalle filas"), {},
         [](const ApplicationController &controller) {
             return controller.viewMode() == QLatin1String("list");
         },
         [](ApplicationController &controller) {
             controller.setViewMode(QStringLiteral("list"));
         }, false},

        {QStringLiteral("view.auto"), QCoreApplication::translate("ActionRegistry", "Vista automática"), {}, QCoreApplication::translate("ActionRegistry", "Vista"),
         QCoreApplication::translate("ActionRegistry", "automático"), {},
         [](const ApplicationController &controller) {
             return controller.viewMode() == QLatin1String("auto");
         },
         [](ApplicationController &controller) {
             controller.setViewMode(QStringLiteral("auto"));
         }, false},

        {QStringLiteral("view.hidden"), QCoreApplication::translate("ActionRegistry", "Mostrar archivos ocultos"), QStringLiteral("Ctrl+H"),
         QCoreApplication::translate("ActionRegistry", "Vista"), QCoreApplication::translate("ActionRegistry", "puntos ocultos"), {},
         [](const ApplicationController &controller) { return controller.showHidden(); },
         [](ApplicationController &controller) {
             controller.setShowHidden(!controller.showHidden());
         }, false},

        {QStringLiteral("view.singleClick"), QCoreApplication::translate("ActionRegistry", "Abrir con un solo clic"), {}, QCoreApplication::translate("ActionRegistry", "Vista"),
         QCoreApplication::translate("ActionRegistry", "ratón clic"), {},
         [](const ApplicationController &controller) { return controller.singleClickOpen(); },
         [](ApplicationController &controller) {
             controller.setSingleClickOpen(!controller.singleClickOpen());
         }, false},

        {QStringLiteral("view.thumbnails"), QCoreApplication::translate("ActionRegistry", "Mostrar miniaturas"), {}, QCoreApplication::translate("ActionRegistry", "Vista"),
         QCoreApplication::translate("ActionRegistry", "previsualización imágenes fotos"), {},
         [](const ApplicationController &controller) { return controller.thumbnails(); },
         [](ApplicationController &controller) {
             controller.setThumbnails(!controller.thumbnails());
         }, false},

        {QStringLiteral("view.infoPanel"), QCoreApplication::translate("ActionRegistry", "Panel de información"), QStringLiteral("F11"),
         QCoreApplication::translate("ActionRegistry", "Vista"), QCoreApplication::translate("ActionRegistry", "propiedades detalles vista previa lateral"), {},
         [](const ApplicationController &controller) { return controller.infoPanel(); },
         [](ApplicationController &controller) {
             controller.setInfoPanel(!controller.infoPanel());
         }, false},

        {QStringLiteral("sort.name"), QCoreApplication::translate("ActionRegistry", "Ordenar por nombre"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QCoreApplication::translate("ActionRegistry", "alfabético"), {},
         [](const ApplicationController &controller) { return controller.sortMode() == 0; },
         [](ApplicationController &controller) { controller.setSortMode(0); }, false},

        {QStringLiteral("sort.size"), QCoreApplication::translate("ActionRegistry", "Ordenar por tamaño"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QCoreApplication::translate("ActionRegistry", "peso"), {},
         [](const ApplicationController &controller) { return controller.sortMode() == 1; },
         [](ApplicationController &controller) { controller.setSortMode(1); }, false},

        {QStringLiteral("sort.modified"), QCoreApplication::translate("ActionRegistry", "Ordenar por fecha"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QCoreApplication::translate("ActionRegistry", "modificado reciente"), {},
         [](const ApplicationController &controller) { return controller.sortMode() == 2; },
         [](ApplicationController &controller) { controller.setSortMode(2); }, false},

        {QStringLiteral("sort.type"), QCoreApplication::translate("ActionRegistry", "Ordenar por tipo"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QCoreApplication::translate("ActionRegistry", "extensión"), {},
         [](const ApplicationController &controller) { return controller.sortMode() == 3; },
         [](ApplicationController &controller) { controller.setSortMode(3); }, false},

        {QStringLiteral("sort.ascending"), QCoreApplication::translate("ActionRegistry", "Orden ascendente"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QStringLiteral("a-z"), {},
         [](const ApplicationController &controller) { return !controller.sortDescending(); },
         [](ApplicationController &controller) { controller.setSortDescending(false); }, false},

        {QStringLiteral("sort.descending"), QCoreApplication::translate("ActionRegistry", "Orden descendente"), {}, QCoreApplication::translate("ActionRegistry", "Orden"),
         QStringLiteral("z-a"), {},
         [](const ApplicationController &controller) { return controller.sortDescending(); },
         [](ApplicationController &controller) { controller.setSortDescending(true); }, false},

        {QStringLiteral("icons.small"), QCoreApplication::translate("ActionRegistry", "Iconos pequeños"), {}, QCoreApplication::translate("ActionRegistry", "Iconos"),
         QCoreApplication::translate("ActionRegistry", "tamaño zoom reducir"), {},
         [](const ApplicationController &controller) {
             return controller.iconSize() == QLatin1String("small");
         },
         [](ApplicationController &controller) {
             controller.setIconSize(QStringLiteral("small"));
         }, false},

        {QStringLiteral("icons.normal"), QCoreApplication::translate("ActionRegistry", "Iconos normales"), {}, QCoreApplication::translate("ActionRegistry", "Iconos"),
         QCoreApplication::translate("ActionRegistry", "tamaño zoom estándar"), {},
         [](const ApplicationController &controller) {
             return controller.iconSize() == QLatin1String("normal");
         },
         [](ApplicationController &controller) {
             controller.setIconSize(QStringLiteral("normal"));
         }, false},

        {QStringLiteral("icons.large"), QCoreApplication::translate("ActionRegistry", "Iconos grandes"), {}, QCoreApplication::translate("ActionRegistry", "Iconos"),
         QCoreApplication::translate("ActionRegistry", "tamaño zoom ampliar"), {},
         [](const ApplicationController &controller) {
             return controller.iconSize() == QLatin1String("large");
         },
         [](ApplicationController &controller) {
             controller.setIconSize(QStringLiteral("large"));
         }, false},

        {QStringLiteral("icons.huge"), QCoreApplication::translate("ActionRegistry", "Iconos muy grandes"), {}, QCoreApplication::translate("ActionRegistry", "Iconos"),
         QCoreApplication::translate("ActionRegistry", "tamaño zoom máximo"), {},
         [](const ApplicationController &controller) {
             return controller.iconSize() == QLatin1String("huge");
         },
         [](ApplicationController &controller) {
             controller.setIconSize(QStringLiteral("huge"));
         }, false},

        {QStringLiteral("density.compact"), QCoreApplication::translate("ActionRegistry", "Densidad compacta"), {}, QCoreApplication::translate("ActionRegistry", "Densidad"),
         QCoreApplication::translate("ActionRegistry", "filas juntas"), {},
         [](const ApplicationController &controller) {
             return controller.density() == QLatin1String("compact");
         },
         [](ApplicationController &controller) {
             controller.setDensity(QStringLiteral("compact"));
         }, false},

        {QStringLiteral("density.normal"), QCoreApplication::translate("ActionRegistry", "Densidad normal"), {}, QCoreApplication::translate("ActionRegistry", "Densidad"),
         QCoreApplication::translate("ActionRegistry", "equilibrada"), {},
         [](const ApplicationController &controller) {
             return controller.density() == QLatin1String("normal");
         },
         [](ApplicationController &controller) {
             controller.setDensity(QStringLiteral("normal"));
         }, false},

        {QStringLiteral("density.comfortable"), QCoreApplication::translate("ActionRegistry", "Densidad cómoda"), {}, QCoreApplication::translate("ActionRegistry", "Densidad"),
         QCoreApplication::translate("ActionRegistry", "amplia"), {},
         [](const ApplicationController &controller) {
             return controller.density() == QLatin1String("comfortable");
         },
         [](ApplicationController &controller) {
             controller.setDensity(QStringLiteral("comfortable"));
         }, false},

        {QStringLiteral("selection.open"), QCoreApplication::translate("ActionRegistry", "Abrir el elemento actual"), QStringLiteral("Return"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "abrir ejecutar"),
         [](const ApplicationController &controller) { return controller.itemCount() > 0; },
         {}, {}, true},

        {QStringLiteral("selection.preview"), QCoreApplication::translate("ActionRegistry", "Vista previa y propiedades"),
         QStringLiteral("Space"), QCoreApplication::translate("ActionRegistry", "Selección"),
         QCoreApplication::translate("ActionRegistry", "previsualizar propiedades información detalles"),
         [](const ApplicationController &controller) { return controller.itemCount() > 0; },
         {}, {}, true},

        {QStringLiteral("selection.bulkRename"), QCoreApplication::translate("ActionRegistry", "Renombrar en lote…"),
         QStringLiteral("Shift+F2"), QCoreApplication::translate("ActionRegistry", "Selección"),
         QCoreApplication::translate("ActionRegistry", "masivo lote varios prefijo sufijo numeración reemplazar"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0 && !controller.inVirtualLocation();
         }, {}, {}, true},

        {QStringLiteral("nav.searchRecursive"), QCoreApplication::translate("ActionRegistry", "Buscar en subcarpetas…"),
         QStringLiteral("Ctrl+Shift+F"), QCoreApplication::translate("ActionRegistry", "Navegación"),
         QCoreApplication::translate("ActionRegistry", "recursiva filtro nombre tipo"),
         [](const ApplicationController &controller) {
             return !controller.currentPath().isEmpty();
         }, {}, {}, true},

        {QStringLiteral("selection.openTerminal"), QCoreApplication::translate("ActionRegistry", "Abrir en la terminal"), {},
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "consola carpeta"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() == 1 && controller.selectedIsDirectory();
         }, {}, {}, true},

        {QStringLiteral("selection.copy"), QCoreApplication::translate("ActionRegistry", "Copiar"), QStringLiteral("Ctrl+C"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "portapapeles"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0;
         },
         {},
         [](ApplicationController &controller) { controller.copySelection(); }, false},

        {QStringLiteral("selection.cut"), QCoreApplication::translate("ActionRegistry", "Cortar"), QStringLiteral("Ctrl+X"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "portapapeles mover"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0 && !controller.inVirtualLocation();
         },
         {},
         [](ApplicationController &controller) { controller.cutSelection(); }, false},

        {QStringLiteral("edit.undo"), QCoreApplication::translate("ActionRegistry", "Deshacer"), QStringLiteral("Ctrl+Z"), QCoreApplication::translate("ActionRegistry", "Edición"),
         QCoreApplication::translate("ActionRegistry", "deshacer revertir"),
         [](const ApplicationController &controller) { return controller.canUndo(); }, {},
         [](ApplicationController &controller) { controller.undo(); }, false,
         [](const ApplicationController &controller) {
             return controller.undoText().isEmpty()
                 ? QCoreApplication::translate("ActionRegistry", "Deshacer")
                 : QCoreApplication::translate("ActionRegistry", "Deshacer: %1").arg(controller.undoText());
         }},

        {QStringLiteral("edit.redo"), QCoreApplication::translate("ActionRegistry", "Rehacer"), QStringLiteral("Ctrl+Shift+Z"), QCoreApplication::translate("ActionRegistry", "Edición"),
         QCoreApplication::translate("ActionRegistry", "rehacer repetir"),
         [](const ApplicationController &controller) { return controller.canRedo(); }, {},
         [](ApplicationController &controller) { controller.redo(); }, false,
         [](const ApplicationController &controller) {
             return controller.redoText().isEmpty()
                 ? QCoreApplication::translate("ActionRegistry", "Rehacer")
                 : QCoreApplication::translate("ActionRegistry", "Rehacer: %1").arg(controller.redoText());
         }},

        {QStringLiteral("selection.paste"), QCoreApplication::translate("ActionRegistry", "Pegar"), QStringLiteral("Ctrl+V"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "portapapeles"),
         [](const ApplicationController &controller) { return controller.canPaste(); },
         {},
         [](ApplicationController &controller) { controller.paste(); }, false},

        {QStringLiteral("selection.rename"), QCoreApplication::translate("ActionRegistry", "Renombrar"), QStringLiteral("F2"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "nombre"),
         [](const ApplicationController &controller) {
             return controller.canRenameSelection();
         }, {}, {}, true},

        {QStringLiteral("selection.trash"), QCoreApplication::translate("ActionRegistry", "Mover a la papelera"), QCoreApplication::translate("ActionRegistry", "Supr"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "eliminar borrar"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0 && (!controller.inVirtualLocation() || controller.inTrash());
         },
         {},
         [](ApplicationController &controller) { controller.trashSelection(); }, false},

        {QStringLiteral("selection.delete"), QCoreApplication::translate("ActionRegistry", "Eliminar permanentemente"),
         QCoreApplication::translate("ActionRegistry", "Shift+Supr"), QCoreApplication::translate("ActionRegistry", "Selección"),
         QCoreApplication::translate("ActionRegistry", "borrar definitivo"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0 && (!controller.inVirtualLocation() || controller.inTrash());
         },
         {},
         [](ApplicationController &controller) { controller.deleteSelection(); }, false},

        {QStringLiteral("trash.restore"), QCoreApplication::translate("ActionRegistry", "Restaurar de la papelera"), {},
         QCoreApplication::translate("ActionRegistry", "Papelera"), QCoreApplication::translate("ActionRegistry", "recuperar deshacer"),
         [](const ApplicationController &controller) {
             return controller.inTrash() && controller.selectionCount() > 0;
         },
         {},
         [](ApplicationController &controller) { controller.restoreSelection(); }, false},

        {QStringLiteral("trash.empty"), QCoreApplication::translate("ActionRegistry", "Vaciar la papelera"), {},
         QCoreApplication::translate("ActionRegistry", "Papelera"), QCoreApplication::translate("ActionRegistry", "borrar eliminar todo"),
         [](const ApplicationController &controller) {
             return controller.inTrash() && controller.itemCount() > 0;
         },
         {},
         [](ApplicationController &controller) { controller.emptyTrash(); }, false},

        {QStringLiteral("selection.copyPath"), QCoreApplication::translate("ActionRegistry", "Copiar ruta"), {}, QCoreApplication::translate("ActionRegistry", "Selección"),
         QCoreApplication::translate("ActionRegistry", "portapapeles ubicación"),
         [](const ApplicationController &controller) {
             return controller.itemCount() > 0;
         }, {}, {}, true},

        {QStringLiteral("select.all"), QCoreApplication::translate("ActionRegistry", "Seleccionar todo"), QStringLiteral("Ctrl+A"),
         QCoreApplication::translate("ActionRegistry", "Selección"), QCoreApplication::translate("ActionRegistry", "marcar todos"),
         [](const ApplicationController &controller) { return controller.itemCount() > 0; },
         {},
         [](ApplicationController &controller) { controller.selectAll(); }, false},

        {QStringLiteral("select.none"), QCoreApplication::translate("ActionRegistry", "Deseleccionar todo"), {}, QCoreApplication::translate("ActionRegistry", "Selección"),
         QCoreApplication::translate("ActionRegistry", "quitar limpiar"),
         [](const ApplicationController &controller) {
             return controller.selectionCount() > 0;
         },
         {},
         [](ApplicationController &controller) { controller.clearSelection(); }, false},

        {QStringLiteral("file.newFolder"), QCoreApplication::translate("ActionRegistry", "Nueva carpeta"), QStringLiteral("Ctrl+Shift+N"),
         QCoreApplication::translate("ActionRegistry", "Crear"), QCoreApplication::translate("ActionRegistry", "directorio"), [](const ApplicationController &controller) { return !controller.inVirtualLocation(); }, {}, {}, true},

        {QStringLiteral("file.newFile"), QCoreApplication::translate("ActionRegistry", "Nuevo archivo"), {}, QCoreApplication::translate("ActionRegistry", "Crear"),
         QCoreApplication::translate("ActionRegistry", "documento"), [](const ApplicationController &controller) { return !controller.inVirtualLocation(); }, {}, {}, true},

        {QStringLiteral("tools.terminal"), QCoreApplication::translate("ActionRegistry", "Abrir terminal aquí"), QStringLiteral("Shift+F4"), QCoreApplication::translate("ActionRegistry", "Herramientas"),
         QCoreApplication::translate("ActionRegistry", "consola comandos"), {}, {},
         [](ApplicationController &controller) {
             controller.openCurrentInTerminal();
         }, false},
    };

    return table;
}

const QVector<ActionDefinition> &definitions()
{
    static QVector<ActionDefinition> table;
    static int builtFor = -1;
    if (builtFor != I18n::generation()) {
        table = buildDefinitions();
        builtFor = I18n::generation();
    }
    return table;
}

struct MenuNode
{
    QString actionId;
    QString label;
    bool separator = false;
    QVector<MenuNode> children;
};

MenuNode item(const char *actionId, const QString &label = {})
{
    MenuNode node;
    node.actionId = QString::fromLatin1(actionId);
    node.label = label;
    return node;
}

MenuNode group(const QString &label, const QVector<MenuNode> &children)
{
    MenuNode node;
    node.label = label;
    node.children = children;
    return node;
}

MenuNode divider()
{
    MenuNode node;
    node.separator = true;
    return node;
}

QVector<MenuNode> menuLayout()
{
    const QVector<MenuNode> layout = {
        item("window.new"),
        item("tools.terminal"),
        divider(),
        item("edit.undo"),
        item("edit.redo"),
        divider(),
        group(QCoreApplication::translate("ActionRegistry", "Pestañas"), {
            item("tab.new"),
            item("tab.duplicate"),
            divider(),
            item("tab.close"),
            item("tab.closeOthers"),
            item("tab.closeRight"),
            divider(),
            item("tab.reopen"),
        }),
        divider(),
        group(QCoreApplication::translate("ActionRegistry", "Vista"), {
            item("view.auto", QCoreApplication::translate("ActionRegistry", "Automática")),
            item("view.grid", QCoreApplication::translate("ActionRegistry", "Cuadrícula")),
            item("view.list", QCoreApplication::translate("ActionRegistry", "Lista")),
        }),
        group(QCoreApplication::translate("ActionRegistry", "Tamaño de iconos"), {
            item("icons.small", QCoreApplication::translate("ActionRegistry", "Pequeños")),
            item("icons.normal", QCoreApplication::translate("ActionRegistry", "Normales")),
            item("icons.large", QCoreApplication::translate("ActionRegistry", "Grandes")),
            item("icons.huge", QCoreApplication::translate("ActionRegistry", "Muy grandes")),
        }),
        group(QCoreApplication::translate("ActionRegistry", "Densidad"), {
            item("density.compact", QCoreApplication::translate("ActionRegistry", "Compacta")),
            item("density.normal", QCoreApplication::translate("ActionRegistry", "Normal")),
            item("density.comfortable", QCoreApplication::translate("ActionRegistry", "Cómoda")),
        }),
        group(QCoreApplication::translate("ActionRegistry", "Ordenar por"), {
            item("sort.name", QCoreApplication::translate("ActionRegistry", "Nombre")),
            item("sort.size", QCoreApplication::translate("ActionRegistry", "Tamaño")),
            item("sort.modified", QCoreApplication::translate("ActionRegistry", "Modificado")),
            item("sort.type", QCoreApplication::translate("ActionRegistry", "Tipo")),
            divider(),
            item("sort.ascending", QCoreApplication::translate("ActionRegistry", "Ascendente")),
            item("sort.descending", QCoreApplication::translate("ActionRegistry", "Descendente")),
        }),
        group(QCoreApplication::translate("ActionRegistry", "Tema"), {
            item("theme.light", QCoreApplication::translate("ActionRegistry", "Claro")),
            item("theme.dark", QCoreApplication::translate("ActionRegistry", "Oscuro")),
            item("theme.system", QCoreApplication::translate("ActionRegistry", "Seguir el sistema")),
        }),
        divider(),
        item("view.hidden"),
        item("view.singleClick"),
        item("view.thumbnails"),
        item("view.infoPanel"),
        divider(),
        group(QCoreApplication::translate("ActionRegistry", "Nuevo"), {
            item("file.newFolder", QCoreApplication::translate("ActionRegistry", "Carpeta")),
            item("file.newFile", QCoreApplication::translate("ActionRegistry", "Archivo")),
        }),
        item("selection.paste"),
        item("select.all"),
        group(QCoreApplication::translate("ActionRegistry", "Herramientas"), {
            item("nav.searchRecursive", QCoreApplication::translate("ActionRegistry", "Buscar en subcarpetas…")),
            item("selection.bulkRename", QCoreApplication::translate("ActionRegistry", "Renombrar en lote…")),
            item("selection.copyPath"),
            item("nav.reload"),
            item("palette.open"),
            item("tools.themes"),
        }),
        divider(),
        item("view.settings"),
    };

    return layout;
}

}

ActionRegistry::ActionRegistry(ApplicationController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    const auto refresh = [this] { rebuild(); };

    connect(controller, &ApplicationController::selectionChanged, this, refresh);
    connect(controller, &ApplicationController::clipboardChanged, this, refresh);
    connect(controller, &ApplicationController::historyChanged, this, refresh);
    connect(controller, &ApplicationController::itemCountChanged, this, refresh);
    connect(controller, &ApplicationController::themeModeChanged, this, refresh);
    connect(controller, &ApplicationController::showHiddenChanged, this, refresh);
    connect(controller, &ApplicationController::singleClickOpenChanged, this, refresh);
    connect(controller, &ApplicationController::thumbnailsChanged, this, refresh);
    connect(controller, &ApplicationController::infoPanelChanged, this, refresh);
    connect(controller, &ApplicationController::undoStateChanged, this, refresh);
    connect(controller, &ApplicationController::viewModeChanged, this, refresh);
    connect(controller, &ApplicationController::densityChanged, this, refresh);
    connect(controller, &ApplicationController::iconSizeChanged, this, refresh);
    connect(controller, &ApplicationController::sortModeChanged, this, refresh);
    connect(controller, &ApplicationController::sortDescendingChanged, this, refresh);
    connect(controller, &ApplicationController::tabsChanged, this, refresh);
    connect(controller, &ApplicationController::activeTabChanged, this, refresh);
    connect(controller, &ApplicationController::reopenStateChanged, this, refresh);
    connect(controller, &ApplicationController::languageChanged, this, refresh);

    rebuild();
}

void ActionRegistry::trigger(const QString &id)
{
    for (const ActionDefinition &action : definitions()) {
        if (action.id != id)
            continue;

        if (action.ui) {
            emit uiRequested(id);
            return;
        }

        if (action.run)
            action.run(*m_controller);
        return;
    }
}

void ActionRegistry::rebuild()
{
    const auto entryFor = [this](const ActionDefinition &action) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), action.id);
        entry.insert(QStringLiteral("label"),
                     action.dynamicLabel ? action.dynamicLabel(*m_controller) : action.label);
        entry.insert(QStringLiteral("hint"), action.hint);
        entry.insert(QStringLiteral("category"), action.category);
        entry.insert(QStringLiteral("keywords"), action.keywords);
        entry.insert(QStringLiteral("enabled"),
                     action.enabled ? action.enabled(*m_controller) : true);
        entry.insert(QStringLiteral("checked"),
                     action.checked ? action.checked(*m_controller) : false);
        return entry;
    };

    QVariantList commands;
    for (const ActionDefinition &action : definitions())
        commands.append(entryFor(action));

    const auto findAction = [](const QString &id) -> const ActionDefinition * {
        for (const ActionDefinition &action : definitions()) {
            if (action.id == id)
                return &action;
        }
        return nullptr;
    };

    const auto separatorEntry = [] {
        QVariantMap separator;
        separator.insert(QStringLiteral("separator"), true);
        return separator;
    };

    std::function<QVariantList(const QVector<MenuNode> &)> build;
    build = [&](const QVector<MenuNode> &nodes) {
        QVariantList entries;

        for (const MenuNode &node : nodes) {
            if (node.separator) {
                if (!entries.isEmpty())
                    entries.append(separatorEntry());
                continue;
            }

            if (!node.children.isEmpty()) {
                QVariantMap groupEntry;
                groupEntry.insert(QStringLiteral("label"), node.label);
                groupEntry.insert(QStringLiteral("enabled"), true);
                groupEntry.insert(QStringLiteral("submenu"), build(node.children));
                entries.append(groupEntry);
                continue;
            }

            const ActionDefinition *action = findAction(node.actionId);
            if (!action)
                continue;

            QVariantMap entry = entryFor(*action);
            if (!node.label.isEmpty())
                entry.insert(QStringLiteral("label"), node.label);
            entries.append(entry);
        }

        return entries;
    };

    QVariantList menu = build(menuLayout());

    QVariantMap separator;
    separator.insert(QStringLiteral("separator"), true);
    menu.append(separator);

    QVariantMap about;
    about.insert(QStringLiteral("label"),
                 QStringLiteral("Ooze Files ") + QCoreApplication::applicationVersion());
    about.insert(QStringLiteral("enabled"), false);
    menu.append(about);

    if (menu == m_mainMenu && commands == m_commands)
        return;

    m_mainMenu = menu;
    m_commands = commands;
    emit entriesChanged();
}
