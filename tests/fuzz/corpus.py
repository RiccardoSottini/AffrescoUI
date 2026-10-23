#!/usr/bin/env python3
"""Writes the seed corpora and dictionaries of the fuzz targets (README.md), from the sources they stand for.

- json:       the templates (examples/templates.json), the JSON examples in the docs, nesting at the limit;
- expression: every {{...}} and every "$" operator in the templates and in SCREEN_FORMAT.md;
- revision:   the revisions in the docs, and edits of every kind to the Kanban template's nodes and state;
- delta:      the Kanban template and changed copies of it (keys reordered, a node added), with deltas to apply;
- session:    every message a peer may send (the table in SCREEN_FORMAT.md, "Peers and the live protocol"), alone
              and in short conversations between the agent and the reviewer (lines starting with '>');
- text:       every text of the templates and of the rendering check's screens;
- cli:        the templates and the rendering check's screens as written, and nodes without ids;
- actions:    every action and handler of the templates, and steps that spend a whole work budget.
- lookups:    the GSUB and GPOS tables of the examples' fonts and the rendering check's (binary seeds);
- image:      the rendering check's PNG, JPEG and SVG files (binary seeds);
- path:       every path data of the templates, the rendering check's screens and the charts, with drawing bytes;
- scenarios:  every scenarios file, the templates' and the rendering check's;
- png:        small PNG files of every kind the golden-image reader takes (RGB, RGBA, each row filter, stored data);
- frames:     Content-Length frames and lines, as a service process writes them;
- peer_handshake: a remote peer's upgrade requests, well formed and not, and first "auth" messages.
- feature_manifest, catalog_index, signature: Forge's feature manifests and its catalogue (examples/forge).
- composition: Sinopia's slide sorter with Bottega's adapters, good and broken, and Tempera's points and exports.
- url:        an allowlist entry, a line feed, then a URL: remote peers', sign-in's and a document server's;
- oauth:      token and device-code responses, an ID token, base64url (the first byte picks which);
- token_file: a document server's token file, with comments and blank lines;
- git_output: `git status --porcelain=v2 -z --branch`, `git for-each-ref` and two texts to diff (the first byte picks);
- vscode_theme: VS Code colour themes, dark and light, with comments and trailing commas;
- history:    refs.json (even first byte) or a commit object (odd first byte) of a history on disk.
- gltf:       a small .gltf (a triangle, its material, a clip) with its buffer as data:, a .glb of it, broken copies;
- scene:      a scene node's scene, animations and time, with primitives, lights, clips and that model placed in it.
- images:     the "images" sections of the docs and the rendering check's screens, with the "image" steps they run.

Generated rather than checked in, so the seeds follow the templates and the docs as they change; the inputs that found
a bug are checked in, in regressions/<target>/, and added as they are.
Usage: tests/fuzz/corpus.py [--out build/fuzz-corpus]  ->  <out>/<target>/ and <out>/<target>.dict
"""

import argparse
import base64
import hashlib
import json
import re
import shutil
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = ROOT / "examples"
DOCS = [ROOT / "SCREEN_FORMAT.md", ROOT / "VERSION_CONTROL.md"]
KANBAN = EXAMPLES / "flow" / "kanban.screen.json"
MAX_NESTING = 256  # maxNesting in modules/core/include/affresco/core/Json.hpp

# Values for the fields the protocol table names, and the fields of the messages it lists without them
FIELD_VALUES = {"revision": "HEAD~1", "proposal": 1, "head": "", "dontAskAgain": True, "from": 0, "to": "HEAD",
                "name": "plugin", "limit": 5, "text": "Make the status bar dark", "request": "r1", "mode": "plan",
                "rule": "agent(/actions)", "reason": "Not now",
                "patch": [{"op": "replace", "path": "#statusText/text", "value": "Saved"}]}
MORE_FIELDS = {"revert": ["revision"], "reject": ["proposal", "reason"], "preview": ["proposal"], "setMode": ["mode"],
               "revoke": ["rule"]}


def templates(pattern):
    """The templates' files that match the pattern, from each one's folder (examples/templates.json), by name."""
    folders = set(json.loads((EXAMPLES / "templates.json").read_text(encoding="utf-8"))["templates"].values())
    files = [path for folder in folders for path in (EXAMPLES / folder).glob(pattern)]
    return sorted(files, key=lambda path: path.name)


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
    seeds = [path.read_text(encoding="utf-8") for path in templates("*.json")]
    seeds += [json.dumps(block, indent=1) for block in doc_blocks()]
    seeds.append("[" * MAX_NESTING + "]" * MAX_NESTING)  # the deepest accepted
    seeds.append("[" * (MAX_NESTING + 1) + "]" * (MAX_NESTING + 1))  # one too deep
    seeds.append('{"a": /* comment */ [1, 2.5e3, -0, "\\u00e9\\ud83d\\ude00", true, null]} // end')
    return seeds


def expression_seeds():
    """The expression target's seeds: insides of {{ }} as they are, operators as JSON."""
    texts = [path.read_text(encoding="utf-8") for path in templates("*.json")]
    texts.append(DOCS[0].read_text(encoding="utf-8"))
    seeds = {inner.strip() for text in texts for inner in re.findall(r"\{\{(.*?)\}\}", text, re.S)}
    seeds = {seed.replace('\\"', '"') for seed in seeds if seed}
    for path in templates("*.json"):
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
    # Over the agent's limits (session.cpp): more changes than its burst, then a message larger than 4 KiB
    renames = [compact({"type": "revision", "id": i, "set": {"#boardName/text": f"B{i}"}}) for i in range(8)]
    seeds.append("\n".join(renames + [compact({"type": "getHistory", "id": 9, "padding": "x" * 5000})]))
    return seeds


