pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Column {
    id: root

    property var info: ({})

    readonly property var details: {
        var list = []
        var add = function (label, value) {
            if (value !== undefined && value !== null && String(value).length > 0)
                list.push({ label: label, value: String(value) })
        }

        add(qsTr("Tipo"), info.mimeDescription)
        add(qsTr("Tamaño"), info.sizeText)
        add(qsTr("Elementos"), info.itemCountText)
        add(qsTr("Dimensiones"), info.dimensions)
        add(qsTr("Modificado"), info.modifiedText)
        add(qsTr("Creado"), info.createdText)
        add(qsTr("Permisos"), info.permissions)
        add(qsTr("Propietario"), info.owner
            ? (info.group ? info.owner + ":" + info.group : info.owner)
            : "")
        add(qsTr("Enlace a"), info.symlinkTarget)
        add(qsTr("Ubicación"), info.path)
        return list
    }

    spacing: Theme.pad

    Repeater {
        model: root.details

        delegate: Column {
            required property var modelData

            width: root.width
            spacing: 2

            Text {
                text: parent.modelData.label
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }

            Text {
                width: parent.width
                text: parent.modelData.value
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
