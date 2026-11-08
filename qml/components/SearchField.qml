import QtQuick
import Ooze.Files

Item {
    id: root

    property alias text: input.text
    property string placeholder: qsTr("Filtrar")
    property int controlWidth: 200

    signal accepted()
    signal cancelled()

    function focusInput() {
        input.forceActiveFocus()
    }

    implicitWidth: controlWidth
    implicitHeight: Theme.rowHeight
    width: controlWidth
    height: Theme.rowHeight

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surface
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
            anchors.leftMargin: 6
            anchors.right: clearButton.visible ? clearButton.left : parent.right
            anchors.rightMargin: clearButton.visible ? 0 : 10
            anchors.verticalCenter: parent.verticalCenter
            clip: true
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            selectionColor: Theme.accent
            selectedTextColor: "#ffffff"
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true

            onAccepted: root.accepted()
            Keys.onEscapePressed: {
                input.text = ""
                root.cancelled()
            }
        }

        Text {
            anchors.left: searchIcon.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            visible: input.text.length === 0
            text: root.placeholder
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        IconButton {
            id: clearButton
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            visible: input.text.length > 0
            icon: "edit-clear"
            buttonSize: root.height - 8
            onClicked: {
                input.text = ""
                input.forceActiveFocus()
            }
        }
    }
}