def delta_seeds():
    """The delta target's seeds: the Kanban template and changed copies of it, and deltas that may or may not apply."""
    kanban = json.loads(KANBAN.read_text(encoding="utf-8"))
    renamed = json.loads(json.dumps(kanban))
    renamed["state"]["board"] = "Renamed"
    reordered = dict(reversed(list(kanban.items())))  # the same fields in another order
    added = json.loads(json.dumps(kanban))
    added["root"]["children"].insert(0, {"type": "label", "id": "first", "text": "First"})
    deltas = [[{"op": "replace", "path": "/state/board", "value": "x"}], [{"op": "remove", "path": "/root/children/0"}],
              [{"op": "add", "path": "/root/children/0/x", "value": 1}], [{"op": "move", "from": "/a", "path": "/a/b"}],
              [{"op": "add", "path": "/window/width/x", "value": 1}], [{"op": "copy", "from": "", "path": "/c"}], {}]
    seeds = [compact({"from": kanban, "to": to, "delta": delta})
             for to, delta in zip([renamed, reordered, added, {}, [kanban], 1, kanban], deltas)]
    seeds += [compact({"from": [1, 2, 3], "to": [3, 1.0, -0.0], "delta": [{"op": "add", "path": "/0/x", "value": 1}]}),
              compact({"from": {"a/b": {"~": 1}, "c": [1]}, "to": {"c": [], "a/b": None}, "delta": []})]
    return seeds


def policy_seeds():
    """Budgets and data rules with a revision by an agent: the policy target's input (tests/fuzz/policy.cpp)."""
    rules = {"allow": ["agent"], "budgets": [{"author": "agent", "added": 1, "removed": 1, "changed": 2}],
             "data": [{"author": "agent", "read": ["/state/board", "/state/cards"], "write": ["/state/cards"],
                       "never": ["/state/filter"]}]}
    asked = {"allow": ["agent"], "budgets": [{"author": "role:design", "added": 0, "over": "ask"}],
             "data": [{"author": "*", "never": ["/state"]}]}
    return [compact({"permissions": permissions, "revision": revision})
            for permissions in (rules, asked) for revision in revisions()[:40]]


def text_seeds():
    """Every text of the templates and of the rendering check's screens: scripts, emoji, marks, ligatures."""
    seeds = []
    for path in templates("*.screen.json") + sorted((ROOT / "tests" / "render" / "screens").glob("*.json")):
        seeds += [value for value in walk(json.loads(path.read_text(encoding="utf-8"))) if isinstance(value, str)]
    return list(dict.fromkeys(seed for seed in seeds if seed.strip() and "{{" not in seed))


def richtext_seeds():
    """Runs as the richtext target reads them: a width byte, then runs cut at U+001F, each led by its style byte."""
    seeds = ["\x40" + "\x1f".join(chr(style) + text for style, text in enumerate(words))
             for words in (["Plain ", "bold", " and ", "big", " italic ", "both", " ", "linked words"],
                           ["one", "\n", "two three four five six", "", "\u0627\u0644 \U0001F600"])]
    seeds += ["\x00" + "\x1f" * 300, "\x10" + "\x01" + "word " * 200, "\xff" + "\x02\n\n\n"]
    return seeds


def layout_seeds():
    """Every node of the layout and damage screens of the rendering check, with their flex and grid keys, and a few
    that push the limits: as many tracks as a grid may have, cells past them, sizes near the float's limit."""
    seeds = []
    for name in ("layout.screen.json", "damage.screen.json"):
        document = json.loads((ROOT / "tests" / "render" / "screens" / name).read_text(encoding="utf-8"))
        seeds += [compact(node) for node in document["root"]["children"]]
    seeds.append(compact({"type": "grid", "columns": ["1fr"] * 1000, "rows": ["auto"] * 3, "fill": 0,
                          "children": [{"cell": {"column": 1000, "row": 1000, "columnSpan": 5}, "height": 10}]}))
    seeds.append(compact({"type": "row", "wrap": True, "justify": "space-evenly", "alignItems": "stretch",
                          "width": "1e38", "children": [{"width": 3e38, "shrink": 1, "aspectRatio": "16/9"}] * 4}))
    return seeds


def cli_seeds():
    """Screen files as written (tabs, blank lines, members on one line), and nodes without ids for `ids` to name."""
    files = templates("*.json") + sorted((ROOT / "tests" / "render" / "screens").glob("*.json"))
    seeds = [path.read_text(encoding="utf-8") for path in files]
    seeds.append('{\n\t"format": "affrescoui-screen",\n\n\t"version": 2,\n\t"root": { "children": [{ "type": "label", '
                 '"text": "{{state.title}}" }, {\n\t\t"type": "row", "children": [] }] }\n}\n')
    return seeds


def schema_words():
    """Every property name the screen schema knows, as a key: what screen files are made of."""
    schema = json.loads((ROOT / "schema" / "affrescoui-screen.schema.json").read_text(encoding="utf-8"))
    names = {key for value in walk(schema) if isinstance(value, dict) and isinstance(value.get("properties"), dict)
             for key in value["properties"]}
    return [f'"{name}":' for name in sorted(names)]


def action_seeds():
    """The actions target's seeds: every action and inline handler of the templates, and steps that spend a whole
    work budget (loops, nested lists, a value that doubles)."""
    seeds = []
    for path in templates("*.screen.json"):
        document = json.loads(path.read_text(encoding="utf-8"))
        seeds += [compact(steps) for steps in document.get("actions", {}).values()]
        for value in walk(document.get("root", {})):
            if isinstance(value, dict):
                seeds += [compact(handler) for key, handler in value.items()
                          if key.startswith("on") and isinstance(handler, (list, dict))]
    seeds += [compact(steps) for steps in [
        [{"while": "{{true}}", "do": [{"increment": "state.spins"}]}],
        [{"while": "{{true}}", "do": [{"forEach": "{{range(100)}}", "do": [{"push": "state.items", "value": "{{it}}"}]}]}],
        [{"set": "state.text", "to": "ab"}, {"while": "{{true}}", "do": [{"set": "state.text",
                                                                           "to": "{{state.text + state.text}}"}]}],
        [{"set": "state.total", "to": "{{sum(range(10000), sum(range(10000), it))}}"}],
    ]]
    return list(dict.fromkeys(seeds))


def font_tables(path):
    """A font file's tables, by tag (the table directory of a TrueType or OpenType file)."""
    data = path.read_bytes()
    count = int.from_bytes(data[4:6], "big")
    tables = {}
    for index in range(count):
        record = data[12 + 16 * index:28 + 16 * index]
        offset, length = int.from_bytes(record[8:12], "big"), int.from_bytes(record[12:16], "big")
        tables[record[:4].decode("latin-1")] = data[offset:offset + length]
    return tables


