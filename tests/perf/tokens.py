#!/usr/bin/env python3
"""Counts the tokens of what agents read and write: per template and for Forge (its features merged in), the minified
document (the first get_screen read), its outline (get_screen with outline=true) and the canonical edits in
tests/perf/edits (submit_revision's arguments); each desktop kit component's general use in tests/perf/kit.json, as an
agent writes it; SCREEN_FORMAT.md (get_format_guide); and, with the affrescoui tool built, each pick in picks.json:
what an agent reads first with it (the pick's line and get_source's reply, from `affrescoui source`) against what it
reads first without one, and their ratio. One JSON object per line, like affrescoui_bench.

    pip install --require-hashes -r tests/perf/requirements.txt
    python3 tests/perf/tokens.py              # JSON lines
    python3 tests/perf/tokens.py --table      # a table to read
    python3 tests/perf/tokens.py --root ../base --out base-tokens.jsonl
    python3 tests/perf/tokens.py --table --cli build/affrescoui   # the picks from this tool (default: build's)

The counter is tiktoken's o200k_base, a stand-in: Claude's tokenizer isn't public, so compare ratios, not totals. The
document and outline come from the MCP server's own functions (mcp/affrescoui_mcp.py under --root), so a change to
what the server sends shows here. The edits are always this checkout's, so two roots are compared on the same edits.
"""

import argparse
import copy
import glob
import importlib.util
import inspect
import json
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SUFFIX = ".screen.json"
OUTLINED = 32 * 1024  # bytes of minified document above which an agent reads the outline first, not the document


