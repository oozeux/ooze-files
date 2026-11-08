import QtQuick
import Ooze.Files

Item {
    id: root

    property var info: ({})
    property bool applyToAll: false

    readonly property bool isMissing: info.kind === "missingFolder"
    readonly property bool isDirectory: info.targetIsDirectory === true
    readonly property bool mismatch: info.sourceIsDirectory !== info.targetIsDirectory

    signal decided(int id, int decision, bool applyToAll)

    anchors.fill: parent
    visible: false
    focus: visible
    z: 310

    function open(details) {
        root.info = details
        root.applyToAll = false
        root.visible = true
        root.forceActiveFocus()
    }

    function decide(decision) {
        var id = root.info.id
        var all = root.applyToAll && decision !== 3
        root.visible = false
        root.decided(id, decision, all)
    }

    function describe(isDirectory, sizeText, modifiedText) {
        var parts = []
        if (!isDirectory && sizeText.length > 0)
            parts.push(sizeText)
        if (modifiedText.length > 0)
            parts.push(modifiedText)
        return parts.join("  ·  ")
    }

    Keys.onEscapePressed: root.decide(3)
    Keys.onReturnPressed: root.decide(root.isMissing ? 4 : 0)
    Keys.onEnterPressed: root.decide(root.isMissing ? 4 : 0)

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: card

        anchors.centerIn: parent
        width: Math.min(480, root.width - 4 * Theme.pad)
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
                text: root.isMissing
                    ? qsTr("La carpeta original de «%1» ya no existe").arg(root.info.name)
                    : root.mismatch
                    ? qsTr("Ya existe un elemento de otro tipo con el nombre «%1»").arg(root.info.name)
                    : root.isDirectory
                        ? qsTr("La carpeta «%1» ya existe").arg(root.info.name)
                        : qsTr("El archivo «%1» ya existe").arg(root.info.name)
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: root.isMissing ? qsTr("Se restauraba en «%1»").arg(root.info.folder)
                                     : qsTr("En «%1»").arg(root.info.folder)
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                elide: Text.ElideMiddle
            }

            Column {
                width: parent.width
                spacing: 2
                visible: !root.isMissing

                Text {
                    width: parent.width
                    text: qsTr("Existente: %1").arg(root.describe(root.info.targetIsDirectory,
                                                                  root.info.targetSizeText || "",
                                                                  root.info.targetModifiedText || ""))
                    color: Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: qsTr("Nuevo: %1").arg(root.describe(root.info.sourceIsDirectory,
                                                              root.info.sourceSizeText || "",
                                                              root.info.sourceModifiedText || ""))
                    color: Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    elide: Text.ElideRight
                }
            }

            Text {
                width: parent.width
                visible: !root.isMissing && root.info.canOverwrite === true
                text: root.info.canMerge === true
                    ? qsTr("Combinar mantiene los archivos que solo existen en la carpeta de destino y solo reemplaza los que se llaman igual. Reemplazar la carpeta entera borra el contenido anterior (se envía a la papelera si es posible).")
                    : qsTr("Sobrescribir reemplaza el archivo; el anterior se envía a la papelera si es posible.")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                visible: !root.isMissing && root.mismatch
                text: qsTr("Un archivo y una carpeta no se pueden combinar ni reemplazar entre sí.")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                wrapMode: Text.Wrap
            }

            Row {
                visible: root.info.multiple === true
                spacing: 8

                Rectangle {
                    width: 18
                    height: 18
                    radius: 4
                    color: root.applyToAll ? Theme.accent : "transparent"
                    border.width: Theme.borderWidth
                    border.color: root.applyToAll ? Theme.accent : Theme.border

                    Text {
                        anchors.centerIn: parent
                        visible: root.applyToAll
                        text: "✓"
                        color: "#ffffff"
                        font.pixelSize: Theme.fontSize - 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.applyToAll = !root.applyToAll
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Aplicar a todos los conflictos de este tipo")
                    color: Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.applyToAll = !root.applyToAll
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.isMissing ? [
                        { decision: 4, label: qsTr("Recrear la carpeta"), primary: true, shown: true },
                        { decision: 5, label: qsTr("Restaurar en Inicio"), primary: false, shown: true },
                        { decision: 3, label: qsTr("Cancelar"), primary: false, shown: true }
                    ] : [
                        { decision: 0, label: qsTr("Pegar con otro nombre"), primary: true, shown: true },
                        { decision: 6, label: qsTr("Combinar y reemplazar los archivos repetidos"), primary: false, shown: root.info.canMerge === true },
                        { decision: 2, label: qsTr("Combinar y preguntar por cada archivo repetido"), primary: false, shown: root.info.canMerge === true },
                        { decision: 1, label: root.info.canMerge === true ? qsTr("Reemplazar la carpeta entera") : qsTr("Sobrescribir"), primary: false, shown: root.info.canOverwrite === true },
                        { decision: 3, label: qsTr("Cancelar"), primary: false, shown: true }
                    ]

                    delegate: Rectangle {
                        id: button

                        required property var modelData

                        visible: modelData.shown
                        width: parent.width
                        height: modelData.shown ? Theme.rowHeight : 0
                        radius: Theme.radiusSmall
                        color: hover.hovered ? (modelData.primary ? Theme.accent : Theme.surfaceHover)
                                             : (modelData.primary ? Theme.surfaceActive : "transparent")
                        border.width: modelData.primary ? 0 : Theme.borderWidth
                        border.color: Theme.border

                        Text {
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
                            onClicked: root.decide(button.modelData.decision)
                        }
                    }
                }
            }
        }
    }
}
