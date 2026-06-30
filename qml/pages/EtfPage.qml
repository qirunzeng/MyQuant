import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyQuant

Item {
    id: page
    property var portfolioRows: []
    property string availableCashText: "40000"
    property bool suppressPortfolioReload: false
    property int enabledColumnWidth: 68
    property int codeColumnWidth: 104
    property int nameColumnWidth: 220
    property int assetColumnWidth: 154
    property int sharesColumnWidth: 96
    property int costColumnWidth: 92
    readonly property int deleteColumnWidth: 62
    property int tradeChartIndex: 0
    property int activeEtfTab: 0
    readonly property int scrollBarGutter: 14
    readonly property int columnGap: 8
    readonly property int portfolioTableWidth: enabledColumnWidth + codeColumnWidth + nameColumnWidth + assetColumnWidth + sharesColumnWidth + costColumnWidth + deleteColumnWidth + columnGap * 6
    readonly property int portfolioScrollWidth: portfolioTableWidth + scrollBarGutter
    property var assetClassValues: ["equity_cn_growth", "equity_cn_large", "equity_cn_style", "equity_hk", "equity_us", "equity_jp", "equity_kr", "commodity", "bond", "unknown"]
    property var assetClassLabels: ["A股成长", "A股宽基", "A股风格", "港股", "美股", "日本", "韩国", "商品", "债券", "其他"]

    function pct(v) {
        if (v === undefined || v === null || isNaN(Number(v)))
            return "--"
        return (Number(v) * 100).toFixed(2) + "%"
    }

    function money(v) {
        if (v === undefined || v === null || isNaN(Number(v)))
            return "--"
        return Number(v).toFixed(2)
    }

    function currentScrollY() {
        return scroll && scroll.contentItem ? scroll.contentItem.contentY : 0
    }

    function restoreScrollY(y) {
        Qt.callLater(function() {
            if (!scroll || !scroll.contentItem)
                return
            var maxY = Math.max(0, scroll.contentItem.contentHeight - scroll.contentItem.height)
            scroll.contentItem.contentY = Math.max(0, Math.min(y, maxY))
        })
    }

    function currentPortfolioY() {
        return portfolioList ? portfolioList.contentY : 0
    }

    function restorePortfolioY(y) {
        Qt.callLater(function() {
            if (!portfolioList)
                return
            var maxY = Math.max(0, portfolioList.contentHeight - portfolioList.height)
            portfolioList.contentY = Math.max(0, Math.min(y, maxY))
        })
    }

    function assetClassIndex(value) {
        for (var i = 0; i < assetClassValues.length; ++i) {
            if (assetClassValues[i] === value)
                return i
        }
        return assetClassValues.length - 1
    }

    function lastEquityValue(role) {
        var rows = etfController.equity || []
        if (rows.length === 0)
            return undefined
        return rows[rows.length - 1][role]
    }

    function growthFromNav(v) {
        if (v === undefined || v === null || isNaN(Number(v)))
            return "--"
        return pct(Number(v) - 1)
    }

    function clampColumn(v, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, Math.round(v)))
    }

    function columnWidth(column) {
        if (column === "enabled")
            return enabledColumnWidth
        if (column === "code")
            return codeColumnWidth
        if (column === "name")
            return nameColumnWidth
        if (column === "asset")
            return assetColumnWidth
        if (column === "shares")
            return sharesColumnWidth
        if (column === "delete")
            return deleteColumnWidth
        return costColumnWidth
    }

    function setColumnWidth(column, width) {
        if (column === "enabled")
            enabledColumnWidth = clampColumn(width, 54, 110)
        else if (column === "code")
            codeColumnWidth = clampColumn(width, 86, 180)
        else if (column === "name")
            nameColumnWidth = clampColumn(width, 160, 420)
        else if (column === "asset")
            assetColumnWidth = clampColumn(width, 120, 280)
        else if (column === "shares")
            sharesColumnWidth = clampColumn(width, 82, 180)
        else if (column === "cost")
            costColumnWidth = clampColumn(width, 82, 180)
    }

    function columnX(column) {
        if (column === "enabled")
            return 0
        if (column === "code")
            return enabledColumnWidth + columnGap
        if (column === "name")
            return enabledColumnWidth + codeColumnWidth + columnGap * 2
        if (column === "asset")
            return enabledColumnWidth + codeColumnWidth + nameColumnWidth + columnGap * 3
        if (column === "shares")
            return enabledColumnWidth + codeColumnWidth + nameColumnWidth + assetColumnWidth + columnGap * 4
        if (column === "cost")
            return enabledColumnWidth + codeColumnWidth + nameColumnWidth + assetColumnWidth + sharesColumnWidth + columnGap * 5
        return enabledColumnWidth + codeColumnWidth + nameColumnWidth + assetColumnWidth + sharesColumnWidth + costColumnWidth + columnGap * 6
    }

    function tradeChartCount() {
        return etfController.tradeCharts ? etfController.tradeCharts.length : 0
    }

    function clampedTradeChartIndex() {
        var count = tradeChartCount()
        if (count <= 0)
            return 0
        return Math.max(0, Math.min(tradeChartIndex, count - 1))
    }

    function activeTradeChart() {
        var count = tradeChartCount()
        if (count <= 0)
            return ({})
        return etfController.tradeCharts[clampedTradeChartIndex()] || ({})
    }

    function enabledPortfolioCodes() {
        var codes = {}
        for (var i = 0; i < portfolioRows.length; ++i) {
            var row = portfolioRows[i] || {}
            if (row.enabled && String(row.code || "").length > 0)
                codes[String(row.code)] = true
        }
        return codes
    }

    function filteredAdvice() {
        var rows = etfController.advice || []
        var codes = enabledPortfolioCodes()
        var out = []
        for (var i = 0; i < rows.length; ++i) {
            var item = rows[i] || {}
            var symbol = String(item.symbol || "")
            if (symbol.length === 0 || codes[symbol])
                out.push(item)
        }
        return out
    }

    function adviceDetail(item) {
        var reason = item.reason || ""
        var amountLabel = item.action === "CASH" ? "预计现金 " : item.action === "SELL" ? "预计回款 " : item.action === "BUY" ? "预计买入 " : "估算市值 "
        var amount = isNaN(Number(item.amount)) ? "" : amountLabel + money(item.amount)
        var price = isNaN(Number(item.price)) ? "" : "参考价 " + money(item.price)
        var shares = Number(item.shares || 0) > 0 ? "数量 " + Number(item.shares || 0).toFixed(0) + " 份" : ""
        var target = ""
        if (!isNaN(Number(item.targetValue)) && Number(item.targetValue) > 0)
            target = "当前 " + money(item.currentValue || 0) + " / 目标 " + money(item.targetValue)
        var parts = []
        if (reason.length > 0)
            parts.push(reason)
        if (target.length > 0)
            parts.push(target)
        if (shares.length > 0)
            parts.push(shares)
        if (amount.length > 0)
            parts.push(amount)
        if (price.length > 0)
            parts.push(price)
        return parts.join(" · ")
    }

    function actionText(action) {
        if (action === "BUY")
            return "买入"
        if (action === "SELL")
            return "卖出"
        if (action === "HOLD")
            return "持有"
        if (action === "WAIT")
            return "等待"
        if (action === "CASH")
            return "现金"
        return action || "--"
    }

    function actionBg(action) {
        if (action === "BUY")
            return "#065f46"
        if (action === "SELL")
            return "#991b1b"
        if (action === "HOLD")
            return "#1d4ed8"
        if (action === "WAIT")
            return "#854d0e"
        return "#334155"
    }

    function actionFg(action) {
        if (action === "BUY")
            return "#86efac"
        if (action === "SELL")
            return "#fca5a5"
        if (action === "HOLD")
            return "#bfdbfe"
        if (action === "WAIT")
            return "#fde68a"
        return "#cbd5e1"
    }

    function reloadPortfolio() {
        var p = etfController.portfolio || {}
        availableCashText = String(Number(p.availableCash || 0).toFixed(2))
        portfolioRows = JSON.parse(JSON.stringify(p.positions || []))
    }

    function updatePortfolioRow(index, key, value) {
        var y = currentScrollY()
        var tableY = currentPortfolioY()
        var rows = JSON.parse(JSON.stringify(portfolioRows))
        rows[index][key] = value
        portfolioRows = rows
        restoreScrollY(y)
        restorePortfolioY(tableY)
    }

    function addPortfolioRow() {
        var y = currentScrollY()
        var tableY = currentPortfolioY()
        var rows = JSON.parse(JSON.stringify(portfolioRows))
        rows.push({ enabled: true, code: "", name: "", asset_class: "", shares: 0, cost_price: 0, note: "" })
        portfolioRows = rows
        restoreScrollY(y)
        restorePortfolioY(tableY)
    }

    function removePortfolioRow(index) {
        if (index < 0 || index >= portfolioRows.length)
            return
        var y = currentScrollY()
        var tableY = currentPortfolioY()
        var rows = JSON.parse(JSON.stringify(portfolioRows))
        rows.splice(index, 1)
        portfolioRows = rows
        savePortfolio()
        restoreScrollY(y)
        restorePortfolioY(tableY)
    }

    function savePortfolio() {
        var y = currentScrollY()
        var tableY = currentPortfolioY()
        suppressPortfolioReload = true
        var ok = etfController.savePortfolio(Number(availableCashText), portfolioRows)
        Qt.callLater(function() { suppressPortfolioReload = false })
        restoreScrollY(y)
        restorePortfolioY(tableY)
        return ok
    }

    Component.onCompleted: {
        reloadPortfolio()
    }

    Connections {
        target: etfController
        function onPortfolioChanged() {
            if (!page.suppressPortfolioReload)
                page.reloadPortfolio()
        }
        function onResultChanged() {
            if (page.tradeChartIndex >= page.tradeChartCount())
                page.tradeChartIndex = 0
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical: AppScrollBar {
            orientation: Qt.Vertical
            policy: ScrollBar.AlwaysOn
        }

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            Item { Layout.preferredHeight: 22 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: "ETF 轮动复盘台"
                        color: "#eef6ff"
                        font.pixelSize: 24
                        font.bold: true
                    }
                    Label {
                        text: "20 日回归动量、10 日复核、现金缓冲和单标的止损。"
                        color: "#8fa4ba"
                        font.pixelSize: 13
                    }
                }

                Button {
                    text: etfController.running ? "运行中..." : "运行 ETF 轮动"
                    enabled: !etfController.running
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 42
                    onClicked: {
                        page.savePortfolio()
                        etfController.runDefault(true, startEdit.text, endEdit.text,
                                                 holdSpin.value, Number(cashEdit.text),
                                                 Number(stopEdit.text), Number(capitalEdit.text))
                    }
                }

                Button {
                    text: "打开 ETF 目录"
                    onClicked: etfController.openEtfFolder()
                }
            }

            TabBar {
                id: etfTabs
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                currentIndex: page.activeEtfTab
                onCurrentIndexChanged: page.activeEtfTab = currentIndex

                TabButton { text: "运行配置" }
                TabButton { text: "回测数据" }
                TabButton { text: "ETF 池" }
                TabButton { text: "操作建议" }
                TabButton { text: "图表" }
                TabButton { text: "交易" }
            }

            SectionPanel {
                title: "运行与配置"
                visible: page.activeEtfTab === 0
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 210 : 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        columns: 6
                        columnSpacing: 10
                        rowSpacing: 8

                        Label { text: "开始日期"; color: "#92a6bc" }
                        TextField { id: startEdit; text: "20210101"; placeholderText: "YYYYMMDD"; Layout.preferredWidth: 118 }
                        Label { text: "结束日期"; color: "#92a6bc" }
                        TextField { id: endEdit; placeholderText: "留空=上一完整交易日"; Layout.preferredWidth: 150 }
                        Label { text: "持有数"; color: "#92a6bc" }
                        SpinBox { id: holdSpin; from: 1; to: 8; value: 4; Layout.preferredWidth: 110 }

                        Label { text: "现金比例"; color: "#92a6bc" }
                        TextField { id: cashEdit; text: "0.25"; Layout.preferredWidth: 118 }
                        Label { text: "止损"; color: "#92a6bc" }
                        TextField { id: stopEdit; text: "0.07"; Layout.preferredWidth: 150 }
                        Label { text: "回测本金"; color: "#92a6bc" }
                        TextField { id: capitalEdit; text: "40000"; Layout.preferredWidth: 110 }

                        Label { text: "可用现金"; color: "#92a6bc" }
                        TextField {
                            text: page.availableCashText
                            Layout.preferredWidth: 118
                            onEditingFinished: page.availableCashText = text
                        }
                        Label { text: "数据补齐"; color: "#92a6bc" }
                        Label {
                            text: "自动判断缓存缺口，必要时调用 AkShare"
                            color: "#92a6bc"
                            wrapMode: Text.WordWrap
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                        }
                        Label {
                            text: etfController.statusMessage
                            color: "#7dd3fc"
                            elide: Text.ElideRight
                            Layout.columnSpan: 6
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 210
                        Layout.fillHeight: true
                        spacing: 10

                        Label {
                            text: "运行时先检查本地 qfq 缓存；缺少预热或结束日期才调用设置里的 Python/AkShare 补齐。"
                            color: "#8fa4ba"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            RowLayout {
                visible: page.activeEtfTab === 1
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 150 : 0
                spacing: 14

                SectionPanel {
                    title: "策略摘要"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Label {
                        text: etfController.summary.headline || "等待运行"
                        color: "#e8eef7"
                        font.pixelSize: 22
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: etfController.summary.conclusion || "运行后显示目标组合、持仓动作和数据状态。"
                        color: "#9fb2c7"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "BUY " + (etfController.summary.buyCount || 0); color: "#86efac" }
                        Label { text: "SELL " + (etfController.summary.sellCount || 0); color: "#fca5a5" }
                        Label { text: "HOLD " + (etfController.summary.holdCount || 0); color: "#93c5fd" }
                        Label { text: "WAIT " + (etfController.summary.waitCount || 0); color: "#cbd5e1" }
                        Item { Layout.fillWidth: true }
                        Label { text: etfController.summary.period || ""; color: "#64748b" }
                    }
                }

                SectionPanel {
                    title: "数据质量"
                    Layout.preferredWidth: 430
                    Layout.fillHeight: true

                    Label {
                        visible: etfController.dataIssues.length === 0
                        text: "未发现超过阈值的单日跳变。若行情源刚更新，仍建议关注 qfq 数据是否完整。"
                        color: "#8fa4ba"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ListView {
                        visible: etfController.dataIssues.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: AppScrollBar {
                            orientation: Qt.Vertical
                            policy: ScrollBar.AlwaysOn
                        }
                        model: etfController.dataIssues
                        delegate: Rectangle {
                            width: Math.max(1, ListView.view.width - page.scrollBarGutter)
                            height: 42
                            color: index % 2 === 0 ? "#1f1114" : "#261318"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                Label { text: modelData.date || "--"; color: "#fca5a5"; Layout.preferredWidth: 86 }
                                Label { text: (modelData.name || "") + " " + (modelData.symbol || ""); color: "#e8eef7"; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: pct(modelData.return); color: "#fca5a5"; Layout.preferredWidth: 72; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: page.activeEtfTab === 2 || page.activeEtfTab === 3
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 14

                SectionPanel {
                    title: "ETF 池"
                    visible: page.activeEtfTab === 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 392 : 0

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: "保存 ETF 池"
                            onClicked: page.savePortfolio()
                        }
                        Button {
                            text: "新增持仓"
                            onClicked: page.addPortfolioRow()
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: "开关控制回测 ETF 池；资产分类、份额和成本在这里统一维护。可用现金在运行配置页设置。"
                            color: "#8fa4ba"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Flickable {
                        id: portfolioHFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: page.portfolioScrollWidth
                        contentHeight: Math.max(1, height - page.scrollBarGutter)
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.horizontal: AppScrollBar {
                            orientation: Qt.Horizontal
                            policy: ScrollBar.AlwaysOn
                        }

                        Item {
                            id: portfolioTable
                            property real resizeStartX: 0
                            property int resizeStartWidth: 0
                            property string resizeColumn: ""

                            function beginResize(column, area, mouse) {
                                resizeColumn = column
                                resizeStartX = area.mapToItem(page, mouse.x, mouse.y).x
                                resizeStartWidth = page.columnWidth(column)
                            }

                            function dragResize(area, mouse) {
                                var x = area.mapToItem(page, mouse.x, mouse.y).x
                                page.setColumnWidth(resizeColumn, resizeStartWidth + x - resizeStartX)
                            }

                            width: page.portfolioScrollWidth
                            height: Math.max(1, portfolioHFlick.height - page.scrollBarGutter)

                            Item {
                                id: portfolioHeader
                                width: page.portfolioTableWidth
                                height: 30

                                Label { x: page.columnX("enabled"); width: page.enabledColumnWidth; text: "启用"; color: "#6f8499"; font.pixelSize: 11; elide: Text.ElideRight }
                                Label { x: page.columnX("code"); width: page.codeColumnWidth; text: "代码"; color: "#6f8499"; font.pixelSize: 11; elide: Text.ElideRight }
                                Label { x: page.columnX("name"); width: page.nameColumnWidth; text: "名称"; color: "#6f8499"; font.pixelSize: 11; elide: Text.ElideRight }
                                Label { x: page.columnX("asset"); width: page.assetColumnWidth; text: "资产分类"; color: "#6f8499"; font.pixelSize: 11; elide: Text.ElideRight }
                                Label { x: page.columnX("shares"); width: page.sharesColumnWidth; text: "份额"; color: "#6f8499"; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                                Label { x: page.columnX("cost"); width: page.costColumnWidth; text: "成本"; color: "#6f8499"; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                                Label { x: page.columnX("delete"); width: page.deleteColumnWidth; text: "操作"; color: "#6f8499"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }

                                Rectangle {
                                    x: page.columnX("enabled") + page.enabledColumnWidth + page.columnGap / 2 - 1
                                    width: 1
                                    height: parent.height
                                    color: "#2a3a4c"
                                    MouseArea {
                                        anchors.centerIn: parent
                                        width: 12
                                        height: parent.height
                                        cursorShape: Qt.SizeHorCursor
                                        onPressed: function(mouse) { portfolioTable.beginResize("enabled", this, mouse) }
                                        onPositionChanged: function(mouse) { if (pressed) portfolioTable.dragResize(this, mouse) }
                                    }
                                }
                                Rectangle {
                                    x: page.columnX("code") + page.codeColumnWidth + page.columnGap / 2 - 1
                                    width: 1
                                    height: parent.height
                                    color: "#2a3a4c"
                                    MouseArea {
                                        anchors.centerIn: parent
                                        width: 12
                                        height: parent.height
                                        cursorShape: Qt.SizeHorCursor
                                        onPressed: function(mouse) { portfolioTable.beginResize("code", this, mouse) }
                                        onPositionChanged: function(mouse) { if (pressed) portfolioTable.dragResize(this, mouse) }
                                    }
                                }
                                Rectangle {
                                    x: page.columnX("name") + page.nameColumnWidth + page.columnGap / 2 - 1
                                    width: 1
                                    height: parent.height
                                    color: "#2a3a4c"
                                    MouseArea {
                                        anchors.centerIn: parent
                                        width: 12
                                        height: parent.height
                                        cursorShape: Qt.SizeHorCursor
                                        onPressed: function(mouse) { portfolioTable.beginResize("name", this, mouse) }
                                        onPositionChanged: function(mouse) { if (pressed) portfolioTable.dragResize(this, mouse) }
                                    }
                                }
                                Rectangle {
                                    x: page.columnX("asset") + page.assetColumnWidth + page.columnGap / 2 - 1
                                    width: 1
                                    height: parent.height
                                    color: "#2a3a4c"
                                    MouseArea {
                                        anchors.centerIn: parent
                                        width: 12
                                        height: parent.height
                                        cursorShape: Qt.SizeHorCursor
                                        onPressed: function(mouse) { portfolioTable.beginResize("asset", this, mouse) }
                                        onPositionChanged: function(mouse) { if (pressed) portfolioTable.dragResize(this, mouse) }
                                    }
                                }
                                Rectangle {
                                    x: page.columnX("shares") + page.sharesColumnWidth + page.columnGap / 2 - 1
                                    width: 1
                                    height: parent.height
                                    color: "#2a3a4c"
                                    MouseArea {
                                        anchors.centerIn: parent
                                        width: 12
                                        height: parent.height
                                        cursorShape: Qt.SizeHorCursor
                                        onPressed: function(mouse) { portfolioTable.beginResize("shares", this, mouse) }
                                        onPositionChanged: function(mouse) { if (pressed) portfolioTable.dragResize(this, mouse) }
                                    }
                                }
                            }

                            ListView {
                                id: portfolioList
                                x: 0
                                y: portfolioHeader.height + 6
                                width: page.portfolioScrollWidth
                                height: Math.max(1, portfolioTable.height - y)
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: AppScrollBar {
                                    orientation: Qt.Vertical
                                    policy: ScrollBar.AlwaysOn
                                }
                                model: page.portfolioRows
                                delegate: Item {
                                    width: page.portfolioTableWidth
                                    height: 38
                                    Switch {
                                        x: page.columnX("enabled")
                                        width: page.enabledColumnWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        checked: modelData.enabled
                                        onToggled: page.updatePortfolioRow(index, "enabled", checked)
                                    }
                                    TextField {
                                        x: page.columnX("code")
                                        width: page.codeColumnWidth
                                        height: 36
                                        text: modelData.code
                                        placeholderText: "代码"
                                        onEditingFinished: page.updatePortfolioRow(index, "code", text)
                                    }
                                    TextField {
                                        x: page.columnX("name")
                                        width: page.nameColumnWidth
                                        height: 36
                                        text: modelData.name
                                        placeholderText: "名称"
                                        onEditingFinished: page.updatePortfolioRow(index, "name", text)
                                    }
                                    ComboBox {
                                        x: page.columnX("asset")
                                        width: page.assetColumnWidth
                                        height: 36
                                        model: page.assetClassLabels
                                        currentIndex: page.assetClassIndex(modelData.asset_class)
                                        onActivated: function(choice) {
                                            if (choice >= 0 && choice < page.assetClassValues.length)
                                                page.updatePortfolioRow(index, "asset_class", page.assetClassValues[choice])
                                        }
                                    }
                                    TextField {
                                        x: page.columnX("shares")
                                        width: page.sharesColumnWidth
                                        height: 36
                                        text: String(modelData.shares || 0)
                                        placeholderText: "份额"
                                        horizontalAlignment: Text.AlignRight
                                        onEditingFinished: page.updatePortfolioRow(index, "shares", Number(text))
                                    }
                                    TextField {
                                        x: page.columnX("cost")
                                        width: page.costColumnWidth
                                        height: 36
                                        text: String(modelData.cost_price || 0)
                                        placeholderText: "成本"
                                        horizontalAlignment: Text.AlignRight
                                        onEditingFinished: page.updatePortfolioRow(index, "cost_price", Number(text))
                                    }
                                    Button {
                                        x: page.columnX("delete")
                                        width: page.deleteColumnWidth
                                        height: 36
                                        text: "删除"
                                        onClicked: page.removePortfolioRow(index)
                                    }
                                }
                            }
                        }
                    }
                }

                SectionPanel {
                    title: "下一步操作建议"
                    visible: page.activeEtfTab === 3
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 560 : 0

                    Label {
                        visible: page.filteredAdvice().length === 0
                        text: "暂无操作建议。运行 ETF 轮动后，这里会按当前 ETF 池和真实持仓生成更详细的买入、卖出、持有与等待动作。"
                        color: "#8fa4ba"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    ListView {
                        visible: page.filteredAdvice().length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: AppScrollBar {
                            orientation: Qt.Vertical
                            policy: ScrollBar.AlwaysOn
                        }
                        model: page.filteredAdvice()
                        delegate: Rectangle {
                            width: Math.max(1, ListView.view.width - page.scrollBarGutter)
                            height: 104
                            color: index % 2 === 0 ? "#0c131a" : "#0f1720"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 16
                                Rectangle {
                                    radius: 8
                                    color: page.actionBg(modelData.action)
                                    Layout.preferredWidth: 104
                                    Layout.preferredHeight: 64
                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Label {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: page.actionText(modelData.action)
                                            color: "#ffffff"
                                            font.pixelSize: 20
                                            font.bold: true
                                        }
                                        Label {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: modelData.action || "--"
                                            color: "#dbeafe"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Label {
                                        text: (modelData.name || "现金") + (modelData.symbol ? "  " + modelData.symbol : "")
                                        color: "#dbe7f4"
                                        font.pixelSize: 17
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: page.adviceDetail(modelData)
                                        color: "#8fa4ba"
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                                Label {
                                    text: money(modelData.amount)
                                    color: page.actionFg(modelData.action)
                                    font.pixelSize: 24
                                    font.bold: modelData.action === "BUY" || modelData.action === "SELL"
                                    Layout.preferredWidth: 150
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                visible: page.activeEtfTab === 1
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 10
                MetricCard { label: "累计收益"; value: pct(etfController.metrics.cumulativeReturn); hint: etfController.metrics.startDate + " / " + etfController.metrics.endDate; accent: "#22c55e" }
                MetricCard { label: "年化收益"; value: pct(etfController.metrics.annualizedReturn); hint: "annualized"; accent: "#38bdf8" }
                MetricCard { label: "最大回撤"; value: pct(etfController.metrics.maxDrawdown); hint: "drawdown"; accent: "#f97316" }
                MetricCard { label: "Calmar"; value: money(etfController.metrics.calmarRatio); hint: "return / drawdown"; accent: "#a78bfa" }
                MetricCard { label: "平均现金"; value: pct(etfController.metrics.averageCashRatio); hint: "cash buffer"; accent: "#facc15" }
            }

            RowLayout {
                visible: page.activeEtfTab === 1
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 10
                MetricCard { label: "最低现金"; value: pct(etfController.metrics.minCashRatioObserved); hint: "min observed"; accent: "#14b8a6" }
                MetricCard { label: "最差月"; value: pct(etfController.metrics.worstMonthlyReturn); hint: "monthly"; accent: "#fb7185" }
                MetricCard { label: "最佳月"; value: pct(etfController.metrics.bestMonthlyReturn); hint: "monthly"; accent: "#34d399" }
                MetricCard { label: "回撤修复"; value: (etfController.metrics.recoveryDaysAfterMaxDrawdown || 0) + " 天"; hint: "after max dd"; accent: "#60a5fa" }
                MetricCard { label: "风险动作"; value: (etfController.metrics.riskHalfCount || 0) + " / " + (etfController.metrics.stopCount || 0); hint: "半仓 / 止损"; accent: "#f59e0b" }
            }

            RowLayout {
                visible: page.activeEtfTab === 4
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 292 : 0
                spacing: 14

                SectionPanel {
                    title: "净值增长"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "累计增长 " + pct(etfController.metrics.cumulativeReturn); color: "#7dd3fc"; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Label { text: "末值增长 " + growthFromNav(lastEquityValue("nav")); color: "#8fa4ba" }
                    }

                    LineChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: etfController.equity
                        valueRole: "nav"
                        lineColor: "#38bdf8"
                        formatMode: "growthPercent"
                        labelPrefix: "末值"
                    }
                }

                SectionPanel {
                    title: "回撤曲线"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "最大回撤 " + pct(etfController.metrics.maxDrawdown); color: "#fdba74"; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Label { text: "当前回撤 " + pct(lastEquityValue("drawdown")); color: "#8fa4ba" }
                    }

                    LineChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: etfController.equity
                        valueRole: "drawdown"
                        lineColor: "#f97316"
                        formatMode: "percent"
                        labelPrefix: "当前"
                    }
                }
            }

            SectionPanel {
                title: "最新候选"
                visible: page.activeEtfTab === 4
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 250 : 0

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: AppScrollBar {
                        orientation: Qt.Vertical
                        policy: ScrollBar.AlwaysOn
                    }
                    model: etfController.rankings.slice(0, 8)
                    delegate: Rectangle {
                        width: Math.max(1, ListView.view.width - page.scrollBarGutter)
                        height: 52
                        color: index % 2 === 0 ? "#0c131a" : "#0f1720"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "#" + modelData.rank; color: "#64748b"; Layout.preferredWidth: 42 }
                                Label { text: modelData.name + "  " + modelData.symbol; color: "#dbe7f4"; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: Number(modelData.score || 0).toFixed(3); color: modelData.selected ? "#86efac" : "#94a3b8"; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight }
                            }
                            Label {
                                text: "20日 " + pct(modelData.ret20) + " / 60日 " + pct(modelData.ret60) + " / 波动 " + Number(modelData.stdScore || 0).toFixed(4) + " / CV " + Number(modelData.cvScore || 0).toFixed(3)
                                color: "#7890a8"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            SectionPanel {
                title: "BS 点位图"
                visible: page.activeEtfTab === 4
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? (etfController.tradeCharts.length > 0 ? 316 : 110) : 0

                Label {
                    visible: etfController.tradeCharts.length === 0
                    text: "运行后会按每只 ETF 生成独立价格曲线，并标出 BUY / SELL 点位。"
                    color: "#8fa4ba"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    visible: etfController.tradeCharts.length > 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            text: "‹"
                            enabled: page.tradeChartCount() > 1
                            Layout.preferredWidth: 34
                            onClicked: page.tradeChartIndex = Math.max(0, page.clampedTradeChartIndex() - 1)
                        }

                        Flickable {
                            id: bsTabFlick
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            clip: true
                            contentWidth: Math.max(width, bsTabRow.width)
                            contentHeight: 30
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.horizontal: AppScrollBar {
                                orientation: Qt.Horizontal
                                policy: ScrollBar.AsNeeded
                            }

                            Row {
                                id: bsTabRow
                                width: childrenRect.width
                                height: parent.height
                                spacing: 6

                                Repeater {
                                    model: etfController.tradeCharts
                                    delegate: Rectangle {
                                        property bool active: index === page.clampedTradeChartIndex()
                                        width: Math.max(92, Math.min(150, tabText.implicitWidth + 22))
                                        height: 30
                                        radius: 6
                                        color: active ? "#0e7490" : "#0c131a"
                                        border.color: active ? "#22d3ee" : "#243241"
                                        border.width: 1

                                        Label {
                                            id: tabText
                                            anchors.centerIn: parent
                                            width: parent.width - 14
                                            text: (modelData.name || "") + " " + (modelData.symbol || "")
                                            color: active ? "#ecfeff" : "#9fb2c7"
                                            font.pixelSize: 12
                                            font.bold: active
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: page.tradeChartIndex = index
                                        }
                                    }
                                }
                            }
                        }

                        Button {
                            text: "›"
                            enabled: page.tradeChartCount() > 1
                            Layout.preferredWidth: 34
                            onClicked: page.tradeChartIndex = Math.min(page.tradeChartCount() - 1, page.clampedTradeChartIndex() + 1)
                        }
                    }

                    Rectangle {
                        id: activeBsCard
                        property var chart: page.activeTradeChart()
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        radius: 8
                        color: "#0c131a"
                        border.color: "#243241"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: (activeBsCard.chart.name || "") + "  " + (activeBsCard.chart.symbol || "")
                                    color: "#e8eef7"
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: (page.clampedTradeChartIndex() + 1) + " / " + page.tradeChartCount()
                                    color: "#64748b"
                                    Layout.preferredWidth: 54
                                    horizontalAlignment: Text.AlignRight
                                }
                                Label {
                                    text: "末值 " + money(activeBsCard.chart.lastClose)
                                    color: "#8fa4ba"
                                    Layout.preferredWidth: 112
                                    horizontalAlignment: Text.AlignRight
                                    elide: Text.ElideRight
                                }
                            }

                            TradeChart {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                series: activeBsCard.chart.series || []
                                trades: activeBsCard.chart.trades || []
                            }

                            Flickable {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                clip: true
                                contentWidth: Math.max(width, bsTradeRow.width)
                                contentHeight: 28
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.horizontal: AppScrollBar {
                                    orientation: Qt.Horizontal
                                    policy: ScrollBar.AsNeeded
                                }

                                Row {
                                    id: bsTradeRow
                                    height: parent.height
                                    spacing: 18
                                    Repeater {
                                        model: activeBsCard.chart.trades ? activeBsCard.chart.trades.slice(Math.max(0, activeBsCard.chart.trades.length - 3), activeBsCard.chart.trades.length) : []
                                        delegate: Row {
                                            spacing: 6
                                            Label { text: modelData.date || "--"; color: "#7890a8" }
                                            Label { text: modelData.side || ""; color: modelData.side === "BUY" ? "#86efac" : "#fca5a5"; font.bold: true }
                                            Label { text: money(modelData.price); color: "#cbd5e1" }
                                            Label { width: 96; text: modelData.reason || ""; color: "#7890a8"; elide: Text.ElideRight }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SectionPanel {
                title: "策略口径"
                visible: page.activeEtfTab === 1
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 142 : 0
                Label {
                    text: "AkShare qfq 日线；20 日加权回归动量乘 R2；固定 10 个交易日复核；T 日信号、T+1 OHLC4 近似成交；目标现金 25%；单标的固定止损；高波动或成交额不稳定时半仓。"
                    color: "#9fb2c7"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: "开关控制回测 ETF 池；买入数量按 A 股 ETF 一手 100 份取整；单日跳变超过 20% 会被视为复权/分红异常候选并阻止回测。"
                    color: "#8fa4ba"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            SectionPanel {
                title: "完整交易明细"
                visible: page.activeEtfTab === 5
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.preferredHeight: visible ? 520 : 0

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: AppScrollBar {
                        orientation: Qt.Vertical
                        policy: ScrollBar.AlwaysOn
                    }
                    model: etfController.trades
                    delegate: Rectangle {
                        width: Math.max(1, ListView.view.width - page.scrollBarGutter)
                        height: 54
                        color: index % 2 === 0 ? "#0c131a" : "#0f1720"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.date || "--"; color: "#8fa4ba"; Layout.preferredWidth: 88 }
                                Label { text: "信号 " + (modelData.signalDate || "--"); color: "#64748b"; Layout.preferredWidth: 112 }
                                Label { text: modelData.side || ""; color: modelData.side === "BUY" ? "#86efac" : "#fca5a5"; Layout.preferredWidth: 42 }
                                Label { text: (modelData.name || "") + " " + (modelData.symbol || ""); color: "#dbe7f4"; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: money(modelData.price); color: "#cbd5e1"; Layout.preferredWidth: 74; horizontalAlignment: Text.AlignRight }
                                Label { text: Number(modelData.shares || 0).toFixed(0) + " 份"; color: "#cbd5e1"; Layout.preferredWidth: 76; horizontalAlignment: Text.AlignRight }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.reason || ""; color: "#7890a8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: "交易前现金 " + money(modelData.cashBefore); color: "#64748b"; Layout.preferredWidth: 150; horizontalAlignment: Text.AlignRight }
                                Label { text: "收盘净值 " + money(modelData.navAfter); color: "#64748b"; Layout.preferredWidth: 140; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 18 }
        }
    }
}
