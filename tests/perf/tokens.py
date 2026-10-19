#!/usr/bin/env python3
"""Counts the tokens of what agents read and write: per template, the minified document (the first get_screen read),
its outline (get_screen with outline=true) and the canonical edits in tests/perf/edits (submit_revision's
arguments); and SCREEN_FORMAT.md (get_format_guide). One JSON object per line, like affrescoui_bench.

    pip install -r tests/perf/requirements.txt
    python3 tests/perf/tokens.py              # JSON lines
    python3 tests/perf/tokens.py --table      # a table to read
    python3 tests/perf/tokens.py --root ../base --out base-tokens.jsonl

The counter is tiktoken's o200k_base, a stand-in: Claude's tokenizer isn't public, so compare ratios, not totals. The
document and outline come from the MCP server's own functions (mcp/affrescoui_mcp.py under --root), so a change to
what the server sends shows here. The edits are always this checkout's, so two roots are compared on the same edits.
"""

import argparse
import glob
import importlib.util
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SUFFIX = ".screen.json"


def load_mcp(root):
    """The MCP server's module under a root, imported without running it."""
    spec = importlib.util.spec_from_file_location("affrescoui_mcp", os.path.join(root, "mcp", "affrescoui_mcp.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def counts(root, edits, encoding):
    """Every count, as (name, tokens), in a fixed order."""
    mcp = load_mcp(root)

    def tokens(text):
        return len(encoding.encode(text, disallowed_special=()))

    with open(os.path.join(root, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
        yield "tokens/format-guide", tokens(guide.read())
    for path in sorted(glob.glob(os.path.join(root, "examples", "assets", "screens", "*" + SUFFIX))):
        name = os.path.basename(path)[:-len(SUFFIX)]
        with open(path, encoding="utf-8") as screen:
            document = json.load(screen)
        yield f"tokens/{name}/document", tokens(mcp.compact(document))
        yield f"tokens/{name}/outline", tokens(mcp.outline(document))
        edit_file = os.path.join(edits, name + ".json")
        if os.path.exists(edit_file):
            with open(edit_file, encoding="utf-8") as file:
                for edit_name, edit in json.load(file).items():
                    yield f"tokens/{name}/edit/{edit_name}", tokens(mcp.compact(edit))


def table(results):
    """A table to read: one row per template, the edits as their smallest, median and largest."""
    templates = {}
    for name, value in results:
        parts = name.split("/")
        if len(parts) >= 3:
            templates.setdefault(parts[1], {"edits": []})
            if parts[2] == "edit":
                templates[parts[1]]["edits"].append(value)
            else:
                templates[parts[1]][parts[2]] = value
    rows = ["| Template | Document | Outline | Edits (min / median / max) |", "|---|---:|---:|---|"]
    for template, row in templates.items():
        edits = sorted(row["edits"])
        spread = f"{edits[0]} / {edits[len(edits) // 2]} / {edits[-1]} ({len(edits)} edits)" if edits else "n/a"
        rows.append(f"| {template} | {row['document']:,} | {row['outline']:,} | {spread} |")
    guide = dict(results)["tokens/format-guide"]
    return "\n".join(rows) + f"\n\nSCREEN_FORMAT.md: {guide:,} tokens (o200k_base)\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=ROOT, help="the checkout whose templates, guide and MCP server to count")
    parser.add_argument("--edits", default=os.path.join(HERE, "edits"), help="the canonical edits")
    parser.add_argument("--table", action="store_true", help="print a table instead of JSON lines")
    parser.add_argument("--out", help="write the results here instead of to stdout")
    args = parser.parse_args()

    try:
        import tiktoken
    except ImportError:
        sys.exit("tokens.py: needs tiktoken: pip install -r tests/perf/requirements.txt")
    results = list(counts(os.path.abspath(args.root), args.edits, tiktoken.get_encoding("o200k_base")))

    if args.table:
        text = table(results)
    else:
        text = "".join(json.dumps({"name": name, "value": value, "unit": "tokens", "runs": 1}) + "\n"
                       for name, value in results)
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as out:
            out.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
