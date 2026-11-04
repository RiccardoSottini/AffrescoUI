#!/usr/bin/env python3
"""Lisp parity: each example's Lisp version draws its first frame exactly as its JSON version does.

Usage:
    python3 tests/render/parity.py [--build DIR] [--only NAME] [--report]

For every example with both a json/ and a lisp/ folder (examples/<name>/json, examples/<name>/lisp), it draws the
entry screen of each (the manifest's "entry") offscreen with `affrescoui render`, at the same size and with the same
default settings, and compares the two pictures byte for byte (the PNG encoder is deterministic, so the same pixels
give the same bytes). An example with only one notation (Calamaio) is skipped. --only checks one example; --report
lists every example's result and fails only on an error, not on a difference (for an app not converted yet). Only the
Python standard library is needed.
"""

import os
import re
import subprocess
import sys
import tempfile

os.environ.setdefault("AFFRESCOUI_HIDDEN", "1")  # its programs never show a window (CONTRIBUTING.md)

HERE = os.path.dirname(os.path.abspath(__file__))
EXAMPLES = os.path.normpath(os.path.join(HERE, "..", "..", "examples"))


def entry_of(folder):
    """The entry screen an example's folder opens: its manifest's "entry" (app.json, else app.lisp)."""
    manifest = os.path.join(folder, "app.json")
    if os.path.exists(manifest):
        with open(manifest, encoding="utf-8") as text:
            found = re.search(r'"entry"\s*:\s*"([^"]+)"', text.read())
        return os.path.join(folder, found.group(1) if found else "index.screen.json")
    manifest = os.path.join(folder, "app.lisp")
    if os.path.exists(manifest):
        with open(manifest, encoding="utf-8") as text:
            found = re.search(r':entry\s+"?([^\s"}]+)"?', text.read())
        return os.path.join(folder, found.group(1) if found else "index.screen.lisp")
    return None


def render(tool, screen, out):
    """Draws a screen's first frame to a PNG; returns the error text, or None."""
    run = subprocess.run([tool, "render", screen, "-o", out], capture_output=True, text=True, timeout=600)
    return None if run.returncode == 0 else (run.stderr or run.stdout).strip()


def main():
    args, build, only, report = sys.argv[1:], "build", None, False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--only" and args:
            only = args.pop(0)
        elif arg == "--report":
            report = True
        else:
            sys.exit(__doc__)
    tool = os.path.join(build, "affrescoui")
    if not os.path.exists(tool):
        sys.exit("parity: no " + tool + ": build the library first")
    failed, errors = [], []
    with tempfile.TemporaryDirectory() as scratch:
        for name in sorted(os.listdir(EXAMPLES)):
            if only and name != only:
                continue
            json_entry = entry_of(os.path.join(EXAMPLES, name, "json"))
            lisp_entry = entry_of(os.path.join(EXAMPLES, name, "lisp"))
            if not json_entry or not lisp_entry:
                continue
            pictures = [os.path.join(scratch, name + "-json.png"), os.path.join(scratch, name + "-lisp.png")]
            problem = render(tool, json_entry, pictures[0]) or render(tool, lisp_entry, pictures[1])
            if problem:
                errors.append(name)
                print(f"{name}: could not draw: {problem}")
                continue
            with open(pictures[0], "rb") as first, open(pictures[1], "rb") as second:
                same = first.read() == second.read()
            if not same:
                failed.append(name)
            print(f"{name}: {'same' if same else 'DIFFERENT'}")
    if errors or (failed and not report):
        sys.exit(f"parity: {len(failed)} differ, {len(errors)} could not be drawn")


if __name__ == "__main__":
    main()
