import QtQuick
import QtQuick.Controls
import MyQuant

TabButton {
    id: control
    implicitHeight: 40
    font.pixelSize: 13
    contentItem: Label {
        text: control.text
        color: control.checked ? AppTheme.text : AppTheme.textMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: control.checked ? AppTheme.surface : AppTheme.surfaceAlt
        border.color: AppTheme.border
        Rectangle {
            visible: control.checked
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: AppTheme.accent
        }
    }
}
