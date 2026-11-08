pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Ooze.Files

Rectangle {
    id: header

    property bool pathEditing: false
    property var crumbs: {
        var all = App.breadcrumb
        if (all.length <= 4)
            return all
        var out = [all[0], { name: "…", url: "" }]
        for (var i = all.length - 3; i < all.length; ++i)
            out.push(all[i])
        return out
    }

    signal sortRequested()
    signal menuRequested()
    signal focusContentRequested()

    color: Theme.bg

    function beginPathEdit() {
        pathEditing = true
        pathInput.text = App.currentPath
        pathInput.forceActiveFocus()
        pathInput.selectAll()
    }

    function endPathEdit() {
        pathEditing = false
        focusContentRequested()
    }

    function focusSearch() {
        search.focusInput()
    }

    Connections {
        target: App
        function onFilterTextChanged() {
            if (search.text !== App.filterText)
                search.text = App.filterText
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: Theme.borderWidth
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pad
        anchors.rightMargin: Theme.pad
        spacing: 2

        IconButton {
            icon: "go-previous"
            label: qsTr("Atrás")
            enabled: App.canGoBack
            onClicked: App.goBack()
        }

        IconButton {
            icon: "go-next"
            label: qsTr("Adelante")
            enabled: App.canGoForward
            onClicked: App.goForward()
        }

        IconButton {
            icon: "go-up"
            label: qsTr("Subir de carpeta")
            onClicked: App.goUp()
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height
            clip: true

            Row {
                id: crumbRow
                z: 1
                visible: !header.pathEditing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Repeater {
                    id: crumbRepeater
                    model: header.crumbs

                    delegate: Row {
                        id: crumbDelegate
                        required property int index
                        required property var modelData

                        spacing: 0

                        Item {
                            width: crumbLabel.implicitWidth + 14
                            height: Theme.rowHeight

                            Rectangle {
                                anchors.fill: parent
                                anchors.verticalCenter: parent.verticalCenter
                                radius: Theme.radiusSmall
                                color: crumbMouse.hovered && modelData.url.length > 0
                                    ? Theme.surfaceHover
                                    : "transparent"
                            }

                            Text {
                                id: crumbLabel
                                anchors.centerIn: parent
                                text: crumbDelegate.modelData.name
                                color: Theme.fg
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                            }

                            HoverHandler {
                                id: crumbMouse
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: modelData.url.length > 0
                                    ? Qt.PointingHandCursor
                                    : Qt.ArrowCursor
                                onClicked: {
                                    if (crumbDelegate.modelData.url.length > 0)
                                        App.goTo(crumbDelegate.modelData.url)
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: index < crumbRepeater.count - 1
                            text: "›"
                            color: Theme.fgMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                        }
                    }
                }
            }

            TextInput {
                id: pathInput
                visible: header.pathEditing
                anchors.fill: parent
                anchors.rightMargin: 8
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                selectionColor: Theme.accent
                selectedTextColor: "#ffffff"

                onAccepted: {
                    var target = text
                    header.endPathEdit()
                    App.goTo(target)
                }

                Keys.onEscapePressed: header.endPathEdit()
            }

            MouseArea {
                anchors.fill: parent
                enabled: !header.pathEditing
                onClicked: header.beginPathEdit()
            }
        }

        SearchField {
            id: search
            Layout.preferredWidth: 210
            Layout.alignment: Qt.AlignVCenter
            placeholder: qsTr("Filtrar en esta carpeta")
            onTextChanged: {
                if (search.text !== App.filterText)
                    App.setFilterText(search.text)
            }
            onAccepted: header.focusContentRequested()
            onCancelled: header.focusContentRequested()
        }

        IconButton {
            icon: App.viewMode === "list" ? "view-grid" : "view-list"
            label: App.viewMode === "list" ? qsTr("Ver como cuadrícula") : qsTr("Ver como lista")
            onClicked: App.setViewMode(App.viewMode === "list" ? "grid" : "list")
        }

        IconButton {
            icon: App.sortDescending ? "view-sort-descending" : "view-sort-ascending"
            label: qsTr("Ordenar por")
            active: false
            onClicked: header.sortRequested()
        }

        IconButton {
            icon: "open-menu-symbolic"
            label: qsTr("Menú")
            onClicked: header.menuRequested()
        }
    }
}
