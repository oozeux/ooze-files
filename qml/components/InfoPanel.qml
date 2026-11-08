import QtQuick
import Ooze.Files

Rectangle {
    id: root

    property var info: ({})
    property bool multiple: false

    readonly property string kind: info.kind !== undefined ? info.kind : "none"
    readonly property string iconName: kind === "directory"
        ? "folder"
        : (info.mime ? String(info.mime).replace("/", "-") : "text-x-generic")
    readonly property bool videoPreview: App.thumbnails
        && String(info.mime || "").indexOf("video/") === 0
    readonly property bool pdfPreview: kind === "pdf" && Tools.pdftoppm
    readonly property bool markdown: kind === "markdown"

    function refresh() {
        if (!root.visible)
            return

        var count = App.selectionCount
        root.multiple = count > 1
        if (count === 1)
            root.info = App.previewRow(App.selectedRows()[0])
        else if (count === 0)
            root.info = App.previewCurrentFolder()
        else
            root.info = ({})
    }

    color: Theme.surfaceAlt
    clip: true
    onVisibleChanged: refresh()
    Component.onCompleted: refresh()

    Timer {
        id: debounce

        interval: 120
        onTriggered: root.refresh()
    }

    Connections {
        target: App

        function onSelectionChanged() { debounce.restart() }
        function onCurrentUrlChanged() { debounce.restart() }
        function onItemCountChanged() { debounce.restart() }
    }

    Rectangle {
        anchors.left: parent.left
        width: Theme.borderWidth
        height: parent.height
        color: Theme.border
    }

    Flickable {
        id: scroll

        anchors.fill: parent
        anchors.leftMargin: Theme.pad + 1
        anchors.rightMargin: Theme.pad
        anchors.topMargin: Theme.pad
        anchors.bottomMargin: Theme.pad
        contentWidth: width
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: body

            width: scroll.width
            spacing: Theme.pad

            Item {
                width: body.width
                height: 180

                Image {
                    id: picture

                    visible: (root.kind === "image" || root.videoPreview || root.pdfPreview)
                        && !root.multiple
                    anchors.fill: parent
                    source: !visible
                        ? ""
                        : (root.pdfPreview
                            ? "image://previews/" + encodeURIComponent(root.info.url)
                            : (root.videoPreview
                                ? "image://thumbnails/" + encodeURIComponent(root.info.url)
                                : root.info.url))
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    autoTransform: true
                    smooth: true
                    sourceSize.width: 512
                    sourceSize.height: 512
                }

                Rectangle {
                    visible: (root.kind === "text" || root.markdown) && !root.multiple
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: Theme.bg
                    border.width: Theme.borderWidth
                    border.color: Theme.border
                    clip: true

                    Text {
                        anchors.fill: parent
                        anchors.margins: 6
                        text: (root.kind === "text" || root.markdown) ? root.info.text : ""
                        textFormat: root.markdown ? Text.MarkdownText : Text.PlainText
                        wrapMode: root.markdown ? Text.WordWrap : Text.NoWrap
                        maximumLineCount: 14
                        elide: Text.ElideRight
                        color: Theme.fg
                        font.family: root.markdown ? Theme.fontFamily : "monospace"
                        font.pixelSize: Theme.fontSize - 2
                    }
                }

                FileIcon {
                    visible: root.multiple
                        || (root.kind !== "text" && !root.markdown
                            && !((root.kind === "image" || root.videoPreview || root.pdfPreview)
                                 && picture.status === Image.Ready))
                    anchors.centerIn: parent
                    icon: root.multiple ? "text-x-generic" : root.iconName
                    size: 72
                }
            }

            Text {
                width: body.width
                text: root.multiple
                    ? qsTr("%1 elementos seleccionados").arg(App.selectionCount)
                    : (root.info.name !== undefined ? root.info.name : "")
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
                wrapMode: Text.WrapAnywhere
            }

            DetailsList {
                width: body.width
                info: root.multiple ? ({}) : root.info
            }
        }
    }

    ScrollIndicator {
        flickable: scroll
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 2
    }
}
