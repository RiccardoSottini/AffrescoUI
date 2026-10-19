#!/usr/bin/env python3
"""Writes the seed corpora and dictionaries of the fuzz targets (README.md), from the sources they stand for.

- json:       the templates in examples/assets/screens, the JSON examples in the docs, nesting at the limit;
- expression: every {{...}} and every "$" operator in the templates and in SCREEN_FORMAT.md;
- revision:   the revisions in the docs, and edits of every kind to the Kanban template's nodes and state;
- session:    every message a peer may send (the table in SCREEN_FORMAT.md, "Peers and the live protocol"), alone
              and in short conversations between the agent and the reviewer (lines starting with '>').

Generated rather than checked in, so the seeds follow the templates and the docs as they change.
Usage: tests/fuzz/corpus.py [--out build/fuzz-corpus]  ->  <out>/<target>/ and <out>/<target>.dict
"""

import argparse
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCREENS = ROOT / "examples" / "assets" / "screens"
DOCS = [ROOT / "SCREEN_FORMAT.md", ROOT / "VERSION_CONTROL.md"]
KANBAN = SCREENS / "kanban.screen.json"
MAX_NESTING = 256  # maxNesting in modules/core/include/affresco/core/Json.hpp

# Values for the fields the protocol table names, and the fields of the messages it lists without them
FIELD_VALUES = {"revision": "HEAD~1", "proposal": 1, "head": "", "dontAskAgain": True, "from": 0, "to": "HEAD",
                "name": "plugin", "limit": 5, "text": "Make the status bar dark", "request": "r1", "mode": "plan",
                "rule": "agent(/actions)", "reason": "Not now",
                "patch": [{"op": "replace", "path": "#statusText/text", "value": "Saved"}]}
MORE_FIELDS = {"revert": ["revision"], "reject": ["proposal", "reason"], "preview": ["proposal"], "setMode": ["mode"],
               "revoke": ["rule"]}


def compact(value):
    """The value as one line of JSON, as peers send it."""
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def doc_blocks():
    """The JSON examples in the docs that parse."""
    blocks = []
    for doc in DOCS:
        for text in re.findall(r"```json\n(.*?)```", doc.read_text(encoding="utf-8"), re.S):
            try:
                blocks.append(json.loads(text))
            except json.JSONDecodeError:
                pass  # an example with "..." in it
    return blocks


def walk(value):
    """Every value inside a JSON value, itself included."""
    yield value
    children = value.values() if isinstance(value, dict) else value if isinstance(value, list) else []
    for child in children:
        yield from walk(child)


def json_seeds():
    """The json target's seeds."""
    seeds = [path.read_text(encoding="utf-8") for path in sorted(SCREENS.glob("*.json"))]
    seeds += [json.dumps(block, indent=1) for block in doc_blocks()]
    seeds.append("[" * MAX_NESTING + "]" * MAX_NESTING)  # the deepest accepted
    seeds.append("[" * (MAX_NESTING + 1) + "]" * (MAX_NESTING + 1))  # one too deep
    seeds.append('{"a": /* comment */ [1, 2.5e3, -0, "\\u00e9\\ud83d\\ude00", true, null]} // end')
    return seeds


def expression_seeds():
    """The expression target's seeds: insides of {{ }} as they are, operators as JSON."""
    texts = [path.read_text(encoding="utf-8") for path in sorted(SCREENS.glob("*.json"))]
    texts.append(DOCS[0].read_text(encoding="utf-8"))
    seeds = {inner.strip() for text in texts for inner in re.findall(r"\{\{(.*?)\}\}", text, re.S)}
    seeds = {seed.replace('\\"', '"') for seed in seeds if seed}
    for path in sorted(SCREENS.glob("*.json")):
        for value in walk(json.loads(path.read_text(encoding="utf-8"))):
            if isinstance(value, dict) and len(value) == 1 and next(iter(value)).startswith("$"):
                seeds.add(compact(value))
    return sorted(seeds)


def kanban_ids():
    """The ids of the Kanban template's nodes, in document order."""
    ids = []
    for value in walk(json.loads(KANBAN.read_text(encoding="utf-8")).get("root", {})):
        if isinstance(value, dict) and isinstance(value.get("id"), str) and value["id"] not in ids:
            ids.append(value["id"])
    return ids


def revisions():
    """Revisions for the Kanban template: from the docs, and edits of every kind to its nodes and state."""
    found = [block for block in doc_blocks() if isinstance(block, dict) and
             any(key in block for key in ("set", "add", "remove", "move", "patch"))]
    found = [{key: value for key, value in block.items() if key != "type"} for block in found]
    ids = kanban_ids()
    state = json.loads(KANBAN.read_text(encoding="utf-8"))["state"]
    for index, node in enumerate(ids):
        author = ("studio", "agent")[index % 2]
        found.append({"author": author, "message": "Recolour", "set": {f"#{node}/color": "#1e293b"}})
        found.append({"author": author, "add": {f"#{node}/children/-": {"id": f"new{index}", "type": "label",
                                                                        "text": "{{state.board}}"}}})
        found.append({"author": author, "remove": [f"#{node}"]})
        found.append({"author": author, "move": {f"#{node}": f"#{ids[0]}/children/0"}})
    for key, value in state.items():
        found.append({"author": "agent", "set": {f"/state/{key}": value}, "parent": 0})
    found.append({"author": "agent", "message": "An action", "set": {"/actions/fuzz": [{"set": "state.filter",
                                                                                         "value": "bug"}]}})
    found.append({"author": "studio", "patch": [
        {"op": "add", "path": "/state/extra", "value": [1, 2]}, {"op": "copy", "from": "/state/extra", "path": "/state/copy"},
        {"op": "move", "from": "/state/copy", "path": "/state/moved"}, {"op": "test", "path": "/state/moved/0", "value": 1},
        {"op": "replace", "path": "#statusText/text", "value": "Saved"}, {"op": "remove", "path": "/state/extra"}]})
    found.append({"author": "agent", "proposal": 1, "set": {"#boardName/text": "Again"}})
    return [compact(revision) for revision in found]


