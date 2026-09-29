import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    id: page
    property var holdingDraft: dividendController.newHolding()
    property var tradeDraft: dividendController.newTrade()
    property var feeEditDraft: ({})
    property string editorMode: "holding"
    property string selectedDate: ""
    property int forecastYears: 1
    readonly property var monthNames: ["一月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月"]

    function clone(value) { return JSON.parse(JSON.stringify(value || {})) }
    function setDraft(target, key, value) {
        var copy = clone(page[target]); copy[key] = value; page[target] = copy
    }
    function startTrade(row, side) {
        var draft = dividendController.newTrade()
        draft.holding_id = row.id
        draft.market = row.market
        draft.symbol = row.symbol
        draft.name = row.name
        draft.currency = row.currency
        draft.instrument_type = row.instrument_type || "stock"
        draft.side = side
        page.tradeDraft = draft
        page.editorMode = "trade"
    }
    function setTradeField(key, value, recalculateFee) {
        var copy = clone(page.tradeDraft)
        copy[key] = value
        if (recalculateFee) {
            var amount = Number(copy.shares || 0) * Number(copy.price || 0)
            copy.fee = dividendController.estimateTradeFee(copy.market, copy.instrument_type, copy.side, amount).toFixed(2)
            copy.fee_is_actual = false
        } else if (key === "fee") {
            copy.fee_is_actual = true
        }
        page.tradeDraft = copy
    }
    function startFeeEdit(row) {
        page.feeEditDraft = clone(row)
        page.editorMode = "fee"
    }
    function money(value) {
        var n = Number(value); return isFinite(n) ? n.toFixed(2) : "--"
    }
    function forecastValue(row) {
        if (forecastYears === 3) return row.average3y
        if (forecastYears === 5) return row.average5y
        return row.average1y
    }
    function forecastTotalsText(years) {
        var totals = {}
        var rows = dividendController.forecasts || []
        var field = years === 3 ? "average3y" : years === 5 ? "average5y" : "average1y"
        for (var i = 0; i < rows.length; ++i) {
            var currency = rows[i].currency || "--"
            totals[currency] = (totals[currency] || 0) + Number(rows[i][field] || 0)
        }
        var currencies = Object.keys(totals).sort()
        if (currencies.length === 0) return "--"
        var lines = []
        for (var j = 0; j < currencies.length; ++j)
            lines.push(currencies[j] + "  " + money(totals[currencies[j]]))
        return lines.join("\n")
    }
    function monthlyDividendSummary() {
        var totals = {}
        var payCount = 0
        var recordCount = 0
        var exCount = 0
        var events = dividendController.events || []
        var holdings = dividendController.holdings || []
        for (var i = 0; i < events.length; ++i) {
            var event = events[i]
            if ((event.record_date || "").slice(0, 7) === dividendController.activeMonth) ++recordCount
            if ((event.ex_date || "").slice(0, 7) === dividendController.activeMonth) ++exCount
            if ((event.pay_date || "").slice(0, 7) !== dividendController.activeMonth) continue
            ++payCount
            for (var j = 0; j < holdings.length; ++j) {
                var holding = holdings[j]
                if (holding.market === event.market && holding.symbol === event.symbol) {
                    var currency = event.currency || holding.currency || "--"
                    totals[currency] = (totals[currency] || 0) + Number(event.amount_per_share || 0) * Number(holding.shares || 0)
                    break
                }
            }
        }
        var currencies = Object.keys(totals).sort()
        var amountLines = []
        for (var k = 0; k < currencies.length; ++k)
            amountLines.push(currencies[k] + "  " + money(totals[currencies[k]]))
        return {recordCount: recordCount, exCount: exCount, payCount: payCount,
                totals: amountLines.length ? amountLines.join("\n") : "本月暂无预计派息"}
    }
    function moveMonth(delta) {
        var p = dividendController.activeMonth.split("-")
        var d = new Date(Number(p[0]), Number(p[1]) - 1 + delta, 1)
        dividendController.loadMonth(d.getFullYear() + "-" + String(d.getMonth() + 1).padStart(2, "0"))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Label { text: "股息日历"; color: AppTheme.text; font.pixelSize: 25; font.bold: true }
            Label { text: "登记、除权与派息安排"; color: AppTheme.textMuted; font.pixelSize: 13 }
            Item { Layout.fillWidth: true }
            Button { text: dividendController.refreshing ? "更新中…" : "更新股息数据"; enabled: !dividendController.refreshing; onClicked: dividendController.refreshOnline() }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            AppTabButton { text: "日历" }
            AppTabButton { text: "持仓与预测" }
            AppTabButton { text: "交易与调整" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ColumnLayout {
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "‹"; Layout.preferredWidth: 42; onClicked: page.moveMonth(-1) }
                    Label {
                        text: dividendController.activeMonth.slice(0,4) + "年 " + page.monthNames[Number(dividendController.activeMonth.slice(5,7))-1]
                        color: AppTheme.text; font.pixelSize: 20; font.bold: true
                    }
                    Button { text: "›"; Layout.preferredWidth: 42; onClicked: page.moveMonth(1) }
                    Button { text: "今天"; onClicked: dividendController.loadMonth(Qt.formatDate(new Date(), "yyyy-MM")) }
                    Item { Layout.fillWidth: true }
                    Repeater {
                        model: [{c: AppTheme.accent,t:"登记"},{c: AppTheme.warning,t:"除权"},{c: AppTheme.positive,t:"派息"}]
                        delegate: RowLayout {
                            spacing: 5
                            Rectangle { width: 8; height: 8; radius: 4; color: modelData.c }
                            Label { text: modelData.t; color: AppTheme.textMuted; font.pixelSize: 12 }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: width >= 900 ? 2 : 1
                    columnSpacing: 14; rowSpacing: 14
                    GridLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 520
                        columns: 5; rowSpacing: 1; columnSpacing: 1
                        Repeater {
                            model: ["一","二","三","四","五"]
                            delegate: Label { Layout.fillWidth: true; text: modelData; horizontalAlignment: Text.AlignHCenter; color: AppTheme.textMuted; font.bold: true }
                        }
                        Repeater {
                            model: dividendController.calendarDays
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 76
                                color: modelData.today ? AppTheme.accentSoft : AppTheme.surface
                                border.color: AppTheme.divider
                                border.width: 1
                                Column {
                                    anchors.fill: parent; anchors.margins: 7; spacing: 4
                                    Label { text: modelData.day; color: modelData.inMonth ? AppTheme.text : AppTheme.textFaint; font.bold: modelData.today }
                                    Repeater {
                                        model: modelData.events.slice(0, 3)
                                        delegate: Rectangle {
                                            width: parent.width; height: 18; radius: 3
                                            color: modelData.date_kind === "登记" ? AppTheme.accentSoft : modelData.date_kind === "除权" ? (AppTheme.light ? "#fff3df" : "#3a2910") : (AppTheme.light ? "#e7f7ee" : "#123326")
                                            Label { anchors.fill: parent; anchors.leftMargin: 5; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: modelData.date_kind + " " + (modelData.name || modelData.symbol); color: AppTheme.text; font.pixelSize: 10 }
                                        }
                                    }
                                }
                                MouseArea { anchors.fill: parent; onClicked: page.selectedDate = modelData.date }
                            }
                        }
                    }
                    SectionPanel {
                        id: monthlySummaryPanel
                        Layout.fillHeight: true
                        Layout.fillWidth: parent.columns === 1
                        Layout.preferredWidth: 250
                        title: "本月分红汇总"
                        property var summary: page.monthlyDividendSummary()
                        Label { text: dividendController.activeMonth; color: AppTheme.textMuted }
                        Label { text: "预计派息（税前）"; color: AppTheme.text; font.bold: true }
                        Label { text: monthlySummaryPanel.summary.totals; color: AppTheme.positive; font.pixelSize: 18; font.bold: true; lineHeight: 1.35; Layout.fillWidth: true }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: AppTheme.divider }
                        Label { text: "登记日  " + monthlySummaryPanel.summary.recordCount; color: AppTheme.textMuted }
                        Label { text: "除权日  " + monthlySummaryPanel.summary.exCount; color: AppTheme.textMuted }
                        Label { text: "派息日  " + monthlySummaryPanel.summary.payCount; color: AppTheme.textMuted }
                        Label { text: "只统计当前持仓；已删除持仓的历史事件不会显示。"; color: AppTheme.textFaint; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11 }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            SplitView {
                orientation: Qt.Horizontal
                SectionPanel {
                    title: "我的持仓与分红估算"
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 440
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "按历史现金分红估算；不含税费、汇率和未来政策变化。"
                            color: AppTheme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        TabBar {
                            id: forecastPeriodTabs
                            Layout.preferredWidth: 300
                            currentIndex: 0
                            AppTabButton { text: "近1年" }
                            AppTabButton { text: "近3年" }
                            AppTabButton { text: "近5年" }
                            onCurrentIndexChanged: page.forecastYears = currentIndex === 1 ? 3 : currentIndex === 2 ? 5 : 1
                        }
                    }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: dividendController.forecasts
                        ScrollBar.vertical: AppScrollBar {}
                        delegate: Rectangle {
                            id: holdingRow
                            property bool alternate: index % 2 === 1
                            width: ListView.view.width - 12; height: 96; color: alternate ? AppTheme.surfaceAlt : AppTheme.surface
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 9
                                Label { text: modelData.market; color: AppTheme.accent; font.bold: true; Layout.preferredWidth: 28 }
                                ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 120; spacing: 2
                                    Label { Layout.fillWidth: true; text: (modelData.name || modelData.symbol) + "  " + modelData.symbol; color: AppTheme.text; font.bold: true; elide: Text.ElideRight }
                                    Label { Layout.fillWidth: true; text: (modelData.instrument_type === "etf" ? "ETF · " : "个股 · ") + Number(modelData.shares).toFixed(0) + " 股 · 成本 " + page.money(modelData.cost_price) + " " + modelData.currency; color: AppTheme.textMuted; font.pixelSize: 11; elide: Text.ElideRight }
                                    Label { Layout.fillWidth: true; text: "历史覆盖：" + modelData.confidence; color: AppTheme.textFaint; font.pixelSize: 11; elide: Text.ElideRight }
                                }
                                ColumnLayout {
                                    Layout.preferredWidth: 178; Layout.minimumWidth: 166; spacing: 3
                                    Label {
                                        text: "近" + page.forecastYears + "年年均估算"
                                        color: AppTheme.textMuted; font.pixelSize: 11
                                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                    }
                                    Label {
                                        text: page.money(page.forecastValue(modelData)) + " " + modelData.currency
                                        color: AppTheme.positive; font.bold: true; font.pixelSize: 17
                                        Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 5
                                        AppRowButton { text: "买入"; alternateRow: holdingRow.alternate; Layout.fillWidth: true; onClicked: page.startTrade(modelData, "买入") }
                                        AppRowButton { text: "卖出"; alternateRow: holdingRow.alternate; Layout.fillWidth: true; onClicked: page.startTrade(modelData, "卖出") }
                                        AppRowButton {
                                            text: "编辑"; alternateRow: holdingRow.alternate; Layout.fillWidth: true
                                            onClicked: { page.holdingDraft = page.clone(modelData); page.editorMode = "holding" }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Button { text: "新增持仓"; onClicked: { page.holdingDraft = dividendController.newHolding(); page.editorMode = "holding" } }
                }
                SectionPanel {
                    title: "组合分红统计"
                    SplitView.preferredWidth: 230
                    SplitView.minimumWidth: 200
                    SplitView.maximumWidth: 280
                    Label { text: "按币种分别汇总，不进行隐含汇率换算。"; color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "近1年年均"; color: page.forecastYears === 1 ? AppTheme.accent : AppTheme.text; font.bold: true }
                    Label { text: page.forecastTotalsText(1); color: AppTheme.positive; lineHeight: 1.35; Layout.fillWidth: true }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: AppTheme.divider }
                    Label { text: "近3年年均"; color: page.forecastYears === 3 ? AppTheme.accent : AppTheme.text; font.bold: true }
                    Label { text: page.forecastTotalsText(3); color: AppTheme.positive; lineHeight: 1.35; Layout.fillWidth: true }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: AppTheme.divider }
                    Label { text: "近5年年均"; color: page.forecastYears === 5 ? AppTheme.accent : AppTheme.text; font.bold: true }
                    Label { text: page.forecastTotalsText(5); color: AppTheme.positive; lineHeight: 1.35; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                }
                SectionPanel {
                    title: editorMode === "trade" ? tradeDraft.side + " " + tradeDraft.name
                         : editorMode === "fee" ? "补录交割单费用"
                         : holdingDraft.id > 0 ? "编辑持仓" : "新增持仓"
                    SplitView.preferredWidth: 330; SplitView.minimumWidth: 260; SplitView.maximumWidth: 460
                    ColumnLayout {
                        visible: page.editorMode === "holding"
                        Layout.fillWidth: true
                        ComboBox { model: ["A","H","US"]; currentIndex: Math.max(0, model.indexOf(holdingDraft.market)); Layout.fillWidth: true; onCurrentTextChanged: page.setDraft("holdingDraft","market",currentText) }
                        ComboBox {
                            model: [{text:"个股",value:"stock"},{text:"ETF",value:"etf"}]
                            textRole: "text"; valueRole: "value"; Layout.fillWidth: true
                            currentIndex: holdingDraft.instrument_type === "etf" ? 1 : 0
                            onCurrentValueChanged: page.setDraft("holdingDraft", "instrument_type", currentValue)
                        }
                        TextField { text: holdingDraft.symbol || ""; placeholderText: "代码，如 600519 / 00700 / AAPL"; Layout.fillWidth: true; onTextEdited: page.setDraft("holdingDraft","symbol",text) }
                        TextField { text: holdingDraft.name || ""; placeholderText: "名称"; Layout.fillWidth: true; onTextEdited: page.setDraft("holdingDraft","name",text) }
                        TextField { text: holdingDraft.shares || ""; placeholderText: "持仓股数"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.fillWidth: true; onTextEdited: page.setDraft("holdingDraft","shares",text) }
                        TextField { text: holdingDraft.cost_price || ""; placeholderText: "成本价"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.fillWidth: true; onTextEdited: page.setDraft("holdingDraft","cost_price",text) }
                        ComboBox { model: ["CNY","HKD","USD"]; currentIndex: Math.max(0, model.indexOf(holdingDraft.currency)); Layout.fillWidth: true; onCurrentTextChanged: page.setDraft("holdingDraft","currency",currentText) }
                        Button { text: "保存持仓"; Layout.fillWidth: true; onClicked: dividendController.saveHolding(holdingDraft) }
                        Button { visible: holdingDraft.id > 0; text: "删除持仓"; Layout.fillWidth: true; onClicked: dividendController.archiveHolding(holdingDraft.id) }
                    }
                    ColumnLayout {
                        visible: page.editorMode === "trade"
                        Layout.fillWidth: true
                        Label { text: (tradeDraft.symbol || "") + " · " + (tradeDraft.currency || ""); color: AppTheme.textMuted }
                        TextField { text: tradeDraft.trade_date || ""; placeholderText: "交易日期 YYYY-MM-DD"; Layout.fillWidth: true; onTextEdited: page.setTradeField("trade_date", text, false) }
                        TextField { text: tradeDraft.shares || ""; placeholderText: "数量"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.fillWidth: true; onTextEdited: page.setTradeField("shares", text, true) }
                        TextField { text: tradeDraft.price || ""; placeholderText: "成交价"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.fillWidth: true; onTextEdited: page.setTradeField("price", text, true) }
                        TextField { text: tradeDraft.fee || ""; placeholderText: "预计/实际总费用"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.fillWidth: true; onTextEdited: page.setTradeField("fee", text, false) }
                        Label {
                            text: (tradeDraft.fee_is_actual ? "已手工填写实际费用。" : "当前为默认费率估算。") + " " + dividendController.feeRuleDescription(tradeDraft.market, tradeDraft.instrument_type)
                            color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11
                        }
                        TextArea { text: tradeDraft.note || ""; placeholderText: "备注"; Layout.fillWidth: true; Layout.preferredHeight: 72; wrapMode: TextEdit.Wrap; onTextChanged: page.setTradeField("note", text, false) }
                        Button {
                            text: "确认" + tradeDraft.side; highlighted: true; Layout.fillWidth: true
                            onClicked: if (dividendController.saveTrade(tradeDraft)) { page.tradeDraft = dividendController.newTrade(); page.editorMode = "holding" }
                        }
                        Button { text: "取消"; Layout.fillWidth: true; onClicked: page.editorMode = "holding" }
                    }
                    ColumnLayout {
                        visible: page.editorMode === "fee"
                        Layout.fillWidth: true
                        Label {
                            text: (feeEditDraft.trade_date || "") + " · " + (feeEditDraft.side || "") + " · " + (feeEditDraft.name || feeEditDraft.symbol || "")
                            color: AppTheme.text; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        Label {
                            text: Number(feeEditDraft.shares || 0).toFixed(0) + " 股 × " + page.money(feeEditDraft.price) + " " + (feeEditDraft.currency || "")
                            color: AppTheme.textMuted
                        }
                        TextField {
                            id: actualFeeField
                            text: feeEditDraft.fee === undefined ? "" : feeEditDraft.fee
                            placeholderText: "交割单实际总费用"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            Layout.fillWidth: true
                        }
                        Label {
                            text: {
                                var amount = Number(feeEditDraft.shares || 0) * Number(feeEditDraft.price || 0)
                                var fee = Number(actualFeeField.text || 0)
                                return amount > 0 ? "折合 " + (fee / amount * 10000).toFixed(4) + " / 万" : "折合费率 --"
                            }
                            color: AppTheme.accent
                        }
                        TextArea { id: actualFeeNote; text: feeEditDraft.note || ""; placeholderText: "交割单备注"; Layout.fillWidth: true; Layout.preferredHeight: 88; wrapMode: TextEdit.Wrap }
                        Button {
                            text: "保存实际费用"; highlighted: true; Layout.fillWidth: true
                            onClicked: if (dividendController.updateTradeFee(feeEditDraft.id, Number(actualFeeField.text), actualFeeNote.text)) page.editorMode = "holding"
                        }
                        Button { text: "取消"; Layout.fillWidth: true; onClicked: page.editorMode = "holding" }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            SplitView {
                orientation: Qt.Horizontal
                SectionPanel {
                    title: "买卖流水"
                    SplitView.fillWidth: true; SplitView.minimumWidth: 400
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: dividendController.trades
                        ScrollBar.vertical: AppScrollBar {}
                        delegate: Rectangle {
                            width: ListView.view.width - 12; height: 72; color: index % 2 ? AppTheme.surfaceAlt : AppTheme.surface
                            RowLayout { anchors.fill: parent; anchors.margins: 9
                                ColumnLayout { Layout.fillWidth: true; spacing: 3
                                    Label { text: modelData.trade_date + "  " + modelData.side + "  " + (modelData.name || modelData.symbol) + "  " + modelData.symbol; color: modelData.side === "买入" ? AppTheme.positive : AppTheme.negative; font.bold: true }
                                    Label { text: Number(modelData.shares).toFixed(0) + " 股 × " + page.money(modelData.price) + " " + modelData.currency + "  ·  " + (modelData.fee_is_actual ? "实际费用 " : "预计费用 ") + page.money(modelData.fee) + "（" + Number(modelData.fee_rate_per_ten_thousand || 0).toFixed(4) + "/万）"; color: AppTheme.textMuted; font.pixelSize: 11 }
                                    Label { text: modelData.side === "卖出" ? "已实现盈亏 " + page.money(modelData.realized_pnl) + " " + modelData.currency : (modelData.note || ""); color: Number(modelData.realized_pnl) >= 0 ? AppTheme.positive : AppTheme.negative; font.pixelSize: 11 }
                                }
                                AppRowButton { text: "补录费用"; alternateRow: index % 2 === 1; Layout.preferredWidth: 76; onClicked: page.startFeeEdit(modelData) }
                            }
                        }
                    }
                }
                SectionPanel {
                    title: "自动除息调整"
                    SplitView.preferredWidth: 390; SplitView.minimumWidth: 320; SplitView.maximumWidth: 520
                    Label { text: "除息日按税前每股现金分红调低账面成本。每个公告只处理一次，记录不会因刷新或重启重复生成。"; color: AppTheme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: dividendController.adjustments
                        ScrollBar.vertical: AppScrollBar {}
                        delegate: Rectangle {
                            width: ListView.view.width - 12; height: 82; color: index % 2 ? AppTheme.surfaceAlt : AppTheme.surface
                            ColumnLayout { anchors.fill: parent; anchors.margins: 9; spacing: 3
                                Label { text: modelData.ex_date + "  " + (modelData.name || modelData.symbol); color: AppTheme.text; font.bold: true }
                                Label { text: "成本 " + page.money(modelData.old_cost_price) + " → " + page.money(modelData.new_cost_price) + " " + modelData.currency; color: AppTheme.accent; font.pixelSize: 11 }
                                Label { text: "税前毛额 " + page.money(modelData.gross_amount) + "（" + page.money(modelData.amount_per_share) + "/股 × " + Number(modelData.shares).toFixed(0) + " 股）"; color: AppTheme.textMuted; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }
        }
    }
}
