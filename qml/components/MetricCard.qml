import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string hint: ""
    property color accent: AppTheme.accent

    radius: 8
    color: AppTheme.surface
    border.color: AppTheme.border
    border.width: 1
    implicitWidth: 160
    implicitHeight: 88

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        Label {
            text: root.label
            color: "#93a4b8"
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: root.value
            color: AppTheme.text
            font.pixelSize: 22
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.preferredHeight: 2
            Layout.fillWidth: true
            color: root.accent
            opacity: 0.8
        }
        Label {
            text: root.hint
            color: AppTheme.textFaint
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
