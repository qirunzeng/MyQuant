import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    default property alias content: body.data

    radius: 8
    color: "#10161d"
    border.color: "#233142"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Label {
            text: root.title
            color: "#dbe7f4"
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
