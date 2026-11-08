import QtQuick
import Ooze.Files

Item {
    id: root

    property string icon: ""
    property string emblem: ""
    property int size: Theme.iconSize

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Image {
        anchors.fill: parent
        source: "image://icons/" + (root.icon.length > 0 ? root.icon : "text-x-generic")
                + "?v=" + App.iconRevision
                + (root.emblem.length > 0 ? "&emblem=" + root.emblem : "")
        sourceSize.width: root.size
        sourceSize.height: root.size
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false
    }
}
