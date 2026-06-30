import QtQuick

Canvas {
    id: root
    property var points: []
    property string valueRole: "nav"
    property string secondRole: ""
    property color lineColor: "#38bdf8"
    property color secondColor: "#f97316"
    property color gridColor: "#273545"
    property string formatMode: "number"
    property int decimals: 2
    property bool showLabels: true
    property string labelPrefix: ""

    onPointsChanged: requestPaint()
    onValueRoleChanged: requestPaint()
    onSecondRoleChanged: requestPaint()
    onFormatModeChanged: requestPaint()
    onDecimalsChanged: requestPaint()
    onShowLabelsChanged: requestPaint()
    onLabelPrefixChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = "#0b1117"
        ctx.fillRect(0, 0, width, height)

        var padL = showLabels ? 54 : 0
        var padR = showLabels ? 14 : 0
        var padT = showLabels ? 24 : 0
        var padB = showLabels ? 24 : 0
        var plotW = Math.max(1, width - padL - padR)
        var plotH = Math.max(1, height - padT - padB)

        ctx.strokeStyle = gridColor
        ctx.lineWidth = 1
        for (var g = 1; g < 4; ++g) {
            var y = padT + plotH * g / 4
            ctx.beginPath()
            ctx.moveTo(padL, y)
            ctx.lineTo(padL + plotW, y)
            ctx.stroke()
        }

        function formatValue(v) {
            var n = Number(v)
            if (!isFinite(n))
                return "--"
            if (formatMode === "growthPercent")
                return ((n - 1) * 100).toFixed(decimals) + "%"
            if (formatMode === "percent")
                return (n * 100).toFixed(decimals) + "%"
            return n.toFixed(decimals)
        }

        function draw(role, color) {
            if (!points || points.length < 2)
                return null
            var vals = []
            var minV = Infinity
            var maxV = -Infinity
            var lastV = NaN
            var firstDate = ""
            var lastDate = ""
            for (var i = 0; i < points.length; ++i) {
                var v = Number(points[i][role])
                if (!isFinite(v))
                    continue
                vals.push({i: i, v: v})
                minV = Math.min(minV, v)
                maxV = Math.max(maxV, v)
                lastV = v
                if (firstDate.length === 0)
                    firstDate = String(points[i].date || "")
                lastDate = String(points[i].date || "")
            }
            if (vals.length < 2)
                return null
            if (Math.abs(maxV - minV) < 0.000001) {
                maxV += 1
                minV -= 1
            }
            ctx.strokeStyle = color
            ctx.lineWidth = 2
            ctx.beginPath()
            for (var j = 0; j < vals.length; ++j) {
                var x = padL + vals[j].i / Math.max(1, points.length - 1) * plotW
                var yy = padT + plotH - (vals[j].v - minV) / (maxV - minV) * plotH
                if (j === 0)
                    ctx.moveTo(x, yy)
                else
                    ctx.lineTo(x, yy)
            }
            ctx.stroke()
            return { min: minV, max: maxV, last: lastV, firstDate: firstDate, lastDate: lastDate }
        }

        var primary = draw(valueRole, lineColor)
        if (secondRole.length > 0)
            draw(secondRole, secondColor)
        if (showLabels && primary) {
            ctx.font = "11px sans-serif"
            ctx.fillStyle = "#7890a8"
            ctx.textAlign = "left"
            ctx.fillText(formatValue(primary.max), 6, padT + 8)
            ctx.fillText(formatValue(primary.min), 6, padT + plotH - 2)
            ctx.fillText(primary.firstDate, padL, height - 7)
            ctx.textAlign = "right"
            ctx.fillStyle = "#9fb2c7"
            ctx.fillText((labelPrefix.length > 0 ? labelPrefix + " " : "末值 ") + formatValue(primary.last), width - 8, 15)
            ctx.fillStyle = "#7890a8"
            ctx.fillText(primary.lastDate, width - padR, height - 7)
            ctx.textAlign = "left"
        }
    }
}
