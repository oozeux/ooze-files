import QtQuick
import Ooze.Files

Item {
    id: root

    property string title: ""
    property string initialText: ""
    property string acceptLabel: qsTr("Aceptar")

    signal accepted(string text)
    signal cancelled()

    anchors.fill: parent
    visible: false
    z: 300

    function open(titleText, textValue, acceptText) {
        root.title = titleText
        root.initialText = textValue
        root.acceptLabel = acceptText
        input.text = textValue
        root.visible = true
        input.forceActiveFocus()
        input.selectAll()
    }

    function accept() {
        var value = input.text.trim()
        if (value.length === 0)
            return
        root.visible = false
        root.accepted(value)
    }

    function cancel() {
        root.visible = false
        root.cancelled()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.cancel()
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(440, root.width - 4 * Theme.pad)
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
                elide: Text.ElideRight
            }

            Rectangle {
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

                    onAccepted: root.accept()
                    Keys.onEscapePressed: root.cancel()
                }
            }

            Row {
                width: parent.width
                spacing: 8
                layoutDirection: Qt.RightToLeft

                Rectangle {
                    width: 96
                    height: Theme.rowHeight
                    radius: Theme.radiusSmall
                    color: acceptHover.hovered ? Theme.accent : Theme.surfaceActive

                    Text {
                        anchors.centerIn: parent
                        text: root.acceptLabel
                        color: acceptHover.hovered ? "#ffffff" : Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }

                    HoverHandler {
                        id: acceptHover
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.accept()
                    }
                }

                Rectangle {
                    width: 96
                    height: Theme.rowHeight
                    radius: Theme.radiusSmall
                    color: cancelHover.hovered ? Theme.surfaceHover : "transparent"
                    border.width: Theme.borderWidth
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Cancelar")
                        color: Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }

                    HoverHandler {
                        id: cancelHover
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancel()
                    }
                }
            }
        }
    }
}
