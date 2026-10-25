#!/usr/bin/env python3
"""The README's before and after: an application changed by asking its assistant, drawn offscreen.

Usage:
    python3 tools/media/before_after.py [--build DIR] [--out DIR]

Plays Flow's scenario "ask-the-assistant-for-status-colours" (examples/flow/index.scenarios.json) with the rendering
check's runner, the way capture.py plays the gallery's: hidden, deterministic, at scale 1. The request is typed in the
assistant's chat, the agent's revision is proposed through the screen's permissions, and the person accepts it on its
card. Its pictures become <out>/flow-before.png (the request, the board as shipped) and <out>/flow-after.png (the
change accepted), each with a ring drawn around the node the position checks before its capture name. --out defaults to
.github/media/before-after/. The scenario's revision must be the one kept in examples/flow/demos/ for anyone to replay;
the script stops if they differ. Only the Python standard library is needed.
"""

import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import capture  # noqa: E402 (next to this file)
import pngfile  # noqa: E402

FLOW = os.path.join(capture.EXAMPLES, "flow")
SCENARIO = "ask-the-assistant-for-status-colours"
REVISION = os.path.join(FLOW, "demos", "status-colours.revision.json")
OUT = os.path.join(capture.MEDIA, "before-after")
PICTURES = (("proposed", "flow-before.png"), ("accepted", "flow-after.png"))
RING = (225, 29, 72)  # a rose that neither theme of the board uses
RING_WIDTH, RING_GAP, RING_RADIUS = 3, 6, 10


def load_scenario():
    """The scenario's steps, after checking that its proposal is the revision kept in demos/."""
    with open(os.path.join(FLOW, "index.scenarios.json"), encoding="utf-8") as file:
        scenarios = json.load(file)["scenarios"]
    steps = next(scenario["steps"] for scenario in scenarios if scenario["name"] == SCENARIO)
    with open(REVISION, encoding="utf-8") as file:
        kept = {key: value for key, value in json.load(file).items() if key not in ("type", "note")}
    proposed = next(step["propose"] for step in steps if "propose" in step)
    if proposed != kept:
        sys.exit(f"the scenario's revision differs from {os.path.relpath(REVISION, capture.ROOT)}")
    return steps


def highlights(steps):
    """Each capture's rectangle to ring, (x, y, width, height) in window units: what the position checks of one node
    (x, y, width, height) just before that capture give."""
    boxes, box = {}, {}
    for step in steps:
        expect = step.get("expect", {})
        if expect.get("property") in ("x", "y", "width", "height"):
            box[expect["property"]] = expect["is"]
        elif "capture" in step:
            if len(box) == 4:
                boxes[step["capture"]] = (box["x"], box["y"], box["width"], box["height"])
            box = {}
    return boxes


def inside_rounded(x, y, left, top, right, bottom, radius):
    """Whether pixel (x, y) is inside the rectangle with corners rounded by `radius`."""
    if not (left <= x < right and top <= y < bottom):
        return False
    near_x = min(max(x, left + radius), right - radius - 1)
    near_y = min(max(y, top + radius), bottom - radius - 1)
    return (x - near_x) ** 2 + (y - near_y) ** 2 <= radius * radius


def draw_ring(width, height, pixels, box):
    """Paints a rounded ring of RING colour around `box`, RING_GAP outside it; returns the new pixels."""
    pixels = bytearray(pixels)
    x, y, w, h = box
    outer = (x - RING_GAP - RING_WIDTH, y - RING_GAP - RING_WIDTH, x + w + RING_GAP + RING_WIDTH, y + h + RING_GAP + RING_WIDTH)
    inner = (outer[0] + RING_WIDTH, outer[1] + RING_WIDTH, outer[2] - RING_WIDTH, outer[3] - RING_WIDTH)
    for py in range(max(outer[1], 0), min(outer[3], height)):
        for px in range(max(outer[0], 0), min(outer[2], width)):
            if inside_rounded(px, py, *outer, RING_RADIUS) and not inside_rounded(px, py, *inner, RING_RADIUS - RING_WIDTH):
                at = (py * width + px) * 3
                pixels[at : at + 3] = bytes(RING)
    return bytes(pixels)


def save(source, target, box=None):
    """Writes the captured picture `source` to `target` (only image chunks, so no metadata), ringed around `box`."""
    width, height, pixels, filtered = pngfile.read(source)
    if box is not None:
        pixels = draw_ring(width, height, pixels, box)
        stride = width * 3
        filtered = pngfile.up_filtered([pixels[row * stride : (row + 1) * stride] for row in range(height)], stride)
    size = pngfile.write(target, width, height, pixels, filtered)
    print(f"wrote   {os.path.relpath(target, capture.ROOT)} ({width}x{height}, {size // 1024} KiB)")


def main(args):
    """Draws both pictures; exits non-zero if the scenario fails or a picture is missing."""
    build, out = "build", OUT
    while args:
        arg = args.pop(0)
        if arg in ("--build", "--out") and args:
            value = args.pop(0)
            build, out = (value, out) if arg == "--build" else (build, value)
        else:
            sys.exit(__doc__)
    steps = load_scenario()
    boxes = highlights(steps)
    os.makedirs(out, exist_ok=True)
    command = capture.load_gallery()["runners"]["render"]["command"]
    with tempfile.TemporaryDirectory(prefix="affrescoui-before-after-") as folder:
        played = [{"name": SCENARIO, "screen": "flow/index.screen.json", "steps": steps}]
        problems = capture.run(command, build, played, folder)
        if problems:
            sys.exit("\n".join(problems))
        for name, output in PICTURES:
            path = os.path.join(folder, f"{SCENARIO}-{name}.png")
            if not os.path.exists(path):
                sys.exit(f"the scenario drew no '{name}' picture")
            save(path, os.path.join(out, output), boxes.get(name))


if __name__ == "__main__":
    main(sys.argv[1:])
