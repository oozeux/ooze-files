import QtQuick
import Ooze.Files

Item {
    id: root

    property string actionId: ""
    property string title: ""
    property string message: ""
    property string acceptLabel: qsTr("Aceptar")

    signal accepted(string actionId)
    signal cancelled()

    anchors.fill: parent
    visible: false
    focus: visible
    z: 320

    function open(id, titleText, messageText, acceptText) {
        root.actionId = id
        root.title = titleText
        root.message = messageText
        root.acceptLabel = acceptText
        root.visible = true
        root.forceActiveFocus()
    }

    function accept() {
        var id = root.actionId
        root.visible = false
        root.accepted(id)
    }

    function cancel() {
        root.visible = false
        root.cancelled()
    }

    Keys.onEscapePressed: root.cancel()
    Keys.onReturnPressed: root.accept()
    Keys.onEnterPressed: root.accept()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(420, root.width - 4 * Theme.pad)
        height: content.implicitHeight + 2 * Theme.pad
        radius: Theme.radius
        color: Theme.surface
        border.width: Theme.borderWidth
        border.color: Theme.border

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.pad
            spacing: Theme.pad

            Text {
                width: parent.width
                text: root.title
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: root.message
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                wrapMode: Text.Wrap
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                Repeater {
                    model: [
                        { label: qsTr("Cancelar"), primary: false },
                        { label: root.acceptLabel, primary: true }
                    ]

                    delegate: Rectangle {
                        id: button

                        required property var modelData

                        width: Math.max(96, label.implicitWidth + 2 * Theme.pad)
                        height: Theme.rowHeight
                        radius: Theme.radiusSmall
                        color: hover.hovered ? (modelData.primary ? Theme.accent : Theme.surfaceHover)
                                             : (modelData.primary ? Theme.surfaceActive : "transparent")
                        border.width: modelData.primary ? 0 : Theme.borderWidth
                        border.color: Theme.border

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: button.modelData.label
                            color: hover.hovered && button.modelData.primary ? "#ffffff" : Theme.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                        }

                        HoverHandler {
                            id: hover
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: button.modelData.primary ? root.accept() : root.cancel()
                        }
                    }
                }
            }
        }
    }
}