def lookups_seeds():
    """The GSUB (first byte 0) and GPOS (first byte 1) tables of the fonts the examples and the check use."""
    fonts = sorted((ROOT / "examples/shared/fonts").glob("*.ttf")) + sorted((ROOT / "tests/render/fonts").glob("*.ttf"))
    seeds = []
    for path in fonts:
        tables = font_tables(path)
        for tag, kind in (("GSUB", b"\x00"), ("GPOS", b"\x01")):
            if tag in tables:
                seeds.append(kind + tables[tag])
    return seeds


def image_seeds():
    """The images of the rendering check's screens."""
    folder = ROOT / "tests/render/screens/images"
    return [path.read_bytes() for path in sorted(folder.glob("*.png")) + sorted(folder.glob("*.jpg")) +
            sorted(folder.glob("*.svg"))]


def path_seeds():
    """Path data as the path target reads it: a drawing byte (rule, join, cap, dash, scale), then the data. Every "d"
    of the templates, the rendering check's screens and the charts, and a few that push the limits."""
    files = templates("*.json") + sorted((ROOT / "tests" / "render" / "screens").glob("*.json"))
    files.append(EXAMPLES / "shared" / "components" / "charts.json")
    data = [value["d"] for path in files for value in walk(json.loads(path.read_text(encoding="utf-8")))
            if isinstance(value, dict) and isinstance(value.get("d"), str) and "{{" not in value["d"]]
    data += ["M0 0 L10 0 L5 8 Z", "M0 0h10v10h-10z m2 2 h6 v6 h-6 z", "M10 80 C 30 10, 70 10, 90 80 S 150 150 180 80",
             "M10 80 Q 52.5 10, 95 80 T 180 80", "M20 50 A 30 30 0 1 1 80 50 a30 20 45 0 0-60 0", "M0 0" + " l1 1" * 4000,
             "M-1e6 -1e6 L1e6 1e6 L-1e6 1e6 Z", "M0 0 A 1e-30 1e-30 0 0 0 1 1", "M.5.5.5.5l-.5-.5e1"]
    return [chr(style) + text for text in dict.fromkeys(data) for style in (0x00, 0x2b, 0x5e, 0xd1)]


def text_model_seeds():
    """Edit commands as the text_model target reads them: 0 insert (line, column, length, bytes), 1 erase (two
    positions), 2 undo, 3 redo, 4 insert outside a step; bit 3 of an insert merges it with the step before."""
    def insert(line, column, text, command=0):
        data = text.encode("utf-8") if isinstance(text, str) else text
        return bytes([command, line, column, len(data)]) + data
    typing = b"".join(insert(0, column, letter, 8) for column, letter in enumerate("hello"))
    lines = insert(0, 0, "one\ntwo\nthree\n") + insert(1, 2, "caf\u00e9 \u20ac")
    return [typing, typing + b"\x02\x02\x03", lines + bytes([1, 0, 1, 2, 2]) + b"\x02\x03",
            lines + insert(0, 3, b"\xff\xc3(\xed\xa0\x80") + insert(9, 99, "\n\n", 4) + b"\x02",
            b"".join(insert(index % 3, index, "ab\n") for index in range(40)) + bytes([1, 3, 0, 30, 5]) + b"\x02" * 8]


def find_pattern_seeds():
    """Options byte (1 regex, 2 whole word, 4 case-sensitive), pattern, zero, replacement, zero, subject."""
    def seed(options, pattern, replacement, subject):
        return bytes([options]) + "\0".join([pattern, replacement, subject]).encode("utf-8")
    return [seed(0, "foo", "bar", "foo food Foo"), seed(2, "foo", "x", "foo food foo_bar"),
            seed(1, r"(\w+) (foo)\b", "$2 $1", "a foo, b food"), seed(5, "^", "", "line"),
            seed(1, "(a+)+$", "", "a" * 64 + "b"), seed(1, "(?<year>\\d{4})", "${year}$$", "in 2026 and 1999"),
            seed(3, "caf\u00e9", "cafe", "un caf\u00e9, CAF\u00c9"), bytes([1]) + b"(\\x{d800}|.)\0\0\xff\xc3(\xed\xa0\x80",
            seed(1, "((((((((((x))))))))))*", "$10", "xxxx")]


def search_glob_seeds():
    """A glob list, zero, a .gitignore text, zero, then paths one per line."""
    def seed(globs, ignore, paths):
        return "\0".join([globs, ignore, "\n".join(paths)]).encode("utf-8")
    paths = ["src/main.cpp", "src/", "build/out.o", "node_modules/a/b.js", "README.md", "sub/x.log", "a/b/c/d.txt"]
    return [seed("*.cpp, src/**", "build/\n*.log\n!keep.log", paths), seed("**/b/**, /README.md", "# a comment\n/out", paths),
            seed("[a-c]*.?pp, [!x]*", "\\#hash\n**/d.txt\n!/sub/", paths), seed("{, ,,*", "[", ["\xe9t\xe9/caf\xe9.md"]),
            seed("**/**/**/*", "*\n!*/\n!*.md", paths), seed("a" * 300 + "*", "/" * 50, paths)]


def scenario_seeds():
    """Every scenarios file: the templates' own and the rendering check's, as they are."""
    paths = templates("*.scenarios.json") + sorted((ROOT / "tests" / "render" / "scenarios").glob("*.json"))
    return [path.read_text(encoding="utf-8") for path in paths]


