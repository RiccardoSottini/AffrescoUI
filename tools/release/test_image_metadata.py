#!/usr/bin/env python3
"""Tests of image_metadata.py, on pictures built in memory: each kind of block is found and stripped, the pixel data
stays byte for byte, a clean picture is left alone, and a JPEG's colour profile and colour transform are kept.

    python3 -m unittest discover -s tools/release -p "test_image_metadata.py"
"""

import os
import struct
import sys
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import image_metadata  # noqa: E402


def png_chunk(kind, body):
    """One PNG chunk: its length, kind, body and checksum."""
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def png(*extra):
    """A one-pixel PNG with the chunks given placed before its image data."""
    header = png_chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
    pixels = png_chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00"))
    return image_metadata.PNG_SIGNATURE + header + b"".join(extra) + pixels + png_chunk(b"IEND", b"")


def jpeg_segment(marker, body):
    """One JPEG marker segment with its length."""
    return bytes([0xFF, marker]) + struct.pack(">H", len(body) + 2) + body


SCAN = b"\xff\xda\x00\x08scan\xff\xd9"  # stands for the scan and the end: everything from here is kept as is


class PngTest(unittest.TestCase):
    """PNG chunks."""

    def test_text_and_exif_are_stripped_and_pixels_kept(self):
        """tEXt, iTXt and eXIf go; the header, the colour chunk and the image data stay in order."""
        colour = png_chunk(b"sRGB", b"\x00")
        found, stripped = image_metadata.strip("a.png", png(colour, png_chunk(b"tEXt", b"Software\x00a tool"),
                                                            png_chunk(b"iTXt", b"XML"), png_chunk(b"eXIf", b"MM")))
        self.assertEqual(found, ["tEXt", "iTXt", "eXIf"])
        self.assertEqual(stripped, png(colour))

    def test_clean_png_is_unchanged(self):
        """A PNG with no metadata reports nothing and comes back as it was."""
        self.assertEqual(image_metadata.strip("a.png", png()), ([], png()))


class JpegTest(unittest.TestCase):
    """JPEG segments."""

    def test_exif_iptc_and_comment_are_stripped(self):
        """Exif, IPTC and a comment go; JFIF, the ICC profile and the colour transform stay; the scan is untouched."""
        jfif = jpeg_segment(0xE0, b"JFIF\x00")
        profile = jpeg_segment(0xE2, b"ICC_PROFILE\x00")
        transform = jpeg_segment(0xEE, b"Adobe\x00")
        tables = jpeg_segment(0xDB, b"\x00" * 65)
        data = (b"\xff\xd8" + jfif + jpeg_segment(0xE1, b"Exif\x00\x00") + profile + jpeg_segment(0xED, b"8BIM")
                + transform + jpeg_segment(0xFE, b"made by") + tables + SCAN)
        found, stripped = image_metadata.strip("a.jpg", data)
        self.assertEqual(found, ["Exif/XMP", "IPTC", "comment"])
        self.assertEqual(stripped, b"\xff\xd8" + jfif + profile + transform + tables + SCAN)

    def test_truncated_jpeg_does_not_crash(self):
        """A file cut inside its header is read as far as it goes."""
        found, _ = image_metadata.strip("a.jpg", b"\xff\xd8\xff\xe1\x00")
        self.assertEqual(found, [])


class IconTest(unittest.TestCase):
    """macOS icons."""

    def test_icon_pngs_are_stripped(self):
        """Each PNG inside the icon loses its metadata, and the lengths are written again."""
        def icon(image):
            entry = b"ic07" + struct.pack(">I", len(image) + 8) + image
            return b"icns" + struct.pack(">I", len(entry) + 8) + entry
        found, stripped = image_metadata.strip("a.icns", icon(png(png_chunk(b"eXIf", b"MM"))))
        self.assertEqual(found, ["eXIf"])
        self.assertEqual(stripped, icon(png()))


if __name__ == "__main__":
    unittest.main()