def load_mcp(root):
    """The MCP server's module under a root, imported without running it."""
    spec = importlib.util.spec_from_file_location("affrescoui_mcp", os.path.join(root, "mcp", "affrescoui_mcp.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def apply_edit(mcp, document, edit):
    """A compact edit applied to a copy of a document, as the host applies one ("#id" paths found with the MCP server's
    find_node): set, add, remove, then move. Enough for the canonical edits; the host's own rules are not checked."""
    document = copy.deepcopy(document)

    def resolve(path):
        if path.startswith("#"):
            node_id, _, rest = path[1:].partition("/")
            path = mcp.find_node(document, node_id)[0] + ("/" + rest if rest else "")
        keys = [key.replace("~1", "/").replace("~0", "~") for key in path.split("/")[1:]]
        parent = document
        for key in keys[:-1]:
            parent = parent[int(key)] if isinstance(parent, list) else parent[key]
        return parent, keys[-1]

    def put(path, value, insert):
        parent, key = resolve(path)
        if isinstance(parent, list):
            parent.insert(len(parent) if key == "-" else int(key), value) if insert or key == "-" else None
            if not insert and key != "-":
                parent[int(key)] = value
        else:
            parent[key] = value

    def take(path):
        parent, key = resolve(path)
        return parent.pop(int(key) if isinstance(parent, list) else key)

    for path, value in edit.get("set", {}).items():
        put(path, value, False)
    for path, value in edit.get("add", {}).items():
        put(path, value, True)
    for path in edit.get("remove", []):
        take(path)
    for source, target in edit.get("move", {}).items():
        put(target, take(source), True)
    return document


def reread(mcp, document, edit):
    """What get_screen returns after an edit: the server's changes_since, or for a server without it, the same diff
    written from node ids."""
    after = apply_edit(mcp, document, edit)
    if hasattr(mcp, "changes_since"):
        return mcp.changes_since(document, after)
    changes = mcp.diff(document, after)
    for op in changes:
        if op["path"].startswith(("/root", "/screens", "/templates", "/components")):
            op["path"] = mcp.by_id(after if op["op"] != "remove" else document, op["path"])
    return changes


SCREENS_OWN = ("$schema", "format", "version", "root", "window")


def merge_include(document, part):
    """Merges one included file into a screen as the host does (Screen.cpp's mergeInclude): each section of the
    file comes first and the screen's entries win; "strings" merge locale by locale and "menus" menu by menu, so an
    include adds keys and items without replacing the screen's own; "persist" lists join; the screen's root, window,
    format and version are kept (taken from the include when missing), and an include's "$schema" is dropped."""
    for key, value in part.items():
        if key in SCREENS_OWN:
            if key not in document and key != "$schema":
                document[key] = value
        elif key not in document:
            document[key] = value
        elif isinstance(value, dict) and isinstance(document[key], dict):
            section = dict(document[key])
            if key in ("menus", "strings"):  # one level deeper: a locale's keys, a menu's items
                for name, entries in value.items():
                    if isinstance(entries, dict) and isinstance(section.get(name), dict):
                        section[name] = {**entries, **section[name]}
            document[key] = {**value, **section}
        elif key == "persist" and isinstance(value, list) and isinstance(document[key], list):
            document[key] = value + [entry for entry in document[key] if entry not in value]


def with_includes(document, folder, depth=0):
    """A screen with its "include" files merged in, recursively (at most 8 deep), as the host merges them."""
    if depth > 8:
        raise ValueError("includes nest too deeply")
    merged = {key: value for key, value in document.items() if key != "include"}
    for include in document.get("include", []):
        with open(os.path.join(folder, include), encoding="utf-8") as file:
            part = with_includes(json.load(file), os.path.dirname(os.path.join(folder, include)), depth + 1)
        merge_include(merged, part)
    return merged


def template_files(root):
    """The templates' (name, screen file), in name order: from examples/templates.json, or, in a root from before it,
    the one folder that held them."""
    listing = os.path.join(root, "examples", "templates.json")
    if not os.path.exists(listing):
        paths = sorted(glob.glob(os.path.join(root, "examples", "assets", "screens", "*" + SUFFIX)))
        return [(os.path.basename(path)[:-len(SUFFIX)], path) for path in paths]
    with open(listing, encoding="utf-8") as file:
        templates = json.load(file)["templates"]
    paths = []
    for name in sorted(templates):  # each folder's index.screen.json, or, in a root from before it, <name>.screen.json
        folder = os.path.join(root, "examples", templates[name])
        if os.path.basename(folder) == "lisp":  # a program's folder: its JSON version beside it is what is counted
            folder = os.path.join(os.path.dirname(folder), "json")
        index = os.path.join(folder, "index" + SUFFIX)
        path = index if os.path.exists(index) else os.path.join(folder, name + SUFFIX)
        if os.path.exists(path):  # an app with only a program (Calamaio) has no JSON to count
            paths.append((name, path))
    return paths


def screens(root):
    """The documents agents read, as the host loads them (its includes merged in), as (name, document): every
    template, then Forge with its features."""
    for name, path in template_files(root):
        with open(path, encoding="utf-8") as screen:
            yield name, with_includes(json.load(screen), os.path.dirname(path))
    forge = os.path.join(root, "examples", "forge", "json", "index" + SUFFIX)
    if not os.path.exists(forge):  # a root from before index.screen.json
        forge = os.path.join(root, "examples", "forge", "forge" + SUFFIX)
    if os.path.exists(forge):  # a root from before Forge has none
        with open(forge, encoding="utf-8") as screen:
            yield "forge", with_includes(json.load(screen), os.path.dirname(forge))


def counts(root, edits, encoding, cli=None):
    """Every count, as (name, tokens), in a fixed order; with the affrescoui tool `cli`, the picks' too (ratios last
    in each pick's rows)."""
    mcp = load_mcp(root)

    def tokens(text):
        return len(encoding.encode(text, disallowed_special=()))

    with open(os.path.join(root, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
        text = guide.read()
    yield "tokens/format-guide", tokens(text)
    if hasattr(mcp, "format_guide"):  # what get_format_guide gives without a topic (servers that have topics)
        yield "tokens/format-guide/contents", tokens(mcp.format_guide(text))
    for name, document in screens(root):
        yield f"tokens/{name}/document", tokens(mcp.compact(document))
        yield f"tokens/{name}/outline", tokens(mcp.outline(document))
        if "depth" in inspect.signature(mcp.outline).parameters:
            yield f"tokens/{name}/outline-depth-2", tokens(mcp.outline(document, depth=2))
        edit_file = os.path.join(edits, name + ".json")
        if os.path.exists(edit_file):
            with open(edit_file, encoding="utf-8") as file:
                canonical = json.load(file)
            for edit_name, edit in canonical.items():
                yield f"tokens/{name}/edit/{edit_name}", tokens(mcp.compact(edit))
            # The re-read after one commit: get_screen's changes once a node's text changed. The edits are this
            # checkout's, so on another root (CI's base) an edit can name an id its templates don't have yet: then
            # the colour change, which every root has, is counted
            for edit in (canonical.get("rename-label"), canonical.get("change-colour")):
                try:
                    changes = reread(mcp, document, edit)
                except (TypeError, KeyError, IndexError, ValueError, AttributeError):
                    continue
                yield f"tokens/{name}/reread", tokens(mcp.compact({"changes": changes}))
                break
    # The kit's uses are this checkout's, like the edits
    with open(os.path.join(HERE, "kit.json"), encoding="utf-8") as file:
        for component, use in json.load(file).items():
            yield f"tokens/kit/{component}", tokens(mcp.compact(use))
    if cli:
        yield from pick_counts(root, mcp, tokens, cli)


def default_cli():
    """This checkout's affrescoui tool (build/affrescoui), whose `source` gives the picks' sources; None if not
    built."""
    path = os.path.join(ROOT, "build", "affrescoui")
    return path if os.path.exists(path) else None


def json_version(root, screen):
    """The JSON document beside a screen, the one tokens are counted on: the screen itself if it is JSON, else the
    index.screen.json of the json folder beside a program's lisp folder; None if there is none."""
    path = os.path.join(root, screen)
    if path.endswith(SUFFIX):
        return path
    folder = os.path.dirname(path)
    if os.path.basename(folder) != "lisp":
        return None
    beside = os.path.join(os.path.dirname(folder), "json", "index" + SUFFIX)
    return beside if os.path.exists(beside) else None


def pick_name(screen):
    """A pick entry's name in its rows: its example folder and notation ("flow-lisp")."""
    parts = screen.replace("\\", "/").split("/")
    return f"{parts[-3]}-{parts[-2]}" if len(parts) >= 3 else parts[-1]


def node_source(cli, screen, at):
    """What `affrescoui source --json` gives for a node, or None if the tool can't give it (none built, a root from
    before the command, nothing at `at`)."""
    try:
        process = subprocess.run([cli, "source", screen, at, "--json"], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if process.returncode != 0:
        return None
    return json.loads(process.stdout)


def repeats_along(document, pointer):
    """The repeats a node is in (its own included), outermost first, along its JSON pointer in the document: what a
    pick's `repeat` holds, which getSource doesn't give."""
    found, node = [], document
    for part in (pointer or "").split("/")[1:]:
        part = part.replace("~1", "/").replace("~0", "~")
        try:
            node = node[int(part)] if isinstance(node, list) else node[part]
        except (KeyError, IndexError, ValueError, TypeError):
            break
        if isinstance(node, dict) and isinstance(node.get("repeat"), dict):
            found.append({"for": node["repeat"].get("for"), "in": node["repeat"].get("in")})
    return found


def pick_read(mcp, document, at, source):
    """What an agent reads first for a request that points: the line pick_lines gives for the pick (its type from the
    document, where it is written from the source), then get_source's reply (the source's fields as compact JSON,
    then its text, as the MCP server gives them)."""
    found = mcp.find_node(document, at[1:]) if at.startswith("#") else None
    record = {"node": at, "type": mcp.node_type(found[1], document.get("styles")) if found else "element",
              "source": {key: source[key] for key in ("address", "kind", "file", "line", "column", "shared")
                         if key in source}}
    if repeats_along(document, source.get("pointer")):
        record["repeat"] = repeats_along(document, source.get("pointer"))
    fields = {key: value for key, value in source.items() if key != "text"}
    return "\n".join(mcp.pick_lines([record])) + "\n" + mcp.compact(fields) + "\n" + source.get("text", "")


def pick_counts(root, mcp, tokens, cli):
    """Per entry of picks.json: the tokens of what an agent reads first with a pick (`pick`), without one
    (`first-read`: the outline for a document over OUTLINED bytes minified, else the document), and their ratio, as
    (name, value). An entry the tool can't answer is said on the error output and skipped."""
    if not hasattr(mcp, "pick_lines"):  # a root from before picks
        return
    with open(os.path.join(HERE, "picks.json"), encoding="utf-8") as file:
        entries = json.load(file)["picks"]
    for entry in entries:
        path = json_version(root, entry["screen"])
        source = node_source(cli, os.path.join(root, entry["screen"]), entry["at"]) if path else None
        if source is None:
            print(f"tokens.py: no pick counted for {entry['screen']} {entry['at']}", file=sys.stderr)
            continue
        with open(path, encoding="utf-8") as screen:
            document = with_includes(json.load(screen), os.path.dirname(path))
        minified = mcp.compact(document)
        first = mcp.outline(document) if len(minified.encode("utf-8")) > OUTLINED else minified
        name = f"tokens/{pick_name(entry['screen'])}/pick/{entry['at'].lstrip('#')}"
        pick, first_read = tokens(pick_read(mcp, document, entry["at"], source)), tokens(first)
        yield name, pick
        yield name + "/first-read", first_read
        yield name + "/ratio", round(pick / first_read, 4)


def table(results):
    """A table to read: one row per template, the edits as their smallest, median and largest."""
    templates = {}
    for name, value in results:
        parts = name.split("/")
        if len(parts) >= 3 and parts[0] == "tokens" and parts[1] not in ("format-guide", "kit") and parts[2] != "pick":
            templates.setdefault(parts[1], {"edits": []})
            if parts[2] == "edit":
                templates[parts[1]]["edits"].append(value)
            else:
                templates[parts[1]][parts[2]] = value
    rows = ["| Template | Document | Outline | Outline, depth 2 | Re-read after a commit | "
            "Edits (min / median / max) |", "|---|---:|---:|---:|---:|---|"]
    for template, row in templates.items():
        edits = sorted(row["edits"])
        spread = f"{edits[0]} / {edits[len(edits) // 2]} / {edits[-1]} ({len(edits)} edits)" if edits else "n/a"
        cells = [f"{row[key]:,}" if key in row else "n/a" for key in ("document", "outline", "outline-depth-2", "reread")]
        rows.append(f"| {template} | {' | '.join(cells)} | {spread} |")
    found = dict(results)
    guide = f"\n\nSCREEN_FORMAT.md: {found['tokens/format-guide']:,} tokens (o200k_base)"
    if "tokens/format-guide/contents" in found:
        guide += f"; get_format_guide without a topic: {found['tokens/format-guide/contents']:,}"
    return "\n".join(rows) + guide + "\n" + pick_table(results)


def pick_table(results):
    """The picks' table: per entry of picks.json, what an agent reads first with the pick and without it, and the
    ratio; then the ratio's median, smallest and largest. Empty without picks."""
    found = dict(results)
    names = [name for name in found if "/pick/" in name and not name.endswith(("/first-read", "/ratio"))]
    if not names:
        return ""
    rows = ["\n| Example | Pointed at | With a pick | First read without | Ratio |", "|---|---|---:|---:|---:|"]
    for name in names:
        parts = name.split("/")
        rows.append(f"| {parts[1]} | #{parts[3]} | {found[name]:,} | {found[name + '/first-read']:,} | "
                    f"{found[name + '/ratio']:.3f} |")
    ratios = sorted(found[name + "/ratio"] for name in names)
    largest = max(names, key=lambda name: found[name])
    return ("\n".join(rows) + f"\n\nRatio over {len(names)} picks: median {statistics.median(ratios):.3f}, smallest "
            f"{ratios[0]:.3f}, largest {ratios[-1]:.3f}; the largest pick: {found[largest]:,} tokens "
            f"({largest.split('/')[1]} {largest.split('/')[3]})\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=ROOT, help="the checkout whose templates, guide and MCP server to count")
    parser.add_argument("--edits", default=os.path.join(HERE, "edits"), help="the canonical edits")
    parser.add_argument("--table", action="store_true", help="print a table instead of JSON lines")
    parser.add_argument("--out", help="write the results here instead of to stdout")
    parser.add_argument("--cli", default=default_cli(),
                        help="the affrescoui tool whose `source` gives the picks' sources (none: no pick rows)")
    args = parser.parse_args()

    try:
        import tiktoken
    except ImportError:
        sys.exit("tokens.py: needs tiktoken: pip install -r tests/perf/requirements.txt")
    cli = args.cli if args.cli and os.path.exists(args.cli) else None
    if not cli:
        print("tokens.py: no affrescoui tool built (--cli): no pick rows", file=sys.stderr)
    results = list(counts(os.path.abspath(args.root), args.edits, tiktoken.get_encoding("o200k_base"), cli))

    if args.table:
        text = table(results)
    else:
        text = "".join(json.dumps({"name": name, "value": value,
                                   "unit": "ratio" if name.endswith("/ratio") else "tokens", "runs": 1}) + "\n"
                       for name, value in results)
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as out:
            out.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
