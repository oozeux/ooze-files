pragma ComponentBehavior: Bound

import QtQuick
import Ooze.Files

Item {
    id: root

    signal tabActivated(int index)
    signal tabCloseRequested(int index)
    signal tabContextRequested(int index, real x, real y)

    implicitHeight: Theme.tabHeight
    visible: App.tabCount > 1 || App.alwaysShowTabBar

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceAlt

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Theme.borderWidth
            color: Theme.border
        }
    }

    ListView {
        id: list

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: addButton.left
        orientation: ListView.Horizontal
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: App.tabs
        spacing: 1
        currentIndex: App.activeTab

        delegate: Rectangle {
            id: tab

            required property int index
            required property string title
            required property string url
            required property bool active

            readonly property bool hovered: tabHover.hovered
            readonly property int available: Math.max(0, list.width - addButton.width)

            width: Math.max(120, Math.min(220, available / Math.max(1, App.tabCount)))
            height: list.height
            color: tab.active
                ? Theme.surface
                : (tab.hovered ? Theme.surfaceHover : "transparent")

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: tab.active ? 2 : 0
                color: Theme.accent
            }

            FileIcon {
                id: tabIcon

                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                icon: "folder"
                size: Math.max(12, Theme.fontSize)
            }

            Text {
                anchors.left: tabIcon.right
                anchors.leftMargin: 7
                anchors.right: closeButton.visible ? closeButton.left : parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: tab.title.length > 0 ? tab.title : qsTr("Sin título")
                color: tab.active ? Theme.fg : Theme.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                elide: Text.ElideMiddle
            }

            Rectangle {
                id: closeButton

                z: 2
                visible: tab.active || tabHover.hovered
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                radius: Theme.radiusSmall
                color: closeHover.hovered ? Theme.surfaceActive : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: Theme.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                }

                HoverHandler {
                    id: closeHover
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabCloseRequested(tab.index)
                }
            }

            HoverHandler {
                id: tabHover
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor

                onClicked: function (mouse) {
                    if (mouse.button === Qt.LeftButton)
                        root.tabActivated(tab.index)
                    else if (mouse.button === Qt.MiddleButton)
                        root.tabCloseRequested(tab.index)
                }

                onPressed: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        var point = tab.mapToItem(root, mouse.x, mouse.y)
                        root.tabContextRequested(tab.index, point.x, point.y)
                    }
                }
            }
        }
    }

    WheelHandler {
        onWheel: function (event) {
            if (event.angleDelta.y > 0 || event.angleDelta.x < 0)
                App.previousTab()
            else
                App.nextTab()
            event.accepted = true
        }
    }

    Rectangle {
        id: addButton

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.tabHeight
        color: addHover.hovered ? Theme.surfaceHover : "transparent"

        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize + 3
        }

        HoverHandler {
            id: addHover
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: App.newTab()
        }
    }

    Connections {
        target: App

        function onActiveTabChanged() {
            if (list.count > 0)
                list.positionViewAtIndex(App.activeTab, ListView.Contain)
        }
    }
}
