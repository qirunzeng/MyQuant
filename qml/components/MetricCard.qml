import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string hint: ""
    property color accent: "#38bdf8"

    radius: 8
    color: "#121820"
    border.color: "#243241"
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
            color: "#e8eef7"
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
            color: "#718196"
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