def png(width, height, filters, channels=3, level=9):
    """A small PNG file whose rows use the given filters in turn, as encoders write them."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    rows = b"".join(bytes([filters[y % len(filters)]]) + bytes((x * 31 + y * 17) % 256 for x in range(width * channels))
                    for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2 if channels == 3 else 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, level)) +
            chunk(b"IEND", b""))


def png_seeds():
    """PNG files of every kind the reader takes: RGB and RGBA, every row filter, stored and compressed data."""
    return [png(3, 2, [0]), png(5, 4, [0, 1, 2, 3, 4]), png(4, 3, [4], channels=4), png(16, 9, [2], level=0),
            png(1, 1, [0], level=1)]


def http_response_seeds():
    """Response heads: statuses, headers, a WebSocket handshake, Retry-After values."""
    return ["HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n",
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n",
            "HTTP/2 503 Busy\nRetry-After: 3\n\n", "HTTP/1.0 302 Found\r\nLocation: /next\r\n\r\n", "120", "Name:  value "]


def sse_seeds():
    """Event streams and line streams (the first byte picks where chunks are cut)."""
    return ["\x03data: one\n\n", "\x01: comment\r\nevent: update\r\nid: 7\r\ndata: {\"n\":1}\r\n\r\n",
            "\x05data: a\rdata: b\r\rretry: 10\n\n", "\x02{\"a\":1}\n{\"b\":2}\r\n\n{\"c\":3}"]


def websocket_seeds():
    """Frames to a client (unmasked, even first byte) and to a server (masked, odd first byte)."""
    hello = b"\x81\x05hello"
    masked = b"\x81\x85\x01\x02\x03\x04" + bytes(byte ^ (1, 2, 3, 4)[index % 4] for index, byte in enumerate(b"hello"))
    fragments = b"\x01\x02he\x89\x01p\x80\x03llo"
    close = b"\x88\x05\x03\xe9bye"
    return [b"\x02" + hello, b"\x03" + masked, b"\x08" + fragments, b"\x04" + close, b"\x02\x81\x7f" + bytes(8)]


def peer_handshake_seeds():
    """Upgrade requests (odd first byte) and first messages (even first byte)."""
    head = ("GET /live HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
    return ["\x01" + head + "\r\n", "\x01" + head + "Authorization: Bearer good\r\n\r\n",
            "\x01" + head + "Origin: https://studio.example.com\r\nOrigin: https://other.example.com\r\n\r\n",
            "\x01GET / HTTP/1.0\r\nUpgrade: h2c\r\n\r\n", '\x00{"type": "auth", "token": "good"}',
            '\x00{"type": "auth", "token": ""}', '\x00{"type": "hello"}']


def frames_seeds():
    """Content-Length frames (even first byte) and lines (odd first byte), cut into pieces of a few sizes."""
    body = '{"jsonrpc":"2.0","id":1,"result":{}}'
    return ["\x06Content-Length: %d\r\n\r\n%s" % (len(body), body),
            "\x02content-length: 2\r\nContent-Type: x\r\n\r\n{}Content-Length: 0\r\n\r\n",
            "\x07" + body + "\n{}\r\n", "\x04Content-Length: 999\r\n\r\n"]


def feature_seeds():
    """Forge's feature manifests, and its catalogue's index and manifest copies (examples/forge)."""
    forge = ROOT / "examples" / "forge"
    return [path.read_text(encoding="utf-8") for path in sorted(forge.glob("features/*/feature.json"))
            + sorted(forge.glob("catalog/**/*.json"))]


def composition_seeds():
    """The slide sorter's manifest with each of Bottega's adapters, and Tempera's points and exports (examples/bottega)."""
    examples = ROOT / "examples"
    manifest = json.loads((examples / "sinopia" / "features" / "slide-sorter" / "feature.json").read_text(encoding="utf-8"))
    tempera = json.loads((examples / "tempera" / "tempera.screen.json").read_text(encoding="utf-8"))
    host = {part: tempera[part] for part in ("points", "exports") if part in tempera}
    adapters = [json.loads(path.read_text(encoding="utf-8")) for path in sorted((examples / "bottega").glob("*.adapter.json"))]
    return [compact({"manifest": manifest, "adapter": adapter, **host}) for adapter in [None] + adapters]


def base64url(data):
    """Bytes as base64url without padding, as in a JSON Web Token."""
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def url_seeds():
    """Allowlist entries and URLs, admitted and not."""
    return ["https://studio.example.com\nhttps://studio.example.com/live?x=1", "\nhttp://127.0.0.1:8080/",
            "http://localhost\nws://localhost:9/peer", "https://*.example.com\nwss://a.example.com:443/#frag",
            "https://Example.COM.\nhttps://example.com:0/", "https://a@b\nhttps://user:pw@[::1]:8/%2e%2e/",
            "\n..", "\n.", "\nhttps://xn--bcher-kva.example/\u00e9 ?q=a b"]


def oauth_seeds():
    """Responses of a sign-in provider (OAuth 2.0, OpenID Connect), with placeholder values only."""
    claims = {"iss": "https://sign-in.example.com", "aud": "affrescoui-fuzz", "sub": "s1", "nonce": "n0nce",
              "exp": 1900000000, "email": "person@example.com"}
    id_token = ".".join([base64url(b'{"alg":"none"}'), base64url(compact(claims).encode()), ""])
    tokens = {"access_token": "example-access", "token_type": "Bearer", "expires_in": 3600,
              "refresh_token": "example-refresh", "scope": "openid", "id_token": id_token}
    device = {"device_code": "example-device", "user_code": "ABCD-EFGH", "interval": 5, "expires_in": 600,
              "verification_uri": "https://sign-in.example.com/device"}
    return ["\x00" + compact({k: v for k, v in tokens.items() if k != "id_token"}), "\x01" + compact(tokens),
            "\x00" + compact({"error": "invalid_grant", "error_description": "expired"}),
            "\x00" + compact({**tokens, "expires_in": 1e300}), "\x02" + compact(device),
            "\x02" + compact({**device, "interval": -1, "expires_in": "600"}), "\x03" + id_token,
            "\x03error=access_denied&state=s&code=c"]


def token_file_seeds():
    """Token files: a SHA-256 in hex and a name per line (apps/server/TokenFile.hpp)."""
    line = hashlib.sha256(b"placeholder").hexdigest()
    return [f"# who may connect\n{line} reviewer\n\n{line[::-1]} agent-1\n", f"{line} \n", f"{line.upper()} a b\n",
            "# only a comment", f"{line}\treviewer\r\n"]


