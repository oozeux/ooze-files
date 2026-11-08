import QtQuick
import Ooze.Files

Item {
    id: root

    property string icon: ""
    property string label: ""
    property bool active: false
    property int buttonSize: Theme.rowHeight

    signal clicked()

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    width: buttonSize
    height: buttonSize
    opacity: enabled ? 1 : 0.35

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: root.clicked()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: root.active
            ? Theme.surfaceActive
            : (hover.hovered && root.enabled ? Theme.surfaceHover : "transparent")
    }

    FileIcon {
        anchors.centerIn: parent
        icon: root.icon
        size: Theme.iconSize
    }

    HoverHandler {
        id: hover
        enabled: root.enabled
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
