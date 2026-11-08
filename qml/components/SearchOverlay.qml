import QtQuick
import Ooze.Files

Item {
    id: root

    readonly property var typeFilters: [
        { id: "all", label: qsTr("Todo") },
        { id: "files", label: qsTr("Archivos") },
        { id: "dirs", label: qsTr("Carpetas") },
        { id: "images", label: qsTr("Imágenes") },
        { id: "videos", label: qsTr("Vídeos") },
        { id: "audio", label: qsTr("Audio") },
        { id: "text", label: qsTr("Texto") },
        { id: "pdf", label: qsTr("PDF") }
    ]

    property string type: "all"

    signal closed()

    anchors.fill: parent
    visible: false
    z: 275

    function open() {
        if (root.visible)
            return

        input.text = ""
        root.type = "all"
        hiddenToggle.checked = App.showHidden
        root.visible = true
        input.forceActiveFocus()
        root.scheduleSearch()
    }

    function close() {
        if (!root.visible)
            return
        root.visible = false
        debounce.stop()
        Search.cancel()
        Search.clear()
        root.closed()
    }

    function scheduleSearch() {
        debounce.stop()
        debounce.restart()
    }

    function runSearch() {
        if (!root.visible)
            return

        var query = input.text.trim()
        if (query.length === 0 && root.type === "all") {
            Search.clear()
            return
        }

        Search.start("" + App.currentUrl, query, root.type, hiddenToggle.checked)
    }

    function openResult(row) {
        if (row < 0 || row >= Search.count)
            return

        var url = Search.urlAt(row)
        var isDir = Search.isDirAt(row)
        root.close()
        App.openSearchResult(url, isDir)
    }

    Keys.onEscapePressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        onClicked: root.close()
        onWheel: function (wheel) { wheel.accepted = true }
    }

    Rectangle {
        id: card

        anchors.centerIn: parent
        width: Math.min(940, root.width - 4 * Theme.pad)
        height: Math.min(620, root.height - 4 * Theme.pad)
        radius: Theme.radius
        color: Theme.surface
        border.width: Theme.borderWidth
        border.color: Theme.border
        clip: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            hoverEnabled: true
            onWheel: function (wheel) { wheel.accepted = true }
        }

        Item {
            id: titleBar

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight + Theme.pad

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Buscar en subcarpetas")
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
            }

            Text {
                anchors.right: closeButton.left
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: Search.searching
                    ? qsTr("Buscando…")
                    : qsTr("%1 resultados").arg(Search.count)
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }

            IconButton {
                id: closeButton

                anchors.right: parent.right
                anchors.rightMargin: Theme.pad / 2
                anchors.verticalCenter: parent.verticalCenter
                icon: "window-close"
                label: qsTr("Cerrar")
                onClicked: root.close()
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }
        }

        Column {
            id: searchBar

            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.pad
            spacing: Theme.pad

            Rectangle {
                width: parent.width
                height: Theme.rowHeight
                radius: Theme.radiusSmall
                color: Theme.bg
                border.width: Theme.borderWidth
                border.color: input.activeFocus ? Theme.accent : Theme.border

                FileIcon {
                    id: searchIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "edit-find"
                    size: 14
                    opacity: 0.7
                }

                TextInput {
                    id: input

                    anchors.left: searchIcon.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    color: Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    selectionColor: Theme.accent
                    selectedTextColor: "#ffffff"
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true

                    onTextChanged: root.scheduleSearch()
                    onAccepted: {
                        debounce.stop()
                        root.runSearch()
                        results.forceActiveFocus()
                    }
                    Keys.onDownPressed: {
                        if (results.count > 0) {
                            results.currentIndex = 0
                            results.forceActiveFocus()
                        }
                    }
                    Keys.onEscapePressed: root.close()
                }

                Text {
                    anchors.left: searchIcon.right
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: input.text.length === 0
                    text: qsTr("Nombre a buscar…")
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }
            }

            Row {
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.typeFilters

                    delegate: Rectangle {
                        id: chip

                        required property var modelData

                        width: chipLabel.width + 16
                        height: Theme.rowHeight - 8
                        radius: height / 2
                        color: root.type === chip.modelData.id
                            ? Theme.accent
                            : (chipHover.hovered ? Theme.surfaceHover : Theme.surfaceActive)

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.modelData.label
                            color: root.type === chip.modelData.id ? "#ffffff" : Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                        }

                        HoverHandler {
                            id: chipHover
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.type = chip.modelData.id
                                root.runSearch()
                            }
                        }
                    }
                }

                Item { width: 1; height: 1 }

                Rectangle {
                    width: hiddenLabel.width + 28
                    height: Theme.rowHeight - 8
                    radius: height / 2
                    color: hiddenToggle.checked ? Theme.surfaceActive : "transparent"
                    border.width: Theme.borderWidth
                    border.color: hiddenToggle.checked ? Theme.accent : Theme.border

                    property bool checked: false
                    id: hiddenToggle

                    Text {
                        id: hiddenLabel
                        anchors.centerIn: parent
                        text: qsTr("Ocultos")
                        color: Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            hiddenToggle.checked = !hiddenToggle.checked
                            root.runSearch()
                        }
                    }
                }
            }
        }

        ListView {
            id: results

            anchors.top: searchBar.bottom
            anchors.topMargin: Theme.pad
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.leftMargin: Theme.pad / 2
            anchors.rightMargin: Theme.pad / 2
            clip: true
            model: Search
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: true

            Keys.onReturnPressed: root.openResult(currentIndex)
            Keys.onEnterPressed: root.openResult(currentIndex)
            Keys.onEscapePressed: root.close()

            delegate: Rectangle {
                id: rowItem

                required property int index
                required property string name
                required property string url
                required property string relativePath
                required property bool isDir
                required property string iconName
                required property string sizeText

                width: results.width
                height: Theme.rowHeight
                radius: Theme.radiusSmall
                color: results.currentIndex === rowItem.index
                    ? Theme.selectionBg
                    : (rowHover.hovered ? Theme.surfaceHover : "transparent")

                FileIcon {
                    id: rowIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    icon: rowItem.iconName
                    size: Theme.listIconSize
                }

                Column {
                    anchors.left: rowIcon.right
                    anchors.leftMargin: 8
                    anchors.right: sizeLabel.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        width: parent.width
                        text: rowItem.name
                        color: Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: rowItem.relativePath
                        color: Theme.fgMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 2
                        elide: Text.ElideMiddle
                    }
                }

                Text {
                    id: sizeLabel
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowItem.sizeText
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }

                HoverHandler {
                    id: rowHover
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        results.currentIndex = rowItem.index
                        root.openResult(rowItem.index)
                    }
                }

                Keys.onReturnPressed: root.openResult(rowItem.index)
                Keys.onEnterPressed: root.openResult(rowItem.index)
                Keys.onEscapePressed: root.close()
            }

            onCurrentIndexChanged: {
                if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, ListView.Contain)
            }
        }

        ScrollIndicator {
            flickable: results
            anchors.right: results.right
            anchors.top: results.top
            anchors.bottom: results.bottom
            anchors.margins: 2
        }

        Text {
            anchors.centerIn: results
            width: results.width - 2 * Theme.pad
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: !Search.searching && Search.count === 0
                && (input.text.trim().length > 0 || root.type !== "all")
            text: qsTr("Sin coincidencias")
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Text {
            anchors.centerIn: results
            width: results.width - 2 * Theme.pad
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: !Search.searching && Search.count === 0
                && input.text.trim().length === 0 && root.type === "all"
            text: qsTr("Escribe un nombre o elige un tipo para buscar en esta carpeta y sus subcarpetas")
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Item {
            id: footer

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight - 6

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: Search.truncated
                    ? qsTr("Mostrando los primeros 2000 resultados · Esc cerrar")
                    : qsTr("Intro abrir · Esc cerrar")
                color: Search.truncated ? Theme.danger : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
                elide: Text.ElideRight
            }
        }
    }

    Timer {
        id: debounce

        interval: 280
        onTriggered: root.runSearch()
    }

    Connections {
        target: Search

        function onSearchingChanged() {
            if (!Search.searching && results.count > 0 && !input.activeFocus)
                results.forceActiveFocus()
        }
    }
}
