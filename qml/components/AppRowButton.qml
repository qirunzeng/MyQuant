import QtQuick
import QtQuick.Controls
import MyQuant

Button {
    id: control
    property bool alternateRow: false

    implicitWidth: 58
    implicitHeight: 32
    padding: 8
    font.pixelSize: 12

    contentItem: Label {
        text: control.text
        color: AppTheme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.down ? AppTheme.accentSoft
                            : control.hovered ? AppTheme.accentSoft
                                              : control.alternateRow ? AppTheme.surface : AppTheme.surfaceAlt
        border.color: control.hovered || control.activeFocus ? AppTheme.accent : AppTheme.border
        border.width: 1
    }
}
