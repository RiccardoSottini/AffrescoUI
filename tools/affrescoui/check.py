#!/usr/bin/env python3
"""Runs every command of the affrescoui tool on every template (ctest "cli"), and checks what each one must do:

- check:  every template passes (exit 0), in text and with --json;
- fmt:    a template formatted once passes fmt --check, and reads back as the same JSON, keys in the same order;
- ids:    every template has an id on every node (ids --check);
- strings: literal text moves to a language file, and a screen that requires that is held to it;
- convert: a template in the Lisp notation is canonical (fmt --check) and converts back to the same JSON, keys in
          the same order; a Lisp file that isn't a value exits with 1, naming its line;
- render: the first frame is pixel for pixel what the rendering check's runner draws for the same scenario;
- test:   every template's scenarios pass, in text and with --json; a failing expectation and a malformed file exit
          with 1;
- export and import: a change set goes into a screen's history on disk once, and comes back out of it;
- the exit codes of a file that isn't JSON (1), a wrong command line (2) and a file that can't be read (3);
- a text too long for the schema's patterns is reported, not matched (std::regex could overflow the stack);
- policy test sets a file's roles, and refuses one that isn't text;
- render connects to no channel the screen declares, even by an absolute path;
- render draws a code editor whose "insert" has no "text".

    python3 tools/affrescoui/check.py [--build DIR] [--headless]

render needs a window: a display, or Xvfb on Linux (as the rendering check). --headless leaves out what draws (the
Windows runners have no OpenGL 4.1). Only the Python standard library is needed.
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
EXAMPLES = os.path.join(ROOT, "examples")
with open(os.path.join(EXAMPLES, "templates.json"), encoding="utf-8") as templates_file:
    TEMPLATE_FOLDERS = {os.path.join(EXAMPLES, folder) for folder in json.load(templates_file)["templates"].values()}
KANBAN = os.path.join(EXAMPLES, "flow", "json")  # the Kanban template's folder
RENDER_SCENARIOS = os.path.join(ROOT, "tests", "render", "scenarios")
sys.path.insert(0, os.path.join(ROOT, "tools", "media"))
import pngfile  # noqa: E402 (tools/media/: reads the PNGs the tool and the runner write)

os.environ.setdefault("AFFRESCOUI_HIDDEN", "1")  # its programs never show a window (CONTRIBUTING.md); =0 shows them


def template_files(pattern):
    """The templates' files that match the pattern, from each one's folder (examples/templates.json), by name."""
    files = [path for folder in TEMPLATE_FOLDERS for path in glob.glob(os.path.join(folder, pattern))]
    return sorted(files, key=os.path.basename)


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
        self.convert(name, path)

    def convert(self, name, path):
        """The template in the Lisp notation, canonical, and back to the same JSON."""
        lisp, back = (os.path.join(self.scratch, name + suffix) for suffix in (".screen.lisp", ".back.json"))
        self.run("convert", path, "--to", "lisp", "-o", lisp)
        self.run("fmt", "--check", lisp)
        self.run("convert", lisp, "-o", back)
        with open(path, encoding="utf-8") as before, open(back, encoding="utf-8") as after:
            same = json.load(before, object_pairs_hook=list) == json.load(after, object_pairs_hook=list)
        self.expect(same, f"convert {name}: the Lisp file converts back to different JSON")

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
        expected = pngfile.read(os.path.join(frames, f"{scenario['name']}-first.png"))[:3]
        out = os.path.join(self.scratch, scenario["name"] + ".png")
        self.run("render", path, "-o", out, "--scale", str(scenario.get("scale", 1)))
        self.expect(pngfile.read(out)[:3] == expected, f"render {scenario['name']}: not the runner's first frame")

    def test(self, path):
        """A template's scenarios pass (their captures written to a scratch folder); a failing one and a malformed file
        exit with 1."""
        captures = os.path.join(self.scratch, "captures")
        os.makedirs(captures, exist_ok=True)
        self.expect(" 0 failed" in self.run("test", path, "--out", captures), f"test {path}: a scenario failed")
        report = json.loads(self.run("test", path, "--json", "--out", captures))
        # Without OpenGL (AFFRESCOUI_NO_OPENGL=1) the scenarios that draw are left out: some files have only those
        least = 0 if os.environ.get("AFFRESCOUI_NO_OPENGL") == "1" else 5
        self.expect(report["failed"] == 0 and report["passed"] >= least, f"test --json {path}: {report}")
        wrong = os.path.join(self.scratch, "wrong.scenarios.json")
        with open(wrong, "w", encoding="utf-8") as file:
            json.dump({"version": 2, "screens": KANBAN, "screen": "index.screen.json", "scenarios": [
                {"name": "wrong", "steps": [{"expect": {"state": "board", "is": "Not the board"}}]}]}, file)
        output = self.run("test", wrong, expect=1)
        self.expect('FAILED wrong, step 1: state board is "Product launch", expected "Not the board"' in output,
                    f"test: a failing expectation: {output}")
        with open(wrong, "w", encoding="utf-8") as file:
            json.dump({"version": 2, "screens": KANBAN, "screen": "index.screen.json",
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

    def insert_without_text(self):
        """An editor whose "insert" has no "text": render draws it and inserts nothing (the code editor this replaced
        read the missing key through a const operator[], which is undefined behaviour and aborted a debug build)."""
        path = os.path.join(self.scratch, "insert.screen.json")
        with open(path, "w", encoding="utf-8") as file:
            json.dump({"format": "affrescoui-screen", "version": 2, "window": {"width": 200, "height": 100},
                       "root": {"id": "root", "children": [{"type": "editor", "id": "code", "width": 200, "height": 100,
                                                            "insert": {"before": 1}}]}}, file)
        self.run("render", path, "-o", os.path.join(self.scratch, "insert.png"))

    def policy_roles(self):
        """A policy test file that gives roles: each one is set, and one that isn't text is refused (the roles were
        read from a temporary that was gone, which GCC's AddressSanitizer reported)."""
        case = {"name": "a designer's change waits for review", "expect": "ask",
                "revision": {"author": "agent/claude-code", "set": {"#panel-root/color": "#000000"}}}
        for role, expect in [("design", 0), (7, 1)]:
            path = os.path.join(self.scratch, "roles.policy.json")
            with open(path, "w", encoding="utf-8") as file:
                json.dump({"screen": os.path.join(KANBAN, "index.screen.json"),
                           "roles": {"agent/claude-code": role}, "cases": [case]}, file)
            self.run("policy", "test", path, expect=expect)

    def lisp_documents(self):
        """A lisp/ folder's own files work as its JSON ones do: its app.lisp, a policy, and a feature's manifest,
        which gives the same answer in either notation."""
        lisp = os.path.join(EXAMPLES, "flow", "lisp")
        self.run("check", lisp)
        self.run("check", os.path.join(lisp, "app.lisp"))
        self.run("policy", "test", os.path.join(lisp, "index.policy.lisp"))
        forge = os.path.join(EXAMPLES, "forge", "lisp")
        written = os.path.join(EXAMPLES, "forge", "json")
        as_lisp = self.run("check", "--compose", os.path.join(forge, "index.screen.lisp"),
                           os.path.join(forge, "features", "word-count"), expect=1)
        as_json = self.run("check", "--compose", os.path.join(written, "index.screen.json"),
                           os.path.join(written, "features", "word-count"), expect=1)
        self.expect(as_lisp == as_json, f"compose: the notations differ:\n{as_lisp}\n{as_json}")

    def strings(self):
        """Literal text moves to a language file and the screen's text doesn't change; a screen that requires it fails
        check until it has; a text with quotes and braces survives as a message; the file's layout stays."""
        folder = os.path.join(self.scratch, "strings")
        os.makedirs(folder)
        path = os.path.join(folder, "index.screen.json")
        with open(path, "w", encoding="utf-8") as file:
            file.write('''{
  "format": "affrescoui-screen", "version": 2,
  "state": { "name": "" },
  "strings": { "default": "en", "required": true, "keep": ["Brand"] },
  "components": { "note": { "params": { "words": "", "tag": "" },
    "node": { "id": "noteLabel", "type": "label", "text": "{{words}}", "tooltip": "{{t(tag)}}" } } },
  "actions": { "go": { "title": "Don't {go}", "category": "Brand", "do": [] } },
  "root": { "id": "root", "children": [
    { "id": "saved", "type": "label", "text": "Saved {{state.name}}" },
    { "id": "plus", "type": "label", "text": "+" },
    { "id": "code", "type": "editor", "text": "not text a person translates" },
    { "id": "hello", "type": "label", "text": "Hello", "hover": { "text": "Hi" } },
    { "id": "use", "use": "note", "with": { "words": "Note", "tag": "key" } }
  ] } }
''')
        self.run("check", path, expect=1)
        report = json.loads(self.run("strings", "report", path, "--json"))
        found = {literal["at"]: literal for literal in report["literals"]}
        self.expect(sorted(found) == ["/actions/go/title", "/root/children/0/text", "/root/children/3/hover/text",
                                      "/root/children/3/text", "/root/children/4/with/words"],
                    f"strings report: found {sorted(found)}")
        self.expect(found["/root/children/0/text"]["built"], "strings report: text around an expression")
        self.run("strings", "extract", path)  # lists, and writes nothing
        self.expect(not os.path.exists(os.path.join(folder, "locales")), "strings extract: wrote without --write")
        self.run("strings", "extract", path, "--write")
        with open(os.path.join(folder, "locales", "en.json"), encoding="utf-8") as file:
            messages = json.load(file)["strings"]["en"]
        self.expect(messages == {"action.go.title": "Don''t '{'go'}'", "hello.text": "Hello", "hello.hover.text": "Hi",
                                 "use.words": "Note"}, f"strings extract: wrote {messages}")
        with open(path, encoding="utf-8") as file:
            written = file.read()
        self.expect('  "format": "affrescoui-screen", "version": 2,\n  "include": ["locales/en.json"],\n  "state"' in written
                    and '{ "id": "plus", "type": "label", "text": "+" },\n' in written,
                    "strings extract: the file's layout changed")
        problems = json.loads(self.run("check", path, "--json", expect=1))["problems"]
        self.expect([problem["source"] for problem in problems] == ["strings"], f"check: {problems}")  # the text left
        with open(path, encoding="utf-8") as file:
            fixed = file.read().replace("Saved {{state.name}}", "{{state.name}}")
        with open(path, "w", encoding="utf-8") as file:
            file.write(fixed)
        self.run("check", path)
        self.run("strings", "check", path, expect=0)
        report = json.loads(self.run("strings", "report", os.path.join(KANBAN), "--json"))  # an app's folder, its entry
        self.expect(report["file"].endswith("index.screen.json"), f"strings report: read {report['file']}")
        lisp = os.path.join(folder, "index.screen.lisp")
        self.run("convert", path, "-o", lisp)
        self.run("strings", "extract", lisp, expect=1)  # Lisp files are generated

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


