#!/usr/bin/env python3
"""Writes the images the rendering check's "visuals" screen shows, with the standard library only (PNG by zlib):

- landscape.png: 160 x 100, a sky, a sun, hills and a frame, so a fit that stretches or cuts it shows;
- badge.png: 48 x 48, a circle with soft edges on a transparent background (packed into a shared page);
- banner.png: 320 x 120, stripes (larger than 256 on a side: a texture of its own).

landscape.jpg is landscape.png as a JPEG: on macOS, `sips -s format jpeg -s formatOptions 90 landscape.png --out
landscape.jpg`. The files are checked in; this script says how they were made.
Usage: tests/render/screens/images/make.py
"""

import math
import struct
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent


def png(path, width, height, pixel):
    """Writes an RGBA PNG; pixel(x, y) gives (r, g, b, a)."""
    rows = b"".join(b"\0" + bytes(c for x in range(width) for c in pixel(x, y)) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) +
                     chunk(b"IEND", b""))


def landscape(x, y):
    """A sky, a sun, two hills and a dark frame."""
    if x < 3 or y < 3 or x >= 157 or y >= 97:
        return (30, 41, 59, 255)
    if (x - 118) ** 2 + (y - 30) ** 2 < 14 ** 2:
        return (250, 204, 21, 255)
    if y > 70 + 12 * math.sin(x / 18):
        return (22, 163, 74, 255)
    if y > 62 + 10 * math.sin(x / 11 + 2):
        return (74, 222, 128, 255)
    blue = 235 - y
    return (125, 180 + y // 4, blue, 255)


def badge(x, y):
    """A violet circle with a white dot, its edge anti-aliased, on transparency."""
    distance = math.hypot(x - 23.5, y - 23.5)
    alpha = max(0.0, min(1.0, 22.0 - distance))
    if math.hypot(x - 23.5, y - 23.5) < 7:
        return (255, 255, 255, 255)
    return (124, 58, 237, round(alpha * 255))


def banner(x, y):
    """Diagonal stripes, orange and slate."""
    return (249, 115, 22, 255) if (x + y) // 20 % 2 == 0 else (51, 65, 85, 255)


if __name__ == "__main__":
    png(HERE / "landscape.png", 160, 100, landscape)
    png(HERE / "badge.png", 48, 48, badge)
    png(HERE / "banner.png", 320, 120, banner)
