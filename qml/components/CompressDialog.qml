import QtQuick
import Ooze.Files

Item {
    id: root

    property var formats: []
    property string format: ""

    readonly property var target: App.archiveTarget(input.text, root.format)
    readonly property bool valid: root.target.fileName.length > 0

    signal accepted(string name, string formatId)
    signal cancelled()

    anchors.fill: parent
    visible: false
    z: 300

    function open(defaultName) {
        root.formats = App.compressFormats()

        var known = false
        for (var i = 0; i < root.formats.length; ++i) {
            if (root.formats[i].id === root.format)
                known = true
        }
        if (!known)
            root.format = root.formats.length > 0 ? root.formats[0].id : ""

        input.text = defaultName
        root.visible = true
        input.forceActiveFocus()
        input.selectAll()
    }

    function accept() {
        if (!root.valid)
            return
        root.visible = false
        root.accepted(input.text.trim(), root.format)
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
        width: Math.min(460, root.width - 4 * Theme.pad)
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
                text: qsTr("Comprimir")
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                text: qsTr("Nombre")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
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

            Text {
                text: qsTr("Formato")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }

            Flow {
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.formats

                    delegate: Rectangle {
                        id: chip

                        required property var modelData

                        readonly property bool selected: root.format === chip.modelData.id

                        width: chipLabel.implicitWidth + 20
                        height: Theme.rowHeight - 6
                        radius: Theme.radiusSmall
                        color: chipHover.hovered
                            ? Theme.surfaceHover
                            : (chip.selected ? Theme.surfaceActive : "transparent")
                        border.width: Theme.borderWidth
                        border.color: chip.selected ? Theme.accent : Theme.border

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.modelData.suffix
                            color: chip.selected ? Theme.fg : Theme.fgMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                        }

                        HoverHandler {
                            id: chipHover
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.format = chip.modelData.id
                                input.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: root.target.error.length > 0
                    ? root.target.error
                    : (root.target.exists
                        ? qsTr("Ya existe un archivo con ese nombre; se creará «%1»").arg(root.target.fileName)
                        : qsTr("Se creará «%1»").arg(root.target.fileName))
                color: root.target.error.length > 0 ? Theme.danger : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            Row {
                width: parent.width
                spacing: 8
                layoutDirection: Qt.RightToLeft

                Rectangle {
                    width: 110
                    height: Theme.rowHeight
                    radius: Theme.radiusSmall
                    opacity: root.valid ? 1.0 : 0.5
                    color: root.valid && acceptHover.hovered ? Theme.accent : Theme.surfaceActive

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Comprimir")
                        color: root.valid && acceptHover.hovered ? "#ffffff" : Theme.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }

                    HoverHandler {
                        id: acceptHover
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: root.valid ? Qt.PointingHandCursor : Qt.ArrowCursor
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
