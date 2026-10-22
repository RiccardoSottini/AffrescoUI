#!/usr/bin/env python3
"""Runs every command of the affrescoui tool on every template (ctest "cli"), and checks what each one must do:

- check:  every template passes (exit 0), in text and with --json;
- fmt:    a template formatted once passes fmt --check, and reads back as the same JSON, keys in the same order;
- ids:    every template has an id on every node (ids --check);
- render: the first frame is pixel for pixel what the rendering check's runner draws for the same scenario;
- test:   every template's scenarios pass, in text and with --json; a failing expectation and a malformed file exit
          with 1;
- export and import: a change set goes into a screen's history on disk once, and comes back out of it;
- the exit codes of a file that isn't JSON (1), a wrong command line (2) and a file that can't be read (3);
- a text too long for the schema's patterns is reported, not matched (std::regex could overflow the stack);
- render connects to no channel the screen declares, even by an absolute path.

    python3 tools/affrescoui/check.py [--build DIR] [--headless]

render needs a window: a display, or Xvfb on Linux (as the rendering check). --headless leaves out what draws (the
Windows runners have no OpenGL 4.1). Only the Python standard library is needed.
"""

import glob
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SCREENS = os.path.join(ROOT, "examples", "assets", "screens")
RENDER_SCENARIOS = os.path.join(ROOT, "tests", "render", "scenarios")


