#!/usr/bin/env python3
"""Writes each example's lisp/ folder: the app of its json/ folder, complete, in the Lisp notation.

An example holds the same app twice, examples/<app>/json/ and examples/<app>/lisp/. The json/ folder is the source, so a
change is made there and this is run again; `--check` fails if what is there isn't what it would write
(`ctest -R cli_lisp_examples`).

Every document an app loads becomes a .lisp file: its entry and every other screen, what they include, its themes,
models and seed data. The files with a reader of their own stay JSON and are copied (app.json, scenarios, policies,
adapters, a package's manifest and catalogue). Assets are never copied: both folders read examples/<app>/assets/. What
the apps share in examples/shared/ gets a .lisp file beside its JSON, as the fonts there serve both.

    python3 tools/notation/lisp_examples.py [--check] [--cli build/affrescoui]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXAMPLES = os.path.join(ROOT, "examples")
JSON_FOLDER = os.sep + "json" + os.sep
LISP_FOLDER = os.sep + "lisp" + os.sep
SHARED = "shared" + os.sep

# What a running app never reads: code and tests
SKIPPED_SUFFIXES = (".py", ".cpp", ".hpp", ".c", ".h", ".cmake")
# Written for the folder it is copied to, not the one it is in: the Launcher's blank app, always copied from the JSON
LEFT_OUT = os.path.join("launcher", "json", "starters") + os.sep


def source_files():
    """The files in examples/ the generator reads (paths from there, git's ignored files left out)."""
    listed = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", "examples"],
                            cwd=ROOT, check=True, capture_output=True, text=True).stdout
    paths = [os.path.relpath(os.path.join(ROOT, path), EXAMPLES) for path in listed.split("\0") if path]
    return sorted(path for path in paths if os.path.exists(os.path.join(EXAMPLES, path)) and not is_generated(path)
                  and not path.startswith(LEFT_OUT))


def is_generated(path):
    """Whether a file of examples/ is one the generator writes."""
    return LISP_FOLDER in os.sep + path or (path.startswith(SHARED) and path.endswith(".lisp"))


def read_json(path):
    """A JSON file's value, its keys in their order."""
    with open(os.path.join(EXAMPLES, path), encoding="utf-8") as file:
        return json.load(file)


def includes_of(document):
    """The files a document includes, as it names them: only a screen's top-level "include" is one."""
    named = document.get("include", []) if isinstance(document, dict) else []
    return [named] if isinstance(named, str) else named


def as_lisp(name):
    """A .json file's name in the Lisp notation."""
    return name[:-len(".json")] + ".lisp"


def target_of(path):
    """Where a file of a json/ folder goes in the lisp/ folder beside it; a shared file stays in its folder."""
    return path if path.startswith(SHARED) else path.replace(JSON_FOLDER, LISP_FOLDER, 1)


def documents_of(path, found):
    """Adds a document and what it includes, relative to examples/, to a set."""
    path = os.path.normpath(path)
    if path in found:
        return
    found.add(path)
    for name in includes_of(read_json(path)):
        documents_of(os.path.join(os.path.dirname(path), name), found)


def documents(files):
    """Every document the example apps load, as paths relative to examples/."""
    found = set()
    for path in files:
        if path.endswith(".screen.json"):
            documents_of(path, found)
        elif os.path.basename(path) == "app.json":
            app = read_json(path)
            folder = os.path.dirname(path)
            data = app.get("data", {})
            named = [app.get("entry", "index.screen.json"), *app.get("themes", [])]
            named += [data["models"]] if "models" in data else []
            named += [file for file in data.get("seed", {}).values() if file.endswith(".json")]
            for name in named:
                documents_of(os.path.join(folder, name), found)
    return found


def is_copied(path):
    """Whether a file that isn't a document belongs in a lisp/ folder: one of a json/ folder, read when running."""
    return JSON_FOLDER in os.sep + path and not path.endswith(SKIPPED_SUFFIXES) and not path.endswith(".md")


