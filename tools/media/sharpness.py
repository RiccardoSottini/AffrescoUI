"""How sharp a picture's text is: the mean of its strongest steps of luminance between neighbouring pixels.

    python3 tools/media/sharpness.py PICTURE... [--crop LEFT,TOP,WIDTH,HEIGHT] [--top 3000]
    python3 tools/media/sharpness.py PICTURE... --crop ... --enlarge 8 --out out/crops.png

Prints one row per picture (8-bit RGB PNGs, as the rendering check writes them): the mean of the --top strongest
steps across columns ("stems", from a pixel to the one on its right: what vertical strokes give) and across rows
("horizontals", to the pixel below: bars, baselines, the tops of x-height letters). Higher is sharper: a stroke
that covers whole pixels steps from the background to the ink in one pixel, one smeared over two pixels steps half as
far twice. Only the strongest steps count, so the empty background doesn't dilute the number and pictures of one
screen at several display scales compare (take --top no larger than the smallest picture's count of edges).

--crop measures one rectangle, in each picture's own pixels (the same rectangle of the window is LEFT * scale, ...
in a picture at a scale). --enlarge N --out FILE writes the crop of each picture enlarged N times with no smoothing,
side by side on white, to look at the pixels. Only the Python standard library is needed (pngfile.py).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngfile  # noqa: E402 (next to this file)

GAP = 8  # pixels of white between two enlarged crops
MAX_ENLARGE = 32  # a crop enlarged more than this is no easier to read, only larger


class SharpnessError(Exception):
    """A request that can't be measured, with what is wrong."""


def luminance(pixels, width, height):
    """The luminance of each pixel of 8-bit RGB pixels (Rec. 601 weights, 0 to 255), as rows of floats."""
    rows = []
    for y in range(height):
        start = y * width * 3
        row = pixels[start : start + width * 3]
        rows.append([0.299 * row[i] + 0.587 * row[i + 1] + 0.114 * row[i + 2] for i in range(0, width * 3, 3)])
    return rows


def crop(rows, box):
    """The rectangle box = (left, top, width, height) of rows of values; SharpnessError if it leaves them."""
    left, top, width, height = box
    if width < 1 or height < 1 or left < 0 or top < 0 or top + height > len(rows) or left + width > len(rows[0]):
        raise SharpnessError(
            f"crop {left},{top},{width},{height} is outside the picture of {len(rows[0])}x{len(rows)} pixels"
        )
    return [row[left : left + width] for row in rows[top : top + height]]


def strongest_mean(steps, top):
    """The mean of the top largest steps (of all of them when there are fewer); 0 when there are none."""
    if not steps:
        return 0.0
    strongest = sorted(steps, reverse=True)[:top]
    return sum(strongest) / len(strongest)


def sharpness(rows, top):
    """(stems, horizontals): the mean of the top strongest steps of luminance across columns and across rows."""
    across_columns = [abs(row[x + 1] - row[x]) for row in rows for x in range(len(row) - 1)]
    across_rows = [abs(below[x] - above[x]) for above, below in zip(rows, rows[1:]) for x in range(len(above))]
    return strongest_mean(across_columns, top), strongest_mean(across_rows, top)


def read_picture(path):
    """(width, height, pixels) of an 8-bit RGB PNG; SharpnessError if it isn't one."""
    try:
        width, height, pixels, _ = pngfile.read(path)
    except (OSError, pngfile.PngError) as error:
        raise SharpnessError(f"{path}: {error}") from error
    return width, height, pixels


def crop_pixels(pixels, width, box):
    """The RGB rows of the rectangle box of a picture whose rows are width pixels wide (box already checked)."""
    left, top, crop_width, crop_height = box
    return [
        pixels[((top + y) * width + left) * 3 : ((top + y) * width + left + crop_width) * 3] for y in range(crop_height)
    ]


