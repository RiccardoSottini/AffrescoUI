"""Tests of sharpness.py: python3 -m unittest tools/media/test_sharpness.py"""

import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngfile  # noqa: E402 (next to this file)
import sharpness  # noqa: E402


def grey(values):
    """One row of 8-bit RGB pixels, each grey at its value."""
    return bytes(channel for value in values for channel in (value, value, value))


class SharpnessTest(unittest.TestCase):
    """The measure on edges whose answer is known, and the crop's checks."""

    def measure(self, row_values, rows, top=3000):
        """(stems, horizontals) of a picture of rows identical rows of grey values."""
        pixels = grey(row_values) * rows
        return sharpness.sharpness(sharpness.luminance(pixels, len(row_values), rows), top)

    def test_a_one_pixel_edge_steps_all_the_way_across_columns_and_not_across_rows(self):
        self.assertEqual(self.measure([0, 255], 4), (255.0, 0.0))

    def test_an_edge_blurred_over_two_pixels_steps_half_as_far(self):
        stems, horizontals = self.measure([0, 127, 255], 4)
        self.assertAlmostEqual(stems, 127.5)
        self.assertEqual(horizontals, 0.0)

    def test_only_the_strongest_steps_count(self):
        # Three flat steps of 0 and one edge per row: the strongest four are the edges
        self.assertEqual(self.measure([0, 0, 255, 255, 255], 4, top=4), (255.0, 0.0))

    def test_an_edge_along_a_row_shows_across_rows(self):
        pixels = grey([0, 0, 0]) + grey([255, 255, 255])
        self.assertEqual(sharpness.sharpness(sharpness.luminance(pixels, 3, 2), 3000), (0.0, 255.0))

    def test_a_crop_outside_the_picture_is_an_error(self):
        rows = sharpness.luminance(grey([0, 255]) * 2, 2, 2)
        with self.assertRaises(sharpness.SharpnessError):
            sharpness.crop(rows, (1, 0, 2, 2))
        with self.assertRaises(sharpness.SharpnessError):
            sharpness.crop(rows, (-1, 0, 1, 1))
        with self.assertRaises(sharpness.SharpnessError):
            sharpness.crop(rows, (0, 0, 0, 1))
        self.assertEqual(sharpness.crop(rows, (1, 1, 1, 1)), [[255.0]])

    def test_enlarged_crops_sit_side_by_side_without_smoothing(self):
        crops = [[grey([0, 255])], [grey([255])]]
        width, height, pixels = sharpness.enlarged_side_by_side(crops, 2)
        self.assertEqual((width, height), (4 + sharpness.GAP + 2, 2))
        first_row = pixels[: width * 3]
        self.assertEqual(first_row[:12], grey([0, 0, 255, 255]))

    def test_the_command_reads_pictures_and_writes_the_enlarged_crops(self):
        with tempfile.TemporaryDirectory() as folder:
            picture = os.path.join(folder, "edge.png")
            sharpness.write_picture(picture, 2, 2, grey([0, 255]) * 2)
            out = os.path.join(folder, "crops.png")
            printed = io.StringIO()
            with contextlib.redirect_stdout(printed), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(sharpness.main([picture, "--crop", "0,0,2,2", "--enlarge", "3", "--out", out]), 0)
                self.assertEqual(sharpness.main([picture, "--crop", "1,1,2,2"]), 1)
            self.assertIn("255.0", printed.getvalue())
            width, height, _, _ = pngfile.read(out)
            self.assertEqual((width, height), (6, 6))


if __name__ == "__main__":
    unittest.main()
