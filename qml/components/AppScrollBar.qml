import QtQuick
import QtQuick.Controls

ScrollBar {
    id: root

    policy: ScrollBar.AsNeeded
    interactive: true
    minimumSize: 0.08
    active: hovered || pressed || size < 0.99

    contentItem: Rectangle {
        implicitWidth: root.orientation === Qt.Vertical ? 10 : 40
        implicitHeight: root.orientation === Qt.Horizontal ? 10 : 40
        radius: 5
        color: root.pressed ? "#38bdf8" : root.hovered ? "#60a5fa" : "#4b657d"
        opacity: root.size < 0.99 ? 0.9 : 0.35
    }

    background: Rectangle {
        implicitWidth: root.orientation === Qt.Vertical ? 10 : 40
        implicitHeight: root.orientation === Qt.Horizontal ? 10 : 40
        radius: 5
        color: "#0a1118"
        opacity: root.size < 0.99 ? 0.55 : 0.25
    }
}
