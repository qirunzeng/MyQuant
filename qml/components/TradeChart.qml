import QtQuick

Canvas {
    id: root
    property var series: []
    property var trades: []
    property color lineColor: "#38bdf8"
    property color buyColor: "#22c55e"
    property color sellColor: "#ef4444"
    property color gridColor: "#273545"
    property int viewStartIndex: 0
    property int viewEndIndex: Math.max(0, series.length - 1)
    property int hoverIndex: -1
    property int hoverTradeIndex: -1
    property real pressX: 0
    property int pressStartIndex: 0
    property int pressEndIndex: 0

    function resetView() {
        viewStartIndex = 0
        viewEndIndex = Math.max(0, series.length - 1)
    }

    function normalizeView() {
        var count = series ? series.length : 0
        if (count <= 1) {
            viewStartIndex = 0
            viewEndIndex = 0
            return
        }
        if (viewEndIndex >= count || viewEndIndex < viewStartIndex) {
            viewStartIndex = 0
            viewEndIndex = count - 1
        }
        viewStartIndex = Math.max(0, Math.min(viewStartIndex, count - 2))
        viewEndIndex = Math.max(viewStartIndex + 1, Math.min(viewEndIndex, count - 1))
    }

    function plotRect() {
        return { left: 8, right: width - 8, top: 12, bottom: height - 18,
                 width: Math.max(1, width - 16), height: Math.max(1, height - 30) }
    }

    function indexAtX(x) {
        normalizeView()
        var r = plotRect()
        var span = Math.max(1, viewEndIndex - viewStartIndex)
        var ratio = Math.max(0, Math.min(1, (x - r.left) / r.width))
        return Math.max(viewStartIndex, Math.min(viewEndIndex, Math.round(viewStartIndex + ratio * span)))
    }

    function zoomAt(x, zoomIn) {
        var count = series ? series.length : 0
        if (count < 3)
            return
        normalizeView()
        var anchor = indexAtX(x)
        var span = viewEndIndex - viewStartIndex + 1
        var nextSpan = Math.round(span * (zoomIn ? 0.78 : 1.28))
        nextSpan = Math.max(12, Math.min(count, nextSpan))
        var leftWeight = span > 1 ? (anchor - viewStartIndex) / (span - 1) : 0.5
        var nextStart = Math.round(anchor - (nextSpan - 1) * leftWeight)
        nextStart = Math.max(0, Math.min(nextStart, count - nextSpan))
        viewStartIndex = nextStart
        viewEndIndex = nextStart + nextSpan - 1
        requestPaint()
    }

    onSeriesChanged: resetView()
    onTradesChanged: requestPaint()
    onViewStartIndexChanged: requestPaint()
    onViewEndIndexChanged: requestPaint()
    onHoverIndexChanged: requestPaint()
    onHoverTradeIndexChanged: requestPaint()

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) { root.zoomAt(event.position.x, event.angleDelta.y > 0) }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.CrossCursor
        onPressed: function(mouse) {
            root.pressX = mouse.x
            root.pressStartIndex = root.viewStartIndex
            root.pressEndIndex = root.viewEndIndex
        }
        onPositionChanged: function(mouse) {
            root.hoverIndex = root.indexAtX(mouse.x)
            if (!pressed)
                return
            var count = root.series ? root.series.length : 0
            var r = root.plotRect()
            var span = root.pressEndIndex - root.pressStartIndex + 1
            var shift = Math.round((root.pressX - mouse.x) / r.width * span)
            var nextStart = Math.max(0, Math.min(root.pressStartIndex + shift, Math.max(0, count - span)))
            root.viewStartIndex = nextStart
            root.viewEndIndex = Math.min(count - 1, nextStart + span - 1)
        }
        onExited: {
            root.hoverIndex = -1
            root.hoverTradeIndex = -1
        }
        onDoubleClicked: root.resetView()
    }

    onPaint: {
        normalizeView()
        var ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = "#0b1117"
        ctx.fillRect(0, 0, width, height)

        var r = plotRect()
        ctx.strokeStyle = gridColor
        ctx.lineWidth = 1
        for (var g = 1; g < 4; ++g) {
            var gy = r.top + r.height * g / 4
            ctx.beginPath()
            ctx.moveTo(r.left, gy)
            ctx.lineTo(r.right, gy)
            ctx.stroke()
        }

        if (!series || series.length < 2)
            return

        var minV = Infinity
        var maxV = -Infinity
        var byDate = {}
        for (var i = 0; i < series.length; ++i)
            byDate[String(series[i].date)] = i
        for (var ri = viewStartIndex; ri <= viewEndIndex; ++ri) {
            var close = Number(series[ri] ? series[ri].close : NaN)
            if (!isFinite(close))
                continue
            minV = Math.min(minV, close)
            maxV = Math.max(maxV, close)
        }
        if (!isFinite(minV) || !isFinite(maxV))
            return
        if (Math.abs(maxV - minV) < 0.000001) {
            maxV += 1
            minV -= 1
        }

        function xAt(i) {
            return r.left + (i - viewStartIndex) / Math.max(1, viewEndIndex - viewStartIndex) * r.width
        }
        function yAt(v) {
            return r.top + r.height - (v - minV) / (maxV - minV) * r.height
        }

        ctx.strokeStyle = lineColor
        ctx.lineWidth = 2
        ctx.beginPath()
        var started = false
        for (var j = viewStartIndex; j <= viewEndIndex; ++j) {
            var v = Number(series[j] ? series[j].close : NaN)
            if (!isFinite(v))
                continue
            var x = xAt(j)
            var yv = yAt(v)
            if (!started) {
                ctx.moveTo(x, yv)
                started = true
            } else {
                ctx.lineTo(x, yv)
            }
        }
        ctx.stroke()

        var closestTrade = -1
        var closestDist = 999999
        if (trades) {
            ctx.font = "10px sans-serif"
            for (var t = 0; t < trades.length; ++t) {
                var tr = trades[t]
                var idx = byDate[String(tr.date)]
                if (idx === undefined || idx < viewStartIndex || idx > viewEndIndex)
                    continue
                var px = Number(tr.price)
                if (!isFinite(px))
                    px = Number(series[idx].close)
                var mx = xAt(idx)
                var my = yAt(px)
                var isBuy = String(tr.side) === "BUY"
                ctx.fillStyle = isBuy ? buyColor : sellColor
                ctx.beginPath()
                ctx.arc(mx, my, 4.5, 0, Math.PI * 2)
                ctx.fill()
                var tag = isBuy ? "B" : "S"
                var tagW = ctx.measureText(tag).width
                var tx = Math.max(r.left + 2, Math.min(r.right - tagW - 2, mx + 6))
                var ty = Math.max(r.top + 10, Math.min(r.bottom - 2, my - 6))
                ctx.fillText(tag, tx, ty)
                if (hoverIndex >= 0) {
                    var d = Math.abs(idx - hoverIndex)
                    if (d < closestDist && d <= 2) {
                        closestDist = d
                        closestTrade = t
                    }
                }
            }
        }
        hoverTradeIndex = closestTrade

        ctx.fillStyle = "#7890a8"
        ctx.font = "10px sans-serif"
        ctx.fillText(Number(minV).toFixed(2), r.left, height - 5)
        ctx.fillText(Number(maxV).toFixed(2), r.left, 10)

        if (hoverIndex >= viewStartIndex && hoverIndex <= viewEndIndex) {
            var row = series[hoverIndex] || {}
            var hv = Number(row.close)
            if (isFinite(hv)) {
                var hx = xAt(hoverIndex)
                var hy = yAt(hv)
                ctx.strokeStyle = "#64748b"
                ctx.setLineDash([4, 4])
                ctx.beginPath()
                ctx.moveTo(hx, r.top)
                ctx.lineTo(hx, r.bottom)
                ctx.moveTo(r.left, hy)
                ctx.lineTo(r.right, hy)
                ctx.stroke()
                ctx.setLineDash([])

                var tip = String(row.date || "") + "  收盘 " + hv.toFixed(2)
                if (hoverTradeIndex >= 0) {
                    var htr = trades[hoverTradeIndex]
                    tip = String(htr.date || "") + "  " + String(htr.side || "") + "  " +
                          Number(htr.price || hv).toFixed(2) + "  " + Number(htr.shares || 0).toFixed(0) +
                          "份  " + String(htr.reason || "")
                }
                var tw = Math.min(width - 16, Math.max(150, ctx.measureText(tip).width + 18))
                var tx2 = Math.max(6, Math.min(width - tw - 6, hx + 10))
                var ty2 = Math.max(6, hy - 32)
                ctx.fillStyle = "#101b26"
                ctx.strokeStyle = "#334155"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.rect(tx2, ty2, tw, 24)
                ctx.fill()
                ctx.stroke()
                ctx.fillStyle = "#e8eef7"
                ctx.textAlign = "left"
                ctx.fillText(tip, tx2 + 9, ty2 + 16)
            }
        }
    }
}