def read_png(path):
    """An 8-bit RGB PNG (any row filter), as the tool and the runner write them: (width, height, RGB bytes)."""
    with open(path, "rb") as file:
        data = file.read()
    position, idat = 8, b""
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        if kind == b"IHDR":
            width, height = struct.unpack(">II", body[:8])
        elif kind == b"IDAT":
            idat += body
        position += 12 + length
    raw, stride, out = zlib.decompress(idat), width * 3, bytearray()
    previous = bytearray(stride)
    for y in range(height):
        kind, row = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride if kind else 0):
            left, up = row[i - 3] if i >= 3 else 0, previous[i]
            corner = previous[i - 3] if i >= 3 else 0
            if kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                predicted = left if pa <= pb and pa <= pc else up if pb <= pc else corner
            else:
                predicted = {1: left, 2: up, 3: (left + up) // 2}[kind]
            row[i] = (row[i] + predicted) & 255
        out += row
        previous = row
    return width, height, bytes(out)


def render_scenarios():
    """The rendering check's scenarios, as (the folder of their screens, the scenario)."""
    for name in sorted(os.listdir(RENDER_SCENARIOS)):
        with open(os.path.join(RENDER_SCENARIOS, name), encoding="utf-8") as file:
            area = json.load(file)
        for scenario in area["scenarios"]:
            yield os.path.normpath(os.path.join(RENDER_SCENARIOS, area["screens"])), scenario

CHANGES = {"format": "affrescoui-changes", "version": 2, "name": "cli-check",
           "revisions": [{"author": "cli-check", "message": "A colour for the tool's check",
                          "patch": [{"op": "add", "path": "/tokens/color.cliCheck", "value": "#123456"}]}]}


class Checker:
    """Runs the tool and keeps the failures."""

    def __init__(self, build, scratch):
        self.tool = os.path.join(build, "affrescoui")
        self.runner = os.path.join(build, "affrescoui_render_runner")
        self.scratch = scratch
        self.failures = []
        self.passed = 0

    def run(self, *args, expect=0):
        """Runs the tool; a different exit code is a failure. Returns its output."""
        result = subprocess.run([self.tool, *args], capture_output=True, text=True, encoding="utf-8")
        self.expect(result.returncode == expect,
                    f"affrescoui {' '.join(args)}: exit {result.returncode}, expected {expect}\n"
                    f"{result.stdout}{result.stderr}")
        return result.stdout

    def expect(self, condition, failure):
        if condition:
            self.passed += 1
        else:
            self.failures.append(failure)

    def check(self, path):
        self.run("check", path)
        report = json.loads(self.run("check", path, "--json"))
        self.expect(report["ok"] and report["problems"] == [], f"check --json {path}: {report}")

    def format(self, name, path):
        copy = os.path.join(self.scratch, name + ".screen.json")
        shutil.copyfile(path, copy)
        self.run("fmt", copy)
        self.run("fmt", "--check", copy)
        with open(path, encoding="utf-8") as before, open(copy, encoding="utf-8") as after:
            same = json.load(before, object_pairs_hook=list) == json.load(after, object_pairs_hook=list)
        self.expect(same, f"fmt {name}: the formatted file reads as different JSON")
        self.run("ids", "--check", path)

    def render(self, screens, scenario, path):
        """The first frame of a scenario whose first step expects it, against the runner's capture of it."""
        frames = os.path.join(self.scratch, "frames")
        os.makedirs(frames, exist_ok=True)
        first = {"format": "affrescoui-scenarios", "version": 2, "screens": screens,
                 "scenarios": [{"name": scenario["name"], "screen": scenario["screen"],
                                "scale": scenario.get("scale", 1), "steps": [{"capture": "first"}]}]}
        scenarios_file = os.path.join(self.scratch, "first.scenarios.json")
        with open(scenarios_file, "w", encoding="utf-8") as file:
            json.dump(first, file)
        if subprocess.run([self.runner, "--out", frames, scenarios_file], capture_output=True).returncode != 0:
            return self.expect(False, f"the runner failed on {scenario['name']}")
        expected = read_png(os.path.join(frames, f"{scenario['name']}-first.png"))
        out = os.path.join(self.scratch, scenario["name"] + ".png")
        self.run("render", path, "-o", out, "--scale", str(scenario.get("scale", 1)))
        self.expect(read_png(out) == expected, f"render {scenario['name']}: not the runner's first frame")

    def test(self, path):
        """A template's scenarios pass; a failing one and a malformed file exit with 1."""
        self.expect(" 0 failed" in self.run("test", path), f"test {path}: a scenario failed")
        report = json.loads(self.run("test", path, "--json"))
        self.expect(report["failed"] == 0 and report["passed"] >= 5, f"test --json {path}: {report}")
        wrong = os.path.join(self.scratch, "wrong.scenarios.json")
        with open(wrong, "w", encoding="utf-8") as file:
            json.dump({"version": 2, "screens": SCREENS, "screen": "kanban.screen.json", "scenarios": [
                {"name": "wrong", "steps": [{"expect": {"state": "board", "is": "Not the board"}}]}]}, file)
        output = self.run("test", wrong, expect=1)
        self.expect('FAILED wrong, step 1: state board is "Product launch", expected "Not the board"' in output,
                    f"test: a failing expectation: {output}")
        with open(wrong, "w", encoding="utf-8") as file:
            json.dump({"version": 2, "screens": SCREENS, "screen": "kanban.screen.json",
                       "scenarios": [{"name": "a", "steps": [{"clik": "#card"}]}]}, file)
        self.run("test", wrong, expect=1)

    def history(self, path):
        data = os.path.join(self.scratch, "data")
        changes = os.path.join(self.scratch, "changes.json")
        with open(changes, "w", encoding="utf-8") as file:
            json.dump(CHANGES, file)
        self.expect("revision 1" in self.run("import", path, changes, "--data", data), "import: not applied")
        self.expect("already applied" in self.run("import", path, changes, "--data", data), "import: applied twice")
        exported = json.loads(self.run("export", path, "--data", data, "--from", "0"))
        patches = [revision["patch"] for revision in exported["revisions"]]
        self.expect(patches == [CHANGES["revisions"][0]["patch"]], f"export: {exported}")

    def render_reaches_nobody(self):
        """A screen whose channel names a listening socket by an absolute path, as a host's studio.sock: render draws
        it without connecting (it used to connect, and send what the screen's open handler said)."""
        if not hasattr(socket, "AF_UNIX"):
            return
        address = os.path.join(self.scratch, "host.sock")
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        listener.bind(address)
        listener.listen(4)
        listener.setblocking(False)
        path = os.path.join(self.scratch, "channels.screen.json")
        with open(path, "w", encoding="utf-8") as file:
            json.dump({"format": "affrescoui-screen", "version": 2, "channels": {"host": {"socket": address}},
                       "actions": {"host:open": [{"send": "host", "data": {"type": "setMode", "mode": "acceptEdits"}}]},
                       "root": {"id": "root", "color": "#203040"}}, file)
        self.run("render", path, "-o", os.path.join(self.scratch, "channels.png"))
        try:
            listener.accept()
            self.expect(False, "render: connected to the socket a channel names")
        except BlockingIOError:
            self.expect(True, "")
        listener.close()

    def long_pattern_input(self):
        """A colour expression of 30,000 characters: the schema's patterns aren't run on it (libstdc++'s std::regex
        overflowed the stack on it), so check reports it as too long to check instead of crashing."""
        path = os.path.join(self.scratch, "long.screen.json")
        with open(path, "w", encoding="utf-8") as file:
            json.dump({"format": "affrescoui-screen", "version": 2,
                       "root": {"id": "root", "color": "{{1" + " " * 30000 + "}}"}}, file)
        report = json.loads(self.run("check", path, "--json", expect=1))
        self.expect(any(problem["source"] == "schema" and "too long to check" in problem["message"]
                        for problem in report["problems"]), f"check: a long expression: {report}")


def main():
    args, build, headless = sys.argv[1:], os.path.join(ROOT, "build"), False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--headless":
            headless = True
        else:
            sys.exit(__doc__)

    with tempfile.TemporaryDirectory(prefix="affrescoui-cli-") as scratch:
        checker = Checker(build, scratch)
        templates = sorted(glob.glob(os.path.join(SCREENS, "*.screen.json")))
        for path in templates:
            name = os.path.basename(path)[:-len(".screen.json")]
            checker.check(path)
            checker.format(name, path)
        for screens, scenario in [] if headless else render_scenarios():
            path = os.path.normpath(os.path.join(screens, scenario["screen"]))
            # The templates' first frames: scenarios that expect a picture before any step, on a shipped template
            if "image" in scenario["steps"][0].get("expect", {}) and os.path.dirname(path) == os.path.normpath(SCREENS):
                checker.render(screens, scenario, path)
        for path in sorted(glob.glob(os.path.join(SCREENS, "*.scenarios.json"))):
            checker.test(path)
        checker.history(os.path.join(SCREENS, "kanban.screen.json"))

        checker.run("check", expect=2)                                  # no file
        checker.run("render", templates[0], expect=2)                  # no -o
        checker.run("nonsense", expect=2)
        checker.run("check", os.path.join(scratch, "missing.json"), expect=3)
        broken = os.path.join(scratch, "broken.json")
        with open(broken, "w", encoding="utf-8") as file:
            file.write('{ "version": 2, ')
        checker.run("fmt", broken, expect=1)
        checker.expect(json.loads(checker.run("check", broken, "--json", expect=1))["problems"][0]["source"] == "json",
                       "check: a file that isn't JSON")
        checker.long_pattern_input()
        if not headless:
            checker.render_reaches_nobody()
            large = os.path.join(scratch, "large.png")  # 9000 x 2 = 18,000 pixels: refused before it allocates
            checker.run("render", templates[0], "-o", large, "--size", "9000x100", "--scale", "2", expect=3)
            checker.expect(not os.path.exists(large), "render: a frame too large to draw left a file")

    for failure in checker.failures:
        print(f"FAILED  {failure}")
    print(f"{len(templates)} templates, {checker.passed} checks passed, {len(checker.failures)} failed")
    sys.exit(1 if checker.failures else 0)


if __name__ == "__main__":
    main()
