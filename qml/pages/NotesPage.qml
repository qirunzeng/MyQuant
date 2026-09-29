import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    id: page
    property string selectedCardKey: ""
    property string selectedType: "day"
    property string selectedDate: todayText()
    property string selectedPeriodKey: weekKey(todayText())
    property bool showLegacy: false
    property int selectedLegacyId: 0
    property var legacyDraft: notesController.createFromTemplate("盘后复盘")
    property var marketDraft: notesController.createMarketDay(todayText())
    property var watchDraft: notesController.createWatchlistItem(todayText())
    property var tradeDraft: notesController.createTradeExecution(todayText())
    property var weekDraft: notesController.createPeriodSummary("week", weekKey(todayText()))
    property var monthDraft: notesController.createPeriodSummary("month", notesController.activeMonth)
    readonly property int scrollBarGutter: 18

    function two(n) { return n < 10 ? "0" + n : "" + n }
    function formColumns(widthValue) { return 2 }
    function valueSpan(widthValue) { return Math.max(1, formColumns(widthValue) - 1) }
    function todayText() {
        var d = new Date()
        return d.getFullYear() + "-" + two(d.getMonth() + 1) + "-" + two(d.getDate())
    }
    function monthToDate(month) { return new Date(Number(month.slice(0, 4)), Number(month.slice(5, 7)) - 1, 1) }
    function formatMonth(d) { return d.getFullYear() + "-" + two(d.getMonth() + 1) }
    function monthOptions() {
        var start = new Date(2026, 5, 1)
        var end = monthToDate(notesController.activeMonth || todayText().slice(0, 7))
        var today = new Date()
        if (end < today)
            end = new Date(today.getFullYear(), today.getMonth(), 1)
        var out = []
        for (var d = new Date(end); d >= start; d.setMonth(d.getMonth() - 1))
            out.push(formatMonth(d))
        return out
    }
    function nextMonth() {
        var opts = monthOptions()
        var max = opts.length > 0 ? opts[0] : "2026-06"
        var d = monthToDate(max)
        d.setMonth(d.getMonth() + 1)
        return formatMonth(d)
    }
    function dayLabel(dateText) {
        var d = new Date(dateText)
        if (isNaN(d.getTime()))
            return dateText
        return (d.getMonth() + 1) + "." + d.getDate()
    }
    function weekKey(dateText) {
        var d = new Date(dateText + "T12:00:00")
        if (isNaN(d.getTime()))
            d = new Date()
        d.setHours(0, 0, 0, 0)
        d.setDate(d.getDate() + 3 - (d.getDay() + 6) % 7)
        var weekYear = d.getFullYear()
        var week1 = new Date(weekYear, 0, 4)
        var week = 1 + Math.round(((d - week1) / 86400000 - 3 + (week1.getDay() + 6) % 7) / 7)
        return weekYear + "-W" + two(week)
    }
    function pctText(v) {
        var n = Number(v)
        if (!isFinite(n))
            return "--"
        return (n * 100).toFixed(2) + "%"
    }
    function money(v) {
        var n = Number(v)
        if (!isFinite(n))
            return "--"
        return n.toFixed(2)
    }
    function clone(o) { return JSON.parse(JSON.stringify(o || {})) }
    function setField(objName, key, value) {
        if (page[objName] && page[objName][key] === value)
            return
        var o = clone(page[objName])
        o[key] = value
        page[objName] = o
    }
    function rowsForDate(rows, key, dateText) {
        var out = []
        rows = rows || []
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i][key] === dateText)
                out.push(rows[i])
        }
        return out
    }
    function summaryFor(type, key) {
        var rows = notesController.periodSummaries || []
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].period_type === type && rows[i].period_key === key)
                return clone(rows[i])
        }
        return notesController.createPeriodSummary(type, key)
    }
    function loadLegacy(note) {
        legacyDraft = clone(note)
        selectedLegacyId = legacyDraft.id
    }
    function loadMonth(month) {
        notesController.ensureMonth(month)
        showLegacy = false
        selectedCardKey = ""
        selectedType = "day"
        selectedDate = month + "-01"
        selectedPeriodKey = weekKey(selectedDate)
        marketDraft = notesController.createMarketDay(selectedDate)
        watchDraft = notesController.createWatchlistItem(selectedDate)
        tradeDraft = notesController.createTradeExecution(selectedDate)
        weekDraft = notesController.createPeriodSummary("week", selectedPeriodKey)
        monthDraft = notesController.createPeriodSummary("month", month)
    }
    function chooseCard(card) {
        showLegacy = false
        selectedCardKey = card.key || ""
        selectedType = card.type || "day"
        if (selectedType === "day") {
            selectedDate = card.date
            selectedPeriodKey = weekKey(selectedDate)
            var markets = rowsForDate(notesController.marketDays, "review_date", selectedDate)
            marketDraft = markets.length > 0 ? clone(markets[0]) : notesController.createMarketDay(selectedDate)
            watchDraft = notesController.createWatchlistItem(selectedDate)
            tradeDraft = notesController.createTradeExecution(selectedDate)
        } else if (selectedType === "week") {
            selectedPeriodKey = card.periodKey
            weekDraft = summaryFor("week", selectedPeriodKey)
        } else {
            selectedPeriodKey = notesController.activeMonth
            monthDraft = summaryFor("month", selectedPeriodKey)
        }
    }
    function createDay() {
        var date = selectedDate
        if (!date || date.slice(0, 7) !== notesController.activeMonth)
            date = notesController.activeMonth + "-01"
        selectedType = "day"
        selectedDate = date
        selectedPeriodKey = weekKey(date)
        marketDraft = notesController.createMarketDay(date)
        watchDraft = notesController.createWatchlistItem(date)
        tradeDraft = notesController.createTradeExecution(date)
    }
    function createWeek() {
        selectedType = "week"
        selectedPeriodKey = weekKey(selectedDate || notesController.activeMonth + "-01")
        selectedCardKey = "W:" + selectedPeriodKey
        weekDraft = notesController.createPeriodSummary("week", selectedPeriodKey)
    }
    function createMonth() {
        selectedType = "month"
        selectedPeriodKey = notesController.activeMonth
        selectedCardKey = "M:" + notesController.activeMonth
        monthDraft = notesController.createPeriodSummary("month", notesController.activeMonth)
    }
    function saveDailyMarket() { notesController.saveMarketDay(marketDraft) }
    function saveDailyWatch() { notesController.saveWatchlistItem(watchDraft) }
    function saveDailyTrade() { notesController.saveTradeExecution(tradeDraft) }
    function saveSummary() {
        if (selectedType === "week") {
            selectedCardKey = "W:" + selectedPeriodKey
            notesController.savePeriodSummary(weekDraft)
        } else if (selectedType === "month") {
            selectedCardKey = "M:" + notesController.activeMonth
            notesController.savePeriodSummary(monthDraft)
        }
    }

    Component.onCompleted: page.loadMonth(notesController.activeMonth)

    Connections {
        target: notesController
        function onReviewDataChanged() {
            if (page.selectedType === "day") {
                var markets = page.rowsForDate(notesController.marketDays, "review_date", page.selectedDate)
                if (markets.length > 0)
                    page.marketDraft = page.clone(markets[0])
            } else if (page.selectedType === "week") {
                page.weekDraft = page.summaryFor("week", page.selectedPeriodKey)
            } else if (page.selectedType === "month") {
                page.monthDraft = page.summaryFor("month", notesController.activeMonth)
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 14
        orientation: Qt.Horizontal

        SectionPanel {
            title: "复盘月份"
            SplitView.preferredWidth: 214
            SplitView.minimumWidth: 170
            SplitView.maximumWidth: 340

            Label {
                text: "从 2026-06 开始。月份不自动生成空表，只在创建日/周/月卡片后保存。"
                color: AppTheme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: monthJump
                    text: notesController.activeMonth
                    placeholderText: "YYYY-MM"
                    Layout.fillWidth: true
                    onAccepted: page.loadMonth(text)
                }
                Button {
                    text: "跳转"
                    Layout.preferredWidth: 58
                    onClicked: page.loadMonth(monthJump.text)
                }
            }

            Button {
                text: "新建月份 " + page.nextMonth()
                Layout.fillWidth: true
                onClicked: page.loadMonth(page.nextMonth())
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: page.monthOptions()
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 42
                    radius: 6
                    color: modelData === notesController.activeMonth ? AppTheme.accent : (index % 2 === 0 ? AppTheme.surfaceAlt : AppTheme.surface)
                    border.color: modelData === notesController.activeMonth ? AppTheme.accent : AppTheme.border
                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: modelData === notesController.activeMonth ? "#ecfeff" : AppTheme.text
                        font.bold: modelData === notesController.activeMonth
                    }
                    MouseArea { anchors.fill: parent; onClicked: page.loadMonth(modelData) }
                }
            }

            Button { text: "旧笔记"; Layout.fillWidth: true; onClicked: page.showLegacy = true }
            Button { text: "导出本月"; Layout.fillWidth: true; onClicked: notesController.exportMonth(notesController.activeMonth) }
        }

        SectionPanel {
            title: page.showLegacy ? "旧笔记" : "复盘时间线 · " + notesController.activeMonth
            SplitView.fillWidth: true
            SplitView.minimumWidth: 520

            RowLayout {
                visible: !page.showLegacy
                Layout.fillWidth: true
                spacing: 8
                Button { text: "新建日表"; onClicked: page.createDay() }
                Button { text: "新建周表"; onClicked: page.createWeek() }
                Button { text: "新建月表"; onClicked: page.createMonth() }
                Item { Layout.fillWidth: true }
                Label { text: notesController.statusMessage; color: AppTheme.accent; elide: Text.ElideRight; Layout.fillWidth: true }
            }

            Flickable {
                id: cardFlick
                visible: !page.showLegacy
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                clip: true
                contentWidth: Math.max(width, cardRow.width)
                contentHeight: 46
                boundsBehavior: Flickable.StopAtBounds
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: function(event) {
                        cardFlick.contentX = Math.max(0, Math.min(cardFlick.contentWidth - cardFlick.width,
                                                                  cardFlick.contentX - event.angleDelta.y))
                    }
                }

                Row {
                    id: cardRow
                    height: 42
                    spacing: 8
                    Repeater {
                        model: notesController.timelineCards
                        delegate: Rectangle {
                            property bool active: modelData.key === page.selectedCardKey
                            width: modelData.type === "day" ? 74 : 104
                            height: 38
                            radius: 7
                            color: active ? AppTheme.accent : AppTheme.surfaceAlt
                            border.color: active ? AppTheme.accent : AppTheme.border
                            border.width: 1
                            Column {
                                anchors.centerIn: parent
                                spacing: 1
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    color: active ? "#ecfeff" : AppTheme.text
                                    font.bold: true
                                }
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.type === "day" ? modelData.count + " 条" : modelData.periodKey
                                    color: active ? "#cffafe" : AppTheme.textFaint
                                    font.pixelSize: 10
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: page.chooseCard(modelData) }
                        }
                    }
                }
            }

            Label {
                visible: !page.showLegacy && notesController.timelineCards.length === 0
                text: "这个月份还没有复盘卡片。先点“新建日表 / 新建周表 / 新建月表”，保存后会出现在上方时间线。"
                color: AppTheme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Flickable {
                id: bodyFlick
                visible: !page.showLegacy
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: Math.max(1, width - page.scrollBarGutter)
                contentHeight: bodyColumn.implicitHeight + page.scrollBarGutter
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar { orientation: Qt.Vertical; policy: ScrollBar.AsNeeded }
                ScrollBar.horizontal: AppScrollBar {
                    orientation: Qt.Horizontal
                    policy: bodyFlick.width < bodyFlick.contentWidth ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                }

                ColumnLayout {
                    id: bodyColumn
                    width: bodyFlick.contentWidth
                    spacing: 12

                    ColumnLayout {
                        visible: page.selectedType === "day"
                        Layout.fillWidth: true
                        spacing: 12

                        SectionPanel {
                            title: "每日市场 · " + page.selectedDate
                            Layout.fillWidth: true

                            GridLayout {
                                Layout.fillWidth: true
                                columns: page.formColumns(bodyFlick.width)
                                rowSpacing: 8
                                columnSpacing: 8
                                Label { text: "日期"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.review_date || ""; Layout.fillWidth: true; onTextChanged: page.setField("marketDraft", "review_date", text) }
                                Label { text: "大盘状态"; color: AppTheme.textMuted }
                                ComboBox { model: ["上涨", "下跌", "震荡"]; currentIndex: Math.max(0, model.indexOf(page.marketDraft.market_status || "震荡")); Layout.fillWidth: true; onCurrentTextChanged: page.setField("marketDraft", "market_status", currentText) }
                                Label { text: "指数涨跌"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.index_change || ""; Layout.fillWidth: true; placeholderText: "沪深300 +0.8%"; onTextChanged: page.setField("marketDraft", "index_change", text) }
                                Label { text: "成交量"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.volume_change || ""; Layout.fillWidth: true; placeholderText: "放量 / 缩量"; onTextChanged: page.setField("marketDraft", "volume_change", text) }
                                Label { text: "市场主线"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.market_theme || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; onTextChanged: page.setField("marketDraft", "market_theme", text) }
                                Label { text: "强势板块"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.strong_sectors || ""; Layout.fillWidth: true; onTextChanged: page.setField("marketDraft", "strong_sectors", text) }
                                Label { text: "弱势板块"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.weak_sectors || ""; Layout.fillWidth: true; onTextChanged: page.setField("marketDraft", "weak_sectors", text) }
                                Label { text: "情绪"; color: AppTheme.textMuted }
                                ComboBox { model: ["亢奋", "中性", "恐慌", "分歧"]; currentIndex: Math.max(0, model.indexOf(page.marketDraft.emotion || "中性")); Layout.fillWidth: true; onCurrentTextChanged: page.setField("marketDraft", "emotion", currentText) }
                                Label { text: "判断/心得"; color: AppTheme.textMuted }
                                TextField { text: page.marketDraft.market_view || ""; Layout.fillWidth: true; placeholderText: "看多/看空/中性 + 一句话理由"; onTextChanged: page.setField("marketDraft", "market_view", text) }
                                Label { text: "新闻/财报"; color: AppTheme.textMuted; Layout.alignment: Qt.AlignTop }
                                TextArea { text: page.marketDraft.news || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; Layout.preferredHeight: 92; wrapMode: TextEdit.WordWrap; onTextChanged: page.setField("marketDraft", "news", text) }
                            }
                            Button { text: "保存每日市场"; Layout.alignment: Qt.AlignRight; onClicked: page.saveDailyMarket() }
                        }

                        SectionPanel {
                            title: "当日交易"
                            Layout.fillWidth: true

                            ListView {
                                property int rowCount: page.rowsForDate(notesController.tradeExecutions, "trade_date", page.selectedDate).length
                                Layout.fillWidth: true
                                Layout.preferredHeight: rowCount > 0 ? Math.min(116, rowCount * 34) : 0
                                visible: rowCount > 0
                                clip: true
                                model: page.rowsForDate(notesController.tradeExecutions, "trade_date", page.selectedDate)
                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 34
                                    color: index % 2 === 0 ? AppTheme.surfaceAlt : AppTheme.surface
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        Label { text: modelData.action || "--"; color: AppTheme.accent; Layout.preferredWidth: 48 }
                                        Label { text: (modelData.symbol || "--") + " " + (modelData.name || ""); color: AppTheme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                                        Label { text: page.money(modelData.price); color: AppTheme.textMuted; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                                        Label { text: page.pctText(modelData.actual_return); color: Number(modelData.actual_return) >= 0 ? AppTheme.positive : AppTheme.negative; Layout.preferredWidth: 78; horizontalAlignment: Text.AlignRight }
                                    }
                                    MouseArea { anchors.fill: parent; onClicked: page.tradeDraft = page.clone(modelData) }
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: page.formColumns(bodyFlick.width)
                                rowSpacing: 8
                                columnSpacing: 8
                                Button { text: "新建交易"; onClicked: page.tradeDraft = notesController.createTradeExecution(page.selectedDate) }
                                Button { text: "保存交易"; onClicked: page.saveDailyTrade() }
                                Button { text: "归档"; enabled: page.tradeDraft.id > 0; onClicked: notesController.archiveTradeExecution(page.tradeDraft.id) }
                                Item { Layout.fillWidth: true }
                                Label { text: "日期"; color: AppTheme.textMuted }
                                TextField { text: page.tradeDraft.trade_date || ""; Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "trade_date", text) }
                                Label { text: "方向"; color: AppTheme.textMuted }
                                ComboBox { model: ["买入", "卖出", "加仓", "减仓", "清仓"]; currentIndex: Math.max(0, model.indexOf(page.tradeDraft.action || "买入")); Layout.fillWidth: true; onCurrentTextChanged: page.setField("tradeDraft", "action", currentText) }
                                Label { text: "标的"; color: AppTheme.textMuted }
                                TextField { text: page.tradeDraft.symbol || ""; Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "symbol", text) }
                                Label { text: "名称"; color: AppTheme.textMuted }
                                TextField { text: page.tradeDraft.name || ""; Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "name", text) }
                                Label { text: "成交价"; color: AppTheme.textMuted }
                                TextField { text: String(page.tradeDraft.price || ""); Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "price", text) }
                                Label { text: "仓位"; color: AppTheme.textMuted }
                                TextField { text: String(page.tradeDraft.position_pct || ""); Layout.fillWidth: true; placeholderText: "0.10 / 10%"; onTextChanged: page.setField("tradeDraft", "position_pct", text) }
                                Label { text: "理由"; color: AppTheme.textMuted; Layout.alignment: Qt.AlignTop }
                                TextArea { text: page.tradeDraft.trade_reasons || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; Layout.preferredHeight: 70; wrapMode: TextEdit.WordWrap; onTextChanged: page.setField("tradeDraft", "trade_reasons", text) }
                                Label { text: "目标"; color: AppTheme.textMuted }
                                TextField { text: String(page.tradeDraft.target_price || ""); Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "target_price", text) }
                                Label { text: "止损"; color: AppTheme.textMuted }
                                TextField { text: String(page.tradeDraft.stop_loss || ""); Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "stop_loss", text) }
                                Label { text: "收益"; color: AppTheme.textMuted }
                                TextField { text: String(page.tradeDraft.actual_return || ""); Layout.fillWidth: true; placeholderText: "0.05 / 5% / -3%"; onTextChanged: page.setField("tradeDraft", "actual_return", text) }
                                Label { text: "归因"; color: AppTheme.textMuted }
                                ComboBox { model: ["待复盘", "判断错", "执行错", "仓位错", "运气差"]; currentIndex: Math.max(0, model.indexOf(page.tradeDraft.error_attribution || "待复盘")); Layout.fillWidth: true; onCurrentTextChanged: page.setField("tradeDraft", "error_attribution", currentText) }
                                Label { text: "改进"; color: AppTheme.textMuted }
                                TextField { text: page.tradeDraft.next_improvement || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; onTextChanged: page.setField("tradeDraft", "next_improvement", text) }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: page.selectedType === "day"
                        Layout.fillWidth: true
                        spacing: 12

                        SectionPanel {
                            title: "当日观察"
                            Layout.fillWidth: true

                            ColumnLayout {
                                Layout.fillWidth: true
                                ListView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 120
                                    clip: true
                                    model: page.rowsForDate(notesController.watchlistItems, "review_date", page.selectedDate)
                                    delegate: Rectangle {
                                        width: ListView.view.width
                                        height: 44
                                        color: index % 2 === 0 ? AppTheme.surfaceAlt : AppTheme.surface
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            Label { text: modelData.priority || "B"; color: AppTheme.accent; Layout.preferredWidth: 30 }
                                            Label { text: (modelData.symbol || "--") + " " + (modelData.name || ""); color: AppTheme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                                        }
                                        MouseArea { anchors.fill: parent; onClicked: page.watchDraft = page.clone(modelData) }
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: page.formColumns(bodyFlick.width)
                                    rowSpacing: 8
                                    columnSpacing: 8
                                    Button { text: "新建观察"; onClicked: page.watchDraft = notesController.createWatchlistItem(page.selectedDate) }
                                    Button { text: "保存观察"; onClicked: page.saveDailyWatch() }
                                    Button { text: "归档"; enabled: page.watchDraft.id > 0; onClicked: notesController.archiveWatchlistItem(page.watchDraft.id) }
                                    Item { Layout.fillWidth: true }
                                    Label { text: "代码"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.symbol || ""; Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "symbol", text) }
                                    Label { text: "名称"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.name || ""; Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "name", text) }
                                    Label { text: "主题"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.industry_theme || ""; Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "industry_theme", text) }
                                    Label { text: "价格"; color: AppTheme.textMuted }
                                    TextField { text: String(page.watchDraft.current_price || ""); Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "current_price", text) }
                                    Label { text: "理由"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.watch_reason || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "watch_reason", text) }
                                    Label { text: "触发"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.trigger_condition || ""; Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "trigger_condition", text) }
                                    Label { text: "无效"; color: AppTheme.textMuted }
                                    TextField { text: page.watchDraft.invalidation_condition || ""; Layout.fillWidth: true; onTextChanged: page.setField("watchDraft", "invalidation_condition", text) }
                                }
                            }
                        }
                    }

                    SectionPanel {
                        visible: page.selectedType === "week" || page.selectedType === "month"
                        title: page.selectedType === "week" ? "每周总结 · " + page.selectedPeriodKey : "每月总结 · " + notesController.activeMonth
                        Layout.fillWidth: true

                        GridLayout {
                            Layout.fillWidth: true
                            columns: page.formColumns(bodyFlick.width)
                            rowSpacing: 8
                            columnSpacing: 8
                            property string draftName: page.selectedType === "month" ? "monthDraft" : "weekDraft"
                            property string typeName: page.selectedType === "month" ? "month" : "week"
                            Button { text: "保存总结"; onClicked: page.saveSummary() }
                            Button { text: "归档"; enabled: page[parent.draftName].id > 0; onClicked: notesController.archivePeriodSummary(page[parent.draftName].id) }
                            Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                            Label { text: "周期"; color: AppTheme.textMuted }
                            TextField { text: page[parent.draftName].period_key || ""; Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "period_key", text) }
                            Label { text: "收益率"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].total_return || ""); Layout.fillWidth: true; placeholderText: "0.05 / 5% / -3%"; onTextChanged: page.setField(parent.draftName, "total_return", text) }
                            Label { text: "基准"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].benchmark_return || ""); Layout.fillWidth: true; placeholderText: "0.02 / 2%"; onTextChanged: page.setField(parent.draftName, "benchmark_return", text) }
                            Label { text: "超额"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].excess_return || ""); Layout.fillWidth: true; placeholderText: "-1.5%"; onTextChanged: page.setField(parent.draftName, "excess_return", text) }
                            Label { text: "胜率"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].win_rate || ""); Layout.fillWidth: true; placeholderText: "55%"; onTextChanged: page.setField(parent.draftName, "win_rate", text) }
                            Label { text: "盈亏比"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].payoff_ratio || ""); Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "payoff_ratio", text) }
                            Label { text: "最大回撤"; color: AppTheme.textMuted }
                            TextField { text: String(page[parent.draftName].max_drawdown || ""); Layout.fillWidth: true; placeholderText: "-8%"; onTextChanged: page.setField(parent.draftName, "max_drawdown", text) }
                            Label { text: "最佳交易"; color: AppTheme.textMuted }
                            TextField { text: page[parent.draftName].best_trade || ""; Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "best_trade", text) }
                            Label { text: "最差交易"; color: AppTheme.textMuted }
                            TextField { text: page[parent.draftName].worst_trade || ""; Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "worst_trade", text) }
                            Label { text: "常见错误"; color: AppTheme.textMuted }
                            TextField { text: page[parent.draftName].common_mistake || ""; Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "common_mistake", text) }
                            Label { text: "唯一改进规则"; color: AppTheme.textMuted }
                            TextField { text: page[parent.draftName].next_rule || ""; Layout.fillWidth: true; onTextChanged: page.setField(parent.draftName, "next_rule", text) }
                            Label { text: "心得"; color: AppTheme.textMuted; Layout.alignment: Qt.AlignTop }
                            TextArea { text: page[parent.draftName].notes || ""; Layout.columnSpan: page.valueSpan(bodyFlick.width); Layout.fillWidth: true; Layout.preferredHeight: 150; wrapMode: TextEdit.WordWrap; onTextChanged: page.setField(parent.draftName, "notes", text) }
                        }
                    }
                }
            }

            RowLayout {
                visible: page.showLegacy
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                SectionPanel {
                    title: "旧笔记列表"
                    Layout.preferredWidth: 330
                    Layout.fillHeight: true
                    TextField { placeholderText: "搜索旧笔记"; Layout.fillWidth: true; onTextChanged: notesController.refresh(text) }
                    Button { text: "新建旧笔记"; Layout.fillWidth: true; onClicked: { page.legacyDraft = notesController.createFromTemplate("盘后复盘"); page.selectedLegacyId = 0 } }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: notesController.notes
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 62
                            color: modelData.id === page.selectedLegacyId ? AppTheme.accentSoft : (index % 2 === 0 ? AppTheme.surfaceAlt : AppTheme.surface)
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                Label { text: modelData.title; color: AppTheme.text; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: modelData.category + " · " + modelData.reviewDate; color: AppTheme.textFaint; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                            MouseArea { anchors.fill: parent; onClicked: page.loadLegacy(modelData) }
                        }
                    }
                }
                SectionPanel {
                    title: "旧笔记编辑"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { text: page.legacyDraft.title || ""; Layout.fillWidth: true; onTextChanged: page.setField("legacyDraft", "title", text) }
                        Button { text: "保存"; onClicked: notesController.saveNote(page.legacyDraft) }
                        Button { text: "导出"; enabled: page.selectedLegacyId > 0; onClicked: notesController.exportNote(page.selectedLegacyId) }
                    }
                    TextArea {
                        text: page.legacyDraft.content || ""
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        wrapMode: TextEdit.WordWrap
                        color: AppTheme.text
                        onTextChanged: page.setField("legacyDraft", "content", text)
                    }
                }
            }
        }
    }
}
