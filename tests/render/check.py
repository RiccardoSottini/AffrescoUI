#!/usr/bin/env python3
"""Rendering check: plays every scenario in scenarios/ and compares each picture with its golden image, pixel by pixel.

Usage:
    python3 tests/render/check.py [--build DIR] [--backend gl41|gles3] [--only NAME] [--update]

The runner (affrescoui_render_runner, built with the library) plays the scenarios with the automation module
(modules/automation): each on a hidden window, on a clock that moves only when a step waits. Every
{"expect": {"image": NAME}} is compared with golden/<scenario>-<NAME>.png, and one that differs is written to
DIR/render-check/ with a diff image (changed pixels in red). A missing golden image is recorded from this run.
--update rewrites them all: run it only when a change in the pictures is intended. Only the Python standard library
is needed.

--backend names the OpenGL backend DIR was built with (AFFRESCOUI_GL_BACKEND, default gl41; the check reads DIR's
CMakeCache.txt and refuses a mismatch). Each backend has its own golden images, golden/ for gl41 and golden-gles3/ for
gles3, both pixel-exact: the two drivers' rasterizers round differently, so a picture is compared only with the
pictures of its own backend. modules/render/README.md ("Backends") lists what differs between them.

The scenarios are in one file per area (scenarios/kanban.json, ...), read in the order of their file names, so changes
that add scenarios to different areas don't edit the same file.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GOLDENS = {"gl41": os.path.join(HERE, "golden"), "gles3": os.path.join(HERE, "golden-gles3")}
SCENARIOS = os.path.join(HERE, "scenarios")


def built_backend(build):
    """The AFFRESCOUI_GL_BACKEND a build folder was configured with: gl41 when its cache doesn't say."""
    try:
        with open(os.path.join(build, "CMakeCache.txt"), encoding="utf-8") as cache:
            for line in cache:
                if line.startswith("AFFRESCOUI_GL_BACKEND:"):
                    return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return "gl41"


def main():
    args, build, backend, only, update = sys.argv[1:], "build", "gl41", None, False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--backend" and args and args[0] in GOLDENS:
            backend = args.pop(0)
        elif arg == "--only" and args:
            only = args.pop(0)
        elif arg == "--update":
            update = True
        else:
            sys.exit(__doc__)

    runner = os.path.join(build, "affrescoui_render_runner")
    if not os.path.exists(runner):
        sys.exit(f"{runner} not found: build first (cmake --build {build})")
    if built_backend(build) != backend:
        sys.exit(f"{build} is built for {built_backend(build)}, not {backend}: pass --backend {built_backend(build)}")
    golden = GOLDENS[backend]
    out = os.path.join(build, "render-check")
    os.makedirs(out, exist_ok=True)
    os.makedirs(golden, exist_ok=True)
    for old in os.listdir(out):  # frames of an earlier run must not pass for this one's
        if old.endswith(".png") or old.endswith(".ppm"):
            os.remove(os.path.join(out, old))

    files = [os.path.join(SCENARIOS, name) for name in sorted(os.listdir(SCENARIOS)) if name.endswith(".json")]
    command = [runner, "--goldens", golden, "--out", out] + (["--update"] if update else [])
    command += (["--only", only] if only else []) + files
    sys.exit(subprocess.run(command).returncode)


if __name__ == "__main__":
    main()
