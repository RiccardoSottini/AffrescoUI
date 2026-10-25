"""The generalism gate: the engine holds mechanisms, and an app is a folder of JSON (tools/quality/README.md).

Its rules, each a finding per place:
- sources-in-examples: no C++ under examples/ (an app is JSON the stock host runs);
- host-sources: the stock host (affrescoui_launcher) compiles nothing from examples/;
- host-registers-types: apps/host registers no node type;
- app-name: no app's name (an examples/*/app.json's id or name) in modules/ or apps/host, Markdown aside;
- theme-outside-themes: no component definition or colour value in modules/ outside modules/themes;
- links-themes: no module but screen links themes (the entry points, outside modules/, may).

What is left over is in generalism-allow.json, each place with its reason. An entry that matches nothing fails too,
so the list only shrinks.

Usage: python3 tools/quality/generalism.py [--root DIR] [--allow FILE]
"""

import argparse
import json
import os
import re
import subprocess
import sys

CPP = (".cpp", ".hpp", ".h")
CODE = CPP + (".json", ".txt", ".cmake", ".in", ".py")
COLOUR = re.compile(r'"#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?"')
COMPONENTS = re.compile(r'"components"\s*:\s*\{|\{\s*"components"\s*,\s*\{')
THEMES = re.compile(r"affrescoui(?:::|_)themes\b")


def tracked(root):
    """The repository's files, tracked or new but not ignored, relative to root."""
    output = subprocess.run(["git", "-C", root, "ls-files", "--cached", "--others", "--exclude-standard"],
                            check=True, capture_output=True, text=True).stdout
    return sorted({path for path in output.splitlines() if os.path.isfile(os.path.join(root, path))})


def read(root, path):
    """A file's lines, with bytes that aren't UTF-8 replaced."""
    with open(os.path.join(root, path), encoding="utf-8", errors="replace") as file:
        return file.read().splitlines()


def code_of(line):
    """A C++ or CMake line without its comment: a whole comment line is empty, a trailing one is cut."""
    stripped = line.strip()
    if stripped.startswith(("//", "/*", "*", "#")) and not stripped.startswith(("#include", "#if", "#define")):
        return ""
    return re.split(r"\s//", line, maxsplit=1)[0]


def app_names(root, paths, ordinary):
    """The names apps go by, lower case: each app.json's id and name, but for ordinary words."""
    names = set()
    for path in paths:
        if path.startswith("examples/") and path.endswith("/app.json"):
            try:
                app = json.loads("\n".join(read(root, path)))
            except ValueError:
                continue
            names.update(str(app.get(key, "")).lower() for key in ("id", "name"))
    return sorted(name for name in names if name and name not in ordinary)


def check_sources(root, paths):
    """sources-in-examples, host-sources and host-registers-types."""
    findings = [("sources-in-examples", path, 0, "C++ in an app's folder")
                for path in paths if path.startswith("examples/") and path.endswith(CPP)]
    for path in paths:
        if os.path.basename(path) == "CMakeLists.txt" or path.endswith(".cmake"):
            for number, line in enumerate(read(root, path), 1):
                text = code_of(line)
                if "affrescoui_launcher" in text and "examples/" in text:
                    findings.append(("host-sources", path, number, text.strip()))
        if path.startswith("apps/host/") and path.endswith(CPP):
            for number, line in enumerate(read(root, path), 1):
                if "registerType" in code_of(line):
                    findings.append(("host-registers-types", path, number, line.strip()))
    return findings


def check_names(root, paths, names):
    """app-name, in modules/ and apps/host."""
    if not names:
        return []
    pattern = re.compile(r"(?<![\w-])(?:" + "|".join(re.escape(name) for name in names) + r")(?![\w-])", re.I)
    findings = []
    for path in paths:
        if path.startswith(("modules/", "apps/host/")) and path.endswith(CODE):
            for number, line in enumerate(read(root, path), 1):
                if match := pattern.search(line):
                    findings.append(("app-name", path, number, match.group(0)))
    return findings


def check_themes(root, paths):
    """theme-outside-themes and links-themes, in modules/."""
    findings = []
    for path in paths:
        if not path.startswith("modules/") or path.startswith("modules/themes/"):
            continue
        if path.endswith(CPP):
            for number, line in enumerate(read(root, path), 1):
                text = code_of(line)
                if COLOUR.search(text) or COMPONENTS.search(text):
                    findings.append(("theme-outside-themes", path, number, text.strip()))
        elif path.endswith(".json"):
            try:
                document = json.loads("\n".join(read(root, path)))
            except ValueError:
                continue
            if isinstance(document, dict) and isinstance(document.get("components"), dict) and document["components"]:
                findings.append(("theme-outside-themes", path, 0, "a components section"))
        elif os.path.basename(path) == "CMakeLists.txt" and not path.startswith("modules/screen/"):
            for number, line in enumerate(read(root, path), 1):
                if THEMES.search(code_of(line)):
                    findings.append(("links-themes", path, number, line.strip()))
    return findings


def matches(place, finding):
    """Whether an allow-list place covers a finding: a path prefix, or "path prefix: text" the finding's text has."""
    prefix, _, text = place.partition(": ")
    return finding[1].startswith(prefix) and text in finding[3]


def check(root, paths, allow):
    """Every finding, and the allow-list's entries that matched none.

    allow is generalism-allow.json's content: {"ordinaryWords": [...], "allowed": {rule: {place: reason}}}, a place
    being a path prefix, or "path prefix: text" for the findings there whose text has it. Returns (the findings not
    allowed, the stale entries as "rule: place").
    """
    ordinary = {word.lower() for word in allow.get("ordinaryWords", [])}
    findings = (check_sources(root, paths) + check_names(root, paths, app_names(root, paths, ordinary)) +
                check_themes(root, paths))
    allowed = allow.get("allowed", {})
    used = set()
    left = []  # the findings no place covers
    for finding in findings:
        places = [place for place in allowed.get(finding[0], {}) if matches(place, finding)]
        used.update((finding[0], place) for place in places)
        if not places:
            left.append(finding)
    stale = [f"{rule}: {place}" for rule, entries in allowed.items() for place in entries
             if (rule, place) not in used]
    return left, stale


def main():
    """Runs the gate on the repository; exits 1 on a finding or a stale entry."""
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(here)))
    parser.add_argument("--allow", default=os.path.join(here, "generalism-allow.json"))
    options = parser.parse_args()
    with open(options.allow, encoding="utf-8") as file:
        allow = json.load(file)
    findings, stale = check(options.root, tracked(options.root), allow)
    for rule, path, line, text in findings:
        print(f"{path}:{line}: {rule}: {text}" if line else f"{path}: {rule}: {text}")
    for entry in stale:
        print(f"generalism-allow.json: {entry}: matches nothing now; remove it")
    if findings or stale:
        print(f"generalism: {len(findings)} findings, {len(stale)} stale entries (tools/quality/README.md)")
        return 1
    print("generalism: no findings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
