import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    visible: true
    title: "MyQuant"
    color: "#080d12"

    property int activePage: 0
    readonly property bool compactLayout: width < 1320
    readonly property var navItems: [
        { label: "ETF", sub: "轮动复盘" },
        { label: "NOTE", sub: "复盘笔记" },
        { label: "SET", sub: "设置" }
    ]

    function pct(v) {
        if (v === undefined || v === null || isNaN(Number(v)))
            return "--"
        return (Number(v) * 100).toFixed(2) + "%"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: window.compactLayout ? 88 : 104
            Layout.fillHeight: true
            color: "#070b10"
            border.color: "#182433"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 14

                Label {
                    text: "MyQuant"
                    color: "#f8fafc"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                Repeater {
                    model: window.navItems
                    delegate: Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        flat: true
                        background: Rectangle {
                            radius: 8
                            color: index === window.activePage ? "#143044" : "transparent"
                            border.color: index === window.activePage ? "#256f91" : "#1c2a38"
                            border.width: 1
                        }
                        contentItem: Column {
                            spacing: 2
                            anchors.centerIn: parent
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: index === window.activePage ? "#e0f2fe" : "#91a3b8"
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.sub
                                color: "#64748b"
                                font.pixelSize: 10
                            }
                        }
                        onClicked: window.activePage = index
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.activePage

            EtfPage {}
            NotesPage {}
            SettingsPage {}
        }

        Rectangle {
            visible: !window.compactLayout
            Layout.preferredWidth: visible ? 308 : 0
            Layout.fillHeight: true
            color: "#0b1117"
            border.color: "#1f2d3d"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Label {
                    text: "复盘摘要"
                    color: "#e5edf6"
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: activePage === 0 ? etfController.statusMessage
                         : activePage === 1 ? notesController.statusMessage
                         : settingsController.statusMessage
                    color: "#8fa4ba"
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#223142"
                }

                ColumnLayout {
                    visible: activePage === 0
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "ETF 核心"; color: "#cbd5e1"; font.bold: true; Layout.fillWidth: true }
                    Label {
                        text: etfController.summary.headline || "等待运行"
                        color: "#e0f2fe"
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                    Label {
                        text: etfController.summary.conclusion || ""
                        color: "#8fa4ba"
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label { text: "累计收益  " + pct(etfController.metrics.cumulativeReturn); color: "#d7e3ef"; Layout.fillWidth: true }
                    Label { text: "最大回撤  " + pct(etfController.metrics.maxDrawdown); color: "#d7e3ef"; Layout.fillWidth: true }
                    Label { text: "平均现金  " + pct(etfController.metrics.averageCashRatio); color: "#d7e3ef"; Layout.fillWidth: true }
                    Label { text: "交易次数  " + (etfController.metrics.tradeCount || "--"); color: "#d7e3ef"; Layout.fillWidth: true }
                    Label {
                        text: etfController.dataIssues.length > 0 ? "数据异常  " + etfController.dataIssues.length + " 条" : "数据质量  OK"
                        color: etfController.dataIssues.length > 0 ? "#fca5a5" : "#86efac"
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout {
                    visible: activePage === 1
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "笔记"; color: "#cbd5e1"; font.bold: true; Layout.fillWidth: true }
                    Label { text: "当前列表  " + notesController.notes.length + " 条"; color: "#d7e3ef"; Layout.fillWidth: true }
                    Label { text: "模板覆盖盘前、盘中、盘后、交易和投研假设。"; color: "#8fa4ba"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }

                ColumnLayout {
                    visible: activePage === 2
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "运行环境"; color: "#cbd5e1"; font.bold: true; Layout.fillWidth: true }
                    Label { text: settingsController.dataRoot; color: "#8fa4ba"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                }

                Item { Layout.fillHeight: true }
                Label {
                    text: "MyQuant 提供证据和复盘材料，不提供买卖指令。"
                    color: "#66788d"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
