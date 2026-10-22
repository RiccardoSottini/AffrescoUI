#!/usr/bin/env python3
"""Every application's pictures: its first frame and one scene, drawn offscreen by the scenario runners.

Usage:
    python3 tools/media/capture.py [--build DIR] [--out DIR] [--only OUTPUT]... [--first-frames]

Reads gallery.json (next to this file): for each application, its screen, the scenario that shows it doing its job,
the pictures' name and the runner that has its natives. It writes a scenarios file that plays, on each screen, the
first frame and that scenario (its steps, without its expectations), each ending in a capture; the runner draws them
the way the rendering check does (tests/render/README.md): hidden, deterministic, at scale 1. Each capture is written
again smaller (pngfile.py) as <out>/<output>.png and <out>/<output>-scene.png; --out defaults to .github/media/.
--first-frames draws only the first frames. Re-running it is the only step needed to refresh every picture.

A row whose screen or scene isn't on this checkout, or whose runner has no command, is skipped with a note. Exits
with 1 if a picture that should have been drawn wasn't. Only the Python standard library is needed.
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EXAMPLES = os.path.join(ROOT, "examples")
GALLERY = os.path.join(HERE, "gallery.json")
MEDIA = os.path.join(ROOT, ".github", "media")
RUN_SECONDS = 300  # a runner that hangs (e.g. a program that opened its window instead) is stopped

sys.path.insert(0, HERE)
import pngfile  # noqa: E402 (next to this file)


def load_gallery(path=GALLERY):
    """The gallery's list: {"runners": {...}, "apps": [...]}."""
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def find_scene(row):
    """The scene's scenario as (screen relative to examples/, steps), or a string saying why it can't be played."""
    path = os.path.join(EXAMPLES, row["scenarios"])
    if not os.path.exists(path):
        return f"{row['scenarios']} is not on this checkout"
    with open(path, encoding="utf-8") as file:
        scenarios = json.load(file)
    screens = os.path.join(os.path.dirname(path), scenarios.get("screens", "."))
    for scenario in scenarios.get("scenarios", []):
        if scenario.get("name") == row["scene"]:
            screen = os.path.join(screens, scenario.get("screen", scenarios.get("screen", "")))
            steps = [step for step in scenario["steps"] if "expect" not in step and "capture" not in step]
            return os.path.relpath(screen, EXAMPLES), steps
    return f"no scenario '{row['scene']}' in {row['scenarios']}"


def scenarios_of(row, first_frames_only):
    """The scenarios that draw a row's pictures, and each picture's (scenario, capture file, output file) names."""
    start = [{"resize": row["size"]}, {"wait": 0}] if "size" in row else [{"wait": 0}]
    first = f"gallery-{row['output']}"
    settle = [{"wait": row["settle"]}] if "settle" in row else []  # e.g. past a debounce that draws something
    played = [{"name": first, "screen": row["screen"], "steps": start + settle + [{"capture": "first"}]}]
    pictures = [(first, f"{first}-first.png", f"{row['output']}.png")]
    if not first_frames_only:
        scene = find_scene(row)
        if isinstance(scene, str):
            print(f"note    {row['output']}-scene: skipped, {scene}")
        else:
            screen, steps = scene
            steps = start + steps + [{"wait": 0}, {"capture": "scene"}]
            played.append({"name": f"{first}-scene", "screen": screen, "steps": steps})
            pictures.append((f"{first}-scene", f"{first}-scene-scene.png", f"{row['output']}-scene.png"))
    return played, pictures


def plan(gallery, only, first_frames_only):
    """Groups the rows by runner: {runner: (scenarios, pictures)}, after printing a note for each row skipped."""
    groups = {}
    for row in gallery["apps"]:
        if only and row["output"] not in only:
            continue
        runner = gallery["runners"].get(row["runner"], {})
        if not os.path.exists(os.path.join(EXAMPLES, row["screen"])):
            print(f"note    {row['output']}: skipped, {row['screen']} is not on this checkout")
        elif not runner.get("command"):
            print(f"note    {row['output']}: skipped, the runner '{row['runner']}' has no command: {runner.get('note')}")
        else:
            played, pictures = scenarios_of(row, first_frames_only)
            group = groups.setdefault(row["runner"], ([], []))
            group[0].extend(played)
            group[1].extend(pictures)
    return groups


def run(command, build, scenarios, folder):
    """Plays scenarios with a runner's command; returns its FAILED lines."""
    path = os.path.join(folder, "gallery.scenarios.json")
    document = {"format": "affrescoui-scenarios", "version": 2, "screens": EXAMPLES, "scenarios": scenarios}
    with open(path, "w", encoding="utf-8") as file:
        json.dump(document, file, indent="\t")
    values = {"{build}": os.path.abspath(build), "{file}": path, "{out}": folder}
    arguments = [part.replace("{build}", values["{build}"]) if part not in values else values[part] for part in command]
    if not os.path.exists(arguments[0]):
        return [f"{arguments[0]} not found: build first"]
    try:
        result = subprocess.run(arguments, capture_output=True, text=True, timeout=RUN_SECONDS, check=False)
    except subprocess.TimeoutExpired:
        return [f"{os.path.basename(arguments[0])} ran for more than {RUN_SECONDS} s"]
    output = (result.stdout + result.stderr).splitlines()
    return [line for line in output if line.startswith("FAILED") or "error" in line.lower()]


def draw(gallery, build, only=(), first_frames_only=False):
    """Draws the rows' pictures into a temporary folder and yields (output name, captured file or None, problems)."""
    for name, (scenarios, pictures) in plan(gallery, only, first_frames_only).items():
        with tempfile.TemporaryDirectory(prefix="affrescoui-gallery-") as folder:
            problems = run(gallery["runners"][name]["command"], build, scenarios, folder)
            for scenario, captured, output in pictures:
                path = os.path.join(folder, captured)
                # the runner's own problems, and the failures of this picture's scenario ("FAILED <name>, step 3: ...")
                mine = [problem for problem in problems if f" {scenario}," in problem or "FAILED" not in problem]
                yield output, (path if os.path.exists(path) else None), mine


def parse_arguments(args):
    """Reads the command line: (the build folder, the output folder, the outputs to draw or [] for all, whether to draw
    only the first frames). Exits with the usage on anything else."""
    build, out, only, first_frames_only = "build", MEDIA, [], False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--out" and args:
            out = args.pop(0)
        elif arg == "--only" and args:
            only.append(args.pop(0))
        elif arg == "--first-frames":
            first_frames_only = True
        else:
            sys.exit(__doc__)
    return build, out, only, first_frames_only


def main():
    """Reads the command line, draws the pictures and writes them smaller; exits with 1 if one is missing."""
    build, out, only, first_frames_only = parse_arguments(sys.argv[1:])
    os.makedirs(out, exist_ok=True)
    total, missing = 0, 0
    for output, captured, problems in draw(load_gallery(), build, only, first_frames_only):
        for problem in problems:
            print(f"        {problem}")
        if captured is None:
            missing += 1
            print(f"MISSING {output}")
            continue
        width, height, pixels, filtered = pngfile.read(captured)
        size = pngfile.write(os.path.join(out, output), width, height, pixels, filtered)
        total += size
        print(f"wrote   {output} ({width}x{height}, {size // 1024} KiB)")
    print(f"{total // 1024} KiB in all, {missing} missing")
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
