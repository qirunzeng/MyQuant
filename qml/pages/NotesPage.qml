import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    id: page
    property var draft: notesController.createFromTemplate("盘后复盘")
    property int selectedId: 0

    function loadNote(note) {
        draft = JSON.parse(JSON.stringify(note))
        selectedId = draft.id
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        SectionPanel {
            title: "复盘笔记"
            Layout.preferredWidth: 330
            Layout.fillHeight: true

            TextField {
                id: searchEdit
                placeholderText: "搜索标题、正文、标签、标的"
                Layout.fillWidth: true
                onTextChanged: notesController.refresh(text)
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox { id: templateCombo; model: ["盘前计划", "盘中观察", "盘后复盘", "交易复盘", "投研假设"]; Layout.fillWidth: true }
                Button { text: "新建"; onClicked: { page.draft = notesController.createFromTemplate(templateCombo.currentText); page.selectedId = 0 } }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: notesController.notes
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 70
                    radius: 6
                    color: modelData.id === page.selectedId ? "#143044" : (index % 2 === 0 ? "#0c131a" : "#0f1720")
                    MouseArea {
                        anchors.fill: parent
                        onClicked: page.loadNote(modelData)
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Label { text: (modelData.favorite ? "★ " : "") + modelData.title; color: "#e5edf6"; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: modelData.category + " · " + modelData.reviewDate + " · " + modelData.tickers; color: "#8193a8"; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
            }
        }

        SectionPanel {
            title: "编辑器"
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    text: page.draft.title || ""
                    placeholderText: "标题"
                    Layout.fillWidth: true
                    onTextChanged: page.draft.title = text
                }
                Button {
                    text: "保存"
                    onClicked: notesController.saveNote(page.draft)
                }
                Button {
                    text: "导出"
                    enabled: page.selectedId > 0
                    onClicked: notesController.exportNote(page.selectedId)
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 8
                Label { text: "分类"; color: "#8fa4ba" }
                TextField { text: page.draft.category || ""; Layout.fillWidth: true; onTextChanged: page.draft.category = text }
                Label { text: "复盘日"; color: "#8fa4ba" }
                TextField { text: page.draft.reviewDate || ""; Layout.fillWidth: true; onTextChanged: page.draft.reviewDate = text }
                Label { text: "标的"; color: "#8fa4ba" }
                TextField { text: page.draft.tickers || ""; Layout.fillWidth: true; onTextChanged: page.draft.tickers = text }
                Label { text: "标签"; color: "#8fa4ba" }
                TextField { text: page.draft.tags || ""; Layout.fillWidth: true; onTextChanged: page.draft.tags = text }
                Label { text: "情绪"; color: "#8fa4ba" }
                ComboBox {
                    model: ["看多", "看空", "中性", "分歧"]
                    currentIndex: Math.max(0, model.indexOf(page.draft.sentiment || "中性"))
                    Layout.fillWidth: true
                    onCurrentTextChanged: page.draft.sentiment = currentText
                }
                Label { text: "方向"; color: "#8fa4ba" }
                ComboBox {
                    model: ["观察", "加深研究", "等待回调", "降低风险", "复核错误"]
                    currentIndex: Math.max(0, model.indexOf(page.draft.direction || "观察"))
                    Layout.fillWidth: true
                    onCurrentTextChanged: page.draft.direction = currentText
                }
            }

            TextArea {
                text: page.draft.content || ""
                wrapMode: TextEdit.WordWrap
                color: "#dbe7f4"
                selectedTextColor: "#071018"
                selectionColor: "#38bdf8"
                placeholderText: "写下复盘、证据、反证和下一步。"
                Layout.fillWidth: true
                Layout.fillHeight: true
                onTextChanged: page.draft.content = text
            }

            RowLayout {
                Layout.fillWidth: true
                CheckBox { text: "收藏"; checked: page.draft.favorite || false; onToggled: page.draft.favorite = checked }
                CheckBox { text: "归档"; checked: page.draft.archived || false; onToggled: page.draft.archived = checked }
                Item { Layout.fillWidth: true }
                Button {
                    text: "删除"
                    enabled: page.selectedId > 0
                    onClicked: {
                        notesController.deleteNote(page.selectedId)
                        page.draft = notesController.createFromTemplate("盘后复盘")
                        page.selectedId = 0
                    }
                }
            }
        }
    }
}
