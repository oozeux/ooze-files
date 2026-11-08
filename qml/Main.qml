import QtQuick
import Ooze.Files

Window {
    id: root

    visible: true
    width: 1180
    height: 760
    minimumWidth: 760
    minimumHeight: 460
    color: Theme.bg
    title: App.currentPath.length > 0 ? App.currentPath : "Ooze Files"

    onActiveFocusItemChanged: {
        if (root.active && (!root.activeFocusItem || root.activeFocusItem === root))
            root.activeView.focusFirst()
    }

    property string openMenu: ""
    property Item activeView: App.viewMode === "list" ? listView : gridView
    property int pendingRow: -1
    property int contextRow: -1
    property real contextX: 0
    property real contextY: 0
    property string promptMode: ""
    property var dropUrls: []
    property string dropTargetUrl: ""
    property string dropTargetName: ""
    property real dropX: 0
    property real dropY: 0
    property int tabMenuIndex: -1
    property real tabMenuX: 0
    property real tabMenuY: 0

    readonly property bool tabShortcutGuard: !root.textInputFocused && !header.pathEditing
        && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible
        && !searchOverlay.visible && !bulkRename.visible && !commandPalette.visible
        && root.openMenu === ""

    property var tabMenuItems: {
        var index = root.tabMenuIndex
        var count = App.tabCount
        return [
            { id: "tabNew", label: qsTr("Nueva pestaña"), hint: "Ctrl+T" },
            { id: "tabDuplicate", label: qsTr("Duplicar pestaña"), enabled: index >= 0 },
            { separator: true },
            { id: "tabClose", label: qsTr("Cerrar pestaña"), hint: "Ctrl+W", enabled: index >= 0 },
            { id: "tabCloseOthers", label: qsTr("Cerrar las demás"), enabled: index >= 0 && count > 1 },
            { id: "tabCloseRight", label: qsTr("Cerrar las de la derecha"), enabled: index >= 0 && index < count - 1 },
            { separator: true },
            { id: "tabReopen", label: qsTr("Reabrir pestaña cerrada"), hint: "Ctrl+Shift+T", enabled: App.canReopenTab },
            { id: "tabCopyPath", label: qsTr("Copiar ruta de la pestaña"), enabled: index >= 0 }
        ]
    }

    property var dropMenuItems: [
        { label: qsTr("Soltar en «%1»").arg(root.dropTargetName), enabled: false },
        { separator: true },
        { label: qsTr("Mover aquí") },
        { label: qsTr("Copiar aquí") },
        { label: qsTr("Enlazar aquí") },
        { separator: true },
        { label: qsTr("Cancelar"), hint: "Esc" }
    ]

    property var sortMenuItems: [
        { label: qsTr("Ordenar por"), enabled: false },
        { separator: true },
        { label: qsTr("Nombre"), checked: App.sortMode === 0 },
        { label: qsTr("Tamaño"), checked: App.sortMode === 1 },
        { label: qsTr("Modificado"), checked: App.sortMode === 2 },
        { label: qsTr("Tipo"), checked: App.sortMode === 3 },
        { separator: true },
        { label: qsTr("Ascendente"), checked: !App.sortDescending },
        { label: qsTr("Descendente"), checked: App.sortDescending }
    ]

    property var openWithItems: []
    property var archiveItems: []
    property bool contextIsArchive: false
    property bool contextIsDirectory: false

    property var trashContextItems: [
        { id: "open", label: qsTr("Abrir"), enabled: App.selectionCount > 0 },
        { separator: true },
        { id: "restore", label: qsTr("Restaurar"), enabled: App.selectionCount > 0 },
        { id: "deleteForever", label: qsTr("Eliminar permanentemente"), hint: qsTr("Supr"), enabled: App.selectionCount > 0 },
        { separator: true },
        { id: "copy", label: qsTr("Copiar"), hint: "Ctrl+C", enabled: App.selectionCount > 0 },
        { id: "copyPath", label: qsTr("Copiar ruta"), enabled: App.selectionCount > 0 },
        { separator: true },
        { id: "properties", label: qsTr("Propiedades"), hint: "Alt+Return", enabled: root.contextRow >= 0 }
    ]

    property var trashBackgroundItems: [
        { id: "emptyTrash", label: qsTr("Vaciar la papelera"), enabled: App.itemCount > 0 },
        { separator: true },
        { id: "selectAll", label: qsTr("Seleccionar todo"), hint: "Ctrl+A", enabled: App.itemCount > 0 },
        { id: "copyPath", label: qsTr("Copiar ruta") },
        { id: "reload", label: qsTr("Recargar"), hint: "F5" },
        { separator: true },
        { id: "settings", label: qsTr("Configuración…"), hint: "Ctrl+," }
    ]

    property var contextMenuItems: {
        if (App.inTrash)
            return root.trashContextItems

        var items = [{ id: "open", label: qsTr("Abrir"), enabled: App.selectionCount > 0 }]

        if (root.openWithItems.length > 0)
            items.push({ id: "openWith", label: qsTr("Abrir con"), submenu: root.openWithItems })

        items.push(
            { id: "openTerminal", label: qsTr("Abrir en terminal"), enabled: App.selectedIsDirectory },
            { id: "preview", label: qsTr("Vista previa"), hint: "Space", enabled: root.contextRow >= 0 },
            { separator: true },
            { id: "cut", label: qsTr("Cortar"), hint: "Ctrl+X", enabled: App.selectionCount > 0 && !App.inVirtualLocation },
            { id: "copy", label: qsTr("Copiar"), hint: "Ctrl+C", enabled: App.selectionCount > 0 },
            { id: "paste", label: qsTr("Pegar"), hint: "Ctrl+V", enabled: App.canPaste },
            { separator: true },
            { id: "rename", label: qsTr("Renombrar"), hint: "F2", enabled: App.canRenameSelection },
            { id: "bulkRename", label: qsTr("Renombrar en lote…"), hint: "Shift+F2", enabled: App.selectionCount > 0 && !App.inVirtualLocation },
            { id: "trash", label: qsTr("Eliminar"), hint: qsTr("Supr"), enabled: App.selectionCount > 0 && !App.inVirtualLocation },
            { id: "delete", label: qsTr("Eliminar permanentemente"), hint: qsTr("Shift+Supr"), enabled: App.selectionCount > 0 && !App.inVirtualLocation },
            { separator: true },
            { id: "newFolder", label: qsTr("Nueva carpeta"), hint: "Ctrl+Shift+N", enabled: !App.inVirtualLocation },
            { id: "newFile", label: qsTr("Nuevo archivo"), enabled: !App.inVirtualLocation },
            { separator: true }
        )

        if (App.inRecents && App.selectionCount === 1)
            items.push({ id: "showInFolder", label: qsTr("Mostrar carpeta contenedora") })

        if (root.archiveItems.length > 0 && !App.inVirtualLocation)
            items.push({ id: "compress", label: qsTr("Comprimir"), submenu: root.archiveItems })

        if (root.contextIsArchive && !App.inVirtualLocation)
            items.push({ id: "extract", label: qsTr("Extraer aquí") })

        if (root.contextIsDirectory && App.selectionCount === 1)
            items.push({ id: "addPlace", label: qsTr("Añadir a Lugares") })

        items.push(
            { id: "copyPath", label: qsTr("Copiar ruta"), enabled: App.selectionCount > 0 },
            { separator: true },
            { id: "properties", label: qsTr("Propiedades"), hint: "Alt+Return", enabled: root.contextRow >= 0 }
        )
        return items
    }

    property var backgroundMenuItems: App.inTrash ? root.trashBackgroundItems : root.defaultBackgroundMenuItems

    property var defaultBackgroundMenuItems: [
        { id: "newFolder", label: qsTr("Nueva carpeta"), hint: "Ctrl+Shift+N", enabled: !App.inVirtualLocation },
        { id: "newFile", label: qsTr("Nuevo archivo"), enabled: !App.inVirtualLocation },
        { id: "paste", label: qsTr("Pegar"), hint: "Ctrl+V", enabled: App.canPaste },
        { separator: true },
        { id: "selectAll", label: qsTr("Seleccionar todo"), hint: "Ctrl+A", enabled: App.itemCount > 0 },
        { id: "searchRecursive", label: qsTr("Buscar en subcarpetas…"), hint: "Ctrl+Shift+F" },
        { id: "terminalHere", label: qsTr("Abrir terminal aquí") },
        { separator: true },
        { id: "addCurrentPlace", label: qsTr("Añadir esta carpeta a Lugares") },
        { id: "copyPath", label: qsTr("Copiar ruta") },
        { id: "reload", label: qsTr("Recargar"), hint: "F5" },
        { separator: true },
        { id: "properties", label: qsTr("Propiedades"), hint: "Alt+Return" },
        { id: "settings", label: qsTr("Configuración…"), hint: "Ctrl+," }
    ]

    property var placeMenuItems: []
    property var placeMenuTarget: ({})
    property real placeX: 0
    property real placeY: 0

    function handleSortMenu(index) {
        if (index >= 2 && index <= 5)
            App.setSortMode(index - 2)
        else if (index === 7)
            App.setSortDescending(false)
        else if (index === 8)
            App.setSortDescending(true)
    }

    function handleMenuItem(item) {
        if (item.appId !== undefined) {
            if (root.contextRow >= 0)
                App.openRowWith(root.contextRow, item.appId)
            return
        }

        if (item.format !== undefined) {
            App.compressSelection(item.format)
            return
        }

        switch (item.id) {
        case "compressAs":
            root.openCompressDialog()
            break
        case "extract":
            if (root.contextRow >= 0)
                App.extractRow(root.contextRow)
            break
        case "showInFolder":
            if (root.contextRow >= 0)
                App.showInFolder(root.contextRow)
            break
        case "addPlace":
            if (root.contextRow >= 0)
                App.addPlaceForRow(root.contextRow)
            break
        case "addCurrentPlace":
            App.addCurrentFolderToPlaces()
            break
        case "properties":
            root.openProperties(root.contextRow)
            break
        case "open":
            if (root.contextRow >= 0)
                App.openRow(root.contextRow)
            break
        case "openTerminal":
            if (root.contextRow >= 0)
                App.openRowInTerminal(root.contextRow)
            break
        case "preview":
            root.openPreview(root.contextRow)
            break
        case "cut":
            App.cutSelection()
            break
        case "copy":
            App.copySelection()
            break
        case "paste":
            App.paste()
            break
        case "rename":
            root.openPrompt("rename")
            break
        case "bulkRename":
            root.openBulkRename()
            break
        case "searchRecursive":
            root.openSearch()
            break
        case "trash":
            App.trashSelection()
            break
        case "delete":
            App.deleteSelection()
            break
        case "restore":
            App.restoreSelection()
            break
        case "deleteForever":
            App.deleteSelection()
            break
        case "emptyTrash":
            App.emptyTrash()
            break
        case "newFolder":
            root.openPrompt("folder")
            break
        case "newFile":
            root.openPrompt("file")
            break
        case "copyPath":
            App.copyPath(root.contextRow)
            break
        case "selectAll":
            App.selectAll()
            break
        case "terminalHere":
            App.openCurrentInTerminal()
            break
        case "reload":
            App.reload()
            break
        case "settings":
            root.openSettings()
            break
        }
    }

    function openProperties(row) {
        root.openMenu = ""
        properties.open(row)
    }

    function handlePlaceMenu(item) {
        var target = root.placeMenuTarget
        switch (item.id) {
        case "removePlace":
            App.removePlace(target.url)
            break
        case "placeUp":
            App.movePlaceBy(target.id, -1)
            break
        case "placeDown":
            App.movePlaceBy(target.id, 1)
            break
        case "deviceOpen":
            App.openDevice(target.objectPath)
            break
        case "deviceMount":
            App.openDevice(target.objectPath)
            break
        case "deviceUnmount":
            App.unmountDevice(target.objectPath)
            break
        case "deviceEject":
            App.ejectDevice(target.objectPath)
            break
        }
    }

    function showPlaceMenu(kind, target, x, y) {
        var items = []
        if (kind === "place") {
            items.push({ id: "placeUp", label: qsTr("Subir"), enabled: App.canMovePlace(target.id, -1) },
                       { id: "placeDown", label: qsTr("Bajar"), enabled: App.canMovePlace(target.id, 1) })
            if (target.custom === true)
                items.push({ id: "removePlace", label: qsTr("Quitar de Lugares") })
        } else if (target.mounted) {
            items.push({ id: "deviceOpen", label: qsTr("Abrir") },
                       { id: "deviceUnmount", label: qsTr("Desmontar") })
            if (target.removable)
                items.push({ id: "deviceEject", label: qsTr("Expulsar") })
        } else {
            items.push({ id: "deviceMount", label: qsTr("Montar y abrir") })
        }

        var point = root.contentItem.mapFromItem(null, x, y)
        root.placeMenuItems = items
        root.placeMenuTarget = target
        root.placeX = point.x
        root.placeY = point.y
        root.openMenu = "place"
    }

    function showTabMenu(index, x, y) {
        var point = tabBar.mapToItem(root.contentItem, x, y)
        root.tabMenuIndex = index
        root.tabMenuX = point.x
        root.tabMenuY = point.y
        root.openMenu = "tab"
    }

    function handleTabMenu(item) {
        switch (item.id) {
        case "tabNew":
            App.newTab()
            break
        case "tabDuplicate":
            App.duplicateTab(root.tabMenuIndex)
            break
        case "tabClose":
            App.closeTab(root.tabMenuIndex)
            break
        case "tabCloseOthers":
            App.closeOtherTabs(root.tabMenuIndex)
            break
        case "tabCloseRight":
            App.closeTabsToRight(root.tabMenuIndex)
            break
        case "tabReopen":
            App.reopenClosedTab()
            break
        case "tabCopyPath":
            App.copyTabPath(root.tabMenuIndex)
            break
        }
        root.tabMenuIndex = -1
        root.openMenu = ""
    }

    function resetForTabSwitch() {
        root.openMenu = ""
        root.pendingRow = -1
        root.contextRow = -1
        root.tabMenuIndex = -1
        gridView.resetView()
        listView.resetView()
        if (preview.visible)
            preview.close()
        if (searchOverlay.visible)
            searchOverlay.close()
        if (properties.visible)
            properties.close()
        if (bulkRename.visible)
            bulkRename.close()
    }

    function openPreview(row) {
        var target = row !== undefined ? row : root.activeView.currentIndex
        if (target < 0 || target >= App.itemCount)
            return
        root.openMenu = ""
        preview.open(target)
    }

    function archiveEntries() {
        if (App.selectionCount < 1)
            return []

        var formats = App.compressFormats()
        var entries = []
        for (var i = 0; i < formats.length; ++i)
            entries.push({ format: formats[i].id, label: formats[i].label })

        if (entries.length > 0) {
            entries.push({ separator: true })
            entries.push({ id: "compressAs", label: qsTr("Comprimir como…") })
        }
        return entries
    }

    function openCompressDialog() {
        root.openMenu = ""
        if (App.selectionCount < 1)
            return
        compressDialog.open(App.suggestedArchiveName())
    }

    function openWithEntries(row) {
        if (row < 0 || App.selectionCount !== 1)
            return []

        var apps = App.openWithApps(row)
        var entries = []
        for (var i = 0; i < apps.length; ++i) {
            entries.push({
                appId: apps[i].id,
                label: apps[i].name,
                hint: apps[i].isDefault ? qsTr("predeterminada") : ""
            })
        }

        var others = App.openWithOtherApps(row)
        if (others.length > 0) {
            var otherEntries = []
            for (var j = 0; j < others.length; ++j)
                otherEntries.push({ appId: others[j].id, label: others[j].name })
            if (entries.length > 0)
                entries.push({ separator: true })
            entries.push({ label: qsTr("Otras aplicaciones"), submenu: otherEntries })
        }
        return entries
    }

    function showContext(row, x, y) {
        if (row >= 0) {
            if (!App.isSelected(row))
                App.selectRow(row, false, false)
            root.activeView.setCurrentRow(row)
        }

        var point = root.activeView.mapToItem(root.contentItem, x, y)
        root.openWithItems = root.openWithEntries(row)
        root.archiveItems = row >= 0 ? root.archiveEntries() : []
        root.contextIsArchive = row >= 0 && App.selectionCount === 1 && App.isArchiveRow(row)
        root.contextIsDirectory = row >= 0 && App.selectionCount === 1 && App.selectedIsDirectory
        root.contextRow = row
        root.contextX = point.x
        root.contextY = point.y
        root.openMenu = "context"
    }

    function openItemContext() {
        var row = root.activeView.currentIndex
        if (row < 0)
            return
        var item = root.activeView.itemAtRow(row)
        var x = 12
        var y = 12
        if (item) {
            var p = item.mapToItem(root.activeView, 12, 12)
            x = p.x
            y = p.y
        }
        root.showContext(row, x, y)
    }

    function openSettings() {
        root.openMenu = ""
        settings.open()
    }

    function openSearch() {
        root.openMenu = ""
        searchOverlay.open()
    }

    function openBulkRename() {
        root.openMenu = ""
        if (App.inVirtualLocation)
            return
        bulkRename.open(App.selectedEntries())
    }

    function urlDisplayName(url) {
        var text = "" + url
        var path = text.replace(/^file:\/\//, "").replace(/\/+$/, "")
        var name = path.substring(path.lastIndexOf("/") + 1)
        try {
            name = decodeURIComponent(name)
        } catch (error) {
            name = ""
        }
        return name.length > 0 ? name : "/"
    }

    function requestDrop(urls, targetUrl, x, y) {
        if (!urls || urls.length === 0)
            return

        if (("" + targetUrl).indexOf("trash:") === 0) {
            App.trashUrls(urls)
            return
        }

        root.dropUrls = urls
        root.dropTargetUrl = targetUrl
        root.dropTargetName = root.urlDisplayName(targetUrl)
        root.dropX = x
        root.dropY = y
        root.openMenu = "drop"
    }

    function handleDropMenu(index) {
        var action = Qt.IgnoreAction

        if (index === 2)
            action = Qt.MoveAction
        else if (index === 3)
            action = Qt.CopyAction
        else if (index === 4)
            action = Qt.LinkAction

        if (action !== Qt.IgnoreAction && root.dropUrls.length > 0 && root.dropTargetUrl.length > 0)
            App.dropUrls(root.dropUrls, action, root.dropTargetUrl)

        root.openMenu = ""
        root.activeView.focusFirst()
    }

    function openPrompt(mode) {
        root.openMenu = ""

        if (mode === "rename" && !App.canRenameSelection)
            return

        root.promptMode = mode

        if (mode === "rename")
            prompt.open(qsTr("Renombrar"), App.selectedName, qsTr("Renombrar"))
        else if (mode === "folder")
            prompt.open(qsTr("Nueva carpeta"), App.suggestedFolderName(), qsTr("Crear"))
        else if (mode === "file")
            prompt.open(qsTr("Nuevo archivo"), App.suggestedFileName(), qsTr("Crear"))
    }

    function applyPendingSelection() {
        if (root.pendingRow < 0)
            return
        var view = root.activeView
        if (!view || view.count <= root.pendingRow)
            return
        view.selectRow(root.pendingRow)
        root.pendingRow = -1
    }

    Connections {
        target: App

        function onCurrentUrlChanged() {
            root.pendingRow = -1
            root.openMenu = ""
        }

        function onSelectRowRequested(row) {
            root.pendingRow = row
            root.applyPendingSelection()
        }

        function onActiveTabChanged() {
            root.resetForTabSwitch()
        }
    }

    Connections {
        target: Actions

        function onUiRequested(id) {
            switch (id) {
            case "nav.path":
                header.beginPathEdit()
                break
            case "nav.search":
                header.focusSearch()
                break
            case "view.settings":
                root.openSettings()
                break
            case "palette.open":
                commandPalette.open()
                break
            case "selection.open":
                if (root.activeView.currentIndex >= 0)
                    App.openRow(root.activeView.currentIndex)
                break
            case "selection.preview":
                root.openPreview()
                break
            case "selection.openTerminal":
                if (root.activeView.currentIndex >= 0)
                    App.openRowInTerminal(root.activeView.currentIndex)
                break
            case "selection.copyPath":
                App.copyPath(root.activeView.currentIndex)
                break
            case "selection.rename":
                root.openPrompt("rename")
                break
            case "selection.bulkRename":
                root.openBulkRename()
                break
            case "nav.searchRecursive":
                root.openSearch()
                break
            case "file.newFolder":
                root.openPrompt("folder")
                break
            case "file.newFile":
                root.openPrompt("file")
                break
            }
        }
    }

    HeaderBar {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.headerHeight
        onSortRequested: root.openMenu = "sort"
        onMenuRequested: root.openMenu = "main"
        onFocusContentRequested: root.activeView.focusFirst()
    }

    TabBar {
        id: tabBar
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? implicitHeight : 0
        onTabActivated: function (index) { App.activateTab(index) }
        onTabCloseRequested: function (index) { App.closeTab(index) }
        onTabContextRequested: function (index, x, y) { root.showTabMenu(index, x, y) }
    }

    Sidebar {
        id: sidebar
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.bottom: status.top
        width: Theme.sidebarWidth
        onDropRequested: function (urls, targetUrl, x, y) {
            root.requestDrop(urls, targetUrl, x, y)
        }
        onContextRequested: function (kind, item, x, y) { root.showPlaceMenu(kind, item, x, y) }
    }

    InfoPanel {
        id: infoPanel
        anchors.top: tabBar.bottom
        anchors.right: parent.right
        anchors.bottom: status.top
        width: Math.min(300, Math.max(0, root.width - sidebar.width - 360))
        visible: App.infoPanel && width >= 220
    }

    Item {
        id: content

        anchors.top: tabBar.bottom
        anchors.left: sidebar.right
        anchors.right: infoPanel.visible ? infoPanel.left : parent.right
        anchors.bottom: status.top

        FileGridView {
            id: gridView
            anchors.fill: parent
            visible: App.viewMode !== "list"
            onCountChanged: root.applyPendingSelection()
            onActivated: function (row) { App.openRow(row) }
            onNewTabRequested: function (row) { App.openRowInNewTab(row) }
            onContextRequested: function (row, x, y) { root.showContext(row, x, y) }
            onDropRequested: function (urls, targetUrl, x, y) {
                root.requestDrop(urls, targetUrl, x, y)
            }
        }

        FileListView {
            id: listView
            anchors.fill: parent
            visible: App.viewMode === "list"
            onCountChanged: root.applyPendingSelection()
            onActivated: function (row) { App.openRow(row) }
            onNewTabRequested: function (row) { App.openRowInNewTab(row) }
            onContextRequested: function (row, x, y) { root.showContext(row, x, y) }
            onDropRequested: function (urls, targetUrl, x, y) {
                root.requestDrop(urls, targetUrl, x, y)
            }
        }

        Text {
            anchors.centerIn: parent
            visible: !App.loading && App.itemCount === 0
            text: App.filterText.length > 0
                ? qsTr("Sin coincidencias")
                : (App.inTrash && App.infoColumns
                    ? qsTr("La papelera está vacía")
                    : (App.inRecents ? qsTr("No hay archivos recientes") : qsTr("Esta carpeta está vacía")))
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize + 1
        }
    }

    StatusBar {
        id: status
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.statusHeight
        activeView: root.activeView
    }

    MouseArea {
        anchors.fill: parent
        z: 150
        visible: root.openMenu !== ""
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        onWheel: function (wheel) { wheel.accepted = true }
        onClicked: root.openMenu = ""
    }

    PopupMenu {
        id: mainMenu
        z: 151
        menuWidth: 264
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, root.width - width - 10)
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, header.height + 4))
        visible: root.openMenu === "main"
        items: Actions.mainMenu
        onTriggeredItem: function (item) {
            if (item && item.id)
                Actions.trigger(item.id)
        }
        onClosed: root.openMenu = ""
    }

    PopupMenu {
        id: sortMenu
        z: 151
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, root.width - width - 10 - Theme.rowHeight - 4)
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, header.height + 4))
        visible: root.openMenu === "sort"
        items: root.sortMenuItems
        onTriggered: function (index) { root.handleSortMenu(index) }
        onClosed: root.openMenu = ""
    }

    PopupMenu {
        id: contextMenu
        z: 151
        menuWidth: 296
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, Math.min(root.width - width - edgeMargin, root.contextX))
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, root.contextY))
        visible: root.openMenu === "context"
        items: root.contextRow >= 0 ? root.contextMenuItems : root.backgroundMenuItems
        onTriggeredItem: function (item) { root.handleMenuItem(item) }
        onClosed: root.openMenu = ""
    }

    PopupMenu {
        id: placeMenu
        z: 151
        menuWidth: 220
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, Math.min(root.width - width - edgeMargin, root.placeX))
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, root.placeY))
        visible: root.openMenu === "place"
        items: root.placeMenuItems
        onTriggeredItem: function (item) { root.handlePlaceMenu(item) }
        onClosed: root.openMenu = ""
    }

    PopupMenu {
        id: tabMenu
        z: 151
        menuWidth: 240
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, Math.min(root.width - width - edgeMargin, root.tabMenuX))
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, root.tabMenuY))
        visible: root.openMenu === "tab"
        items: root.tabMenuItems
        onTriggeredItem: function (item) { root.handleTabMenu(item) }
        onClosed: root.openMenu = ""
    }

    PopupMenu {
        id: dropMenu
        z: 151
        boundsWidth: root.width
        boundsHeight: root.height
        x: Math.max(edgeMargin, Math.min(root.width - width - edgeMargin, root.dropX))
        y: Math.max(edgeMargin, Math.min(root.height - height - edgeMargin, root.dropY))
        visible: root.openMenu === "drop"
        items: root.dropMenuItems
        onTriggered: function (index) { root.handleDropMenu(index) }
        onVisibleChanged: {
            if (!visible) {
                root.dropUrls = []
                root.dropTargetUrl = ""
                root.dropTargetName = ""
            }
        }
    }

    PromptDialog {
        id: prompt

        onAccepted: function (text) {
            if (root.promptMode === "rename")
                App.renameSelection(text)
            else if (root.promptMode === "folder")
                App.createFolder(text)
            else if (root.promptMode === "file")
                App.createFile(text)
            root.promptMode = ""
            root.activeView.focusFirst()
        }

        onCancelled: {
            root.promptMode = ""
            root.activeView.focusFirst()
        }
    }

    CompressDialog {
        id: compressDialog

        onAccepted: function (name, formatId) {
            App.compressSelectionAs(formatId, name)
            root.activeView.focusFirst()
        }

        onCancelled: root.activeView.focusFirst()
    }

    ConflictDialog {
        id: conflictDialog

        onDecided: function (id, decision, applyToAll) {
            App.resolveConflict(id, decision, applyToAll)
            root.activeView.focusFirst()
        }
    }

    Connections {
        target: App

        function onConflictRequested(info) {
            conflictDialog.open(info)
        }

        function onConfirmationRequested(id, title, message, acceptLabel) {
            confirmDialog.open(id, title, message, acceptLabel)
        }
    }

    ConfirmDialog {
        id: confirmDialog

        onAccepted: function (actionId) {
            App.confirmAction(actionId)
            root.activeView.focusFirst()
        }
        onCancelled: root.activeView.focusFirst()
    }

    SettingsWindow {
        id: settings

        onClosed: root.activeView.focusFirst()
    }

    PropertiesDialog {
        id: properties

        onClosed: root.activeView.focusFirst()
    }

    PreviewOverlay {
        id: preview

        onClosed: root.activeView.focusFirst()
        onRowRequested: function (row) {
            App.selectRow(row, false, false)
            root.activeView.setCurrentRow(row)
        }
    }

    SearchOverlay {
        id: searchOverlay

        onClosed: root.activeView.focusFirst()
    }

    BulkRenameDialog {
        id: bulkRename

        onClosed: root.activeView.focusFirst()
    }

    CommandPalette {
        id: commandPalette

        onClosed: root.activeView.focusFirst()
    }

    Shortcut {
        sequence: "Space"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !properties.visible
                 && !commandPalette.visible && !searchOverlay.visible && !bulkRename.visible
                 && root.openMenu === "" && App.itemCount > 0
        onActivated: {
            if (preview.visible)
                preview.close()
            else
                root.openPreview()
        }
    }

    Shortcut {
        sequence: "Ctrl+Comma"
        onActivated: root.openSettings()
    }

    Shortcut {
        sequence: "Ctrl+K"
        enabled: !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && root.openMenu === ""
        onActivated: commandPalette.toggle()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Ctrl+N"
        enabled: root.tabShortcutGuard
        onActivated: App.openNewWindow()
    }

    Shortcut {
        sequence: "Ctrl+T"
        enabled: root.tabShortcutGuard
        onActivated: App.newTab()
    }

    Shortcut {
        sequence: "Ctrl+W"
        enabled: root.tabShortcutGuard
        onActivated: App.closeTab(App.activeTab)
    }

    Shortcut {
        sequence: "Ctrl+Shift+T"
        enabled: root.tabShortcutGuard && App.canReopenTab
        onActivated: App.reopenClosedTab()
    }

    Shortcut {
        sequence: "Ctrl+Tab"
        enabled: root.tabShortcutGuard && App.tabCount > 1
        onActivated: App.nextTab()
    }

    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        enabled: root.tabShortcutGuard && App.tabCount > 1
        onActivated: App.previousTab()
    }

    Shortcut {
        sequence: "Alt+1"
        enabled: root.tabShortcutGuard && App.tabCount > 1
        onActivated: App.activateTab(0)
    }

    Shortcut {
        sequence: "Alt+2"
        enabled: root.tabShortcutGuard && App.tabCount > 2
        onActivated: App.activateTab(1)
    }

    Shortcut {
        sequence: "Alt+3"
        enabled: root.tabShortcutGuard && App.tabCount > 3
        onActivated: App.activateTab(2)
    }

    Shortcut {
        sequence: "Alt+4"
        enabled: root.tabShortcutGuard && App.tabCount > 4
        onActivated: App.activateTab(3)
    }

    Shortcut {
        sequence: "Alt+5"
        enabled: root.tabShortcutGuard && App.tabCount > 5
        onActivated: App.activateTab(4)
    }

    Shortcut {
        sequence: "Alt+6"
        enabled: root.tabShortcutGuard && App.tabCount > 6
        onActivated: App.activateTab(5)
    }

    Shortcut {
        sequence: "Alt+7"
        enabled: root.tabShortcutGuard && App.tabCount > 7
        onActivated: App.activateTab(6)
    }

    Shortcut {
        sequence: "Alt+8"
        enabled: root.tabShortcutGuard && App.tabCount > 8
        onActivated: App.activateTab(7)
    }

    Shortcut {
        sequence: "Alt+9"
        enabled: root.tabShortcutGuard && App.tabCount > 1
        onActivated: App.activateTab(App.tabCount - 1)
    }

    Shortcut {
        sequence: "Ctrl+A"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible
                 && !commandPalette.visible && !searchOverlay.visible && !bulkRename.visible && App.itemCount > 0
        onActivated: App.selectAll()
    }

    Shortcut {
        sequence: "Menu"
        onActivated: root.openItemContext()
    }

    Shortcut {
        sequence: "Shift+F10"
        onActivated: root.openItemContext()
    }

    Shortcut {
        sequence: "Ctrl+L"
        onActivated: header.beginPathEdit()
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: header.focusSearch()
    }

    Shortcut {
        sequence: "Ctrl+Shift+F"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible
                 && !properties.visible && !searchOverlay.visible && !bulkRename.visible
                 && !commandPalette.visible && root.openMenu === ""
        onActivated: root.openSearch()
    }

    Shortcut {
        sequence: "Shift+F2"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible
                 && !properties.visible && !searchOverlay.visible && !bulkRename.visible
                 && !commandPalette.visible && root.openMenu === "" && App.selectionCount > 0
        onActivated: root.openBulkRename()
    }

    Shortcut {
        sequence: "Shift+F4"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible
                 && !properties.visible && !searchOverlay.visible && !bulkRename.visible
                 && !commandPalette.visible && root.openMenu === ""
        onActivated: Actions.trigger("tools.terminal")
    }

    Shortcut {
        sequence: "Ctrl+H"
        onActivated: App.setShowHidden(!App.showHidden)
    }

    Shortcut {
        sequence: "Ctrl+Shift+N"
        enabled: !header.pathEditing && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !searchOverlay.visible && !bulkRename.visible
        onActivated: root.openPrompt("folder")
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.openMenu !== ""
        onActivated: root.openMenu = ""
    }

    readonly property bool textInputFocused: root.activeFocusItem instanceof TextInput

    Shortcut {
        sequence: "F11"
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && root.openMenu === ""
        onActivated: App.infoPanel = !App.infoPanel
    }

    Shortcut {
        sequences: ["Alt+Return", "Alt+Enter"]
        enabled: !root.textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && root.openMenu === "" && App.itemCount > 0
        onActivated: {
            var rows = App.selectedRows()
            root.openProperties(rows.length === 1 ? rows[0] : (rows.length === 0 ? -1 : rows[0]))
        }
    }

    Shortcut {
        sequence: "Ctrl+Z"
        enabled: !textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && root.openMenu === ""
        onActivated: App.undo()
    }

    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        enabled: !textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && root.openMenu === ""
        onActivated: App.redo()
    }

    Shortcut {
        sequence: "F2"
        enabled: !textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && App.canRenameSelection
        onActivated: root.openPrompt("rename")
    }

    Shortcut {
        sequence: "Delete"
        enabled: !textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && App.selectionCount > 0
        onActivated: App.trashSelection()
    }

    Shortcut {
        sequences: ["Shift+Delete"]
        enabled: !textInputFocused && !prompt.visible && !compressDialog.visible && !conflictDialog.visible && !confirmDialog.visible && !settings.visible && !preview.visible && !properties.visible && !searchOverlay.visible && !bulkRename.visible && App.selectionCount > 0
        onActivated: App.deleteSelection()
    }

    Shortcut {
        sequence: "F5"
        onActivated: App.reload()
    }

    Shortcut {
        sequence: "Alt+Left"
        onActivated: App.goBack()
    }

    Shortcut {
        sequence: "Alt+Right"
        onActivated: App.goForward()
    }

    Shortcut {
        sequence: "Alt+Up"
        onActivated: App.goUp()
    }
}
