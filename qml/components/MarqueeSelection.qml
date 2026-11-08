import QtQuick
import Ooze.Files

Item {
    id: root

    property Item view: null
    property bool gridMode: false
    property real rowHeight: Theme.rowHeight
    property real cellWidth: 110
    property real cellHeight: 106

    readonly property bool active: dragging

    property bool tracking: false
    property bool dragging: false
    property bool updated: false
    property real startX: 0
    property real startY: 0
    property real lastX: 0
    property real lastY: 0
    property var lastRows: []
    property bool toggleMode: false
    property bool addMode: false

    signal started()
    signal selectionRequested(var rows, int mode)
    signal finished(var rows)
    signal cancelled()
    signal emptyClicked()

    anchors.fill: view
    z: 5

    function stop() {
        tracking = false
        dragging = false
        updated = false
        lastRows = []
        toggleMode = false
        addMode = false
        autoScroll.stop()
    }

    function cancel() {
        stop()
        root.cancelled()
    }

    function rowsInRect(left, top, right, bottom) {
        if (!view)
            return []

        if (right - left < 1 || bottom - top < 1)
            return []

        var rows = []

        if (gridMode) {
            var columns = Math.max(1, Math.floor(view.width / cellWidth))
            var firstRow = Math.max(0, Math.floor(top / cellHeight))
            var lastRow = Math.min(Math.ceil(view.count / columns) - 1,
                                   Math.ceil(bottom / cellHeight) - 1)
            var firstColumn = Math.max(0, Math.floor(left / cellWidth))
            var lastColumn = Math.min(columns - 1, Math.ceil(right / cellWidth) - 1)

            for (var row = firstRow; row <= lastRow; ++row) {
                for (var column = firstColumn; column <= lastColumn; ++column) {
                    var index = row * columns + column
                    if (index < view.count)
                        rows.push(index)
                }
            }
        } else {
            var first = Math.max(0, Math.floor(top / rowHeight))
            var last = Math.min(view.count - 1, Math.ceil(bottom / rowHeight) - 1)
            for (var line = first; line <= last; ++line)
                rows.push(line)
        }

        return rows
    }

    function sameRows(left, right) {
        if (left.length !== right.length)
            return false
        for (var i = 0; i < left.length; ++i) {
            if (left[i] !== right[i])
                return false
        }
        return true
    }

    function rowSet(rows) {
        var set = {}
        for (var i = 0; i < rows.length; ++i)
            set[rows[i]] = true
        return set
    }

    function difference(left, right) {
        var excluded = rowSet(right)
        var result = []
        for (var i = 0; i < left.length; ++i) {
            if (!excluded[left[i]])
                result.push(left[i])
        }
        return result
    }

    function symmetricDifference(left, right) {
        return difference(left, right).concat(difference(right, left))
    }

    function updateFrame() {
        frame.x = Math.min(startX, lastX)
        frame.y = Math.min(startY, lastY)
        frame.width = Math.abs(lastX - startX)
        frame.height = Math.abs(lastY - startY)
    }

    function updateSelection() {
        if (!view)
            return

        var left = Math.min(startX, lastX) + view.contentX
        var top = Math.min(startY, lastY) + view.contentY
        var right = Math.max(startX, lastX) + view.contentX
        var bottom = Math.max(startY, lastY) + view.contentY
        var rows = rowsInRect(left, top, right, bottom)

        if (toggleMode) {
            if (sameRows(rows, lastRows))
                return
            var toggled = symmetricDifference(lastRows, rows)
            lastRows = rows
            if (toggled.length > 0)
                selectionRequested(toggled, 2)
        } else if (addMode) {
            var added = difference(rows, lastRows)
            lastRows = rows
            if (added.length > 0)
                selectionRequested(added, 1)
        } else {
            if (updated && sameRows(rows, lastRows))
                return
            updated = true
            lastRows = rows
            selectionRequested(rows, 0)
        }
    }

    function updateAutoScroll() {
        if (!dragging || !view) {
            autoScroll.stop()
            return
        }

        var margin = 24
        var canGoUp = view.contentY > 0
        var canGoDown = view.contentY < view.contentHeight - view.height

        if (lastY < margin && canGoUp) {
            autoScroll.direction = -1
            if (!autoScroll.running)
                autoScroll.start()
        } else if (lastY > view.height - margin && canGoDown) {
            autoScroll.direction = 1
            if (!autoScroll.running)
                autoScroll.start()
        } else {
            autoScroll.stop()
        }
    }

    Rectangle {
        id: frame

        visible: root.dragging
        radius: Theme.radiusSmall
        color: Theme.marqueeFill
        border.width: 1
        border.color: Theme.marqueeBorder
    }

    Timer {
        id: autoScroll

        property int direction: 0

        interval: 30
        repeat: true

        onTriggered: {
            if (!root.dragging || !root.view) {
                stop()
                return
            }

            var limit = Math.max(0, root.view.contentHeight - root.view.height)
            var next = Math.max(0, Math.min(limit, root.view.contentY + direction * 12))

            if (next === root.view.contentY) {
                stop()
                return
            }

            root.view.contentY = next
            root.updateSelection()
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton

        onPressed: function (mouse) {
            if (!root.view)
                return

            var point = root.view.mapToItem(root.view.contentItem, mouse.x, mouse.y)
            if (root.view.indexAt(point.x, point.y) >= 0) {
                mouse.accepted = false
                return
            }

            root.tracking = true
            root.dragging = false
            root.updated = false
            root.startX = mouse.x
            root.startY = mouse.y
            root.lastX = mouse.x
            root.lastY = mouse.y
            root.lastRows = []
            root.toggleMode = (mouse.modifiers & Qt.ControlModifier) !== 0
            root.addMode = !root.toggleMode && (mouse.modifiers & Qt.ShiftModifier) !== 0
            root.view.forceActiveFocus()
            root.started()
        }

        onPositionChanged: function (mouse) {
            if (!root.tracking)
                return

            root.lastX = Math.max(0, Math.min(root.view.width, mouse.x))
            root.lastY = Math.max(0, Math.min(root.view.height, mouse.y))

            if (!root.dragging) {
                if (Math.max(Math.abs(root.lastX - root.startX),
                             Math.abs(root.lastY - root.startY)) < 3)
                    return
                root.dragging = true
            }

            root.updateFrame()
            root.updateSelection()
            root.updateAutoScroll()
        }

        onReleased: {
            if (!root.dragging) {
                var clear = !root.toggleMode && !root.addMode
                root.stop()
                if (clear)
                    root.emptyClicked()
                return
            }

            var rows = root.lastRows
            root.stop()
            root.finished(rows)
        }

        onCanceled: root.cancel()
    }
}
