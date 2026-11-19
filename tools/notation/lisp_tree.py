#!/usr/bin/env python3
"""Compares the tree each example written as a program builds before and after a change to its Lisp code: what the host
builds from `examples/<app>/lisp`'s entry, its includes and features merged (`affrescoui run --print` on the folder).

    python3 tools/notation/lisp_tree.py --save DIR [app...] [--cli build/affrescoui]
    python3 tools/notation/lisp_tree.py --compare DIR [app...] [--cli build/affrescoui] [--json]
                                        [--examples DIR]

`--save` writes each app's built document as canonical JSON (keys sorted, one file per app, `<app>.json`); `shared` is
the host's two windows (`examples/shared/host/*.screen.lisp`) and the assistant's `e2e.screen.lisp`, in one file.
`--compare` builds again and prints one line per app: `identical`, or how many values differ, then each difference's
path and both texts, sorted into "a binding's text" (a string holding `{{` on either side) and "anything else", and the
count of `lisp(` calls (bindings that call a Lisp function) before and after. The document's `program` (each file's
definitions as source text) is the code being rewritten: a definition written otherwise is counted as "a definition's
text", not listed, while a file or a definition added, removed, renamed or moved is "anything else". A file's `ns`
form is a label, not a definition: adding, changing or removing one is a definition's text. It exits with 0
when every difference is a binding's or a definition's text and no app calls `lisp(` more often, else with 1 (or 2
when a build fails). No app is given: every one.
`--examples` reads the apps from another copy of `examples/` (a scratch copy, to try a change on).

It never opens a window or starts a host: `run --print` only builds, and AFFRESCOUI_HIDDEN=1 is set besides. Only the
standard library.
"""

import argparse
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXAMPLES = os.path.join(ROOT, "examples")
# The shared documents an app opens beside its own: the host's two windows and the assistant's end-to-end screen
SHARED = {"changes": "shared/host/changes.screen.lisp", "permissions": "shared/host/permissions.screen.lisp",
          "e2e": "shared/addons/assistant/e2e.screen.lisp"}
# How much of a value a difference prints, so that a whole subtree missing stays one readable line
SHOWN = 160
BINDING = "a binding's text"
DEFINITION = "a definition's text"
OTHER = "anything else"
NS = "ns"  # the program entry an `ns` form is kept as


def apps(examples):
    """Every example written as a program (a lisp/ folder) in an examples' folder, and `shared`."""
    found = sorted(name for name in os.listdir(examples) if os.path.isdir(os.path.join(examples, name, "lisp")))
    return found + ["shared"]


def printed(cli, examples, path):
    """The tree `affrescoui run --print` prints for a file or an app's folder; raises RuntimeError naming the place."""
    environment = dict(os.environ, AFFRESCOUI_HIDDEN="1")
    run = subprocess.run([cli, "run", "--print", path], capture_output=True, text=True, env=environment, check=False)
    if run.returncode != 0:
        raise RuntimeError(f"{os.path.relpath(path, examples)}: {(run.stderr.strip() or run.stdout.strip())[:400]}")
    return json.loads(run.stdout)


def built(cli, examples, app):
    """The document an app of an examples' folder builds; for `shared`, its documents by name."""
    if app == "shared":
        return {name: printed(cli, examples, os.path.join(examples, path)) for name, path in SHARED.items()}
    return printed(cli, examples, os.path.join(examples, app, "lisp"))


def canonical(value):
    """A value as canonical JSON text: keys sorted, one space of indentation, a newline at the end."""
    return json.dumps(value, sort_keys=True, indent=1, ensure_ascii=False) + "\n"


def lisp_calls(value):
    """How many `lisp(` calls the strings of a value hold."""
    if isinstance(value, str):
        return value.count("lisp(")
    if isinstance(value, dict):
        return sum(lisp_calls(item) for item in value.values())
    if isinstance(value, list):
        return sum(lisp_calls(item) for item in value)
    return 0


def shown(value):
    """A value as one short line of JSON; `(missing)` for none."""
    if value is MISSING:
        return "(missing)"
    text = json.dumps(value, sort_keys=True, ensure_ascii=False)
    return text if len(text) <= SHOWN else text[:SHOWN] + "..."


class Missing:
    """Stands for a key or an item one side doesn't have."""


MISSING = Missing()


def kind(before, after):
    """Which sort a difference is: a binding's text when both sides are strings and either holds `{{`."""
    if isinstance(before, str) and isinstance(after, str) and ("{{" in before or "{{" in after):
        return BINDING
    return OTHER


def is_program(value):
    """Whether a value is a program's source as the document holds it: each file's definitions, by name, as text."""
    return isinstance(value, dict) and all(
        isinstance(file, dict) and all(isinstance(text, str) for text in file.values()) for file in value.values())


def program_differences(before, after, path):
    """Where two programs differ: a file or a definition added, removed or moved is anything else (names and order
    stay), a definition written otherwise is a definition's text."""
    found = []
    for file in sorted(set(before) | set(after)):
        place = f"{path}/{escaped(file)}"
        old, new = before.get(file), after.get(file)
        if old is None or new is None or names(old) != names(new):
            found.append((place, MISSING if old is None else list(old), MISSING if new is None else list(new), OTHER))
            continue
        found += [(f"{place}/{escaped(name)}", old.get(name, MISSING), new.get(name, MISSING), DEFINITION)
                  for name in dict.fromkeys([*old, *new]) if old.get(name) != new.get(name)]
    return found


def names(definitions):
    """A file's definitions by name, in order, without its `ns` form: a label (decision D2) names the file and defines
    nothing, so adding, changing or removing one is a definition's text, not a definition added or moved."""
    return [name for name in definitions if name != NS]


