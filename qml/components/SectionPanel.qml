import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    default property alias content: body.data
    implicitWidth: 360
    implicitHeight: panelLayout.implicitHeight + 28

    radius: 8
    color: AppTheme.surface
    border.color: AppTheme.border
    border.width: 1

    ColumnLayout {
        id: panelLayout
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Label {
            text: root.title
            color: AppTheme.text
            font.pixelSize: 15
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
        }
    }
}
