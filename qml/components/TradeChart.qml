import QtQuick

Canvas {
    id: root
    property var series: []
    property var trades: []
    property color lineColor: "#38bdf8"
    property color buyColor: "#22c55e"
    property color sellColor: "#ef4444"
    property color gridColor: "#273545"

    onSeriesChanged: requestPaint()
    onTradesChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = "#0b1117"
        ctx.fillRect(0, 0, width, height)

        var padL = 8
        var padR = 8
        var padT = 10
        var padB = 18
        var w = Math.max(1, width - padL - padR)
        var h = Math.max(1, height - padT - padB)

        ctx.strokeStyle = gridColor
        ctx.lineWidth = 1
        for (var g = 1; g < 4; ++g) {
            var y = padT + h * g / 4
            ctx.beginPath()
            ctx.moveTo(padL, y)
            ctx.lineTo(padL + w, y)
            ctx.stroke()
        }

        if (!series || series.length < 2)
            return

        var minV = Infinity
        var maxV = -Infinity
        var byDate = {}
        for (var i = 0; i < series.length; ++i) {
            var close = Number(series[i].close)
            if (!isFinite(close))
                continue
            minV = Math.min(minV, close)
            maxV = Math.max(maxV, close)
            byDate[String(series[i].date)] = i
        }
        if (!isFinite(minV) || !isFinite(maxV))
            return
        if (Math.abs(maxV - minV) < 0.000001) {
            maxV += 1
            minV -= 1
        }

        function xAt(i) {
            return padL + i / Math.max(1, series.length - 1) * w
        }
        function yAt(v) {
            return padT + h - (v - minV) / (maxV - minV) * h
        }

        ctx.strokeStyle = lineColor
        ctx.lineWidth = 2
        ctx.beginPath()
        var started = false
        for (var j = 0; j < series.length; ++j) {
            var v = Number(series[j].close)
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

        if (trades) {
            ctx.font = "10px sans-serif"
            for (var t = 0; t < trades.length; ++t) {
                var tr = trades[t]
                var idx = byDate[String(tr.date)]
                if (idx === undefined)
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
                ctx.fillText(isBuy ? "B" : "S", mx + 6, Math.max(10, my - 6))
            }
        }

        ctx.fillStyle = "#7890a8"
        ctx.font = "10px sans-serif"
        ctx.fillText(Number(minV).toFixed(2), padL, height - 5)
        ctx.fillText(Number(maxV).toFixed(2), padL, 10)
    }
}
