#!/usr/bin/env python3
"""Writes the seed corpora and dictionaries of the fuzz targets (README.md), from the sources they stand for.

- json:       the templates in examples/assets/screens, the JSON examples in the docs, nesting at the limit;
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

Generated rather than checked in, so the seeds follow the templates and the docs as they change; the inputs that found
a bug are checked in, in regressions/<target>/, and added as they are.
Usage: tests/fuzz/corpus.py [--out build/fuzz-corpus]  ->  <out>/<target>/ and <out>/<target>.dict
"""

import argparse
import json
import re
import shutil
import struct
import zlib
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


def text_seeds():
    """Every text of the templates and of the rendering check's screens: scripts, emoji, marks, ligatures."""
    seeds = []
    for path in sorted(SCREENS.glob("*.screen.json")) + sorted((ROOT / "tests" / "render" / "screens").glob("*.json")):
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
    files = sorted(SCREENS.glob("*.json")) + sorted((ROOT / "tests" / "render" / "screens").glob("*.json"))
    seeds = [path.read_text(encoding="utf-8") for path in files]
    seeds.append('{\n\t"format": "affrescoui-screen",\n\n\t"version": 1,\n\t"root": { "children": [{ "type": "label", '
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
    for path in sorted(SCREENS.glob("*.screen.json")):
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
    fonts = sorted((ROOT / "examples/assets/fonts").glob("*.ttf")) + sorted((ROOT / "tests/render/fonts").glob("*.ttf"))
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
    files = sorted(SCREENS.glob("*.json")) + sorted((ROOT / "tests" / "render" / "screens").glob("*.json"))
    files.append(ROOT / "examples" / "assets" / "components" / "charts.json")
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


def scenario_seeds():
    """Every scenarios file: the templates' own and the rendering check's, as they are."""
    paths = sorted(SCREENS.glob("*.scenarios.json")) + sorted((ROOT / "tests" / "render" / "scenarios").glob("*.json"))
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
    delta = tokens + ["from", "to", "delta", "op", "add", "remove", "replace", "move", "copy", "test", "path", "value",
                      "/0", "/-", "~0", "~1", "1.0", "-0.0"]
    # The steps and the keys they read, from the step groups' registrations (modules/format/src/*Steps.cpp)
    groups = "".join(path.read_text() for path in sorted((ROOT / "modules/format/src").glob("*Steps.cpp")))
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
            "http_response": ["HTTP/1.1 ", "HTTP/2 ", "\r\n", "\r\n\r\n", ": ", "Content-Length", "Retry-After",
                              "Location", "Upgrade", "websocket", "Connection", "Sec-WebSocket-Accept"],
            "sse": ["data:", "data: ", "event:", "id:", "retry:", ":", "\n", "\r\n", "\r", "\n\n"],
            "frames": ["Content-Length: ", "content-length:", "\r\n", "\r\n\r\n", "\n", '"jsonrpc":"2.0"'],
            "peer_handshake": ["GET ", " HTTP/1.1", "\r\n", "\r\n\r\n", ": ", "Upgrade", "websocket", "Connection",
                               "Sec-WebSocket-Key", "Sec-WebSocket-Version", "13", "Origin", "Authorization",
                               "Bearer ", '"type"', '"auth"', '"token"'],
            "websocket": ["\x81", "\x01", "\x80", "\x88", "\x89", "\x8a", "\x7e", "\x7f", "\x03\xe8"],
            "text_model": ["\x00", "\x01", "\x02", "\x03", "\x04", "\x08", "\n", "\u00e9", "\u20ac", "\U0001F600"]}


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
    write(out, "text", text_seeds(), words["text"])
    write(out, "richtext", richtext_seeds(), words["text"] + ["\x1f", "\x01", "\x02", "\x04", "\x08"])
    write(out, "cli", cli_seeds(), words["cli"])
    write(out, "actions", action_seeds(), words["actions"])
    write(out, "layout", layout_seeds(), words["layout"])
    write(out, "lookups", lookups_seeds(), words["lookups"])
    write(out, "image", image_seeds(), words["image"])
    write(out, "path", path_seeds(), words["path"])
    write(out, "text_model", text_model_seeds(), words["text_model"])
    write(out, "scenarios", scenario_seeds(), words["scenarios"])
    write(out, "png", png_seeds(), words["png"])
    write(out, "http_response", http_response_seeds(), words["http_response"])
    write(out, "sse", sse_seeds(), words["sse"])
    write(out, "websocket", websocket_seeds(), words["websocket"])
    write(out, "frames", frames_seeds(), words["frames"])
    write(out, "peer_handshake", peer_handshake_seeds(), words["peer_handshake"])


if __name__ == "__main__":
    main()
