import QtQuick
import Ooze.Files

Item {
    id: root

    property var info: ({})
    property int count: 1

    readonly property string kind: info.kind !== undefined ? info.kind : "none"
    readonly property string iconName: kind === "directory"
        ? "folder"
        : (info.mime ? String(info.mime).replace("/", "-") : "text-x-generic")

    signal closed()

    anchors.fill: parent
    visible: false
    z: 285

    function open(row) {
        root.count = App.selectionCount
        root.info = row >= 0 ? App.previewRowWithSize(row)
                             : App.previewCurrentFolderWithSize()
        root.visible = true
        root.forceActiveFocus()
    }

    function close() {
        root.visible = false
        root.info = ({})
        root.closed()
    }

    Keys.onEscapePressed: root.close()
    Keys.onReturnPressed: root.close()
    Keys.onEnterPressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
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
        width: Math.min(440, root.width - 4 * Theme.pad)
        height: Math.min(header.height + scroll.contentHeight + footer.height + 3 * Theme.pad,
                         root.height - 4 * Theme.pad)
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

        Row {
            id: header

            x: Theme.pad
            y: Theme.pad
            width: parent.width - 2 * Theme.pad
            spacing: Theme.pad

            FileIcon {
                icon: root.iconName
                size: 48
            }

            Text {
                width: parent.width - 48 - Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.info.name !== undefined ? root.info.name : ""
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 2
                font.bold: true
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2
                elide: Text.ElideMiddle
            }
        }

        Flickable {
            id: scroll

            anchors.top: header.bottom
            anchors.topMargin: Theme.pad
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.pad
            anchors.rightMargin: Theme.pad
            contentWidth: width
            contentHeight: list.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            DetailsList {
                id: list

                width: scroll.width
                info: root.info
            }
        }

        Item {
            id: footer

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight + Theme.pad

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
    }
}
