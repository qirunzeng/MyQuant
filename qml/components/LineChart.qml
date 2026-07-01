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
    property int viewStartIndex: 0
    property int viewEndIndex: Math.max(0, points.length - 1)
    property int hoverIndex: -1
    property real pressX: 0
    property int pressStartIndex: 0
    property int pressEndIndex: 0

    function resetView() {
        viewStartIndex = 0
        viewEndIndex = Math.max(0, points.length - 1)
    }

    function normalizeView() {
        var count = points ? points.length : 0
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

    function plotRect() {
        var padL = showLabels ? 54 : 0
        var padR = showLabels ? 14 : 0
        var padT = showLabels ? 24 : 0
        var padB = showLabels ? 24 : 0
        return { left: padL, right: width - padR, top: padT, bottom: height - padB,
                 width: Math.max(1, width - padL - padR), height: Math.max(1, height - padT - padB) }
    }

    function indexAtX(x) {
        normalizeView()
        var r = plotRect()
        var span = Math.max(1, viewEndIndex - viewStartIndex)
        var ratio = Math.max(0, Math.min(1, (x - r.left) / r.width))
        return Math.max(viewStartIndex, Math.min(viewEndIndex, Math.round(viewStartIndex + ratio * span)))
    }

    function zoomAt(x, zoomIn) {
        var count = points ? points.length : 0
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

    onPointsChanged: resetView()
    onValueRoleChanged: requestPaint()
    onSecondRoleChanged: requestPaint()
    onFormatModeChanged: requestPaint()
    onDecimalsChanged: requestPaint()
    onShowLabelsChanged: requestPaint()
    onLabelPrefixChanged: requestPaint()
    onViewStartIndexChanged: requestPaint()
    onViewEndIndexChanged: requestPaint()
    onHoverIndexChanged: requestPaint()

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
            var count = root.points ? root.points.length : 0
            var r = root.plotRect()
            var span = root.pressEndIndex - root.pressStartIndex + 1
            var shift = Math.round((root.pressX - mouse.x) / r.width * span)
            var nextStart = Math.max(0, Math.min(root.pressStartIndex + shift, Math.max(0, count - span)))
            root.viewStartIndex = nextStart
            root.viewEndIndex = Math.min(count - 1, nextStart + span - 1)
        }
        onExited: root.hoverIndex = -1
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

        function rangeFor(role) {
            var minV = Infinity
            var maxV = -Infinity
            var lastV = NaN
            for (var i = viewStartIndex; i <= viewEndIndex; ++i) {
                var v = Number(points[i] ? points[i][role] : NaN)
                if (!isFinite(v))
                    continue
                minV = Math.min(minV, v)
                maxV = Math.max(maxV, v)
                lastV = v
            }
            if (!isFinite(minV) || !isFinite(maxV))
                return null
            if (Math.abs(maxV - minV) < 0.000001) {
                maxV += 1
                minV -= 1
            }
            return { min: minV, max: maxV, last: lastV }
        }

        var primary = rangeFor(valueRole)
        if (!points || points.length < 2 || !primary)
            return

        function xAt(i) {
            return r.left + (i - viewStartIndex) / Math.max(1, viewEndIndex - viewStartIndex) * r.width
        }
        function yAt(v, range) {
            return r.top + r.height - (v - range.min) / (range.max - range.min) * r.height
        }
        function draw(role, color, range) {
            if (!range)
                return
            ctx.strokeStyle = color
            ctx.lineWidth = 2
            ctx.beginPath()
            var started = false
            for (var j = viewStartIndex; j <= viewEndIndex; ++j) {
                var v = Number(points[j] ? points[j][role] : NaN)
                if (!isFinite(v))
                    continue
                var x = xAt(j)
                var y = yAt(v, range)
                if (!started) {
                    ctx.moveTo(x, y)
                    started = true
                } else {
                    ctx.lineTo(x, y)
                }
            }
            ctx.stroke()
        }

        draw(valueRole, lineColor, primary)
        if (secondRole.length > 0)
            draw(secondRole, secondColor, rangeFor(secondRole))

        if (showLabels) {
            ctx.font = "11px sans-serif"
            ctx.fillStyle = "#7890a8"
            ctx.textAlign = "left"
            ctx.fillText(formatValue(primary.max), 6, r.top + 8)
            ctx.fillText(formatValue(primary.min), 6, r.bottom - 2)
            ctx.fillText(String(points[viewStartIndex].date || ""), r.left, height - 7)
            ctx.textAlign = "right"
            ctx.fillStyle = "#9fb2c7"
            ctx.fillText((labelPrefix.length > 0 ? labelPrefix + " " : "末值 ") + formatValue(primary.last), width - 8, 15)
            ctx.fillStyle = "#7890a8"
            ctx.fillText(String(points[viewEndIndex].date || ""), width - 8, height - 7)
        }

        if (hoverIndex >= viewStartIndex && hoverIndex <= viewEndIndex) {
            var row = points[hoverIndex] || {}
            var hv = Number(row[valueRole])
            if (isFinite(hv)) {
                var hx = xAt(hoverIndex)
                var hy = yAt(hv, primary)
                ctx.strokeStyle = "#64748b"
                ctx.setLineDash([4, 4])
                ctx.beginPath()
                ctx.moveTo(hx, r.top)
                ctx.lineTo(hx, r.bottom)
                ctx.moveTo(r.left, hy)
                ctx.lineTo(r.right, hy)
                ctx.stroke()
                ctx.setLineDash([])

                var tip = String(row.date || "") + "  " + formatValue(hv)
                var tw = Math.min(width - 16, Math.max(118, ctx.measureText(tip).width + 18))
                var tx = Math.max(6, Math.min(width - tw - 6, hx + 10))
                var ty = Math.max(6, hy - 32)
                ctx.fillStyle = "#101b26"
                ctx.strokeStyle = "#334155"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.rect(tx, ty, tw, 24)
                ctx.fill()
                ctx.stroke()
                ctx.fillStyle = "#e8eef7"
                ctx.textAlign = "left"
                ctx.fillText(tip, tx + 9, ty + 16)
            }
        }
    }
}