def git_output_seeds():
    """git's output as Forge asks for it (examples/forge/features/scm/GitStatus.hpp), and texts to diff."""
    status = ["# branch.oid 0123456789abcdef0123456789abcdef01234567", "# branch.head main",
              "# branch.upstream origin/main", "# branch.ab +2 -1",
              "1 .M N... 100644 100644 100644 0123456789abcdef0123456789abcdef01234567 "
              "0123456789abcdef0123456789abcdef01234567 src/main.cpp",
              "2 R. N... 100644 100644 100644 0123456789abcdef0123456789abcdef01234567 "
              "0123456789abcdef0123456789abcdef01234567 R100 new name.txt", "old name.txt",
              "u UU N... 100644 100644 100644 100644 0123456789abcdef0123456789abcdef01234567 "
              "0123456789abcdef0123456789abcdef01234567 0123456789abcdef0123456789abcdef01234567 both.txt",
              "? notes.md", "! build/"]
    return ["\x00" + "\0".join(status) + "\0", "\x00# branch.oid (initial)\0# branch.head (detached)\0",
            "\x01*\0main\n \0feature/x\n \0fix\n", "\x01*\0a\n*\0b\n",
            "\x02one\ntwo\nthree\n\0one\n2\nthree\nfour", "\x02\0added\n", "\x02a\r\nb\0a\nb\n"]


def vscode_theme_seeds():
    """VS Code colour themes, which allow comments and trailing commas."""
    dark = {"name": "Fuzz Dark", "type": "dark", "colors": {"editor.background": "#1e1e1e",
            "editor.foreground": "#d4d4d4", "focusBorder": "#007fd4", "statusBar.background": "#007acc80",
            "list.hoverBackground": "#2a2d2e", "errorForeground": "#f48771"},
            "tokenColors": [{"scope": ["comment"], "settings": {"foreground": "#6a9955"}}]}
    light = {"name": "Fuzz Light", "type": "light", "colors": {"editor.background": "#fff", "foreground": "red"}}
    return [json.dumps(dark, indent=2), compact(light), '// a comment\n{"name": "Ünïcode ✓", "colors": {},}',
            '{"type": "hc", "colors": {"editor.background": "#12345"}}', '{"name": "' + "x" * 300 + '"}']


def history_seeds():
    """A history's files (VERSION_CONTROL.md, "History on disk"): refs.json, and a commit object."""
    refs = {"version": 2, "main": "0" * 64, "proposals": {}, "nextProposal": 1, "grants": [], "mode": "default",
            "packs": [], "checksum": "0" * 64}
    commit = {"parents": [], "author": "agent", "committer": "studio", "time": 0, "message": "Start",
              "patch": [], "tree": "0" * 64}
    return ["\x00" + compact(refs), "\x00{}", "\x00" + compact({**refs, "version": 1}),
            "\x01" + compact(commit), "\x01" + compact({**commit, "delta": [{"op": "add", "path": "/x", "value": 1}]}),
            "\x01" + compact({**commit, "snapshot": True})]


def adapters_seeds():
    """Documents in another format (SCREEN_FORMAT.md, "Other formats"): the A2UI stream and Adaptive Card the rendering
    check converts, and small ones."""
    screens = ROOT / "tests/render/screens"
    card = {"type": "AdaptiveCard", "version": "1.5", "body": [{"type": "TextBlock", "text": "Hi ${name}"},
            {"type": "Input.Text", "id": "name"}], "actions": [{"type": "Action.Submit", "title": "Send"}]}
    update = {"surfaceUpdate": {"surfaceId": "s", "components": [{"id": "root", "component": {"Text": {
              "text": {"literalString": "Hi"}}}}]}}
    return [(screens / "a2ui.jsonl").read_text(encoding="utf-8"), (screens / "adaptive-card.json").read_text(
            encoding="utf-8"), compact(card), compact(update) + "\n" + compact({"beginRendering": {
            "surfaceId": "s", "root": "root"}}), compact({**card, "body": [{"type": "Container", "items": []}]})]


def dap_seeds():
    """Message bodies a debug adapter writes (modules/services/src/dap/DapProtocol.hpp): responses, events and the reverse
    requests, with sources inside and outside the fuzz target's folder (/work)."""
    frame = {"id": 1, "name": "main", "line": 3, "column": 1, "source": {"name": "a.c", "path": "/work/a.c"}}
    return [compact({"seq": 1, "type": "response", "request_seq": 1, "success": True, "command": "stackTrace",
                     "body": {"stackFrames": [frame, {**frame, "source": {"path": "/elsewhere/b.c"}}]}}),
            compact({"seq": 2, "type": "response", "request_seq": 2, "success": False, "command": "evaluate",
                     "message": "no such variable"}),
            compact({"seq": 3, "type": "event", "event": "output", "body": {"category": "stdout", "output": "hi\n"}}),
            compact({"seq": 4, "type": "event", "event": "stopped", "body": {"reason": "breakpoint", "threadId": 1}}),
            compact({"seq": 5, "type": "request", "command": "runInTerminal", "arguments": {"cwd": "/work",
                     "args": ["./a.out", "--flag"]}}),
            compact({"seq": 6, "type": "response", "request_seq": 3, "success": True, "command": "variables",
                     "body": {"variables": [{"name": "x", "value": "1", "variablesReference": 0}]}})]


def syntax_seeds():
    """Texts for the syntax engine (tests/fuzz/syntax.cpp): the first byte picks the language, then the text, a zero
    byte and edits of five bytes each."""
    texts = ["int main() { return (1 + 2) * 3; }\n", "def f(x):\n    return [x, {'a': 1}]\n",
             '{"a": [1, 2, {"b": null}]}\n', "# Title\n\n- item `code`\n", "const f = (a) => { return `x${a}`; };\n",
             "template <class T> struct S { T t; };\n/* open"]
    edit = "\x00\x00\x05\x01\x02x\x00\x10\x00\x01("
    return [chr(language) + text + edit for language, text in enumerate(texts)] + [chr(index) for index in range(8)]