def parse_arguments(args):
    """Reads the command line: (the build folder, whether to leave out what draws). Exits with the usage on anything
    else."""
    build, headless = os.path.join(ROOT, "build"), False
    while args:
        arg = args.pop(0)
        if arg == "--build" and args:
            build = args.pop(0)
        elif arg == "--headless":
            headless = True
        else:
            sys.exit(__doc__)
    return build, headless


def check_templates(checker, headless):
    """Checks, formats, renders and tests every template, and round-trips a history. Returns the screens."""
    templates = template_files("*.screen.json")
    for path in templates:
        name = os.path.basename(path)[:-len(".screen.json")]
        checker.check(path)
        checker.format(name, path)
    for screens, scenario in [] if headless else render_scenarios():
        path = os.path.normpath(os.path.join(screens, scenario["screen"]))
        # The templates' first frames: scenarios that expect a picture before any step, on a shipped template
        if "image" in scenario["steps"][0].get("expect", {}) and os.path.dirname(path) in TEMPLATE_FOLDERS:
            checker.render(screens, scenario, path)
    for path in template_files("*.scenarios.json"):
        checker.test(path)
    checker.history(os.path.join(KANBAN, "index.screen.json"))
    return templates


def main():
    """Runs every check and prints the failures; exits with 1 if there is one."""
    build, headless = parse_arguments(sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="affrescoui-cli-") as scratch:
        checker = Checker(build, scratch)
        templates = check_templates(checker, headless)

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
        broken_lisp = os.path.join(scratch, "broken.screen.lisp")
        with open(broken_lisp, "w", encoding="utf-8") as file:
            file.write('{:version 2\n :root (row #a')
        problems = json.loads(checker.run("check", broken_lisp, "--json", expect=1))["problems"]
        checker.expect(problems[0]["source"] == "lisp" and "line 2, column 15" in problems[0]["message"],
                       f"check: a Lisp file that isn't a value: {problems}")
        checker.run("convert", broken_lisp, expect=1)
        checker.run("convert", broken, "--to", "yaml", expect=2)
        checker.long_pattern_input()
        checker.strings()
        checker.policy_roles()
        checker.lisp_documents()
        if not headless:
            checker.render_reaches_nobody()
            checker.insert_without_text()
            large = os.path.join(scratch, "large.png")  # 9000 x 2 = 18,000 pixels: refused before it allocates
            checker.run("render", templates[0], "-o", large, "--size", "9000x100", "--scale", "2", expect=3)
            checker.expect(not os.path.exists(large), "render: a frame too large to draw left a file")

    for failure in checker.failures:
        print(f"FAILED  {failure}")
    print(f"{len(templates)} templates, {checker.passed} checks passed, {len(checker.failures)} failed")
    sys.exit(1 if checker.failures else 0)


if __name__ == "__main__":
    main()
