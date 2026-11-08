pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Rectangle {
    id: root

    property var parentMenu: null
    property Component submenuComponent: null
    property var items: parentMenu ? parentMenu.openSubItems : []
    property int menuWidth: parentMenu ? 260 : 236
    property real boundsWidth: parentMenu ? parentMenu.boundsWidth : 0
    property real boundsHeight: parentMenu ? parentMenu.boundsHeight : 0
    property int edgeMargin: parentMenu ? parentMenu.edgeMargin : 8
    property int openSubIndex: -1
    property real subRowY: 0

    readonly property real windowX: (parentMenu ? parentMenu.windowX : 0) + x
    readonly property real windowY: (parentMenu ? parentMenu.windowY : 0) + y
    readonly property var openSubItems: openSubIndex >= 0 && openSubIndex < items.length
        && items[openSubIndex] && items[openSubIndex].submenu
        ? items[openSubIndex].submenu
        : []
    readonly property int padding: Math.round(Theme.pad * 0.6)
    readonly property real fullHeight: column.implicitHeight + 2 * padding
    readonly property real maxWidth: boundsWidth > 0 ? Math.max(120, boundsWidth - 2 * edgeMargin) : menuWidth
    readonly property real maxHeight: boundsHeight > 0 ? Math.max(Theme.rowHeight + 2 * padding, boundsHeight - 2 * edgeMargin) : fullHeight
    readonly property bool scrollable: fullHeight > height

    signal triggered(int index)
    signal triggeredItem(var item)
    signal closed()

    width: Math.min(menuWidth, maxWidth)
    height: Math.min(fullHeight, maxHeight)
    radius: Theme.radius
    color: Theme.surface
    border.width: Theme.borderWidth
    border.color: Theme.border
    visible: parentMenu !== null

    x: {
        if (!parentMenu)
            return 0
        var right = parentMenu.windowX + parentMenu.width - 4
        var left = parentMenu.windowX - width + 4
        var target = (boundsWidth > 0 && right + width > boundsWidth - edgeMargin) ? left : right
        return Math.max(edgeMargin, target) - parentMenu.windowX
    }

    y: {
        if (!parentMenu)
            return 0
        var top = parentMenu.windowY + parentMenu.subRowY - parentMenu.padding
        var limit = boundsHeight > 0 ? boundsHeight - height - edgeMargin : top
        return Math.max(edgeMargin, Math.min(limit, top)) - parentMenu.windowY
    }

    function hoverRow(row) {
        if (row.hasSubmenu && row.modelData.enabled !== false) {
            root.subRowY = row.mapToItem(root, 0, 0).y
            root.openSubIndex = row.index
        } else {
            root.openSubIndex = -1
        }
    }

    onVisibleChanged: {
        if (visible)
            flick.contentY = 0
        else
            root.openSubIndex = -1
    }

    onItemsChanged: {
        flick.contentY = 0
        root.openSubIndex = -1
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        onWheel: function (wheel) { wheel.accepted = true }
    }

    Flickable {
        id: flick

        anchors.fill: parent
        anchors.margins: root.padding
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: root.scrollable
        interactive: root.scrollable
        boundsBehavior: Flickable.StopAtBounds

        onMovementStarted: root.openSubIndex = -1

        Column {
            id: column
            width: flick.width
            spacing: 1

            Repeater {
                model: root.items

                delegate: Item {
                    id: row
                    required property int index
                    required property var modelData

                    readonly property bool hasSubmenu: modelData !== undefined && modelData !== null
                        && modelData.submenu !== undefined && modelData.submenu.length > 0

                    width: column.width
                    height: modelData && modelData.separator ? 9 : Theme.rowHeight
                    visible: modelData !== undefined && modelData !== null

                    Rectangle {
                        visible: row.modelData !== undefined && row.modelData !== null
                            && row.modelData.separator === true
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 1
                        color: Theme.border
                    }

                    Rectangle {
                        id: background
                        visible: !(row.modelData && row.modelData.separator)
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: (rowMouse.hovered || (row.hasSubmenu && root.openSubIndex === row.index))
                               && row.modelData.enabled !== false
                            ? Theme.surfaceHover
                            : "transparent"
                    }

                    FileIcon {
                        id: check
                        visible: row.modelData && row.modelData.checked === true
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        icon: "object-select"
                        size: 14
                    }

                    Text {
                        anchors.left: check.visible ? check.right : parent.left
                        anchors.leftMargin: check.visible ? 6 : 10
                        anchors.right: shortcut.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !(row.modelData && row.modelData.separator)
                        text: row.modelData ? (row.modelData.label || "") : ""
                        color: row.modelData && row.modelData.enabled === false
                            ? Theme.fgMuted
                            : Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        elide: Text.ElideRight
                    }

                    Text {
                        id: shortcut
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.hasSubmenu ? "›"
                            : (row.modelData && row.modelData.hint ? row.modelData.hint : "")
                        color: Theme.fgMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: row.hasSubmenu ? Theme.fontSize + 3 : Theme.fontSize - 1
                    }

                    HoverHandler {
                        id: rowMouse
                        enabled: !(row.modelData && row.modelData.separator)
                        onHoveredChanged: {
                            if (hovered)
                                root.hoverRow(row)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: row.modelData !== undefined
                                 && row.modelData !== null
                                 && !row.modelData.separator
                                 && row.modelData.enabled !== false
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (row.hasSubmenu) {
                                root.hoverRow(row)
                                return
                            }
                            root.triggered(row.index)
                            root.triggeredItem(row.modelData)
                            root.closed()
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: subLoader
        active: root.submenuComponent !== null && root.openSubIndex >= 0
                && root.openSubItems.length > 0
        sourceComponent: root.submenuComponent
        z: 2

        onLoaded: {
            item.parentMenu = root
            item.triggeredItem.connect(root.triggeredItem)
            item.closed.connect(root.closed)
        }
    }

    ScrollIndicator {
        flickable: flick
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.padding
        anchors.bottomMargin: root.padding
        anchors.rightMargin: 1
        visible: root.scrollable
    }
}