def gltf_model():
    """A small .gltf: one triangle with its material, in a node a clip moves, its buffer a data: URI."""
    data = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0) + struct.pack("<3H2x", 0, 1, 2)
    data += struct.pack("<2f", 0, 1) + struct.pack("<6f", 0, 0, 0, 0, 2, 0)
    uri = "data:application/octet-stream;base64," + base64.b64encode(data).decode()
    return {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": [{"name": "tri", "mesh": 0, "translation": [0, 0, 1], "children": [1]}, {"name": "child"}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
            "materials": [{"name": "red", "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1],
                                                                   "metallicFactor": 0, "roughnessFactor": 0.5}}],
            "buffers": [{"byteLength": len(data), "uri": uri}],
            "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36},
                            {"buffer": 0, "byteOffset": 36, "byteLength": 6},
                            {"buffer": 0, "byteOffset": 44, "byteLength": 8},
                            {"buffer": 0, "byteOffset": 52, "byteLength": 24}],
            "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                           "min": [0, 0, 0], "max": [1, 1, 0]},
                          {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
                          {"bufferView": 2, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
                          {"bufferView": 3, "componentType": 5126, "count": 2, "type": "VEC3"}],
            "animations": [{"name": "rise", "samplers": [{"input": 2, "output": 3, "interpolation": "LINEAR"}],
                            "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]}]}


def gltf_seeds():
    """glTF files (modules/scene3d/src/gltf/): the small model, broken copies of it, and a .glb holding it."""
    model = gltf_model()
    text = compact(model)
    chunk = text.encode() + b" " * (-len(text) % 4)
    glb = struct.pack("<3I", 0x46546C67, 2, 12 + 8 + len(chunk)) + struct.pack("<2I", len(chunk), 0x4E4F534A) + chunk
    broken = [{**model, "accessors": [{**model["accessors"][0], "count": 1000000}, *model["accessors"][1:]]},
              {**model, "nodes": [{"children": [0]}]}, {**model, "buffers": [{"byteLength": 4, "uri": "missing.bin"}]},
              {**model, "bufferViews": [{"buffer": 0, "byteOffset": 70, "byteLength": 36}]}]
    return [text, glb, *[compact(each) for each in broken], compact({"asset": {"version": "2.0"}})]


def image_operations():
    """The "image" step's operations and the "with" keys they read, from SCREEN_FORMAT.md's generated table."""
    text = (ROOT / "SCREEN_FORMAT.md").read_text(encoding="utf-8")
    table = text.split("<!-- generated: imageOperations -->", 1)[1].split("<!-- end generated -->", 1)[0]
    operations = {}
    for name, keys in re.findall(r"^\| `(\w+)` \| ([^|]*) \|", table, re.M):
        operations[name] = re.findall(r"`(\w+)`", keys)
    return operations


def images_seeds():
    """The images target's input (tests/fuzz/images.cpp): each document's images with the "image" steps it runs."""
    operations = image_operations()
    documents = [json.loads(path.read_text(encoding="utf-8"))
                 for path in sorted((ROOT / "tests" / "render" / "screens").glob("*.json"))]
    for text in re.findall(r"```json\n(.*?)```", (ROOT / "SCREEN_FORMAT.md").read_text(encoding="utf-8"), re.S):
        try:
            documents.append(json.loads("{" + text + "}"))  # the examples of Images are a document's keys
        except json.JSONDecodeError:
            pass
    seeds = []
    for document in documents:
        if not isinstance(document, dict) or not isinstance(document.get("images"), dict):
            continue
        steps = [value for value in walk(document) if isinstance(value, dict)
                 and isinstance(value.get("image"), str) and value["image"] in operations]
        seeds.append(compact({"images": document["images"], "steps": steps}))
    board = {"board": {"width": 64, "height": 48, "into": "state.board",
                       "tools": {"pen": {"gesture": "paint", "size": 4}, "box": {"gesture": "select-rectangle"}}}}
    pointer = [{"image": "setTool", "with": {"tool": "pen"}}] + [
        {"image": "pointer", "with": {"phase": phase, "x": x, "y": 20}}
        for phase, x in [("press", 4), ("move", 30), ("release", 60)]]
    seeds.append(compact({"images": board, "steps": pointer + [{"image": "undo"}, {"image": "redo"}]}))
    seeds.append(compact({"images": board, "steps": [{"image": name, "with": {}} for name in operations]}))
    return seeds


def scene_seeds():
    """Scene nodes' input (tests/fuzz/scene.cpp): scenes of primitives, lights and materials, clips, a placed model."""
    nodes = [{"id": "cube", "mesh": "cube", "material": "paint", "translation": [0, 1, 0], "rotationDegrees": [0, 45, 0]},
             {"id": "ball", "parent": "cube", "mesh": "sphere", "scale": [0.5, 0.5, 0.5], "rotation": [0, 0, 0, 1]},
             {"id": "sun", "light": {"type": "directional", "intensity": 3}},
             {"id": "lamp", "light": {"type": "spot", "color": [1, 0.9, 0.8], "range": 10, "innerCone": 0.2,
                                      "outerCone": 0.6}, "translation": [2, 4, 2]},
             {"id": "floor", "mesh": "plane", "scale": [10, 1, 10], "visible": False}]
    materials = [{"id": "paint", "baseColor": [0.8, 0.2, 0.1, 1], "metallic": 0.1, "roughness": 0.4,
                  "alphaMode": "BLEND", "doubleSided": True}]
    clip = [{"name": "spin", "tracks": [{"node": "cube", "path": "rotation", "interpolation": "linear",
                                         "times": [0, 1], "values": [[0, 0, 0, 1], [0, 0.7071, 0, 0.7071]]},
                                        {"node": "ball", "path": "translation", "interpolation": "step",
                                         "times": [0, 0.5], "values": [[0, 0, 0], [0, 2, 0]]}]}]
    model = [{"id": "robot", "asset": "m", "translation": [3, 0, 0]}, {"id": "arm", "parent": "robot", "mesh": "cone"}]
    return [compact({"scene": {"nodes": nodes, "materials": materials}, "animations": clip, "time": 0.25}),
            compact({"gltf": gltf_model(), "scene": {"nodes": model}, "time": 0.5}),
            compact({"gltf": gltf_model(), "scene": {"nodes": [*model, {"id": "robot/tri", "mesh": "torus"}]}}),
            compact({"scene": {"nodes": [{"id": "a", "parent": "b"}, {"id": "b", "parent": "a"}]}}),
            compact({"scene": {"nodes": [{"id": "a", "mesh": "cylinder", "scale": [1e38, 0, -1]}]}, "time": 1e300}),
            compact({"scene": {"nodes": nodes}, "animations": [{"tracks": [{"node": "cube", "path": "scale",
                     "interpolation": "cubic", "times": [1, 0], "values": [[1, 1, 1]]}]}], "time": -3})]


def ask_reply_seeds():
    """Replies to the screen's own request (tests/fuzz/ask_reply.cpp): one reply or a list, the request's id filled in
    when a reply names none."""
    replies = [{"text": "Done"}, {"text": "On it", "done": False}, {"error": "no agent is connected"},
               {"from": "agent", "text": "Proposed a **blue** title", "done": True}, {"request": "ask-0-0"},
               {"request": 7, "text": 42, "done": "yes", "error": []}, {"text": "\u00e9" * 8 + "\U0001F600"}]
    lists = [[{"text": "On it", "done": False}, {"text": "Done"}], [{"error": "failed"}, {"text": "Again"}], [], [1]]
    return [json.dumps(reply) for reply in replies + lists] + ["null", '"text"', "{"]


def dictionaries():
    """libFuzzer dictionaries: the words each target's input is made of."""
    # The function registry's rows (ExpressionFunctions*.cpp), their aliases, and the operators the lowering knows
    registry = "".join(path.read_text() for path in sorted((ROOT / "modules/format/src/functions").glob("ExpressionFunctions*.cpp")))
    operators = set(re.findall(r'\.(?:name|alias) = "(\w+)"', registry))
    operators |= set(re.findall(r'"\$(\w+)"', (ROOT / "modules/format/src/expression/ExpressionLower.cpp").read_text()))
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
    delta = tokens + ["from", "to", "delta", "op", "add", "remove", "replace", "move", "copy", "test", "path", "value",
                      "/0", "/-", "~0", "~1", "1.0", "-0.0"]
    # The steps and the keys they read, from the step groups' registrations (modules/format/src/**/*Steps.cpp)
    groups = "".join(path.read_text() for path in sorted((ROOT / "modules/format/src").rglob("*Steps.cpp")))
    steps = sorted(set(re.findall(r'\{"(\w+)", \{', groups)) | set(re.findall(r'"(\w+)"', "".join(
        re.findall(r'\{"\w+", \{([^}]*)\}', groups)))) | {"do", "with", "run", "state.", "to", "value"})
    # Characters shaping treats apart: joiners, variation selectors, marks, spaces, line breaks, right-to-left letters
    text = ["\u200d", "\ufe0f", "\u0301", "\u0323", " ", "\n", "\t", "\u0627\u0644", "\u094d", "\U0001F1EE",
            "\U0001F3FD", "fi", "ffl", "->", "!=="]
    # The steps and keys of scenarios files (SCREEN_FORMAT.md, "Scenarios"), and the Kanban template's ids
    scenarios = tokens + ["version", "screen", "scenarios", "name", "steps", "note", "nth", "toNth", "moves", "dx",
                          "dy", "event", "capture", "move", "click", "press", "release", "drag", "to", "scroll", "type",
                          "key", "revision", "action", "resize", "wait", "expect", "state", "node", "image", "is",
                          "exists", "text", "property", "shown", "focused", "cmd+", "mod+", "shift+", "enter", "escape",
                          ".length", "[0]"]
    scenarios += [f"#{node}" for node in kanban_ids()]
    pngs = ["PNG", "IHDR", "IDAT", "IEND", "x\x01", "x^", "\x00\x00\x00\x0d"]  # ASCII: the dictionary is written UTF-8
    return {"json": tokens, "expression": expression, "revision": tokens + edits, "session": tokens + edits + session,
            "delta": delta, "text": text, "cli": tokens + ["\n\t", "\n\n", "{{", "}}", "#", "@"] + schema_words(),
            "actions": tokens + expression + steps,
            "layout": tokens + schema_words() + ["\"1fr\"", "\"auto\"", "\"minmax(", "\"25%\"", "\"space-between\"",
                                                 "\"stretch\"", "\"16/9\"", "\"none\"", "\"row\"", "\"grid\"",
                                                 "\"column\"", "1e38"],
            "lookups": [chr(0) + chr(value) for value in range(10)],  # big-endian 0..9
            "path": list("MmLlHhVvCcSsQqTtAaZz") + [" ", ",", "-", ".", "e-", "1e6", "0 0 1 1"],
            "image": ["<svg", "viewBox=", "<path d=", "fill=", "stroke=", "transform=", "<g>", "</g>", "IHDR", "IDAT", "IEND", "PLTE", "tRNS", "gAMA", "PNG\r\n", "JFIF", "Exif"],
            "scenarios": scenarios, "png": pngs,
            "ask_reply": tokens + [f'"{word}"' for word in ["request", "text", "from", "done", "error", "type",
                                                            "reply", "ask-0-0"]],
            "http_response": ["HTTP/1.1 ", "HTTP/2 ", "\r\n", "\r\n\r\n", ": ", "Content-Length", "Retry-After",
                              "Location", "Upgrade", "websocket", "Connection", "Sec-WebSocket-Accept"],
            "sse": ["data:", "data: ", "event:", "id:", "retry:", ":", "\n", "\r\n", "\r", "\n\n"],
            "frames": ["Content-Length: ", "content-length:", "\r\n", "\r\n\r\n", "\n", '"jsonrpc":"2.0"'],
            "peer_handshake": ["GET ", " HTTP/1.1", "\r\n", "\r\n\r\n", ": ", "Upgrade", "websocket", "Connection",
                               "Sec-WebSocket-Key", "Sec-WebSocket-Version", "13", "Origin", "Authorization",
                               "Bearer ", '"type"', '"auth"', '"token"'],
            "websocket": ["\x81", "\x01", "\x80", "\x88", "\x89", "\x8a", "\x7e", "\x7f", "\x03\xe8"],
            "find_pattern": ["\x00", "\x01", "\x03", "\x05", "(", ")", "(?", "*", "+?", "{2,}", "\\b", "\\w", "\\d",
                             "[^", "]", "|", "$1", "${", "}", "$$", "\\p{L}", "(?i)", "(?s)", "\u00e9"],
            "search_glob": ["\x00", "\n", ",", "*", "**", "**/", "/", "?", "[", "]", "[!", "[^", "-", "\\", "!", "#",
                            ".gitignore", "src/", "build"],
            "text_model": ["\x00", "\x01", "\x02", "\x03", "\x04", "\x08", "\n", "\u00e9", "\u20ac", "\U0001F600"],
            "url": ["https://", "http://", "wss://", "ws://", "\n", "localhost", "127.0.0.1", "[::1]", ":443", ":0",
                    "*.", "/", "?", "#", "%2e", "%2F", "@", ".."],
            "oauth": tokens + ['"access_token"', '"token_type"', '"Bearer"', '"expires_in"', '"refresh_token"',
                               '"id_token"', '"scope"', '"error"', '"device_code"', '"user_code"', '"interval"',
                               '"verification_uri"', '"iss"', '"aud"', '"nonce"', '"exp"', '"sub"', ".", "=", "&",
                               "-", "_"],
            "token_file": ["#", "\n", "\r\n", " ", "\t", "0123456789abcdef"],
            "git_output": ["\x00", "\n", "# branch.oid ", "# branch.head ", "# branch.upstream ", "# branch.ab ",
                           "1 ", "2 ", "u ", "? ", "! ", ".M", "M.", "R.", "D.", "A.", "UU", "N...", "100644 ",
                           "R100 ", "*"],
            "vscode_theme": tokens + ['"name"', '"type"', '"dark"', '"light"', '"colors"', '"tokenColors"',
                                      '"editor.background"', '"editor.foreground"', '"focusBorder"', '"#"',
                                      '"#ffffff"', '"#00000080"', ",}"],
            "history": tokens + [f'"{word}"' for word in ["version", "main", "proposals", "nextProposal", "grants",
                                                          "mode", "packs", "checksum", "parents", "author",
                                                          "committer", "time", "message", "patch", "tree", "delta",
                                                          "snapshot", "head", "base", "rule", "expires"]]}


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
        path = directory / f"seed-{index:04}"
        if isinstance(seed, bytes):
            path.write_bytes(seed)
        else:
            path.write_text(seed, encoding="utf-8")
    # Inputs that found a bug, kept as they were (regressions/<target>/), so the fuzzer starts from them too
    regressions = sorted((ROOT / "tests" / "fuzz" / "regressions" / target).glob("*"))
    for found in regressions:
        shutil.copyfile(found, directory / f"regression-{found.name}")
    seeds = [*seeds, *regressions]
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
    write(out, "delta", delta_seeds(), words["delta"])
    write(out, "policy", policy_seeds(),
          words["revision"] + ["budgets", "data", "read", "write", "never", "over", "role:"])
    write(out, "text", text_seeds(), words["text"])
    write(out, "richtext", richtext_seeds(), words["text"] + ["\x1f", "\x01", "\x02", "\x04", "\x08"])
    write(out, "cli", cli_seeds(), words["cli"])
    write(out, "actions", action_seeds(), words["actions"])
    write(out, "layout", layout_seeds(), words["layout"])
    write(out, "lookups", lookups_seeds(), words["lookups"])
    write(out, "image", image_seeds(), words["image"])
    write(out, "path", path_seeds(), words["path"])
    write(out, "text_model", text_model_seeds(), words["text_model"])
    write(out, "find_pattern", find_pattern_seeds(), words["find_pattern"])
    write(out, "search_glob", search_glob_seeds(), words["search_glob"])
    write(out, "scenarios", scenario_seeds(), words["scenarios"])
    write(out, "png", png_seeds(), words["png"])
    write(out, "http_response", http_response_seeds(), words["http_response"])
    write(out, "sse", sse_seeds(), words["sse"])
    write(out, "websocket", websocket_seeds(), words["websocket"])
    write(out, "frames", frames_seeds(), words["frames"])
    write(out, "peer_handshake", peer_handshake_seeds(), words["peer_handshake"])
    features = ["affrescoui-feature", "affrescoui-catalog", "requires", "provides", "points", "exports", "signature",
                "publisher", "releases", "release", "digest", "url", "size", "ed25519:", "sha512:", "1.0.0", "^1.0.0"]
    write(out, "feature_manifest", feature_seeds(), words["json"] + features)
    migrations = [path.read_text(encoding="utf-8") for path in sorted((ROOT / "tests" / "migrate").glob("*/*.json"))]
    write(out, "migrate", migrations, words["json"] + ["from", "justify", "align", "len(", "length(", "requires"])
    write(out, "catalog_index", feature_seeds(), words["json"] + features)
    write(out, "signature", feature_seeds() + ["ed25519-secret:", "-_8AEA"], words["json"] + features)
    write(out, "composition", composition_seeds(), words["json"] + features + ["needs", "bindings", "adapter", "dock",
          "affrescoui-adapter", "affrescoui-proof", "list", "action", "image", "{{", "}}"])
    for target, seeds in [("url", url_seeds()), ("oauth", oauth_seeds()), ("token_file", token_file_seeds()),
                          ("git_output", git_output_seeds()), ("vscode_theme", vscode_theme_seeds()),
                          ("history", history_seeds())]:
        write(out, target, seeds, words[target])
    write(out, "adapters", adapters_seeds(), words["json"] + ["surfaceUpdate", "beginRendering", "dataModelUpdate",
          "literalString", "explicitList", "AdaptiveCard", "TextBlock", "Input.Text", "Action.Submit", "${"])
    write(out, "dap", dap_seeds(), words["json"] + ["response", "event", "request", "request_seq", "success",
          "command", "body", "source", "path", "/work/", "../", "stackFrames", "variables", "runInTerminal"])
    write(out, "gltf", gltf_seeds(), words["json"] + ["asset", "scenes", "nodes", "meshes", "primitives", "attributes",
          "POSITION", "NORMAL", "TEXCOORD_0", "accessors", "bufferViews", "buffers", "byteOffset", "byteLength",
          "componentType", "count", "VEC3", "SCALAR", "animations", "channels", "samplers", "data:", ";base64,", "glTF"])
    write(out, "scene", scene_seeds(), words["json"] + ["gltf", "scene", "animations", "time", "nodes", "materials",
          "mesh", "parent", "translation", "rotation", "rotationDegrees", "scale", "light", "asset", "tracks", "times",
          "values", "cube", "sphere", "cylinder", "cone", "torus", "plane", "directional", "point", "spot", "m"])


    write(out, "ask_reply", ask_reply_seeds(), words["ask_reply"])
    operations = image_operations()
    write(out, "images", images_seeds(), words["json"] + ["images", "steps", "image", "of", "with", "limits"]
          + list(operations) + sorted({key for keys in operations.values() for key in keys}))
    write(out, "syntax", syntax_seeds(), ["(", ")", "{", "}", "[", "]", "\"", "'", "`", "/*", "*/", "//", "#", "\n"])


if __name__ == "__main__":
    main()
