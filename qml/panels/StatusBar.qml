import QtQuick
import QtQuick.Layouts
import Ooze.Files

Rectangle {
    id: status

    property Item activeView: null

    color: Theme.bg

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: Theme.borderWidth
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pad
        anchors.rightMargin: Theme.pad
        spacing: 12

        Text {
            visible: App.selectionCount > 0
            text: App.selectionCount === 1
                ? qsTr("1 seleccionado")
                : qsTr("%1 seleccionados").arg(App.selectionCount)
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }

        Text {
            text: {
                if (App.loading)
                    return qsTr("Cargando…")
                var count = App.itemCount
                if (count === 0)
                    return qsTr("Vacío")
                return count === 1 ? qsTr("1 elemento") : qsTr("%1 elementos").arg(count)
            }
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: statusText.implicitHeight

            Text {
                id: statusText

                property bool errored: false

                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                text: {
                    if (App.statusMessage.length > 0)
                        return App.statusMessage
                    var item = status.activeView ? status.activeView.currentItem : null
                    if (item && item.entryName)
                        return item.entryName
                    return ""
                }
                color: errored ? Theme.danger : (App.statusMessage.length > 0 ? Theme.fg : Theme.fgMuted)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                elide: Text.ElideRight

                Connections {
                    target: App

                    function onStatusMessageChanged() { statusText.errored = false }
                    function onErrorOccurred(message) {
                        statusText.errored = message === App.statusMessage
                    }
                }
            }
        }

        Text {
            text: App.freeSpaceText
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
            visible: text.length > 0
        }
    }
}
