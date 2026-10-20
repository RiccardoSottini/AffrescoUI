#!/usr/bin/env python3
"""Runs every command of the affrescoui tool on every template (ctest "cli"), and checks what each one must do:

- check:  every template passes (exit 0), in text and with --json;
- fmt:    a template formatted once passes fmt --check, and reads back as the same JSON, keys in the same order;
- ids:    every template has an id on every node (ids --check);
- render: the first frame is pixel for pixel what the rendering check's runner draws for the same scenario;
- export and import: a change set goes into a screen's history on disk once, and comes back out of it;
- the exit codes of a file that isn't JSON (1), a wrong command line (2) and a file that can't be read (3);
- a text too long for the schema's patterns is reported, not matched (std::regex could overflow the stack);
- render connects to no channel the screen declares, even by an absolute path.

    python3 tools/affrescoui/check.py [--build DIR]

render needs a window: a display, or Xvfb on Linux (as the rendering check). Only the Python standard library is
needed; the PNG and PPM readers are the rendering check's.
"""

import glob
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SCREENS = os.path.join(ROOT, "examples", "assets", "screens")
sys.path.insert(0, os.path.join(ROOT, "tests", "render"))
import check as render_check  # noqa: E402  (read_png, read_ppm)

CHANGES = {"format": "affrescoui-changes", "version": 1, "name": "cli-check",
           "revisions": [{"author": "cli-check", "message": "A colour for the tool's check",
                          "patch": [{"op": "add", "path": "/colors/cliCheck", "value": "#123456"}]}]}


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
        result = subprocess.run([self.tool, *args], capture_output=True, text=True)
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

    def render(self, scenarios_file, scenario, path):
        """The first frame of a scenario that captures it first, against the runner's."""
        frames = os.path.join(self.scratch, "frames")
        os.makedirs(frames, exist_ok=True)
        capture = scenario["steps"][0]["capture"]
        if subprocess.run([self.runner, scenarios_file, scenario["name"], frames]).returncode != 0:
            return self.expect(False, f"the runner failed on {scenario['name']}")
        expected = render_check.read_ppm(os.path.join(frames, f"{scenario['name']}-{capture}.ppm"))
        out = os.path.join(self.scratch, scenario["name"] + ".png")
        self.run("render", path, "-o", out, "--scale", str(scenario.get("scale", 1)))
        self.expect(render_check.read_png(out) == expected, f"render {scenario['name']}: not the runner's first frame")

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
            json.dump({"format": "affrescoui-screen", "version": 1, "channels": {"host": {"socket": address}},
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
            json.dump({"format": "affrescoui-screen", "version": 1,
                       "root": {"id": "root", "color": "{{1" + " " * 30000 + "}}"}}, file)
        report = json.loads(self.run("check", path, "--json", expect=1))
        self.expect(any(problem["source"] == "schema" and "too long to check" in problem["message"]
                        for problem in report["problems"]), f"check: a long expression: {report}")


def main():
    args, build = sys.argv[1:], os.path.join(ROOT, "build")
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        else:
            sys.exit(__doc__)

    with tempfile.TemporaryDirectory(prefix="affrescoui-cli-") as scratch:
        checker = Checker(build, scratch)
        templates = sorted(glob.glob(os.path.join(SCREENS, "*.screen.json")))
        for path in templates:
            name = os.path.basename(path)[:-len(".screen.json")]
            checker.check(path)
            checker.format(name, path)
        for scenarios_file, scenario in render_check.read_scenarios():
            with open(scenarios_file, encoding="utf-8") as file:
                screens = os.path.join(os.path.dirname(scenarios_file), json.load(file)["screens"])
            path = os.path.normpath(os.path.join(screens, scenario["screen"]))
            # The templates' first frames: scenarios that capture before any step, on a shipped template
            if "capture" in scenario["steps"][0] and os.path.dirname(path) == os.path.normpath(SCREENS):
                checker.render(scenarios_file, scenario, path)
        checker.history(os.path.join(SCREENS, "kanban.screen.json"))

        checker.run("check", expect=2)                                  # no file
        checker.run("render", templates[0], expect=2)                  # no -o
        checker.run("nonsense", expect=2)
        checker.run("check", os.path.join(scratch, "missing.json"), expect=3)
        broken = os.path.join(scratch, "broken.json")
        with open(broken, "w", encoding="utf-8") as file:
            file.write('{ "version": 1, ')
        checker.run("fmt", broken, expect=1)
        checker.expect(json.loads(checker.run("check", broken, "--json", expect=1))["problems"][0]["source"] == "json",
                       "check: a file that isn't JSON")
        checker.long_pattern_input()
        checker.render_reaches_nobody()
        large = os.path.join(scratch, "large.png")  # 9000 x 2 = 18,000 pixels: refused before it allocates anything
        checker.run("render", templates[0], "-o", large, "--size", "9000x100", "--scale", "2", expect=3)
        checker.expect(not os.path.exists(large), "render: a frame too large to draw left a file")

    for failure in checker.failures:
        print(f"FAILED  {failure}")
    print(f"{len(templates)} templates, {checker.passed} checks passed, {len(checker.failures)} failed")
    sys.exit(1 if checker.failures else 0)


if __name__ == "__main__":
    main()
