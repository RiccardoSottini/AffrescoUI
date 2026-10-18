#!/usr/bin/env python3
"""Rendering check: plays every scenario in scenarios.json and compares each frame with its golden image, pixel by pixel.

Usage:
    python3 tests/render/check.py [--build DIR] [--only NAME] [--update]

The runner (affrescoui_render_runner, built with the library) plays the scenario on a hidden window; this script
compares its frames with golden/<scenario>-<capture>.png. A frame that differs is saved with a diff image (changed
pixels in red) in DIR/render-check/. --update rewrites the golden images: run it only when a change in the pictures is
intended. The window is never shown, so frames are at 1x on any display. Only the Python standard library is needed.
"""

import json
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
GOLDEN = os.path.join(HERE, "golden")


def read_ppm(path):
    """A binary PPM (P6, 8-bit), as the runner writes it: (width, height, RGB bytes)."""
    with open(path, "rb") as file:
        data = file.read()
    fields, start = [], 0
    while len(fields) < 4:  # magic, width, height, max value, separated by whitespace
        while data[start:start + 1].isspace():
            start += 1
        end = start
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[start:end])
        start = end
    if fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError(f"{path}: not an 8-bit binary PPM")
    width, height = int(fields[1]), int(fields[2])
    return width, height, data[start + 1:start + 1 + width * height * 3]


def write_png(path, width, height, rgb):
    """An 8-bit RGB PNG, rows unfiltered."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    stride = width * 3
    rows = b"".join(b"\0" + rgb[y * stride:(y + 1) * stride] for y in range(height))
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                   chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def read_png(path):
    """An 8-bit RGB PNG (any row filter), as write_png writes them: (width, height, RGB bytes)."""
    with open(path, "rb") as file:
        data = file.read()
    position, idat = 8, b""
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
            if (depth, color) != (8, 2):
                raise ValueError(f"{path}: expected an 8-bit RGB PNG")
        elif kind == b"IDAT":
            idat += body
        position += 12 + length

    raw, stride, out = zlib.decompress(idat), width * 3, bytearray()
    previous = bytearray(stride)
    for y in range(height):
        kind, row = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride if kind else 0):  # filter 0 (what write_png writes): the row is as stored
            left = row[i - 3] if i >= 3 else 0
            up, corner = previous[i], previous[i - 3] if i >= 3 else 0
            if kind == 1:
                row[i] = (row[i] + left) & 255
            elif kind == 2:
                row[i] = (row[i] + up) & 255
            elif kind == 3:
                row[i] = (row[i] + (left + up) // 2) & 255
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                row[i] = (row[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        out += row
        previous = row
    return width, height, bytes(out)


def compare(actual, golden):
    """How many pixels differ, and a picture of them: the golden image greyed out, changed pixels in red."""
    (width, height, a), (_, _, g) = actual, golden
    if a == g:
        return 0, None
    diff, changed = bytearray(len(a)), 0
    for i in range(0, len(a), 3):
        if a[i:i + 3] != g[i:i + 3]:
            changed += 1
            diff[i:i + 3] = b"\xff\x00\x00"
        else:
            grey = (g[i] + g[i + 1] + g[i + 2]) // 6 + 128  # faded, so the red stands out
            diff[i:i + 3] = bytes((grey, grey, grey))
    return changed, bytes(diff)


def main():
    args, build, only, update = sys.argv[1:], "build", None, False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--only" and args:
            only = args.pop(0)
        elif arg == "--update":
            update = True
        else:
            sys.exit(__doc__)

    runner = os.path.join(build, "affrescoui_render_runner")
    if not os.path.exists(runner):
        sys.exit(f"{runner} not found: build first (cmake --build {build})")
    out = os.path.join(build, "render-check")
    os.makedirs(out, exist_ok=True)
    os.makedirs(GOLDEN, exist_ok=True)

    scenarios_file = os.path.join(HERE, "scenarios.json")
    with open(scenarios_file, encoding="utf-8") as file:
        scenarios = [s for s in json.load(file)["scenarios"] if not only or s["name"] == only]

    failures, frames = [], 0
    for scenario in scenarios:
        name = scenario["name"]
        for old in os.listdir(out):  # frames of an earlier run must not pass for this one's
            if old.startswith(name + "-"):
                os.remove(os.path.join(out, old))
        if subprocess.run([runner, scenarios_file, name, out]).returncode != 0:
            failures.append(f"{name}: the runner failed")
            continue

        for step in scenario["steps"]:
            if "capture" not in step:
                continue
            frame = f"{name}-{step['capture']}"
            frames += 1
            actual = read_ppm(os.path.join(out, frame + ".ppm"))
            golden_path = os.path.join(GOLDEN, frame + ".png")
            if update or not os.path.exists(golden_path):
                write_png(golden_path, *actual)
                print(f"golden  {frame}")
                continue
            golden = read_png(golden_path)
            if golden[:2] != actual[:2]:
                failures.append(f"{frame}: {actual[0]}x{actual[1]}, golden is {golden[0]}x{golden[1]} "
                                f"(another display scale?)")
                continue
            changed, diff = compare(actual, golden)
            if changed:
                write_png(os.path.join(out, frame + ".png"), *actual)
                write_png(os.path.join(out, frame + ".diff.png"), actual[0], actual[1], diff)
                failures.append(f"{frame}: {changed} pixels differ (see {out}/{frame}.diff.png)")
            else:
                print(f"same    {frame}")

    for failure in failures:
        print(f"FAILED  {failure}")
    print(f"{frames} frames, {len(failures)} failed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