def protocol_messages():
    """Every message type a peer may send, from the protocol table, with its fields: (reviewer only, message)."""
    text = DOCS[0].read_text(encoding="utf-8")
    messages = []
    for sender, cell in re.findall(r"^\| (peer|reviewer) → \| (.*) \|$", text, re.M):
        for name, fields in re.findall(r"`(\w+)`(?:\s*\(([^)]*)\))?", cell):
            names = re.findall(r"`(\w+)`", fields) + MORE_FIELDS.get(name, [])
            message = {"type": name, "id": len(messages) + 1}
            message.update({field: FIELD_VALUES.get(field, "x") for field in names})
            if name == "revision":
                message["set"] = {"#boardName/text": "Q4"}
                del message["proposal"]  # its own proposal: there is none yet
            if name == "request":
                message["id"] = "r1"
            messages.append((sender == "reviewer", message))
    return messages


def session_seeds():
    """The session target's seeds: each message alone, then short conversations."""
    line = lambda reviewer, message: (">" if reviewer else "") + compact(message)
    seeds = [line(reviewer, message) for reviewer, message in protocol_messages()]
    asks = {"type": "revision", "id": 1, "message": "An action", "set": {"/actions/fuzz": [{"log": "hi"}]}}
    seeds.append("\n".join([compact(asks), ">" + compact({"type": "preview", "proposal": 1}), ">{\"type\":\"endPreview\"}",
                            ">" + compact({"type": "accept", "proposal": 1, "dontAskAgain": True}), compact(asks),
                            ">" + compact({"type": "revoke", "rule": "agent(/actions)"}),
                            compact({"type": "revert", "revision": "HEAD"}), compact({"type": "getHistory", "limit": 3}),
                            compact({"type": "diff", "from": 0, "to": "HEAD"}),
                            compact({"type": "exportChanges", "from": 0, "name": "plugin"})]))
    seeds.append("\n".join([compact(asks), compact({"type": "revision", "proposal": 1, "set": {"#boardName/text": "B"}}),
                            ">" + compact({"type": "setMode", "mode": "acceptEdits"}), compact(asks),
                            ">" + compact({"type": "reject", "proposal": 1, "reason": "No"}), compact({"type": "undo"}),
                            compact({"type": "request", "id": "r1", "text": "hi"}), ">{\"type\":\"reply\",\"request\":\"r1\"}"]))
    return seeds


def dictionaries():
    """libFuzzer dictionaries: the words each target's input is made of."""
    # The function registry's rows (ExpressionFunctions*.cpp), their aliases, and the operators the lowering knows
    registry = "".join(path.read_text() for path in sorted((ROOT / "modules/format/src").glob("ExpressionFunctions*.cpp")))
    operators = set(re.findall(r'\.(?:name|alias) = "(\w+)"', registry))
    operators |= set(re.findall(r'"\$(\w+)"', (ROOT / "modules/format/src/ExpressionLower.cpp").read_text()))
    operators = sorted(operators)
    expression = [f"{name}(" for name in operators] + [f"${name}" for name in operators]
    expression += ["{{", "}}", "state.", "event.", "window.", "computed.", "nodes.", "&&", "||", "??", "==", "!=",
                   "<=", ">=", "=>", "?", ":", "[", "]", "'", ".length"]
    edits = ["set", "add", "remove", "move", "patch", "op", "path", "from", "value", "author", "message", "parent",
             "proposal", "replace", "copy", "test", "/state/", "/children/-", "#", "/actions/", "studio", "agent"]
    edits += [f"#{node}" for node in kanban_ids()]
    session = sorted({message["type"] for _, message in protocol_messages()}) + list(FIELD_VALUES) + ["type", "id"]
    tokens = ["{", "}", "[", "]", ",", ":", "true", "false", "null", "\\u0000", "\\ud83d", "1e400", "-0", "/*", "*/",
              "//"]
    return {"json": tokens, "expression": expression, "revision": tokens + edits, "session": tokens + edits + session}


def dictionary_entry(word):
    """A word as a line of a libFuzzer dictionary, which knows only the escapes \\\\, \\" and \\xHH (not JSON's)."""
    escaped = "".join(f"\\{chr(byte)}" if chr(byte) in '\\"' else chr(byte) if 0x20 <= byte < 0x7F else f"\\x{byte:02X}"
                      for byte in word.encode("utf-8"))
    return f'"{escaped}"'


def write(out, target, seeds, words):
    """Writes one target's seeds as files, and its dictionary."""
    directory = out / target
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    for index, seed in enumerate(seeds):
        (directory / f"seed-{index:04}").write_text(seed, encoding="utf-8")
    entries = [dictionary_entry(word) for word in dict.fromkeys(words)]
    (out / f"{target}.dict").write_text("\n".join(entries) + "\n", encoding="utf-8")
    print(f"{target}: {len(seeds)} seeds, {len(entries)} dictionary words")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "fuzz-corpus", help="where to write them")
    out = parser.parse_args().out
    words = dictionaries()
    write(out, "json", json_seeds(), words["json"])
    write(out, "expression", expression_seeds(), words["expression"])
    write(out, "revision", revisions(), words["revision"])
    write(out, "session", session_seeds(), words["session"])


if __name__ == "__main__":
    main()
