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

    property alias currentIndex: grid.currentIndex
    property alias currentItem: grid.currentItem
    property alias count: grid.count
    property bool suppressSelection: false
    property var marqueeBaseline: []

    function focusFirst() {
        grid.forceActiveFocus()
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
        grid.currentIndex = -1
        root.suppressSelection = false
        App.clearSelection()
    }

    function setCurrentRow(row) {
        if (row < 0 || row >= grid.count)
            return
        root.suppressSelection = true
        grid.currentIndex = row
        root.suppressSelection = false
    }

    function itemAtRow(row) {
        return grid.itemAtIndex(row)
    }

    function resetView() {
        root.suppressSelection = true
        grid.currentIndex = -1
        grid.positionViewAtBeginning()
        root.suppressSelection = false
    }

    function selectRow(row) {
        if (row < 0 || row >= grid.count)
            return
        root.setCurrentRow(row)
        grid.positionViewAtIndex(row, GridView.Contain)
        grid.forceActiveFocus()
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
            if (grid.currentIndex >= 0)
                root.newTabRequested(grid.currentIndex)
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
        } else if (shift && (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                             || event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
            var columns = Math.max(1, Math.floor(grid.width / grid.cellWidth))
            var delta = event.key === Qt.Key_Left ? -1
                      : event.key === Qt.Key_Right ? 1
                      : event.key === Qt.Key_Up ? -columns
                      : columns
            var target = Math.max(0, Math.min(grid.count - 1, grid.currentIndex + delta))
            if (grid.count > 0) {
                root.setCurrentRow(target)
                App.selectRow(target, ctrl, true)
            }
            event.accepted = true
        }
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: Theme.pad
        anchors.rightMargin: Theme.pad + 8
        clip: true
        focus: true
        currentIndex: -1
        model: App.files

        readonly property int columns: Math.max(1, Math.floor(width / Theme.gridCellWidth))
        cellWidth: width > 0 ? Math.floor(width / columns) : Theme.gridCellWidth
        cellHeight: Theme.gridCellHeight
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: false
        cacheBuffer: 400
        acceptedButtons: Qt.LeftButton

        Keys.onReturnPressed: root.activated(grid.currentIndex)
        Keys.onEnterPressed: root.activated(grid.currentIndex)
        onCountChanged: {
            if (grid.currentIndex >= grid.count)
                grid.currentIndex = -1
        }
        onCurrentIndexChanged: {
            if (!root.suppressSelection && grid.currentIndex >= 0)
                App.selectRow(grid.currentIndex, false, false)
        }
        Keys.onPressed: function (event) {
            root.handleKey(event)
        }

        delegate: Item {
            id: cell

            required property int index
            required property string name
            required property string url
            required property string iconName
            required property bool isDir
            required property bool isSymlink
            required property bool hidden
            required property string mimeType
            required property var size

            property string entryName: name
            property bool entryIsDir: isDir
            property bool selected: App.selectionRevision >= 0 && App.isSelected(cell.index)
            property bool dragSuppressClick: false
            readonly property int clipMark: (App.hasClipboard, App.clipboardMark(url))
            readonly property real contentOpacity: clipMark === 2 ? Theme.cutOpacity : (hidden ? Theme.hiddenOpacity : 1)

            width: grid.cellWidth
            height: grid.cellHeight

            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
            Drag.hotSpot: Qt.point(Theme.dragHotSpot, Theme.dragHotSpot)
            Drag.onDragFinished: {
                cell.dragSuppressClick = true
                cell.Drag.imageSource = ""
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: Theme.radiusSmall
                border.width: cell.clipMark === 1 ? 1 : 0
                border.color: Theme.accent

                color: (cellDrop.containsDrag && cell.isDir)
                    ? Theme.selectionBg
                    : (cell.selected
                        ? Theme.selectionBg
                        : (grid.currentIndex === cell.index
                            ? Theme.surfaceActive
                            : (cellHover.hovered ? Theme.surfaceHover : "transparent")))
            }

            FileIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 12
                visible: thumbnail.status !== Image.Ready
                opacity: cell.contentOpacity
                icon: cell.iconName
                emblem: cell.isSymlink ? "link" : ""
                size: Theme.gridIconSize
            }

            Image {
                id: thumbnail

                readonly property bool isImage: cell.mimeType.indexOf("image/") === 0
                readonly property bool isVideo: cell.mimeType.indexOf("video/") === 0
                readonly property bool wanted: App.thumbnails && !cell.isDir
                    && (isImage || isVideo)
                    && cell.size > 0
                    && cell.size <= (isVideo ? 4 * 1024 * 1024 * 1024
                                             : 30 * 1024 * 1024)

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 12
                width: Theme.gridIconSize
                height: Theme.gridIconSize
                visible: status === Image.Ready
                opacity: cell.contentOpacity
                source: !wanted
                    ? ""
                    : (isVideo
                        ? "image://thumbnails/" + encodeURIComponent(cell.url)
                        : cell.url)
                sourceSize.width: Theme.gridIconSize * 2
                sourceSize.height: Theme.gridIconSize * 2
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                smooth: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Theme.gridTextTop
                width: grid.cellWidth - 18
                text: cell.name
                opacity: cell.contentOpacity
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.NoWrap
                maximumLineCount: 1
                elide: Text.ElideMiddle
            }

            HoverHandler {
                id: cellHover
            }

            MouseArea {
                id: cellMouse
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                cursorShape: Qt.PointingHandCursor

                onPressed: function (mouse) {
                    cell.dragSuppressClick = false
                    if (mouse.button === Qt.RightButton) {
                        var point = cell.mapToItem(root, mouse.x, mouse.y)
                        root.contextRequested(cell.index, point.x, point.y)
                    }
                }

                onClicked: function (mouse) {
                    if (mouse.button === Qt.MiddleButton) {
                        if (!cell.dragSuppressClick)
                            root.newTabRequested(cell.index)
                        return
                    }

                    if (mouse.button !== Qt.LeftButton || cell.dragSuppressClick)
                        return

                    grid.forceActiveFocus()

                    var ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                    var shift = (mouse.modifiers & Qt.ShiftModifier) !== 0

                    root.setCurrentRow(cell.index)
                    App.selectRow(cell.index, ctrl, shift)

                    if (App.singleClickOpen && !ctrl && !shift)
                        root.activated(cell.index)
                }

                onDoubleClicked: function (mouse) {
                    if (mouse.button === Qt.LeftButton)
                        root.activated(cell.index)
                }

                DragHandler {
                    target: null

                    onActiveChanged: {
                        if (!active)
                            return

                        var uriList = App.dragUriList(cell.index)
                        if (uriList.length === 0)
                            return

                        cell.Drag.imageSource = Theme.dragImageUrl(
                            cell.name, cell.iconName, cell.isSymlink ? "link" : "",
                            App.isSelected(cell.index) ? App.selectionCount : 1)
                        cell.Drag.mimeData = { "text/uri-list": uriList }
                        cell.Drag.active = true
                    }
                }
            }

            DropArea {
                id: cellDrop
                anchors.fill: parent

                onEntered: function (drag) {
                    drag.accepted = drag.hasUrls
                }

                onDropped: function (drop) {
                    if (!drop.hasUrls) {
                        drop.accepted = false
                        return
                    }

                    var target = cell.isDir ? cell.url : App.currentUrl.toString()
                    root.resolveDrop(target, drop, cellDrop)
                }
            }
        }
    }

    MarqueeSelection {
        id: marquee

        view: grid
        gridMode: true
        cellWidth: grid.cellWidth
        cellHeight: grid.cellHeight

        onStarted: root.marqueeBaseline = App.selectedRows()
        onSelectionRequested: function (rows, mode) { App.selectRows(rows, mode) }
        onFinished: function (rows) { root.finishMarquee(rows) }
        onCancelled: root.cancelMarquee()
        onEmptyClicked: root.clearViewSelection()
    }

    DropArea {
        id: viewDrop
        anchors.fill: grid
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
        anchors.fill: grid
        acceptedButtons: Qt.RightButton
        z: -1

        onPressed: function (mouse) {
            root.contextRequested(-1, mouse.x, mouse.y)
        }
    }

    ScrollIndicator {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.pad
        anchors.bottomMargin: Theme.pad
        anchors.rightMargin: 2
        flickable: grid
    }
}
