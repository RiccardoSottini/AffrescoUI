"""PNG files for the gallery: reads the 8-bit RGB pictures the scenario runners write, and writes them again smaller.

Only the Python standard library is needed. The runners write every row with the None, Sub or Up filter and fixed
Huffman codes; write() keeps the pixels and tries a few encodings (the runner's filters, all rows Up, an indexed
palette when the picture has 256 colours or fewer), each with zlib at level 9, and keeps the smallest. The same
pixels always give the same bytes.
"""

import struct
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"
MAX_SIDE = 16384  # a picture of the gallery is a window; anything larger is not one of ours


class PngError(Exception):
    """A file that isn't a PNG this module reads, with what is wrong."""


def chunks(data):
    """Yields each (type, payload) of a PNG file's bytes, after checking the signature."""
    if not data.startswith(SIGNATURE):
        raise PngError("not a PNG file")
    position = len(SIGNATURE)
    while position + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[position : position + 8])
        payload = data[position + 8 : position + 8 + length]
        if len(payload) != length:
            raise PngError(f"chunk {kind!r} is cut short")
        yield kind, payload
        position += 12 + length


def unfilter(filtered, width, height):
    """The pixels of a filtered 8-bit RGB image (3 bytes a pixel, one filter byte before each row)."""
    stride = width * 3
    if len(filtered) != (stride + 1) * height:
        raise PngError(f"{len(filtered)} bytes of image data for {width}x{height}")
    pixels = bytearray(stride * height)
    above = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, row = filtered[start], bytearray(filtered[start + 1 : start + 1 + stride])
        if kind == 1:  # Sub
            for i in range(3, stride):
                row[i] = (row[i] + row[i - 3]) & 0xFF
        elif kind == 2:  # Up
            row = bytearray((a + b) & 0xFF for a, b in zip(row, above))
        elif kind == 3:  # Average
            for i in range(stride):
                row[i] = (row[i] + ((row[i - 3] if i >= 3 else 0) + above[i]) // 2) & 0xFF
        elif kind == 4:  # Paeth
            for i in range(stride):
                a, b, c = (row[i - 3] if i >= 3 else 0), above[i], (above[i - 3] if i >= 3 else 0)
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        elif kind != 0:
            raise PngError(f"row {y} has the unknown filter {kind}")
        pixels[y * stride : (y + 1) * stride] = row
        above = row
    return bytes(pixels)


def read(path):
    """Reads an 8-bit RGB PNG, not interlaced: (width, height, pixels, filtered rows)."""
    with open(path, "rb") as file:
        data = file.read()
    header, compressed = None, bytearray()
    for kind, payload in chunks(data):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed += payload
    if header is None:
        raise PngError(f"{path}: no IHDR chunk")
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour != 2 or interlace != 0:
        raise PngError(f"{path}: not 8-bit RGB without interlacing")
    if not (0 < width <= MAX_SIDE and 0 < height <= MAX_SIDE):
        raise PngError(f"{path}: {width}x{height} is not a window's size")
    filtered = zlib.decompress(bytes(compressed))
    return width, height, unfilter(filtered, width, height), filtered


def colours(pixels):
    """The distinct colours of RGB pixels, as 3-byte strings."""
    return {pixels[i : i + 3] for i in range(0, len(pixels), 3)}


def chunk(kind, payload):
    """A chunk's bytes: length, type, payload and CRC."""
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def up_filtered(rows, stride):
    """Every row filtered with Up (the byte above subtracted), each after its filter byte."""
    out, above = bytearray(), bytes(stride)
    for row in rows:
        out.append(2)
        out += bytes((a - b) & 0xFF for a, b in zip(row, above))
        above = row
    return bytes(out)


def candidates(width, height, pixels, filtered):
    """The encodings write() compares: (IHDR payload, extra chunks, filtered image data)."""
    rgb = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    stride = width * 3
    rows = [pixels[y * stride : (y + 1) * stride] for y in range(height)]
    yield rgb, b"", filtered
    yield rgb, b"", up_filtered(rows, stride)
    palette = sorted(colours(pixels))
    if len(palette) <= 256:  # an indexed picture: a byte a pixel
        index = {colour: i for i, colour in enumerate(palette)}
        indexed = b"".join(b"\x00" + bytes(index[row[i : i + 3]] for i in range(0, stride, 3)) for row in rows)
        yield struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0), chunk(b"PLTE", b"".join(palette)), indexed


def write(path, width, height, pixels, filtered):
    """Writes the smallest of the candidates' encodings of the pixels; returns its size in bytes."""
    best = None
    for header, extra, data in candidates(width, height, pixels, filtered):
        png = SIGNATURE + chunk(b"IHDR", header) + extra + chunk(b"IDAT", zlib.compress(data, 9)) + chunk(b"IEND", b"")
        if best is None or len(png) < len(best):
            best = png
    with open(path, "wb") as file:
        file.write(best)
    return len(best)
