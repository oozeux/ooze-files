import QtQuick
import Ooze.Files

Item {
    id: root

    property var entries: []
    property string mode: "replace"
    property string error: ""

    readonly property var previewRows: root.buildPreview()
    readonly property var currentNames: root.visible ? App.currentNames() : []
    readonly property int changedCount: {
        var rows = root.previewRows
        var count = 0
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].changed)
                ++count
        }
        return count
    }
    readonly property bool hasConflicts: {
        var rows = root.previewRows
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].conflict)
                return true
        }
        return false
    }
    readonly property bool canApply: root.entries.length > 0 && root.error.length === 0
        && !root.hasConflicts && root.changedCount > 0

    signal closed()

    anchors.fill: parent
    visible: false
    z: 285

    function open(items) {
        if (!items || items.length === 0)
            return

        root.entries = items.slice()
        root.mode = "replace"
        root.error = ""
        findInput.text = ""
        replaceInput.text = ""
        prefixInput.text = ""
        suffixInput.text = ""
        startInput.text = "1"
        stepInput.text = "1"
        paddingInput.text = "0"
        regexToggle.checked = false
        caseToggle.checked = false
        root.visible = true
        root.forceActiveFocus()
        findInput.forceFocus()
    }

    function close() {
        if (!root.visible)
            return
        root.visible = false
        root.entries = []
        root.closed()
    }

    function splitName(name, isDir) {
        if (isDir)
            return { base: name, ext: "" }
        var dot = name.lastIndexOf(".")
        if (dot <= 0)
            return { base: name, ext: "" }
        return { base: name.substring(0, dot), ext: name.substring(dot) }
    }

    function intValue(text, fallback) {
        var value = parseInt(text, 10)
        return isNaN(value) ? fallback : value
    }

    function padNumber(value, padding) {
        var text = "" + value
        while (text.length < padding)
            text = "0" + text
        return text
    }

    function buildPreview() {
        var rows = []
        if (root.entries.length === 0)
            return rows

        root.error = ""

        var pattern = null
        var needle = findInput ? findInput.text : ""
        var replacement = replaceInput ? replaceInput.text : ""
        var ignoreCase = caseToggle ? !caseToggle.checked : false

        if (root.mode === "replace" && regexToggle && regexToggle.checked) {
            if (needle.length > 0) {
                try {
                    pattern = new RegExp(needle, ignoreCase ? "gi" : "g")
                } catch (error) {
                    root.error = qsTr("Expresión regular no válida")
                    return rows
                }
            }
        }

        var oldNames = {}
        for (var o = 0; o < root.entries.length; ++o)
            oldNames[root.entries[o].name] = true

        var existing = {}
        for (var e = 0; e < root.currentNames.length; ++e)
            existing[root.currentNames[e]] = true

        var seen = {}
        var prefix = root.mode === "sequence"
            ? (sequencePrefixInput ? sequencePrefixInput.text : "")
            : (prefixInput ? prefixInput.text : "")
        var suffix = root.mode === "sequence"
            ? (sequenceSuffixInput ? sequenceSuffixInput.text : "")
            : (suffixInput ? suffixInput.text : "")
        var start = root.intValue(startInput ? startInput.text : "1", 1)
        var step = root.intValue(stepInput ? stepInput.text : "1", 1)
        var padding = Math.max(0, root.intValue(paddingInput ? paddingInput.text : "0", 0))

        for (var i = 0; i < root.entries.length; ++i) {
            var entry = root.entries[i]
            var parts = root.splitName(entry.name, entry.isDir)
            var base = parts.base
            var ext = parts.ext
            var nextBase = base

            if (root.mode === "replace") {
                if (needle.length > 0) {
                    if (pattern) {
                        nextBase = base.replace(pattern, replacement)
                    } else if (ignoreCase) {
                        var escaped = needle.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
                        nextBase = base.replace(new RegExp(escaped, "gi"), replacement)
                    } else {
                        nextBase = base.split(needle).join(replacement)
                    }
                }
            } else if (root.mode === "affix") {
                nextBase = prefix + base + suffix
            } else if (root.mode === "sequence") {
                nextBase = prefix + root.padNumber(start + i * step, padding) + suffix
            }

            var nextName = nextBase + ext
            var changed = nextName !== entry.name
            var conflict = false

            if (nextName.length === 0 || nextName.indexOf("/") >= 0)
                conflict = true
            else if (seen[nextName])
                conflict = true
            else if (existing[nextName] && !oldNames[nextName])
                conflict = true

            seen[nextName] = true

            rows.push({
                url: entry.url,
                oldName: entry.name,
                newName: nextName,
                isDir: entry.isDir,
                changed: changed,
                conflict: conflict
            })
        }

        return rows
    }

    function apply() {
        if (!root.canApply)
            return

        var payload = []
        var rows = root.previewRows
        for (var i = 0; i < rows.length; ++i) {
            payload.push({
                url: rows[i].url,
                name: rows[i].newName
            })
        }

        App.renameEntries(payload)
        root.close()
    }

    Keys.onEscapePressed: root.close()

    Shortcut {
        sequence: "Ctrl+Return"
        enabled: root.visible && root.canApply
        onActivated: root.apply()
    }

    component ModeChip: Rectangle {
        id: chip

        property string value: ""
        property string label: ""

        width: chipText.width + 20
        height: Theme.rowHeight - 6
        radius: height / 2
        color: root.mode === chip.value
            ? Theme.accent
            : (chipHover.hovered ? Theme.surfaceHover : Theme.surfaceActive)

        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: root.mode === chip.value ? "#ffffff" : Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }

        HoverHandler {
            id: chipHover
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.mode = chip.value
        }
    }

    component FieldLabel: Text {
        color: Theme.fgMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 1
    }

    component TextBox: Rectangle {
        id: box

        property alias text: field.text
        property alias inputMethodHints: field.inputMethodHints
        property string placeholder: ""

        function forceFocus() {
            field.forceActiveFocus()
        }

        width: parent.width
        height: Theme.rowHeight - 4
        radius: Theme.radiusSmall
        color: Theme.bg
        border.width: Theme.borderWidth
        border.color: field.activeFocus ? Theme.accent : Theme.border

        TextInput {
            id: field
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            clip: true
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            selectionColor: Theme.accent
            selectedTextColor: "#ffffff"
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            activeFocusOnTab: true
            Keys.onEscapePressed: root.close()
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: Text.AlignVCenter
            visible: field.text.length === 0
            text: box.placeholder
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            elide: Text.ElideRight
        }
    }

    component CheckBox: Rectangle {
        id: check

        property string label: ""
        property bool checked: false

        width: box.width + 6 + checkLabel.implicitWidth
        height: 20
        color: "transparent"

        Rectangle {
            id: box

            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            radius: 3
            color: check.checked ? Theme.accent : "transparent"
            border.width: Theme.borderWidth
            border.color: check.checked ? Theme.accent : Theme.border

            Text {
                anchors.centerIn: parent
                visible: check.checked
                text: "✓"
                color: "#ffffff"
                font.pixelSize: 11
            }
        }

        Text {
            id: checkLabel

            anchors.left: box.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: check.label
            color: Theme.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: check.checked = !check.checked
        }
    }

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
        width: Math.min(820, root.width - 4 * Theme.pad)
        height: Math.min(660, root.height - 4 * Theme.pad)
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
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Renombrar en lote")
                color: Theme.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.bold: true
            }

            Text {
                anchors.right: closeButton.left
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 elementos").arg(root.entries.length)
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

        Column {
            id: form

            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.pad
            spacing: Theme.pad / 2 + 2

            Row {
                spacing: 6

                ModeChip { value: "replace"; label: qsTr("Buscar y reemplazar") }
                ModeChip { value: "affix"; label: qsTr("Prefijo / sufijo") }
                ModeChip { value: "sequence"; label: qsTr("Numeración") }
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Theme.pad
                rowSpacing: 4
                visible: root.mode === "replace"

                FieldLabel { text: qsTr("Buscar") }
                FieldLabel { text: qsTr("Reemplazar por") }

                TextBox {
                    id: findInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Texto a buscar")
                }

                TextBox {
                    id: replaceInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Nuevo texto")
                }
            }

            Row {
                visible: root.mode === "replace"
                spacing: Theme.pad

                CheckBox { id: regexToggle; label: qsTr("Expresión regular") }
                CheckBox { id: caseToggle; label: qsTr("Distinguir mayúsculas") }
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Theme.pad
                rowSpacing: 4
                visible: root.mode === "affix"

                FieldLabel { text: qsTr("Prefijo") }
                FieldLabel { text: qsTr("Sufijo") }

                TextBox {
                    id: prefixInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Antes del nombre")
                }

                TextBox {
                    id: suffixInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Después del nombre")
                }
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Theme.pad
                rowSpacing: 4
                visible: root.mode === "sequence"

                FieldLabel { text: qsTr("Prefijo") }
                FieldLabel { text: qsTr("Sufijo") }

                TextBox {
                    id: sequencePrefixInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Antes del número")
                }

                TextBox {
                    id: sequenceSuffixInput
                    width: (form.width - Theme.pad) / 2
                    placeholder: qsTr("Después del número")
                }

                FieldLabel { text: qsTr("Desde") }
                FieldLabel { text: qsTr("Incremento") }

                TextBox {
                    id: startInput
                    width: (form.width - Theme.pad) / 2
                    text: "1"
                    inputMethodHints: Qt.ImhDigitsOnly
                }

                TextBox {
                    id: stepInput
                    width: (form.width - Theme.pad) / 2
                    text: "1"
                    inputMethodHints: Qt.ImhDigitsOnly
                }

                FieldLabel { text: qsTr("Dígitos (relleno con ceros)") }

                TextBox {
                    id: paddingInput
                    width: (form.width - Theme.pad) / 2
                    text: "0"
                    inputMethodHints: Qt.ImhDigitsOnly
                }
            }

            Text {
                width: parent.width
                visible: root.error.length > 0
                text: root.error
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            id: previewHeader

            anchors.top: form.bottom
            anchors.topMargin: Theme.pad / 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.pad
            anchors.rightMargin: Theme.pad
            height: Theme.rowHeight - 8
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Vista previa")
                color: Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                font.bold: true
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 de %2 cambiarán").arg(root.changedCount).arg(root.entries.length)
                color: root.hasConflicts ? Theme.danger : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }
        }

        ListView {
            id: previewList

            anchors.top: previewHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.leftMargin: Theme.pad / 2
            anchors.rightMargin: Theme.pad / 2
            anchors.bottomMargin: Theme.pad / 2
            clip: true
            model: root.previewRows
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: previewRow

                required property var modelData

                width: previewList.width
                height: Theme.rowHeight - 4
                radius: Theme.radiusSmall
                color: previewRow.modelData.conflict
                    ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.18)
                    : (previewHover.hovered ? Theme.surfaceHover : "transparent")

                FileIcon {
                    id: rowIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    icon: previewRow.modelData.isDir ? "folder" : "text-x-generic"
                    size: 16
                }

                Text {
                    anchors.left: rowIcon.right
                    anchors.leftMargin: 8
                    anchors.right: arrow.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: previewRow.modelData.oldName
                    color: previewRow.modelData.changed ? Theme.fgMuted : Theme.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                    elide: Text.ElideMiddle
                }

                Text {
                    id: arrow
                    anchors.right: newName.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "→"
                    color: Theme.fgMuted
                    font.pixelSize: Theme.fontSize - 1
                }

                Text {
                    id: newName
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(60, previewRow.width * 0.42)
                    horizontalAlignment: Text.AlignRight
                    text: previewRow.modelData.newName
                    color: previewRow.modelData.conflict
                        ? Theme.danger
                        : (previewRow.modelData.changed ? Theme.fg : Theme.fgMuted)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                    font.bold: previewRow.modelData.changed
                    elide: Text.ElideMiddle
                }

                HoverHandler {
                    id: previewHover
                }
            }
        }

        ScrollIndicator {
            flickable: previewList
            anchors.right: previewList.right
            anchors.top: previewList.top
            anchors.bottom: previewList.bottom
            anchors.margins: 2
        }

        Item {
            id: footer

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.rowHeight + Theme.pad

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: Theme.borderWidth
                color: Theme.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                text: root.hasConflicts
                    ? qsTr("Hay nombres repetidos o inválidos")
                    : qsTr("Se conserva la extensión de cada archivo")
                color: root.hasConflicts ? Theme.danger : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
                elide: Text.ElideRight
            }

            Rectangle {
                id: cancelButton

                anchors.right: applyButton.left
                anchors.rightMargin: Theme.pad / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 96
                height: Theme.rowHeight - 4
                radius: Theme.radiusSmall
                color: cancelHover.hovered ? Theme.surfaceHover : Theme.surfaceActive

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
                    onClicked: root.close()
                }
            }

            Rectangle {
                id: applyButton

                anchors.right: parent.right
                anchors.rightMargin: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                width: 116
                height: Theme.rowHeight - 4
                radius: Theme.radiusSmall
                color: root.canApply
                    ? (applyHover.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
                    : Theme.surfaceActive
                opacity: root.canApply ? 1 : 0.6

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Renombrar")
                    color: root.canApply ? "#ffffff" : Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }

                HoverHandler {
                    id: applyHover
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.canApply
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.apply()
                }
            }
        }
    }
}
