import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    id: page

    Binding {
        target: AppTheme
        property: "light"
        value: lightThemeButton.checked
    }

    function reloadFields() {
        pythonField.text = settingsController.pythonPath
        sourceField.text = settingsController.dataSources
        var fees = settingsController.feeSettings
        aStockRate.text = fees.aStockRate
        aStockMinimum.text = fees.aStockMinimum
        aEtfRate.text = fees.aEtfRate
        aEtfMinimum.text = fees.aEtfMinimum
        aSellStampRate.text = fees.aSellStampRate
        hkRate.text = fees.hkRate
        hkMinimum.text = fees.hkMinimum
        usRate.text = fees.usRate
        usMinimum.text = fees.usMinimum
    }

    function feeDraft() {
        return {
            aStockRate: aStockRate.text, aStockMinimum: aStockMinimum.text,
            aEtfRate: aEtfRate.text, aEtfMinimum: aEtfMinimum.text,
            aSellStampRate: aSellStampRate.text,
            hkRate: hkRate.text, hkMinimum: hkMinimum.text,
            usRate: usRate.text, usMinimum: usMinimum.text
        }
    }

    Component.onCompleted: reloadFields()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(320, parent.width)
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22; Layout.topMargin: 20
                Label { text: "设置"; color: AppTheme.text; font.pixelSize: 25; font.bold: true }
                Item { Layout.fillWidth: true }
                Label { text: settingsController.statusMessage; color: AppTheme.textMuted }
            }

            SectionPanel {
                title: "外观"
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Label { text: "界面主题"; color: AppTheme.textMuted }
                ButtonGroup { id: themeGroup }
                RowLayout {
                    Layout.fillWidth: true
                    RadioButton {
                        id: lightThemeButton
                        text: "浅色"; checked: settingsController.theme !== "dark"; ButtonGroup.group: themeGroup
                        onClicked: {
                            settingsController.theme = "light"
                            AppTheme.light = true
                        }
                    }
                    RadioButton {
                        text: "深色"; checked: settingsController.theme === "dark"; ButtonGroup.group: themeGroup
                        onClicked: {
                            settingsController.theme = "dark"
                            AppTheme.light = false
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
                Label { text: "采用 VS Code / Typora 风格的中性灰阶。主题即时预览，保存后在下次启动继续使用。"; color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }

            SectionPanel {
                title: "数据与 Python"
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 760 ? 2 : 1
                    columnSpacing: 14; rowSpacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "数据目录"; color: AppTheme.textMuted }
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 40
                            color: AppTheme.surfaceAlt; border.color: AppTheme.border
                            Label { anchors.fill: parent; anchors.margins: 10; verticalAlignment: Text.AlignVCenter; text: settingsController.dataRoot; color: AppTheme.text; elide: Text.ElideMiddle }
                            ToolTip.visible: dataPathHover.containsMouse
                            ToolTip.text: settingsController.dataRoot
                            MouseArea { id: dataPathHover; anchors.fill: parent; hoverEnabled: true }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "Python 路径"; color: AppTheme.textMuted }
                        TextField { id: pythonField; Layout.fillWidth: true; placeholderText: "/path/to/python" }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "ETF 行情来源顺序"; color: AppTheme.textMuted }
                        TextField { id: sourceField; Layout.fillWidth: true; placeholderText: "em,tx,sina" }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "本地数据策略"; color: AppTheme.textMuted }
                        Label { text: "优先读取缓存，只补齐缺失区间；股息事件按代码合并，不覆盖手工记录。"; color: AppTheme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "打开数据目录"; onClicked: settingsController.openDataRoot() }
                    Button { text: "清理临时缓存"; onClicked: settingsController.clearCache() }
                    Item { Layout.fillWidth: true }
                }
            }

            SectionPanel {
                title: "交易费率"
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Label {
                    text: "费率统一按“每万元成交额”填写，最低收费按每笔填写。港股和美股默认按微牛账户设置；交易后仍应以交割单实际总费用为准。"
                    color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 760 ? 3 : 1
                    columnSpacing: 14; rowSpacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "A 股个股"; color: AppTheme.text; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField { id: aStockRate; Layout.fillWidth: true; placeholderText: "费率，如 0.741"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            TextField { id: aStockMinimum; Layout.fillWidth: true; placeholderText: "最低，如 0.3"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "A 股 ETF"; color: AppTheme.text; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField { id: aEtfRate; Layout.fillWidth: true; placeholderText: "费率，如 0.5"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            TextField { id: aEtfMinimum; Layout.fillWidth: true; placeholderText: "最低，如 0.1"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "A 股个股卖出印花税"; color: AppTheme.text; font.bold: true }
                        TextField { id: aSellStampRate; Layout.fillWidth: true; placeholderText: "每万元，如 5"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "港股 · 微牛综合估算"; color: AppTheme.text; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField { id: hkRate; Layout.fillWidth: true; placeholderText: "每万元费率"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            TextField { id: hkMinimum; Layout.fillWidth: true; placeholderText: "最低收费"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "美股 · 微牛综合估算"; color: AppTheme.text; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField { id: usRate; Layout.fillWidth: true; placeholderText: "每万元费率"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            TextField { id: usMinimum; Layout.fillWidth: true; placeholderText: "最低收费"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        }
                    }
                }
                Label {
                    text: "提示：微牛当前宣传港美股佣金及平台费为 0，但交易税费、监管费和活动政策可能变化。这里保存的是你的估算规则，不替代交割单。"
                    color: AppTheme.textFaint; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11
                }
            }

            SectionPanel {
                title: "数据安全"
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Label {
                    text: "复盘记录与股息持仓分别保存在 data/myquant.db 和 data/dividends.db。应用更新不会覆盖数据库；复盘结构升级前会自动备份。"
                    color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22; Layout.bottomMargin: 22
                Item { Layout.fillWidth: true }
                Button { text: "放弃未保存修改"; onClicked: { settingsController.load(); page.reloadFields() } }
                Button {
                    text: "保存设置"; highlighted: true
                    onClicked: {
                        settingsController.pythonPath = pythonField.text
                        settingsController.dataSources = sourceField.text
                        settingsController.feeSettings = page.feeDraft()
                        settingsController.save()
                    }
                }
            }
        }
    }
}
