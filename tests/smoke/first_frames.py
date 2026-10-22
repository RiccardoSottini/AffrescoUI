#!/usr/bin/env python3
"""Smoke test: every application opens and draws its first frame.

Usage:
    python3 tests/smoke/first_frames.py [--build DIR]

Reads the gallery's list (tools/media/gallery.json, the list the pictures in .github/media/ are drawn from). For each
application, `affrescoui check` loads its screen: an error loading it ("json", "format") or a checker warning fails the
test, and a mismatch with the JSON Schema is printed as a note. Then the scenario runners draw every first frame
offscreen (tools/media/capture.py), and a frame that is missing or blank (fewer than MIN_COLOURS colours) fails the test.
A row whose screen isn't on this checkout, or whose runner has no command, is skipped with a note: a new application is
covered by a new row. On Linux it runs under Xvfb (.github/workflows/ci.yml). Only the Python standard library is needed.
"""

import json
import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "media"))
import capture  # noqa: E402 (tools/media/)
import pngfile  # noqa: E402 (tools/media/)

MIN_COLOURS = 16  # a screen's first frame has text and controls: a blank or single-colour frame has far fewer
FAILING = {"json", "format", "checker"}  # "schema" is a note: the loader, not the schema, decides what loads


def check(build, row):
    """Loads a row's screen with `affrescoui check`; returns its failures, after printing its notes."""
    tool = os.path.join(build, "affrescoui")
    screen = os.path.join(capture.EXAMPLES, row["screen"])
    result = subprocess.run([tool, "check", screen, "--json"], capture_output=True, text=True, check=False)
    try:
        problems = json.loads(result.stdout)["problems"]
    except (ValueError, KeyError):
        return [f"affrescoui check printed no report: {result.stderr.strip()}"]
    for problem in problems:
        if problem["source"] not in FAILING:
            print(f"note    {row['output']}: {problem['source']}: {problem['place']}: {problem['message']}")
    return [f"{p['source']}: {p['place']}: {p['message']}" for p in problems if p["source"] in FAILING]


def blank(path):
    """Whether a captured frame is blank: how many colours it has, if fewer than MIN_COLOURS, else None."""
    _, _, pixels, _ = pngfile.read(path)
    count = len(pngfile.colours(pixels))
    return count if count < MIN_COLOURS else None


def main():
    """Checks every screen, draws every first frame; exits with 1 if one failed."""
    args, build = sys.argv[1:], "build"
    if len(args) == 2 and args[0] == "--build":
        build = args[1]
    elif args:
        sys.exit(__doc__)
    if not os.path.exists(os.path.join(build, "affrescoui")):
        sys.exit(f"{os.path.join(build, 'affrescoui')} not found: build first (cmake --build {build})")

    started, failed = time.monotonic(), 0
    gallery = capture.load_gallery()
    for row in gallery["apps"]:
        if os.path.exists(os.path.join(capture.EXAMPLES, row["screen"])):
            for problem in check(build, row):
                failed += 1
                print(f"FAILED  {row['output']}: {problem}")
    for output, captured, problems in capture.draw(gallery, build, first_frames_only=True):
        colours = None if captured is None else blank(captured)
        if captured is None or problems or colours is not None:
            failed += 1
            why = "not drawn" if captured is None else f"blank ({colours} colours)" if colours is not None else "drawn"
            print(f"FAILED  {output}: {why}" + "".join(f"\n        {problem}" for problem in problems))
        else:
            print(f"drew    {output}")
    print(f"{failed} failed, in {time.monotonic() - started:.0f} s")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