def write_lisp(path, out, cli):
    """Writes a document in the Lisp notation, with its includes named in that notation too."""
    document = read_json(path)
    target = os.path.join(out, as_lisp(target_of(path)))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    source = os.path.join(EXAMPLES, path)
    if includes_of(document):
        if isinstance(document["include"], str):
            document["include"] = as_lisp(document["include"])
        else:
            document["include"] = [as_lisp(name) for name in document["include"]]
        source = target + ".json"
        with open(source, "w", encoding="utf-8") as file:
            json.dump(document, file, ensure_ascii=False)
    subprocess.run([cli, "convert", source, "--to", "lisp", "-o", target], check=True)
    if source == target + ".json":
        os.remove(source)


def manifest_text(path, loaded):
    """An app.json for its lisp/ folder: the same app, naming its files in the Lisp notation."""
    app = read_json(path)
    folder = os.path.dirname(path)

    def named(name):
        return as_lisp(name) if os.path.normpath(os.path.join(folder, name)) in loaded else name

    app["entry"] = named(app.get("entry", "index.screen.json"))
    if "themes" in app:
        app["themes"] = [named(name) for name in app["themes"]]
    data = app.get("data", {})
    if "models" in data:
        data["models"] = named(data["models"])
    if "seed" in data:
        data["seed"] = {collection: named(name) for collection, name in data["seed"].items()}
    return json.dumps(app, ensure_ascii=False, indent="\t") + "\n"


def checks_text(path):
    """A scenarios or policy file for the lisp/ folder: its screen in the Lisp notation."""
    with open(os.path.join(EXAMPLES, path), encoding="utf-8") as file:
        return re.sub(r'("screen"\s*:\s*"[^"]*\.screen)\.json"', r'\1.lisp"', file.read())


def write_tree(out, cli):
    """Writes every generated file, at its path from examples/, into a folder."""
    files = source_files()
    loaded = documents(files)
    for path in files:
        if path in loaded:
            write_lisp(path, out, cli)
        elif is_copied(path):
            target = os.path.join(out, target_of(path))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            text = None
            if os.path.basename(path) == "app.json":
                text = manifest_text(path, loaded)
            elif path.endswith((".scenarios.json", ".policy.json")):
                text = checks_text(path)
            if text is None:
                shutil.copyfile(os.path.join(EXAMPLES, path), target)
            else:
                with open(target, "w", encoding="utf-8", newline="\n") as file:
                    file.write(text)


def generated_files():
    """The generated files in examples/, as paths from there."""
    found = set()
    for place, _, names in os.walk(EXAMPLES):
        found.update(os.path.relpath(os.path.join(place, name), EXAMPLES) for name in names)
    return {path for path in found if is_generated(path)}


def written_files(folder):
    """The files under a folder, relative to it."""
    found = set()
    for place, _, names in os.walk(folder):
        found.update(os.path.relpath(os.path.join(place, name), folder) for name in names)
    return found


def contents(path):
    """A file's bytes, its line ends as git keeps them: a Windows checkout's are its own."""
    with open(path, "rb") as file:
        return file.read().replace(b"\r\n", b"\n")


def differences(written):
    """What differs between what would be written and what is there, one line each."""
    wanted, there = written_files(written), generated_files()
    lines = [f"missing: {path}" for path in sorted(wanted - there)]
    lines += [f"not generated: {path}" for path in sorted(there - wanted)]
    lines += [f"out of date: {path}" for path in sorted(wanted & there)
              if contents(os.path.join(written, path)) != contents(os.path.join(EXAMPLES, path))]
    return lines


def main():
    """Writes the lisp/ folders or, with --check, compares them; returns the exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if a lisp/ folder is out of date; write nothing")
    parser.add_argument("--cli", default=os.path.join(ROOT, "build", "affrescoui"), help="the affrescoui program")
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory() as scratch:
        write_tree(scratch, arguments.cli)
        if arguments.check:
            lines = differences(scratch)
            for line in lines[:40]:
                print(line)
            if lines:
                print(f"the Lisp examples are out of date ({len(lines)} files): run tools/notation/lisp_examples.py")
                return 1
            print(f"the Lisp examples are up to date ({len(written_files(scratch))} files)")
            return 0
        for path in generated_files():
            os.remove(os.path.join(EXAMPLES, path))
        for path in written_files(scratch):
            target = os.path.join(EXAMPLES, path)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copyfile(os.path.join(scratch, path), target)
        print(f"wrote the Lisp examples ({len(written_files(scratch))} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
