import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Label { text: "设置"; color: "#eef6ff"; font.pixelSize: 24; font.bold: true }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            SectionPanel {
                title: "数据与 Python"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label { text: "数据目录"; color: "#8fa4ba" }
                TextField { text: settingsController.dataRoot; readOnly: true; Layout.fillWidth: true }
                Label { text: "Python 路径"; color: "#8fa4ba" }
                TextField { text: settingsController.pythonPath; Layout.fillWidth: true; onTextChanged: settingsController.pythonPath = text }
                Label { text: "AkShare 来源顺序"; color: "#8fa4ba" }
                TextField { text: settingsController.dataSources; Layout.fillWidth: true; onTextChanged: settingsController.dataSources = text }
                RowLayout {
                    Button { text: "保存设置"; onClicked: settingsController.save() }
                    Button { text: "打开数据目录"; onClicked: settingsController.openDataRoot() }
                    Button { text: "清缓存"; onClicked: settingsController.clearCache() }
                }
            }

            SectionPanel {
                title: "LLM 增强"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label { text: "OpenAI-compatible Base URL"; color: "#8fa4ba" }
                TextField { text: settingsController.llmBaseUrl; placeholderText: "https://api.openai.com/v1"; Layout.fillWidth: true; onTextChanged: settingsController.llmBaseUrl = text }
                Label { text: "API Key"; color: "#8fa4ba" }
                TextField { text: settingsController.llmApiKey; echoMode: TextInput.Password; Layout.fillWidth: true; onTextChanged: settingsController.llmApiKey = text }
                Label { text: "Model"; color: "#8fa4ba" }
                TextField { text: settingsController.llmModel; Layout.fillWidth: true; onTextChanged: settingsController.llmModel = text }
                Label {
                    text: "LLM 只用于投研核验和结构化解释；本地规则在无 API 时仍可运行。"
                    color: "#8fa4ba"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            SectionPanel {
                title: "外观"
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                ComboBox {
                    model: ["dark", "focus"]
                    currentIndex: Math.max(0, model.indexOf(settingsController.theme))
                    Layout.fillWidth: true
                    onCurrentTextChanged: settingsController.theme = currentText
                }
                Label {
                    text: "第一版以深色专业复盘台为主，后续可以扩展更多主题。"
                    color: "#8fa4ba"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