def enlarged_side_by_side(crops, factor):
    """(width, height, pixels): RGB crops (lists of rows) each enlarged factor times, left to right on white."""
    widths = [len(rows[0]) // 3 * factor for rows in crops]
    height = max(len(rows) for rows in crops) * factor
    width = sum(widths) + GAP * (len(crops) - 1)
    out = bytearray(b"\xff" * (width * height * 3))
    left = 0
    for rows, crop_width in zip(crops, widths):
        for y, row in enumerate(rows):
            wide = b"".join(row[x : x + 3] * factor for x in range(0, len(row), 3))
            for repeat in range(factor):
                start = ((y * factor + repeat) * width + left) * 3
                out[start : start + len(wide)] = wide
        left += crop_width + GAP
    return width, height, bytes(out)


def check_enlarged_size(crops, factor):
    """SharpnessError if the enlarged crops side by side would be larger than a PNG pngfile reads back."""
    width = sum(len(rows[0]) // 3 * factor for rows in crops) + GAP * (len(crops) - 1)
    height = max(len(rows) for rows in crops) * factor
    if max(width, height) > pngfile.MAX_SIDE:
        raise SharpnessError(f"the enlarged crops would be {width}x{height} pixels: crop less or enlarge less")


def write_picture(path, width, height, pixels):
    """Writes 8-bit RGB pixels as a PNG (pngfile.write keeps the smallest of its encodings)."""
    stride = width * 3
    filtered = b"".join(b"\x00" + pixels[y * stride : (y + 1) * stride] for y in range(height))
    pngfile.write(path, width, height, pixels, filtered)


def parse_box(text):
    """LEFT,TOP,WIDTH,HEIGHT as four whole numbers; argparse's error otherwise."""
    try:
        box = tuple(int(part) for part in text.split(","))
    except ValueError:
        box = ()
    if len(box) != 4:
        raise argparse.ArgumentTypeError(f"expected LEFT,TOP,WIDTH,HEIGHT in whole pixels, got '{text}'")
    return box


def positive(text):
    """A whole number of 1 or more; argparse's error otherwise."""
    if not text.isdigit() or int(text) < 1:
        raise argparse.ArgumentTypeError(f"expected a whole number of 1 or more, got '{text}'")
    return int(text)


def main(arguments):
    """Measures the pictures, prints a row for each, and writes the enlarged crops when asked; returns the exit code."""
    parser = argparse.ArgumentParser(description="How sharp a picture's text is (higher is sharper).")
    parser.add_argument("pictures", nargs="+", help="8-bit RGB PNG files, e.g. tests/render/golden/*.png")
    parser.add_argument("--crop", type=parse_box, help="LEFT,TOP,WIDTH,HEIGHT in each picture's pixels")
    parser.add_argument("--top", type=positive, default=3000, help="how many of the strongest steps count")
    parser.add_argument("--enlarge", type=positive, help="enlarge each crop this many times (with --out)")
    parser.add_argument("--out", help="the PNG the enlarged crops go to, side by side")
    options = parser.parse_args(arguments)
    if (options.enlarge is None) != (options.out is None):
        parser.error("--enlarge and --out go together")
    if options.enlarge is not None and options.enlarge > MAX_ENLARGE:
        parser.error(f"--enlarge: at most {MAX_ENLARGE}")
    try:
        crops = []
        print(f"{'picture':<48} {'stems':>8} {'horizontals':>12}")
        for path in options.pictures:
            width, height, pixels = read_picture(path)
            box = options.crop or (0, 0, width, height)
            rows = crop(luminance(pixels, width, height), box)
            stems, horizontals = sharpness(rows, options.top)
            print(f"{os.path.basename(path):<48} {stems:>8.1f} {horizontals:>12.1f}")
            crops.append(crop_pixels(pixels, width, box))
        if options.out:
            check_enlarged_size(crops, options.enlarge)
            write_picture(options.out, *enlarged_side_by_side(crops, options.enlarge))
            print(options.out)
    except (SharpnessError, OSError) as error:
        print(f"sharpness: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
