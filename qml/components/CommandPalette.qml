pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Item {
    id: root

    signal closed()

    anchors.fill: parent
    visible: false
    z: 250

    property string query: ""
    property int activeIndex: 0
    readonly property var results: root.filterActions(Actions.commands, root.query)

    function open() {
        root.query = ""
        root.activeIndex = 0
        root.visible = true
        input.text = ""
        input.forceActiveFocus()
    }

    function close() {
        if (!root.visible)
            return
        root.visible = false
        root.query = ""
        root.closed()
    }

    function toggle() {
        if (root.visible)
            root.close()
        else
            root.open()
    }

    function filterActions(actions, text) {
        var trimmed = text.trim().toLowerCase()
        var ordered = []

        if (trimmed.length === 0) {
            for (var i = 0; i < actions.length; ++i)
                ordered.push(actions[i])
            return ordered
        }

        var tokens = trimmed.split(/\s+/)
        var scored = []

        for (var j = 0; j < actions.length; ++j) {
            var item = actions[j]
            var label = item.label.toLowerCase()
            var haystack = label + " " + item.id.toLowerCase() + " "
                + item.keywords.toLowerCase() + " " + item.category.toLowerCase()
            var matches = true

            for (var t = 0; t < tokens.length; ++t) {
                if (haystack.indexOf(tokens[t]) < 0) {
                    matches = false
                    break
                }
            }

            if (!matches)
                continue

            var score = 0
            for (var s = 0; s < tokens.length; ++s) {
                var position = label.indexOf(tokens[s])
                if (position < 0)
                    score += 4
                else if (position === 0)
                    score += 0
                else if (label.charAt(position - 1) === " ")
                    score += 1
                else
                    score += 2
            }

            scored.push({ action: item, score: score })
        }

        scored.sort(function (left, right) { return left.score - right.score })

        for (var k = 0; k < scored.length; ++k)
            ordered.push(scored[k].action)

        return ordered
    }

    function activate(index) {
        if (index < 0 || index >= root.results.length)
            return

        var item = root.results[index]
        if (!item.enabled)
            return

        root.close()
        Actions.trigger(item.id)
    }

    function move(delta) {
        if (root.results.length === 0)
            return

        var next = root.activeIndex + delta
        if (next < 0)
            next = root.results.length - 1
        else if (next >= root.results.length)
            next = 0

        root.activeIndex = next
        resultsView.positionViewAtIndex(next, ListView.Contain)
    }

    onQueryChanged: {
        root.activeIndex = 0
        if (resultsView)
            resultsView.positionViewAtBeginning()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.close()
    }

    Rectangle {
        id: card

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(Theme.pad * 3, Math.round(root.height * 0.1))
        width: Math.min(560, root.width - 4 * Theme.pad)
        height: Math.min(400, root.height - y - Theme.pad * 3)
        radius: Theme.radius
        color: Theme.surface
        border.width: Theme.borderWidth
        border.color: Theme.border

        MouseArea {
            anchors.fill: parent
        }

        Item {
            id: searchRow

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.headerHeight

            FileIcon {
                id: searchIcon

                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                icon: "edit-find"
                size: 16
                opacity: 0.7
            }

            TextInput {
                id: input

                anchors.left: searchIcon.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                selectionColor: Theme.accent
                selectedTextColor: "#ffffff"
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                onTextChanged: root.query = text
                onAccepted: root.activate(root.activeIndex)
                Keys.onDownPressed: root.move(1)
                Keys.onUpPressed: root.move(-1)
                Keys.onEscapePressed: root.close()
            }

            Text {
                anchors.left: input.left
                anchors.right: input.right
                anchors.verticalCenter: parent.verticalCenter
                visible: input.text.length === 0
                text: qsTr("Escribe un comando…")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                elide: Text.ElideRight
            }
        }

        Rectangle {
            id: separator

            anchors.top: searchRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.border
        }

        ListView {
            id: resultsView

            anchors.top: separator.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            clip: true
            model: root.results
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row

                required property int index
                required property var modelData

                x: 6
                width: ListView.view.width - 12
                height: Theme.rowHeight
                radius: Theme.radiusSmall
                color: index === root.activeIndex
                    ? Theme.selectionBg
                    : (rowHover.hovered ? Theme.surfaceHover : "transparent")

                FileIcon {
                    id: checkIcon

                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.modelData.checked === true
                    icon: "object-select"
                    size: 14
                }

                Text {
                    id: labelText

                    anchors.left: checkIcon.visible ? checkIcon.right : parent.left
                    anchors.leftMargin: checkIcon.visible ? 6 : 10
                    anchors.right: hintText.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.label
                    color: row.modelData.enabled ? Theme.fg : Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    elide: Text.ElideRight
                }

                Text {
                    id: hintText

                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.hint
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }

                HoverHandler {
                    id: rowHover
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPositionChanged: root.activeIndex = row.index
                    onClicked: root.activate(row.index)
                }
            }
        }

        Text {
            anchors.centerIn: resultsView
            visible: root.results.length === 0
            text: qsTr("Sin coincidencias")
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
        }

        Item {
            id: footer

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 26

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("↑↓ moverse · Enter ejecutar · Esc cerrar")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
        }
    }
}
