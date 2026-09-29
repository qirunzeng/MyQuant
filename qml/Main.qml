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
    color: AppTheme.background

    Component.onCompleted: AppTheme.light = settingsController.theme !== "dark"
    Connections {
        target: settingsController
        function onSettingsChanged() { AppTheme.light = settingsController.theme !== "dark" }
    }

    palette.window: AppTheme.background
    palette.windowText: AppTheme.text
    palette.base: AppTheme.input
    palette.text: AppTheme.inputText
    palette.button: AppTheme.button
    palette.buttonText: AppTheme.inputText
    palette.highlight: AppTheme.accent
    palette.highlightedText: "#ffffff"

    property int activePage: 0
    property bool summaryVisible: width >= 1120
    readonly property var navItems: [
        { label: "ETF", sub: "轮动复盘" },
        { label: "NOTE", sub: "复盘笔记" },
        { label: "DIV", sub: "股息日历" },
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
            Layout.preferredWidth: 88
            Layout.fillHeight: true
            color: AppTheme.navigation
            border.color: AppTheme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 14

                Label {
                    text: "MyQuant"
                    color: AppTheme.text
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
                            color: index === window.activePage ? AppTheme.accentSoft : "transparent"
                            border.color: index === window.activePage ? AppTheme.accent : AppTheme.border
                            border.width: 1
                        }
                        contentItem: Column {
                            spacing: 2
                            anchors.centerIn: parent
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: index === window.activePage ? AppTheme.accent : AppTheme.textMuted
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.sub
                                color: AppTheme.textFaint
                                font.pixelSize: 10
                            }
                        }
                        onClicked: window.activePage = index
                    }
                }

                Item { Layout.fillHeight: true }
                ToolButton {
                    Layout.fillWidth: true
                    text: window.summaryVisible ? "◫" : "▯"
                    ToolTip.visible: hovered
                    ToolTip.text: window.summaryVisible ? "隐藏辅助侧栏" : "显示辅助侧栏"
                    onClicked: window.summaryVisible = !window.summaryVisible
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            StackLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 560
                currentIndex: window.activePage

                EtfPage {}
                NotesPage {}
                DividendPage {}
                SettingsPage {}
            }

            Rectangle {
                visible: window.summaryVisible
                SplitView.preferredWidth: visible ? 292 : 0
                SplitView.minimumWidth: visible ? 230 : 0
                SplitView.maximumWidth: visible ? 460 : 0
                color: AppTheme.surface
                border.color: AppTheme.border
                border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Label {
                    text: "复盘摘要"
                    color: AppTheme.text
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: activePage === 0 ? etfController.statusMessage
                         : activePage === 1 ? notesController.statusMessage
                         : activePage === 2 ? dividendController.statusMessage
                         : settingsController.statusMessage
                    color: AppTheme.textMuted
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: AppTheme.divider
                }

                ColumnLayout {
                    visible: activePage === 0
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "ETF 核心"; color: AppTheme.text; font.bold: true; Layout.fillWidth: true }
                    Label {
                        text: etfController.summary.headline || "等待运行"
                        color: AppTheme.text
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                    Label {
                        text: etfController.summary.conclusion || ""
                        color: AppTheme.textMuted
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label { text: "累计收益  " + pct(etfController.metrics.cumulativeReturn); color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "最大回撤  " + pct(etfController.metrics.maxDrawdown); color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "平均现金  " + pct(etfController.metrics.averageCashRatio); color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "交易次数  " + (etfController.metrics.tradeCount || "--"); color: AppTheme.text; Layout.fillWidth: true }
                    Label {
                        text: etfController.dataIssues.length > 0 ? "数据异常  " + etfController.dataIssues.length + " 条" : "数据质量  OK"
                        color: etfController.dataIssues.length > 0 ? AppTheme.negative : AppTheme.positive
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout {
                    visible: activePage === 1
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "笔记"; color: AppTheme.text; font.bold: true; Layout.fillWidth: true }
                    Label { text: "当前列表  " + notesController.notes.length + " 条"; color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "按日、周、月追踪判断、执行和改进。"; color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }

                ColumnLayout {
                    visible: activePage === 2
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "股息安排"; color: AppTheme.text; font.bold: true; Layout.fillWidth: true }
                    Label { text: "持仓  " + dividendController.holdings.length + " 项"; color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "本月事件  " + dividendController.events.length + " 项"; color: AppTheme.text; Layout.fillWidth: true }
                    Label { text: "预测仅基于历史现金分红，不代表公司承诺。"; color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }

                ColumnLayout {
                    visible: activePage === 3
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "运行环境"; color: AppTheme.text; font.bold: true; Layout.fillWidth: true }
                    Label { text: settingsController.dataRoot; color: AppTheme.textMuted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                }

                Item { Layout.fillHeight: true }
                Label {
                    text: "MyQuant 提供证据和复盘材料，不提供买卖指令。"
                    color: AppTheme.textFaint
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        }
    }
}
