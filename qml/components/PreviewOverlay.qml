import QtQuick
import Ooze.Files

Item {
    id: root

    property int row: -1
    property var info: ({})

    readonly property string kind: info.kind !== undefined ? info.kind : "none"
    readonly property bool showSidebar: card.width >= 620
    readonly property bool videoKind: kind === "video"
    readonly property bool pdfKind: kind === "pdf"
    readonly property bool markdownKind: kind === "markdown"
    readonly property string iconName: kind === "directory"
        ? "folder"
        : (info.mime ? String(info.mime).replace("/", "-") : "text-x-generic")
    signal closed()
    signal rowRequested(int row)

    anchors.fill: parent
    visible: false
    z: 280

    function open(targetRow) {
        root.show(targetRow)
        root.visible = true
        root.forceActiveFocus()
    }

    function close() {
        root.visible = false
        root.info = ({})
        root.closed()
    }

    function show(targetRow) {
        root.row = targetRow
        root.info = App.previewRow(targetRow)
        textScroll.contentY = 0
        textScroll.contentX = 0
        markdownScroll.contentY = 0
    }

    function step(delta) {
        var next = root.row + delta
        if (next < 0 || next >= App.itemCount)
            return
        root.show(next)
        root.rowRequested(next)
    }

    function openCurrent() {
        var target = root.row
        root.close()
        App.openRow(target)
    }

    Keys.onEscapePressed: root.close()
    Keys.onLeftPressed: root.step(-1)
    Keys.onUpPressed: root.step(-1)
    Keys.onRightPressed: root.step(1)
    Keys.onDownPressed: root.step(1)
    Keys.onReturnPressed: root.openCurrent()
    Keys.onEnterPressed: root.openCurrent()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
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
        width: Math.min(1000, root.width - 4 * Theme.pad)
        height: Math.min(680, root.height - 4 * Theme.pad)
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

        Item {
            id: titleBar

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight + Theme.pad

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad
                anchors.right: counter.left
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.info.name !== undefined ? root.info.name : ""
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
                elide: Text.ElideMiddle
            }

            Text {
                id: counter

                anchors.right: closeButton.left
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.row >= 0 ? (root.row + 1) + " / " + App.itemCount : ""
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }

            IconButton {
                id: closeButton

                anchors.right: parent.right
                anchors.rightMargin: Theme.pad / 2
                anchors.verticalCenter: parent.verticalCenter
                icon: "window-close"
                label: qsTr("Cerrar")
                onClicked: root.close()
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }
        }

        Item {
            id: stage

            anchors.top: titleBar.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: root.showSidebar ? sidebar.left : parent.right
            clip: true

            Image {
                id: picture

                visible: root.kind === "image"
                anchors.fill: parent
                anchors.margins: Theme.pad
                source: root.kind === "image" ? root.info.url : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                smooth: true
                sourceSize.width: 2048
                sourceSize.height: 2048
            }

            Text {
                anchors.centerIn: parent
                visible: root.kind === "image" && picture.status !== Image.Ready
                text: picture.status === Image.Error ? qsTr("No se pudo cargar la imagen")
                                                     : qsTr("Cargando…")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }

            Image {
                id: poster

                visible: root.videoKind
                anchors.fill: parent
                anchors.margins: Theme.pad
                source: root.videoKind
                    ? "image://thumbnails/" + encodeURIComponent(root.info.url)
                    : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                sourceSize.width: 512
                sourceSize.height: 512
            }

            Rectangle {
                id: playButton

                visible: root.videoKind && poster.status === Image.Ready
                anchors.centerIn: parent
                width: 72
                height: 72
                radius: 36
                color: Qt.rgba(0, 0, 0, 0.55)
                border.width: Theme.borderWidth
                border.color: Qt.rgba(1, 1, 1, 0.35)

                Text {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 2
                    text: "▶"
                    color: "#ffffff"
                    font.pixelSize: 28
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openCurrent()
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 4 * Theme.pad
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: root.videoKind && poster.status === Image.Error
                text: Tools.ffmpeg
                    ? qsTr("No se pudo generar el póster del vídeo")
                    : qsTr("Instala ffmpeg para ver el póster del vídeo")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }

            Image {
                id: pdfPicture

                visible: root.pdfKind
                anchors.fill: parent
                anchors.margins: Theme.pad
                source: root.pdfKind
                    ? "image://previews/" + encodeURIComponent(root.info.url)
                    : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                sourceSize.width: 1024
                sourceSize.height: 1024
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 4 * Theme.pad
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: root.pdfKind && pdfPicture.status !== Image.Ready
                text: pdfPicture.status === Image.Error
                    ? (Tools.pdftoppm
                        ? qsTr("No se pudo generar la vista previa del PDF")
                        : qsTr("Instala poppler-utils (pdftoppm) para ver el PDF"))
                    : qsTr("Cargando…")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
            }

            Flickable {
                id: markdownScroll

                visible: root.markdownKind
                anchors.fill: parent
                anchors.margins: Theme.pad
                contentWidth: width
                contentHeight: markdownBody.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: markdownBody

                    width: markdownScroll.width
                    text: root.markdownKind ? root.info.text : ""
                    textFormat: Text.MarkdownText
                    wrapMode: Text.WordWrap
                    color: Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                }
            }

            ScrollIndicator {
                flickable: markdownScroll
                visible: root.markdownKind
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 2
            }

            Flickable {
                id: textScroll

                visible: root.kind === "text"
                anchors.fill: parent
                anchors.margins: Theme.pad
                contentWidth: body.implicitWidth
                contentHeight: body.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: body

                    text: root.kind === "text" ? root.info.text : ""
                    textFormat: Text.PlainText
                    wrapMode: Text.NoWrap
                    color: Theme.fg
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSize
                }
            }

            ScrollIndicator {
                flickable: textScroll
                visible: root.kind === "text"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 2
            }

            Column {
                visible: root.kind === "directory" || root.kind === "none"
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.pad
                spacing: Theme.pad

                FileIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    icon: root.iconName
                    size: 96
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: root.info.error !== undefined ? root.info.error
                        : (root.kind === "directory"
                            ? qsTr("%1 elementos").arg(root.info.itemCountText)
                            : qsTr("Sin vista previa para este tipo de archivo"))
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }
            }

            Text {
                visible: (root.kind === "text" || root.markdownKind)
                         && root.info.truncated === true
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: Theme.pad
                text: qsTr("Vista parcial")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }
        }

        Rectangle {
            id: sidebar

            visible: root.showSidebar
            anchors.top: titleBar.bottom
            anchors.bottom: footer.top
            anchors.right: parent.right
            width: 250
            color: Theme.bg

            Rectangle {
                anchors.left: parent.left
                width: Theme.borderWidth
                height: parent.height
                color: Theme.border
            }

            Flickable {
                id: detailsScroll

                anchors.fill: parent
                anchors.margins: Theme.pad
                contentWidth: width
                contentHeight: detailsColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                DetailsList {
                    id: detailsColumn

                    width: detailsScroll.width
                    info: root.info
                }
            }
        }

        Item {
            id: footer

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight - 6

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("← → navegar   ·   Espacio o Esc cerrar   ·   Intro abrir")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
                elide: Text.ElideRight
            }
        }
    }
}
