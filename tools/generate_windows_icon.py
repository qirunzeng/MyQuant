"""Wrap the existing PNG artwork in a multiresolution Windows ICO container."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1] / "resources" / "icons"
sources = [(16, "icon_16x16.png"), (32, "icon_32x32.png"),
           (64, "icon_32x32@2x.png"), (128, "icon_128x128.png"),
           (256, "icon_256x256.png")]
offset = 6 + 16 * len(sources)
entries = bytearray()
payload = bytearray()
for size, name in sources:
    png = (root / "MyQuant.iconset" / name).read_bytes()
    entries.extend(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0,
                               1, 32, len(png), offset))
    payload.extend(png)
    offset += len(png)
(root / "MyQuant.ico").write_bytes(struct.pack("<HHH", 0, 1, len(sources)) + entries + payload)