def differences(before, after, path=""):
    """The places where two values differ, as (path, before, after, kind), in the order of the tree. A `program` (the
    source a document carries, its definitions' texts) is compared by program_differences()."""
    if path.endswith("/program") and is_program(before) and is_program(after):
        return program_differences(before, after, path)
    if isinstance(before, dict) and isinstance(after, dict):
        return member_differences(dict_members(before, after), path)
    if isinstance(before, list) and isinstance(after, list):
        return member_differences(list_members(before, after), path)
    if type(before) is type(after) and before == after:  # 1 and True are equal in Python, not in the tree
        return []
    return [(path or "/", before, after, kind(before, after))]


def dict_members(before, after):
    """Two objects' members, as (key, before, after), by key; a member one lacks is MISSING there."""
    keys = sorted(set(before) | set(after))
    return [(escaped(key), before.get(key, MISSING), after.get(key, MISSING)) for key in keys]


def list_members(before, after):
    """Two arrays' items, as (index, before, after), in order; an item one lacks is MISSING there."""
    def item(items, at):
        return items[at] if at < len(items) else MISSING
    return [(at, item(before, at), item(after, at)) for at in range(max(len(before), len(after)))]


def member_differences(members, path):
    """The places where the members of two collections differ (differences() of each), in order."""
    found = []
    for key, before, after in members:
        found += differences(before, after, f"{path}/{key}")
    return found


def escaped(key):
    """A key as a JSON Pointer's part (RFC 6901)."""
    return key.replace("~", "~0").replace("/", "~1")


def save(cli, examples, folder, names):
    """Writes each app's built document into a folder; returns the exit code."""
    os.makedirs(folder, exist_ok=True)
    failed = 0
    for app in names:
        try:
            document = built(cli, examples, app)
        except RuntimeError as error:
            print(f"{app}: build failed: {error}")
            failed += 1
            continue
        with open(os.path.join(folder, app + ".json"), "w", encoding="utf-8") as file:
            file.write(canonical(document))
        print(f"{app}: saved ({lisp_calls(document)} lisp( calls)")
    return 2 if failed else 0


def compared(cli, examples, folder, app):
    """An app's comparison with what was saved, as a dictionary."""
    with open(os.path.join(folder, app + ".json"), encoding="utf-8") as file:
        before = json.load(file)
    after = json.loads(canonical(built(cli, examples, app)))  # through the same text, so both sides are read the same way
    found = differences(before, after)
    return {"app": app, "identical": not found, "differences": len(found),
            "bindingTexts": sum(1 for item in found if item[3] == BINDING),
            "definitionTexts": sum(1 for item in found if item[3] == DEFINITION),
            "other": sum(1 for item in found if item[3] == OTHER),
            "lispCalls": {"before": lisp_calls(before), "after": lisp_calls(after)},
            "places": [{"path": path, "before": shown(old), "after": shown(new), "kind": sort}
                       for path, old, new, sort in found]}


def report(result):
    """Prints one app's comparison."""
    calls = result["lispCalls"]
    counted = f"lisp( calls {calls['before']} -> {calls['after']}"
    if result["identical"]:
        print(f"{result['app']}: identical ({counted})")
        return
    print(f"{result['app']}: {result['differences']} values differ ({result['bindingTexts']} {BINDING}, "
          f"{result['definitionTexts']} {DEFINITION}, {result['other']} {OTHER}; {counted})")
    for sort in (BINDING, OTHER):  # the definitions' texts are the rewrite itself: counted, not listed
        places = [place for place in result["places"] if place["kind"] == sort]
        if places:
            print(f"  {sort}:")
        for place in places:
            print(f"    {place['path']}\n      before: {place['before']}\n      after:  {place['after']}")


def compare(cli, examples, folder, names, as_json):
    """Compares each app with what was saved; returns the exit code."""
    results, failed = [], False
    for app in names:
        try:
            results.append(compared(cli, examples, folder, app))
        except (RuntimeError, OSError) as error:
            results.append({"app": app, "error": str(error)})
            failed = True
    if as_json:
        print(json.dumps(results, indent=1))
    else:
        for result in results:
            if "error" in result:
                print(f"{result['app']}: {result['error']}")
            else:
                report(result)
    if failed:
        return 2
    worse = any(result["other"] or result["lispCalls"]["after"] > result["lispCalls"]["before"] for result in results)
    return 1 if worse else 0


def main():
    """Saves or compares the built trees; returns the exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--save", metavar="DIR", help="write each app's built document into DIR")
    action.add_argument("--compare", metavar="DIR", help="build again and compare with DIR")
    parser.add_argument("apps", nargs="*", help="the apps (examples/<app>/lisp, or shared); none: every one")
    parser.add_argument("--cli", default=os.path.join(ROOT, "build", "affrescoui"), help="the built affrescoui")
    parser.add_argument("--json", action="store_true", help="print the comparison as JSON")
    parser.add_argument("--examples", help="the examples' folder (default: the repository's)")
    arguments = parser.parse_args()
    os.environ["AFFRESCOUI_HIDDEN"] = "1"
    examples = os.path.abspath(arguments.examples or EXAMPLES)
    known = apps(examples)
    names = arguments.apps or known
    unknown = [name for name in names if name not in known]
    if unknown:
        parser.error(f"not an example written as a program: {', '.join(unknown)}")
    if arguments.save:
        return save(arguments.cli, examples, arguments.save, names)
    return compare(arguments.cli, examples, arguments.compare, names, arguments.json)


if __name__ == "__main__":
    sys.exit(main())
