pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Rectangle {
    id: sidebar

    color: Theme.surfaceAlt

    signal dropRequested(var urls, string targetUrl, real x, real y)
    signal contextRequested(string kind, var item, real x, real y)

    function resolveDrop(targetUrl, drop, area) {
        var scene = area.mapToItem(null, drop.x, drop.y)
        var action = (drop.supportedActions & Qt.CopyAction) ? Qt.CopyAction : drop.proposedAction
        drop.accept(action)
        sidebar.dropRequested(drop.urls, targetUrl, scene.x, scene.y)
    }

    readonly property var placeItems: App.places.filter(function (p) { return p.section === "places" })
    readonly property var recentItems: App.places.filter(function (p) { return p.section === "recent" })
    readonly property var systemItems: App.places.filter(function (p) { return p.section === "system" })
    readonly property var deviceItems: App.devices

    property string dragPlaceId: ""
    property int dragInsertIndex: -1
    property real ghostY: 0
    property string ghostName: ""
    property string ghostIcon: ""

    function insertIndexAt(columnY) {
        var count = placesRepeater.count
        for (var i = 0; i < count; ++i) {
            var item = placesRepeater.itemAt(i)
            if (item && columnY < item.y + item.height / 2)
                return i
        }
        return count
    }

    function insertLineY(index) {
        var count = placesRepeater.count
        if (count === 0)
            return 0
        if (index >= count) {
            var last = placesRepeater.itemAt(count - 1)
            return last ? last.y + last.height : 0
        }
        var item = placesRepeater.itemAt(index)
        return item ? item.y : 0
    }

    function beginPlaceDrag(place) {
        sidebar.dragPlaceId = place.id
        sidebar.ghostName = place.name
        sidebar.ghostIcon = place.icon
    }

    function updatePlaceDrag(sceneX, sceneY) {
        var local = sidebar.mapFromItem(null, sceneX, sceneY)
        sidebar.ghostY = local.y - Theme.rowHeight / 2

        if (local.x < 0 || local.x > sidebar.width) {
            sidebar.dragInsertIndex = -1
            return
        }

        var edge = Theme.rowHeight
        if (local.y < flick.y + edge)
            flick.contentY = Math.max(0, flick.contentY - 8)
        else if (local.y > flick.y + flick.height - edge)
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, flick.contentY + 8))

        var inColumn = column.mapFromItem(null, sceneX, sceneY)
        sidebar.dragInsertIndex = sidebar.insertIndexAt(inColumn.y)
    }

    function endPlaceDrag() {
        var id = sidebar.dragPlaceId
        var index = sidebar.dragInsertIndex
        sidebar.dragPlaceId = ""
        sidebar.dragInsertIndex = -1
        if (id !== "" && index >= 0)
            App.movePlace(id, index)
    }

    Rectangle {
        anchors.right: parent.right
        width: Theme.borderWidth
        height: parent.height
        color: Theme.border
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.topMargin: Theme.pad
        clip: true
        contentHeight: column.height
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            width: parent.width
            spacing: 2

            Text {
                height: 26
                leftPadding: Theme.pad + 4
                verticalAlignment: Text.AlignVCenter
                text: qsTr("LUGARES")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                font.letterSpacing: 0.8
            }

            Repeater {
                id: placesRepeater
                model: sidebar.placeItems
                delegate: placeDelegate
            }

            Item {
                width: 1
                height: 8
            }

            Text {
                height: 26
                leftPadding: Theme.pad + 4
                verticalAlignment: Text.AlignVCenter
                text: qsTr("RECIENTES")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                font.letterSpacing: 0.8
            }

            Repeater {
                model: sidebar.recentItems
                delegate: placeDelegate
            }

            Item {
                width: 1
                height: 8
            }

            Text {
                height: 26
                leftPadding: Theme.pad + 4
                verticalAlignment: Text.AlignVCenter
                text: qsTr("SISTEMA")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                font.letterSpacing: 0.8
            }

            Repeater {
                model: sidebar.systemItems
                delegate: placeDelegate
            }

            Item {
                visible: sidebar.deviceItems.length > 0
                width: 1
                height: visible ? 8 : 0
            }

            Text {
                visible: sidebar.deviceItems.length > 0
                height: visible ? 26 : 0
                leftPadding: Theme.pad + 4
                verticalAlignment: Text.AlignVCenter
                text: qsTr("DISPOSITIVOS")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                font.letterSpacing: 0.8
            }

            Repeater {
                model: sidebar.deviceItems
                delegate: deviceDelegate
            }
        }
    }

    Rectangle {
        id: insertLine

        visible: sidebar.dragPlaceId !== "" && sidebar.dragInsertIndex >= 0
        parent: flick.contentItem
        x: Theme.pad * 0.5
        width: sidebar.width - Theme.pad
        height: 2
        radius: 1
        y: sidebar.insertLineY(sidebar.dragInsertIndex) - 1
        color: Theme.accent
        z: 10
    }

    Rectangle {
        id: dragGhost

        visible: sidebar.dragPlaceId !== ""
        x: Theme.pad * 0.5
        y: sidebar.ghostY
        width: sidebar.width - Theme.pad
        height: Theme.rowHeight + 2
        radius: Theme.radiusSmall
        color: Theme.surfaceActive
        opacity: 0.9
        z: 20

        FileIcon {
            id: ghostIconItem
            anchors.left: parent.left
            anchors.leftMargin: Theme.pad * 0.5 + 4
            anchors.verticalCenter: parent.verticalCenter
            icon: sidebar.ghostIcon
            size: Theme.iconSize
        }

        Text {
            anchors.left: ghostIconItem.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: Theme.pad
            anchors.verticalCenter: parent.verticalCenter
            text: sidebar.ghostName
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            elide: Text.ElideRight
        }
    }

    Component {
        id: deviceDelegate

        Item {
            id: deviceRow

            required property var modelData

            readonly property bool mounted: modelData.mounted === true
            readonly property bool isCurrent: mounted
                && (App.currentUrl.toString() === modelData.mountUrl
                    || App.currentUrl.toString().indexOf(modelData.mountUrl + "/") === 0)

            width: column.width
            height: Theme.rowHeight + 2
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.name

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.pad * 0.5
                anchors.rightMargin: Theme.pad * 0.5
                radius: Theme.radiusSmall
                color: deviceDrop.containsDrag || deviceRow.isCurrent
                    ? Theme.selectionBg
                    : (deviceHover.hovered ? Theme.surfaceHover : "transparent")
            }

            FileIcon {
                id: deviceIcon
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad + 4
                anchors.verticalCenter: parent.verticalCenter
                icon: deviceRow.modelData.icon
                size: Theme.iconSize
                opacity: deviceRow.mounted ? 1 : Theme.inactiveOpacity
            }

            Text {
                anchors.left: deviceIcon.right
                anchors.leftMargin: 10
                anchors.right: ejectButton.visible ? ejectButton.left : parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: deviceRow.modelData.name
                font.italic: !deviceRow.mounted
                color: deviceRow.mounted ? Theme.fg : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                elide: Text.ElideRight
            }

            HoverHandler {
                id: deviceHover
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                onClicked: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        var scene = mapToItem(null, mouse.x, mouse.y)
                        sidebar.contextRequested("device", deviceRow.modelData, scene.x, scene.y)
                        return
                    }

                    App.openDevice(deviceRow.modelData.objectPath)
                }
            }

            Item {
                id: ejectButton

                visible: deviceRow.mounted
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad + 2
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: ejectHover.hovered ? Theme.surfaceActive : "transparent"
                }

                FileIcon {
                    anchors.centerIn: parent
                    icon: "media-eject"
                    size: 14
                }

                HoverHandler {
                    id: ejectHover
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (deviceRow.modelData.removable)
                            App.ejectDevice(deviceRow.modelData.objectPath)
                        else
                            App.unmountDevice(deviceRow.modelData.objectPath)
                    }
                }
            }

            DropArea {
                id: deviceDrop
                anchors.fill: parent
                enabled: deviceRow.mounted

                onEntered: function (drag) {
                    drag.accepted = drag.hasUrls
                }

                onDropped: function (drop) {
                    if (!drop.hasUrls) {
                        drop.accepted = false
                        return
                    }

                    sidebar.resolveDrop(deviceRow.modelData.mountUrl, drop, deviceDrop)
                }
            }
        }
    }

    Component {
        id: placeDelegate

        Item {
            id: placeRow

            required property var modelData

            width: column.width
            height: Theme.rowHeight + 2
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.name

            readonly property bool isCurrent: App.currentUrl.toString() === modelData.url
            readonly property bool reorderable: modelData.section === "places"
            readonly property bool beingDragged: sidebar.dragPlaceId !== "" && sidebar.dragPlaceId === modelData.id
            property bool suppressClick: false

            opacity: beingDragged ? 0.4 : 1

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.pad * 0.5
                anchors.rightMargin: Theme.pad * 0.5
                radius: Theme.radiusSmall
                color: placeDrop.containsDrag
                    ? Theme.selectionBg
                    : (placeRow.isCurrent
                        ? Theme.selectionBg
                        : (placeHover.hovered ? Theme.surfaceHover : "transparent"))
            }

            FileIcon {
                id: placeIcon
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad + 4
                anchors.verticalCenter: parent.verticalCenter
                icon: placeRow.modelData.icon
                size: Theme.iconSize
            }

            Text {
                anchors.left: placeIcon.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: placeRow.modelData.name
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                elide: Text.ElideRight
            }

            HoverHandler {
                id: placeHover
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                onPressed: placeRow.suppressClick = false
                onClicked: function (mouse) {
                    if (placeRow.suppressClick)
                        return

                    if (mouse.button === Qt.RightButton) {
                        if (placeRow.reorderable) {
                            var scene = mapToItem(null, mouse.x, mouse.y)
                            sidebar.contextRequested("place", placeRow.modelData, scene.x, scene.y)
                        }
                        return
                    }

                    App.goTo(placeRow.modelData.url)
                }

                DragHandler {
                    id: reorderHandler

                    target: null
                    enabled: placeRow.reorderable
                    acceptedButtons: Qt.LeftButton

                    onActiveChanged: {
                        if (active) {
                            placeRow.suppressClick = true
                            sidebar.beginPlaceDrag(placeRow.modelData)
                            sidebar.updatePlaceDrag(centroid.scenePosition.x, centroid.scenePosition.y)
                        } else {
                            Qt.callLater(sidebar.endPlaceDrag)
                        }
                    }

                    onCentroidChanged: {
                        if (active)
                            sidebar.updatePlaceDrag(centroid.scenePosition.x, centroid.scenePosition.y)
                    }
                }
            }

            DropArea {
                id: placeDrop
                anchors.fill: parent

                onEntered: function (drag) {
                    drag.accepted = drag.hasUrls
                }

                onDropped: function (drop) {
                    if (!drop.hasUrls) {
                        drop.accepted = false
                        return
                    }

                    sidebar.resolveDrop(placeRow.modelData.url, drop, placeDrop)
                }
            }
        }
    }
}
