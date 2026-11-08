pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Window {
    id: root

    property int tab: 0
    property var appStates: []
    property string chooserCategory: ""
    property var chooserItems: []
    property real chooserX: 0
    property real chooserY: 0

    readonly property var tabs: [qsTr("Apariencia"), qsTr("Vista"), qsTr("Comportamiento"),
                                 qsTr("Aplicaciones")]
    readonly property var currentFlick: [appearancePage, viewPage, behaviorPage, appsPage][root.tab]

    signal closed()

    title: qsTr("Configuración")
    width: 820
    height: 600
    minimumWidth: 580
    minimumHeight: 400
    flags: Qt.Dialog
    color: Theme.bg
    visible: false

    function open() {
        Palette.refreshChoices()
        terminalInput.text = App.terminalCommand
        fontInput.text = App.fontFamilySetting
        root.appStates = App.defaultAppStates()
        chooser.visible = false
        root.visible = true
        root.raise()
        root.requestActivate()
    }

    function openChooser(state, anchorItem) {
        var entries = []
        var apps = state.apps
        for (var i = 0; i < apps.length; ++i) {
            entries.push({
                appId: apps[i].id,
                label: apps[i].name,
                checked: apps[i].id === state.appId
            })
        }

        var point = anchorItem.mapToItem(surface, 0, anchorItem.height + 2)
        root.chooserCategory = state.id
        root.chooserItems = entries
        root.chooserX = point.x
        root.chooserY = point.y
        chooser.visible = true
    }

    onVisibleChanged: {
        if (!visible)
            root.closed()
    }

    component SectionLabel: Text {
        color: Theme.fgMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 1
        font.bold: true
    }

    component Hint: Text {
        width: parent.width
        color: Theme.fgMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 1
        wrapMode: Text.WordWrap
    }

    component Separator: Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    component PageView: Flickable {
        default property alias items: column.data

        contentWidth: width
        contentHeight: column.implicitHeight + 2 * Theme.pad
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column

            x: Theme.pad * 2
            y: Theme.pad * 2
            width: Math.max(0, parent.width - 4 * Theme.pad)
            spacing: Theme.pad
        }
    }

    component ChoiceOption: Rectangle {
        id: option

        property string group: ""
        property string value: ""
        property string label: ""

        readonly property bool checkedValue: App[option.group] === option.value

        width: optionLabel.width + 18
        height: Theme.rowHeight - 8
        radius: Theme.radiusSmall
        color: optionMouse.hovered
            ? Theme.surfaceHover
            : (option.checkedValue ? Theme.surfaceActive : "transparent")

        Text {
            id: optionLabel
            anchors.centerIn: parent
            width: Math.min(implicitWidth, 170)
            text: option.label
            elide: Text.ElideRight
            color: option.checkedValue ? Theme.fg : Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }

        HoverHandler {
            id: optionMouse
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: App[option.group] = option.value
        }
    }

    component ChoiceRow: Flow {
        id: row

        property string label: ""
        property string group: ""
        property var options: []

        width: parent.width
        spacing: 6

        Text {
            width: 150
            height: Theme.rowHeight - 8
            verticalAlignment: Text.AlignVCenter
            text: row.label
            elide: Text.ElideRight
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Repeater {
            model: row.options

            ChoiceOption {
                required property var modelData
                group: row.group
                value: modelData.value
                label: modelData.label
            }
        }
    }

    component ToggleOption: Item {
        id: toggleRow

        property string label: ""
        property string group: ""

        readonly property bool checkedValue: App[toggleRow.group]

        width: Math.min(parent.width, 460)
        height: Math.max(Theme.rowHeight - 8, toggleLabel.implicitHeight + 6)

        Text {
            id: toggleLabel
            anchors.left: parent.left
            anchors.right: toggleSwitch.left
            anchors.rightMargin: Theme.pad
            anchors.verticalCenter: parent.verticalCenter
            text: toggleRow.label
            wrapMode: Text.WordWrap
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Rectangle {
            id: toggleSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 38
            height: 20
            radius: height / 2
            color: toggleRow.checkedValue ? Theme.accent : Theme.border

            Rectangle {
                x: toggleRow.checkedValue ? parent.width - width - 2 : 2
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                radius: 8
                color: "#ffffff"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: App[toggleRow.group] = !App[toggleRow.group]
            }
        }
    }

    component TextField: Rectangle {
        id: field

        property alias text: input.text
        property alias input: input

        signal accepted(string value)

        width: parent.width
        height: Theme.rowHeight
        radius: Theme.radiusSmall
        color: Theme.bg
        border.width: Theme.borderWidth
        border.color: input.activeFocus ? Theme.accent : Theme.border

        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            selectionColor: Theme.accent
            selectedTextColor: "#ffffff"
            selectByMouse: true

            onAccepted: field.accepted(text)
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !chooser.visible
        onActivated: root.close()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible && chooser.visible
        onActivated: chooser.visible = false
    }

    Item {
        id: surface

        anchors.fill: parent

        Rectangle {
            id: nav

            anchors.top: parent.top
            anchors.bottom: footer.top
            anchors.left: parent.left
            width: 190
            color: Theme.surfaceAlt

            Rectangle {
                anchors.right: parent.right
                width: Theme.borderWidth
                height: parent.height
                color: Theme.border
            }

            Column {
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: 2

                Repeater {
                    model: root.tabs

                    delegate: Rectangle {
                        id: tabRow

                        required property int index
                        required property string modelData

                        width: parent.width
                        height: Theme.rowHeight + 4
                        radius: Theme.radiusSmall
                        color: root.tab === index
                            ? Theme.selectionBg
                            : (tabHover.hovered ? Theme.surfaceHover : "transparent")

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.pad
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: tabRow.modelData
                            color: Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            font.bold: root.tab === tabRow.index
                            elide: Text.ElideRight
                        }

                        HoverHandler {
                            id: tabHover
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.tab = tabRow.index
                        }
                    }
                }
            }
        }

        Item {
            id: pages

            anchors.top: parent.top
            anchors.bottom: footer.top
            anchors.left: nav.right
            anchors.right: parent.right

            PageView {
                id: appearancePage

                anchors.fill: parent
                visible: root.tab === 0

                SectionLabel {
                    text: qsTr("Tema y colores")
                }

                ChoiceRow {
                    label: qsTr("Tema")
                    group: "themeMode"
                    options: [
                        { label: qsTr("Claro"), value: "light" },
                        { label: qsTr("Oscuro"), value: "dark" },
                        { label: qsTr("Sistema"), value: "system" }
                    ]
                }

                ChoiceRow {
                    label: qsTr("Paleta")
                    group: "paletteId"
                    options: Palette.choices
                }

                Row {
                    spacing: 4

                    Repeater {
                        model: [Theme.bg, Theme.surface, Theme.surfaceHover,
                                Theme.fgMuted, Theme.fg, Theme.accent, Theme.danger]

                        Rectangle {
                            required property color modelData

                            width: 24
                            height: 18
                            radius: 3
                            color: modelData
                            border.width: 1
                            border.color: Theme.border
                        }
                    }
                }

                Row {
                    spacing: 8

                    Rectangle {
                        width: Math.max(96, paletteLabel.implicitWidth + 16)
                        height: 26
                        radius: Theme.radiusSmall
                        color: openHover.hovered ? Theme.accent : Theme.surfaceActive

                        Text {
                            id: paletteLabel
                            anchors.centerIn: parent
                            text: qsTr("Abrir carpeta de paletas")
                            color: openHover.hovered ? "#ffffff" : Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                        }

                        HoverHandler {
                            id: openHover
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.openThemesFolder()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Palette.paletteName
                        color: Theme.fgMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                        elide: Text.ElideRight
                        width: 200
                    }
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Tipografía")
                }

                TextField {
                    id: fontInput

                    onAccepted: function (value) { App.fontFamilySetting = value }
                }

                Hint {
                    text: qsTr("Familia tipográfica; vacío usa la del sistema. Enter para aplicar.")
                }
            }

            PageView {
                id: viewPage

                anchors.fill: parent
                visible: root.tab === 1

                SectionLabel {
                    text: qsTr("Vista de archivos")
                }

                ChoiceRow {
                    label: qsTr("Vista")
                    group: "viewMode"
                    options: [
                        { label: qsTr("Automático"), value: "auto" },
                        { label: qsTr("Cuadrícula"), value: "grid" },
                        { label: qsTr("Lista"), value: "list" }
                    ]
                }

                ChoiceRow {
                    label: qsTr("Densidad")
                    group: "density"
                    options: [
                        { label: qsTr("Compacto"), value: "compact" },
                        { label: qsTr("Normal"), value: "normal" },
                        { label: qsTr("Cómodo"), value: "comfortable" }
                    ]
                }

                ChoiceRow {
                    label: qsTr("Tamaño de iconos")
                    group: "iconSize"
                    options: [
                        { label: qsTr("Pequeños"), value: "small" },
                        { label: qsTr("Normales"), value: "normal" },
                        { label: qsTr("Grandes"), value: "large" },
                        { label: qsTr("Muy grandes"), value: "huge" }
                    ]
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Paneles")
                }

                ToggleOption {
                    label: qsTr("Miniaturas")
                    group: "thumbnails"
                }

                ToggleOption {
                    label: qsTr("Panel de información")
                    group: "infoPanel"
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Rendimiento")
                }

                ToggleOption {
                    label: qsTr("Modo ligero")
                    group: "lightweight"
                }

                Hint {
                    text: qsTr("Dibuja la interfaz por software para ahorrar memoria (unos 70 MB con controladores NVIDIA). Requiere reiniciar Ooze Files.")
                }
            }

            PageView {
                id: behaviorPage

                anchors.fill: parent
                visible: root.tab === 2

                SectionLabel {
                    text: qsTr("Idioma")
                }

                ChoiceRow {
                    label: qsTr("Idioma de la interfaz")
                    group: "language"
                    options: [
                        { label: qsTr("Sistema"), value: "system" },
                        { label: "Español", value: "es" },
                        { label: "English", value: "en" }
                    ]
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Comportamiento")
                }

                ToggleOption {
                    label: qsTr("Mostrar ocultos")
                    group: "showHidden"
                }

                ToggleOption {
                    label: qsTr("Abrir con un clic")
                    group: "singleClickOpen"
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Pestañas")
                }

                ToggleOption {
                    label: qsTr("Restaurar sesión al iniciar")
                    group: "restoreSession"
                }

                ToggleOption {
                    label: qsTr("Mostrar siempre la barra")
                    group: "alwaysShowTabBar"
                }

                Hint {
                    text: qsTr("La sesión guarda hasta 16 pestañas. Las rutas que ya no existan se omiten al restaurar.")
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Ventanas")
                }

                ToggleOption {
                    label: qsTr("Reutilizar la ventana abierta")
                    group: "singleInstance"
                }

                Hint {
                    text: qsTr("Si lo desactivas, cada lanzamiento abre una ventana nueva. También puedes usar Ctrl+N. Las ventanas adicionales no guardan cambios de configuración.")
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Terminal")
                }

                TextField {
                    id: terminalInput

                    onAccepted: function (value) { App.terminalCommand = value }
                }

                Hint {
                    text: qsTr("Se aplica al confirmar con Enter")
                }

                Separator {}

                SectionLabel {
                    text: qsTr("Dependencias opcionales")
                }

                Hint {
                    text: qsTr("Ooze Files funciona sin ellas, pero estas herramientas añaden miniaturas y vistas previas.")
                }

                Repeater {
                    model: Tools.optional

                    delegate: Row {
                        id: dependencyRow

                        required property var modelData

                        spacing: Theme.pad

                        Text {
                            width: 110
                            anchors.verticalCenter: parent.verticalCenter
                            text: dependencyRow.modelData.label
                            color: Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                        }

                        Text {
                            width: 240
                            anchors.verticalCenter: parent.verticalCenter
                            text: dependencyRow.modelData.found
                                ? qsTr("Instalado")
                                : qsTr("Falta · instala %1").arg(dependencyRow.modelData.package)
                            color: dependencyRow.modelData.found ? Theme.fgMuted : Theme.danger
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width - 350 - 2 * Theme.pad
                            anchors.verticalCenter: parent.verticalCenter
                            text: dependencyRow.modelData.purpose
                            color: Theme.fgMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            PageView {
                id: appsPage

                anchors.fill: parent
                visible: root.tab === 3

                SectionLabel {
                    text: qsTr("Aplicaciones predeterminadas")
                }

                Hint {
                    text: qsTr("Elige qué aplicación abre cada tipo de archivo. La preferencia se guarda en tu mimeapps.list y la comparten el resto de aplicaciones.")
                }

                Repeater {
                    model: root.appStates

                    delegate: Row {
                        id: appRow

                        required property var modelData

                        spacing: Theme.pad

                        Text {
                            width: 200
                            height: Theme.rowHeight - 4
                            verticalAlignment: Text.AlignVCenter
                            text: appRow.modelData.label
                            color: Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            id: appButton

                            readonly property bool usable: appRow.modelData.apps.length > 0

                            width: 240
                            height: Theme.rowHeight - 4
                            radius: Theme.radiusSmall
                            color: appHover.hovered && usable ? Theme.surfaceHover : Theme.surfaceActive
                            opacity: usable ? 1 : 0.6

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: arrow.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: appRow.modelData.appName.length > 0
                                    ? appRow.modelData.appName
                                    : (appButton.usable ? qsTr("Sin predeterminada")
                                                        : qsTr("Sin aplicaciones"))
                                color: Theme.fg
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                elide: Text.ElideRight
                            }

                            Text {
                                id: arrow

                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: "▾"
                                color: Theme.fgMuted
                                font.pixelSize: Theme.fontSize
                            }

                            HoverHandler {
                                id: appHover
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: appButton.usable
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.openChooser(appRow.modelData, appButton)
                            }
                        }
                    }
                }
            }

            ScrollIndicator {
                flickable: root.currentFlick
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 2
            }
        }

        Rectangle {
            id: footer

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight + Theme.pad
            color: Theme.bg

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                width: 96
                height: Theme.rowHeight - 4
                radius: Theme.radiusSmall
                color: closeHover.hovered ? Theme.accent : Theme.surfaceActive

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Cerrar")
                    color: closeHover.hovered ? "#ffffff" : Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }

                HoverHandler {
                    id: closeHover
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: 100
            visible: chooser.visible
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            hoverEnabled: true
            onWheel: function (wheel) { wheel.accepted = true }
            onClicked: chooser.visible = false
        }

        PopupMenu {
            id: chooser

            z: 101
            boundsWidth: surface.width
            boundsHeight: surface.height
            menuWidth: 240
            items: root.chooserItems
            x: Math.max(edgeMargin, Math.min(surface.width - width - edgeMargin, root.chooserX))
            y: Math.max(edgeMargin, Math.min(surface.height - height - edgeMargin, root.chooserY))

            onTriggeredItem: function (item) {
                if (item.appId !== undefined && App.setDefaultApp(root.chooserCategory, item.appId))
                    root.appStates = App.defaultAppStates()
            }
            onClosed: chooser.visible = false
        }
    }
}
