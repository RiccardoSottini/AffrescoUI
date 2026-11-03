#!/usr/bin/env python3
"""Measures an example's lisp/ folder as a program to read: how long its files and definitions are, how deep its lines
are indented, how many `{{...}}` texts are left, how much visible text isn't in its locales, and how long its first
frame and its scenarios take beside its json/ folder's.

    python3 tools/notation/lisp_metrics.py <example> [--cli build/affrescoui] [--runner build/affrescoui_render_runner]
                                           [--runs 5] [--json]

A definition is a top-level form; a data file (app.lisp, models, seed, locales, policies) is one form. The locales/ and
seed/ folders are data and are left out of the longest definition and the `{{` count. Only the standard library.
"""

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXAMPLES = os.path.join(ROOT, "examples")
DATA_FOLDERS = ("locales", "seed")


def top_level_forms(text):
    """The top-level forms of a Lisp text, as (first line, last line, head word)."""
    forms, depth, start, quote, at, line = [], 0, None, False, 0, 1
    head = ""
    while at < len(text):
        c = text[at]
        if c == "\n":
            line += 1
        if quote:
            if c == "\\":
                at += 1
            elif c == '"':
                quote = False
        elif c == ";":
            while at < len(text) and text[at] != "\n":
                at += 1
            continue
        elif c == '"':
            quote = True
        elif c in "([{":
            if depth == 0:
                start = line
                found = re.match(r"[(\[{]\s*([^\s()\[\]{}\"]*)", text[at:])
                head = found.group(1) if found else ""
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0 and start is not None:
                forms.append((start, line, head))
                start = None
        at += 1
    return forms


def lisp_files(folder):
    """The .lisp files of a folder, relative to it."""
    found = []
    for place, _, names in os.walk(folder):
        found += [os.path.relpath(os.path.join(place, name), folder) for name in names if name.endswith(".lisp")]
    return sorted(found)


def entry_of(folder, manifest, pattern, default):
    """The entry screen a manifest names."""
    path = os.path.join(folder, manifest)
    if not os.path.exists(path):
        return os.path.join(folder, default)
    with open(path, encoding="utf-8") as text:
        found = re.search(pattern, text.read())
    return os.path.join(folder, found.group(1) if found else default)


def timed(command, runs):
    """The median wall time of a command, in milliseconds; None if it fails."""
    times = []
    for _ in range(runs):
        began = time.perf_counter()
        if subprocess.run(command, capture_output=True).returncode != 0:
            return None
        times.append((time.perf_counter() - began) * 1000)
    return round(statistics.median(times), 1)


def in_place(cli, entry):
    """How many visible texts are written in place rather than read from the locales (`affrescoui strings report`)."""
    run = subprocess.run([cli, "strings", "report", entry], capture_output=True, text=True)
    found = re.search(r"(\d+) texts written in place", run.stdout)
    return int(found.group(1)) if found else None


def measure(example, cli, runner, runs):
    """The measures of an example's lisp/ folder, and the times of both folders."""
    lisp = os.path.join(EXAMPLES, example, "lisp")
    files, longest, deepest, bindings = {}, (0, ""), (0, ""), 0
    for name in lisp_files(lisp):
        with open(os.path.join(lisp, name), encoding="utf-8") as file:
            text = file.read()
        lines = text.split("\n")
        files[name] = len(lines) - (1 if text.endswith("\n") else 0)
        if name.split(os.sep)[0] in DATA_FOLDERS:
            continue
        bindings += text.count("{{")
        for first, last, head in top_level_forms(text):
            if last - first + 1 > longest[0]:
                longest = (last - first + 1, f"{name}:{first} ({head})")
        for number, line in enumerate(lines, 1):
            indent = len(line) - len(line.lstrip(" "))
            if line.strip() and indent > deepest[0]:
                deepest = (indent, f"{name}:{number}")
    lisp_entry = entry_of(lisp, "app.lisp", r':entry\s+"?([^\s"}]+)"?', "index.screen.lisp")
    json_folder = os.path.join(EXAMPLES, example, "json")
    json_entry = entry_of(json_folder, "app.json", r'"entry"\s*:\s*"([^"]+)"', "index.screen.json")
    result = {"example": example, "files": files, "longestDefinition": longest, "deepestIndent": deepest,
              "bindingTexts": bindings, "textInPlace": in_place(cli, lisp_entry)}
    with tempfile.TemporaryDirectory() as scratch:
        picture = os.path.join(scratch, "frame.png")
        result["firstFrameMs"] = {"lisp": timed([cli, "render", lisp_entry, "-o", picture], runs)}
        if os.path.exists(json_entry):
            result["firstFrameMs"]["json"] = timed([cli, "render", json_entry, "-o", picture], runs)
        result["scenariosMs"] = {}
        for notation, folder in (("lisp", lisp), ("json", json_folder)):
            scenarios = [os.path.join(folder, name) for name in sorted(os.listdir(folder))
                         if name.endswith(".scenarios.json")] if os.path.isdir(folder) else []
            if scenarios and os.path.exists(runner):
                result["scenariosMs"][notation] = timed([runner, "--out", scratch, *scenarios], max(1, runs // 2))
    return result


def main():
    """Prints the measures; returns the exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("example")
    parser.add_argument("--cli", default=os.path.join(ROOT, "build", "affrescoui"))
    parser.add_argument("--runner", default=os.path.join(ROOT, "build", "affrescoui_render_runner"))
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--json", action="store_true", help="print one JSON object")
    arguments = parser.parse_args()
    os.environ.setdefault("AFFRESCOUI_HIDDEN", "1")
    result = measure(arguments.example, arguments.cli, arguments.runner, arguments.runs)
    if arguments.json:
        print(json.dumps(result))
        return 0
    for name, count in result["files"].items():
        print(f"{count:6}  {name}")
    print(f"longest definition: {result['longestDefinition'][0]} lines, {result['longestDefinition'][1]}")
    print(f"deepest indentation: {result['deepestIndent'][0]} spaces, {result['deepestIndent'][1]}")
    print(f"{{{{...}}}} texts left: {result['bindingTexts']}")
    print(f"visible text not in the locales: {result['textInPlace']}")
    print(f"first frame (ms, median): {result['firstFrameMs']}")
    print(f"scenarios (ms, median): {result['scenariosMs']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
