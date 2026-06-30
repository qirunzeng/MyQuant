#!/usr/bin/env python3
import math
import os
import struct
import subprocess
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "resources" / "icons"
ICONSET = ICON_DIR / "MyQuant.iconset"
ICNS = ICON_DIR / "MyQuant.icns"


def clamp(value):
    return max(0, min(255, int(round(value))))


def blend(dst, src):
    sr, sg, sb, sa = src
    dr, dg, db, da = dst
    a = sa / 255.0
    out_a = a + da / 255.0 * (1 - a)
    if out_a <= 0:
        return (0, 0, 0, 0)
    r = (sr * a + dr * da / 255.0 * (1 - a)) / out_a
    g = (sg * a + dg * da / 255.0 * (1 - a)) / out_a
    b = (sb * a + db * da / 255.0 * (1 - a)) / out_a
    return (clamp(r), clamp(g), clamp(b), clamp(out_a * 255))


def rounded_mask(x, y, w, h, radius):
    cx = min(max(x, radius), w - radius)
    cy = min(max(y, radius), h - radius)
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius


def set_px(img, size, x, y, color):
    if 0 <= x < size and 0 <= y < size:
        idx = y * size + x
        img[idx] = blend(img[idx], color)


def fill_circle(img, size, cx, cy, r, color):
    x0 = max(0, int(cx - r - 1))
    x1 = min(size, int(cx + r + 2))
    y0 = max(0, int(cy - r - 1))
    y1 = min(size, int(cy + r + 2))
    rr = r * r
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= rr:
                set_px(img, size, x, y, color)


def draw_line(img, size, x0, y0, x1, y1, width, color):
    steps = max(1, int(math.hypot(x1 - x0, y1 - y0) * 1.5))
    for i in range(steps + 1):
        t = i / steps
        x = x0 + (x1 - x0) * t
        y = y0 + (y1 - y0) * t
        fill_circle(img, size, x, y, width / 2, color)


def write_png(path, size, pixels):
    def chunk(name, data):
        return (
            struct.pack(">I", len(data))
            + name
            + data
            + struct.pack(">I", zlib.crc32(name + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            raw.extend(pixels[y * size + x])
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


def draw_icon(size):
    img = [(0, 0, 0, 0)] * (size * size)
    pad = size * 0.055
    radius = size * 0.21
    w = h = size - pad * 2

    for y in range(size):
        for x in range(size):
            lx = x - pad
            ly = y - pad
            if not rounded_mask(lx, ly, w, h, radius):
                continue
            t = y / max(1, size - 1)
            r = 7 + 11 * t
            g = 16 + 26 * t
            b = 24 + 42 * t
            set_px(img, size, x, y, (r, g, b, 255))

    for i, alpha in enumerate((90, 60, 35)):
        inset = int(size * (0.10 + i * 0.025))
        line_w = max(1, int(size * 0.006))
        color = (41, 82, 108, alpha)
        draw_line(img, size, inset, size * 0.30, size - inset, size * 0.30, line_w, color)
        draw_line(img, size, inset, size * 0.53, size - inset, size * 0.53, line_w, color)
        draw_line(img, size, inset, size * 0.76, size - inset, size * 0.76, line_w, color)

    chart = [
        (0.18, 0.66),
        (0.31, 0.56),
        (0.43, 0.60),
        (0.56, 0.42),
        (0.69, 0.48),
        (0.82, 0.27),
    ]
    glow = max(2, size * 0.032)
    line = max(2, size * 0.018)
    for a, b in zip(chart, chart[1:]):
        draw_line(img, size, a[0] * size, a[1] * size, b[0] * size, b[1] * size, glow, (21, 184, 166, 70))
    for a, b in zip(chart, chart[1:]):
        draw_line(img, size, a[0] * size, a[1] * size, b[0] * size, b[1] * size, line, (45, 212, 191, 255))
    for x, y in chart:
        fill_circle(img, size, x * size, y * size, max(2, size * 0.026), (226, 252, 247, 255))

    # Stylized M.
    m = [(0.22, 0.78), (0.22, 0.43), (0.34, 0.61), (0.46, 0.43), (0.46, 0.78)]
    for a, b in zip(m, m[1:]):
        draw_line(img, size, a[0] * size, a[1] * size, b[0] * size, b[1] * size, max(2, size * 0.026), (234, 245, 255, 235))

    # Stylized Q.
    cx, cy, rr = size * 0.68, size * 0.68, size * 0.115
    for i in range(36):
        a0 = math.tau * i / 36
        a1 = math.tau * (i + 1) / 36
        draw_line(
            img,
            size,
            cx + math.cos(a0) * rr,
            cy + math.sin(a0) * rr,
            cx + math.cos(a1) * rr,
            cy + math.sin(a1) * rr,
            max(2, size * 0.025),
            (234, 245, 255, 235),
        )
    draw_line(img, size, cx + rr * 0.55, cy + rr * 0.55, cx + rr * 1.25, cy + rr * 1.25, max(2, size * 0.024), (234, 245, 255, 235))

    return img


def main():
    ICONSET.mkdir(parents=True, exist_ok=True)
    sizes = [
        (16, "icon_16x16.png"),
        (32, "icon_16x16@2x.png"),
        (32, "icon_32x32.png"),
        (64, "icon_32x32@2x.png"),
        (128, "icon_128x128.png"),
        (256, "icon_128x128@2x.png"),
        (256, "icon_256x256.png"),
        (512, "icon_256x256@2x.png"),
        (512, "icon_512x512.png"),
        (1024, "icon_512x512@2x.png"),
    ]
    for px, name in sizes:
        write_png(ICONSET / name, px, draw_icon(px))
    subprocess.run(["iconutil", "-c", "icns", str(ICONSET), "-o", str(ICNS)], check=True)
    print(ICNS)


if __name__ == "__main__":
    main()
