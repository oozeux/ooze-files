import QtQuick
import Ooze.Files

Item {
    id: root

    property var flickable: null
    property int barWidth: 4

    readonly property bool engaged: hover.hovered || area.pressed

    width: barWidth + 8
    visible: flickable !== null
             && flickable.contentHeight > flickable.height
             && thumb.height < root.height

    function scrollToThumb(thumbY) {
        if (!flickable)
            return
        var travel = Math.max(1, root.height - thumb.height)
        var ratio = Math.max(0, Math.min(1, thumbY / travel))
        flickable.cancelFlick()
        flickable.contentY = flickable.originY + ratio * thumb.maxOffset
    }

    Rectangle {
        id: thumb
        width: root.engaged ? root.barWidth + 4 : root.barWidth
        radius: width / 2
        color: Theme.fgMuted
        opacity: area.pressed ? 0.9 : (root.engaged ? 0.7 : 0.45)
        x: root.width - width - 2

        readonly property real ratio: root.flickable
            ? Math.min(1.0, root.flickable.height / Math.max(1.0, root.flickable.contentHeight))
            : 1.0
        readonly property real maxOffset: root.flickable
            ? Math.max(1.0, root.flickable.contentHeight - root.flickable.height)
            : 1.0

        height: Math.max(28, root.height * ratio)
        y: root.flickable
            ? Math.max(0, Math.min(root.height - height,
                  ((root.flickable.contentY - root.flickable.originY) / maxOffset) * (root.height - height)))
            : 0
    }

    HoverHandler {
        id: hover
    }

    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        cursorShape: Qt.ArrowCursor

        property real grabOffset: 0

        onPressed: function (mouse) {
            var onThumb = mouse.y >= thumb.y && mouse.y <= thumb.y + thumb.height
            grabOffset = onThumb ? mouse.y - thumb.y : thumb.height / 2
            if (!onThumb)
                root.scrollToThumb(mouse.y - grabOffset)
        }

        onPositionChanged: function (mouse) {
            if (pressed)
                root.scrollToThumb(mouse.y - grabOffset)
        }

        onWheel: function (wheel) {
            if (!root.flickable)
                return
            var delta = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 2
            var limit = root.flickable.originY + thumb.maxOffset
            root.flickable.cancelFlick()
            root.flickable.contentY = Math.max(root.flickable.originY,
                                               Math.min(limit, root.flickable.contentY - delta))
        }
    }
}
