pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Item {
    id: root

    signal activated(int row)
    signal newTabRequested(int row)
    signal contextRequested(int row, real x, real y)
    signal dropRequested(var urls, string targetUrl, real x, real y)

    function resolveDrop(targetUrl, drop, area) {
        var scene = area.mapToItem(null, drop.x, drop.y)
        var action = (drop.supportedActions & Qt.CopyAction) ? Qt.CopyAction : drop.proposedAction
        drop.accept(action)
        root.dropRequested(drop.urls, targetUrl, scene.x, scene.y)
    }

    property alias currentIndex: list.currentIndex
    property alias currentItem: list.currentItem
    property alias count: list.count
    property bool suppressSelection: false
    property var marqueeBaseline: []

    function focusFirst() {
        list.forceActiveFocus()
    }

    function finishMarquee(rows) {
        for (var i = 0; i < rows.length; ++i) {
            if (App.isSelected(rows[i])) {
                root.setCurrentRow(rows[i])
                return
            }
        }
    }

    function cancelMarquee() {
        App.selectRows(root.marqueeBaseline, 0)
    }

    function clearViewSelection() {
        root.suppressSelection = true
        list.currentIndex = -1
        root.suppressSelection = false
        App.clearSelection()
    }

    function setCurrentRow(row) {
        if (row < 0 || row >= list.count)
            return
        root.suppressSelection = true
        list.currentIndex = row
        root.suppressSelection = false
    }

    function itemAtRow(row) {
        return list.itemAtIndex(row)
    }

    function resetView() {
        root.suppressSelection = true
        list.currentIndex = -1
        list.positionViewAtBeginning()
        root.suppressSelection = false
    }

    function selectRow(row) {
        if (row < 0 || row >= list.count)
            return
        root.setCurrentRow(row)
        list.positionViewAtIndex(row, ListView.Contain)
        list.forceActiveFocus()
        App.selectRow(row, false, false)
    }

    function handleKey(event) {
        if (marquee.active && event.key === Qt.Key_Escape) {
            marquee.cancel()
            event.accepted = true
            return
        }

        var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        var shift = (event.modifiers & Qt.ShiftModifier) !== 0

        if (ctrl && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            if (list.currentIndex >= 0)
                root.newTabRequested(list.currentIndex)
            event.accepted = true
        } else if (ctrl && event.key === Qt.Key_A) {
            App.selectAll()
            event.accepted = true
        } else if (ctrl && event.key === Qt.Key_C) {
            App.copySelection()
            event.accepted = true
        } else if (ctrl && event.key === Qt.Key_X) {
            App.cutSelection()
            event.accepted = true
        } else if (ctrl && event.key === Qt.Key_V) {
            App.paste()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            App.clearSelection()
            App.setFilterText("")
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            App.goUp()
            event.accepted = true
        } else if (shift && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
            var delta = event.key === Qt.Key_Up ? -1 : 1
            var target = Math.max(0, Math.min(list.count - 1, list.currentIndex + delta))
            if (list.count > 0) {
                root.setCurrentRow(target)
                App.selectRow(target, ctrl, true)
            }
            event.accepted = true
        }
    }

    Rectangle {
        id: columnHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.pad
        anchors.rightMargin: Theme.pad + 14
        height: 24
        color: "transparent"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Nombre")
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }

        Item {
            id: locationHeader
            visible: App.infoColumns
            anchors.right: modifiedHeader.left
            anchors.rightMargin: 110
            width: visible ? 220 : 0
            height: parent.height

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: App.inTrash ? qsTr("Ubicación original") : qsTr("Ubicación")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
        }

        Text {
            id: sizeHeader
            anchors.right: modifiedHeader.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Tamaño")
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }

        Item {
            id: modifiedHeader
            anchors.right: parent.right
            width: 150
            height: parent.height

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: App.infoColumns ? (App.inTrash ? qsTr("Eliminado") : qsTr("Último uso")) : qsTr("Modificado")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
        }
    }

    ListView {
        id: list
        anchors.top: columnHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.pad * 0.5
        anchors.rightMargin: Theme.pad * 0.5 + 8
        clip: true
        focus: true
        currentIndex: -1
        model: App.files
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: false
        acceptedButtons: Qt.LeftButton

        footer: Item {
            width: list.width
            height: Theme.listRowHeight
        }

        Keys.onReturnPressed: root.activated(list.currentIndex)
        Keys.onEnterPressed: root.activated(list.currentIndex)
        onCountChanged: {
            if (list.currentIndex >= list.count)
                list.currentIndex = -1
        }
        onCurrentIndexChanged: {
            if (!root.suppressSelection && list.currentIndex >= 0)
                App.selectRow(list.currentIndex, false, false)
        }
        Keys.onPressed: function (event) {
            root.handleKey(event)
        }

        delegate: Item {
            id: rowItem

            required property int index
            required property string name
            required property string url
            required property string iconName
            required property bool isDir
            required property bool isSymlink
            required property bool hidden
            required property string sizeText
            required property string modifiedText
            required property string locationText
            required property string activityText

            property string entryName: name
            property bool entryIsDir: isDir
            property bool selected: App.selectionRevision >= 0 && App.isSelected(rowItem.index)
            property bool dragSuppressClick: false
            readonly property int clipMark: (App.hasClipboard, App.clipboardMark(url))
            readonly property real contentOpacity: clipMark === 2 ? Theme.cutOpacity : (hidden ? Theme.hiddenOpacity : 1)

            width: list.width
            height: Theme.listRowHeight

            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
            Drag.hotSpot: Qt.point(Theme.dragHotSpot, Theme.dragHotSpot)
            Drag.onDragFinished: {
                rowItem.dragSuppressClick = true
                rowItem.Drag.imageSource = ""
            }

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                radius: Theme.radiusSmall
                border.width: rowItem.clipMark === 1 ? 1 : 0
                border.color: Theme.accent

                color: (rowDrop.containsDrag && rowItem.isDir)
                    ? Theme.selectionBg
                    : (rowItem.selected
                        ? Theme.selectionBg
                        : (list.currentIndex === rowItem.index
                            ? Theme.surfaceActive
                            : (rowHover.hovered ? Theme.surfaceHover : "transparent")))
            }

            FileIcon {
                id: icon
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                opacity: rowItem.contentOpacity
                icon: rowItem.iconName
                emblem: rowItem.isSymlink ? "link" : ""
                size: Theme.listIconSize
            }

            Text {
                anchors.left: icon.right
                anchors.leftMargin: 10
                anchors.right: locationColumn.left
                anchors.rightMargin: App.infoColumns ? 12 : 0
                anchors.verticalCenter: parent.verticalCenter
                text: rowItem.name
                opacity: rowItem.contentOpacity
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                elide: Text.ElideMiddle
            }

            Item {
                id: locationColumn
                visible: App.infoColumns
                anchors.right: sizeColumn.left
                anchors.rightMargin: 12
                width: visible ? 220 : 0
                height: parent.height

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowItem.locationText
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                    elide: Text.ElideMiddle
                }
            }

            Item {
                id: sizeColumn
                anchors.right: modifiedColumn.left
                anchors.rightMargin: 12
                width: 86
                height: parent.height

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: rowItem.sizeText
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }
            }

            Item {
                id: modifiedColumn
                anchors.right: parent.right
                anchors.rightMargin: 10
                width: 150
                height: parent.height

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: App.infoColumns ? rowItem.activityText : rowItem.modifiedText
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }
            }

            HoverHandler {
                id: rowHover
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                cursorShape: Qt.PointingHandCursor

                onPressed: function (mouse) {
                    rowItem.dragSuppressClick = false
                    if (mouse.button === Qt.RightButton) {
                        var point = rowItem.mapToItem(root, mouse.x, mouse.y)
                        root.contextRequested(rowItem.index, point.x, point.y)
                    }
                }

                onClicked: function (mouse) {
                    if (mouse.button === Qt.MiddleButton) {
                        if (!rowItem.dragSuppressClick)
                            root.newTabRequested(rowItem.index)
                        return
                    }

                    if (mouse.button !== Qt.LeftButton || rowItem.dragSuppressClick)
                        return

                    list.forceActiveFocus()

                    var ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                    var shift = (mouse.modifiers & Qt.ShiftModifier) !== 0

                    root.setCurrentRow(rowItem.index)
                    App.selectRow(rowItem.index, ctrl, shift)

                    if (App.singleClickOpen && !ctrl && !shift)
                        root.activated(rowItem.index)
                }

                onDoubleClicked: function (mouse) {
                    if (mouse.button === Qt.LeftButton)
                        root.activated(rowItem.index)
                }

                DragHandler {
                    target: null

                    onActiveChanged: {
                        if (!active)
                            return

                        var uriList = App.dragUriList(rowItem.index)
                        if (uriList.length === 0)
                            return

                        rowItem.Drag.imageSource = Theme.dragImageUrl(
                            rowItem.name, rowItem.iconName,
                            rowItem.isSymlink ? "link" : "",
                            App.isSelected(rowItem.index) ? App.selectionCount : 1)
                        rowItem.Drag.mimeData = { "text/uri-list": uriList }
                        rowItem.Drag.active = true
                    }
                }
            }

            DropArea {
                id: rowDrop
                anchors.fill: parent

                onEntered: function (drag) {
                    drag.accepted = drag.hasUrls
                }

                onDropped: function (drop) {
                    if (!drop.hasUrls) {
                        drop.accepted = false
                        return
                    }

                    var target = rowItem.isDir ? rowItem.url : App.currentUrl.toString()
                    root.resolveDrop(target, drop, rowDrop)
                }
            }
        }
    }

    MarqueeSelection {
        id: marquee

        view: list
        rowHeight: Theme.listRowHeight

        onStarted: root.marqueeBaseline = App.selectedRows()
        onSelectionRequested: function (rows, mode) { App.selectRows(rows, mode) }
        onFinished: function (rows) { root.finishMarquee(rows) }
        onCancelled: root.cancelMarquee()
        onEmptyClicked: root.clearViewSelection()
    }

    DropArea {
        id: viewDrop
        anchors.fill: list
        z: -1

        onEntered: function (drag) {
            drag.accepted = drag.hasUrls
        }

        onDropped: function (drop) {
            if (!drop.hasUrls) {
                drop.accepted = false
                return
            }

            root.resolveDrop(App.currentUrl.toString(), drop, viewDrop)
        }
    }

    MouseArea {
        anchors.fill: list
        acceptedButtons: Qt.RightButton
        z: -1

        onPressed: function (mouse) {
            root.contextRequested(-1, mouse.x, mouse.y)
        }
    }

    ScrollIndicator {
        anchors.top: columnHeader.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 2
        flickable: list
    }
}
