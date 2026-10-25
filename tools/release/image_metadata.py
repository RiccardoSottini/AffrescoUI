#!/usr/bin/env python3
"""Image metadata check: no tracked PNG, JPEG or macOS icon carries a metadata block (text, Exif, XMP, IPTC, comments).

    python3 tools/release/image_metadata.py [--root DIR] [--strip]

A public repository's pictures should say nothing about the tools or the machines that made them. The check lists every
block it finds, one line per file, and fails if there is one. --strip removes them in place without touching pixel data:
a PNG keeps its image and colour chunks, a JPEG its frame, tables, JFIF, colour profile and colour transform segments
(nothing is decoded or encoded again), and an icon's PNGs are stripped one by one. Only the standard library is needed.
"""

import argparse
import os
import struct
import subprocess
import sys

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_METADATA = {b"tEXt", b"zTXt", b"iTXt", b"eXIf", b"tIME"}  # chunks that describe, never draw
JPEG_KEPT_APPLICATION = {0xE0, 0xE2, 0xEE}  # JFIF, the ICC profile, the colour transform: they change pixels
EXTENSIONS = (".png", ".jpg", ".jpeg", ".icns")


def png_blocks(data):
    """A PNG's chunks as (kind, bytes) pairs, each with its length, kind and checksum; None if it isn't a PNG."""
    if not data.startswith(PNG_SIGNATURE):
        return None
    blocks, position = [], len(PNG_SIGNATURE)
    while position + 12 <= len(data):
        length, = struct.unpack(">I", data[position:position + 4])
        end = position + 12 + length
        blocks.append((data[position + 4:position + 8], data[position:end]))
        position = end
    return blocks


def strip_png(data):
    """A PNG's metadata chunk names, and the PNG without them."""
    blocks = png_blocks(data) or []
    found = [kind.decode("latin-1") for kind, _ in blocks if kind in PNG_METADATA]
    kept = b"".join(block for kind, block in blocks if kind not in PNG_METADATA)
    return found, PNG_SIGNATURE + kept if found else data


def jpeg_name(marker):
    """A metadata segment's name, or None for a segment that draws or that the decoder needs."""
    if marker == 0xFE:
        return "comment"
    if marker == 0xE1:
        return "Exif/XMP"
    if marker == 0xED:
        return "IPTC"
    if 0xE0 <= marker <= 0xEF and marker not in JPEG_KEPT_APPLICATION:
        return "APP%d" % (marker - 0xE0)
    return None


def strip_jpeg(data):
    """A JPEG's metadata segment names, and the JPEG without them; the scan and everything after it are kept as is."""
    found, kept, position = [], bytearray(data[:2]), 2
    while position + 4 <= len(data) and data[position] == 0xFF:
        marker = data[position + 1]
        if marker == 0xDA:  # the start of the scan: entropy-coded data follows, with no more length fields
            break
        length, = struct.unpack(">H", data[position + 2:position + 4])
        name = jpeg_name(marker)
        if name:
            found.append(name)
        else:
            kept += data[position:position + 2 + length]
        position += 2 + length
    return found, bytes(kept + data[position:]) if found else data


def strip_icns(data):
    """A macOS icon's metadata (in its PNG images), and the icon without it."""
    found, body, position = [], bytearray(), 8
    while position + 8 <= len(data):
        kind = data[position:position + 4]
        length, = struct.unpack(">I", data[position + 4:position + 8])
        image = data[position + 8:position + length]
        if image.startswith(PNG_SIGNATURE):
            names, image = strip_png(image)
            found += names
        body += kind + struct.pack(">I", len(image) + 8) + image
        position += length
    return found, b"icns" + struct.pack(">I", len(body) + 8) + bytes(body) if found else data


def strip(path, data):
    """A file's metadata block names, and its bytes without them."""
    if path.lower().endswith(".png"):
        return strip_png(data)
    if path.lower().endswith(".icns"):
        return strip_icns(data)
    return strip_jpeg(data)


def images(root):
    """The tracked pictures, relative to root."""
    output = subprocess.run(["git", "-C", root, "ls-files"], check=True, capture_output=True, text=True).stdout
    return [path for path in output.splitlines() if path.lower().endswith(EXTENSIONS)]


def main():
    """Lists (or strips) the metadata of every tracked picture; exits 1 when some is left."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--strip", action="store_true", help="remove the blocks in place")
    arguments = parser.parse_args()
    left = 0
    for path in images(arguments.root):
        full = os.path.join(arguments.root, path)
        with open(full, "rb") as file:
            found, stripped = strip(path, file.read())
        if not found:
            continue
        if arguments.strip:
            with open(full, "wb") as file:
                file.write(stripped)
            print("stripped %s: %s" % (path, ", ".join(found)))
        else:
            print("%s: %s" % (path, ", ".join(found)))
            left += 1
    if left:
        print("%d pictures carry metadata: python3 tools/release/image_metadata.py --strip" % left)
    return 1 if left else 0


if __name__ == "__main__":
    sys.exit(main())
