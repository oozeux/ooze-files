pragma Singleton

import QtQuick
import Ooze.Files

QtObject {
    readonly property bool dark: App.effectiveDark

    readonly property color bg: Palette.bg
    readonly property color surface: Palette.surface
    readonly property color surfaceAlt: Palette.surfaceAlt
    readonly property color surfaceHover: Palette.surfaceHover
    readonly property color surfaceActive: Palette.surfaceActive
    readonly property color border: Palette.border
    readonly property color fg: Palette.fg
    readonly property color fgMuted: Palette.fgMuted
    readonly property color accent: Palette.accent
    readonly property color danger: Palette.danger

    readonly property color selectionBg: Palette.selectionBg
    readonly property color marqueeFill: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.20 : 0.14)
    readonly property color marqueeBorder: Qt.rgba(accent.r, accent.g, accent.b, 0.85)

    readonly property int radius: Palette.radius
    readonly property int radiusSmall: Palette.radiusSmall
    readonly property int borderWidth: Palette.borderWidth

    readonly property real scale: App.uiFontScale

    readonly property int pad: Math.round((App.density === "compact" ? 6
                                        : App.density === "comfortable" ? 14
                                        : 10) * scale)
    readonly property int rowHeight: Math.round((App.density === "compact" ? 28
                                               : App.density === "comfortable" ? 40
                                               : 34) * scale)
    readonly property int headerHeight: Math.round((App.density === "compact" ? 42
                                                  : App.density === "comfortable" ? 54
                                                  : 48) * scale)
    readonly property int statusHeight: Math.round((App.density === "compact" ? 24 : 28) * scale)
    readonly property int tabHeight: Math.round((App.density === "compact" ? 30
                                               : App.density === "comfortable" ? 38
                                               : 34) * scale)
    readonly property int sidebarWidth: Math.round((App.density === "compact" ? 190 : 212) * scale)
    readonly property int fontSize: Math.max(9, Math.round((App.density === "compact" ? 12
                                                          : App.density === "comfortable" ? 14
                                                          : 13) * scale))
    readonly property string fontFamily: App.uiFontFamily

    readonly property real hiddenOpacity: 0.5
    readonly property real cutOpacity: 0.4
    readonly property real inactiveOpacity: 0.55
    readonly property int iconSize: App.density === "compact" ? 16 : 18

    readonly property int gridIconSize: App.iconSize === "small" ? 40
                                        : App.iconSize === "large" ? 80
                                        : App.iconSize === "huge" ? 112
                                        : 56
    readonly property int gridCellWidth: gridIconSize + 68
    readonly property int gridCellHeight: gridIconSize + 64
    readonly property int gridTextTop: gridIconSize + 20

    readonly property int listIconSize: App.iconSize === "small" ? 16
                                        : App.iconSize === "large" ? 28
                                        : App.iconSize === "huge" ? 40
                                        : iconSize
    readonly property int listRowHeight: Math.max(rowHeight, listIconSize + 10)

    readonly property int dragHotSpot: Math.round(fontSize * 1.45)

    function dragImageUrl(label, iconName, emblem, count) {
        var url = "image://dragpreview/" + encodeURIComponent(label)
        url += "?icon=" + encodeURIComponent(iconName)
        if (emblem.length > 0)
            url += "&emblem=" + encodeURIComponent(emblem)
        url += "&count=" + count
        url += "&fg=" + encodeURIComponent(String(fg))
        url += "&family=" + encodeURIComponent(fontFamily)
        url += "&size=" + fontSize
        url += "&v=" + App.iconRevision
        return url
    }
}
