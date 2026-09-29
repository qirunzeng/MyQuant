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
        color: root.pressed ? AppTheme.accent : root.hovered ? AppTheme.accent : AppTheme.textFaint
        opacity: root.size < 0.99 ? 0.9 : 0.35
    }

    background: Rectangle {
        implicitWidth: root.orientation === Qt.Vertical ? 10 : 40
        implicitHeight: root.orientation === Qt.Horizontal ? 10 : 40
        radius: 5
        color: AppTheme.surfaceAlt
        opacity: root.size < 0.99 ? 0.55 : 0.25
    }
}
