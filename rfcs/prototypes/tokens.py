#!/usr/bin/env python3
"""Counts the tokens of the RFC prototypes: every JSON document in a prototype folder (minified, as the MCP server
sends documents) and every entry of its edits.json (minified, as an agent writes submit_revision's arguments).

    pip install -r tests/perf/requirements.txt
    python3 rfcs/prototypes/tokens.py rfcs/prototypes/0001-contributions [more folders]

The counter is tiktoken's o200k_base, the stand-in tests/perf/tokens.py uses: Claude's tokenizer isn't public, so
compare ratios, not totals. Keys named "note" are left out: they explain the prototype, and an agent wouldn't send
them.
"""

import argparse
import glob
import json
import os
import sys


def minified(value):
    """A JSON value as one compact line, the way tools send it."""
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def without_notes(value):
    """A copy of a value without the prototype's "note" keys."""
    if isinstance(value, dict):
        return {key: without_notes(item) for key, item in value.items() if key != "note"}
    if isinstance(value, list):
        return [without_notes(item) for item in value]
    return value


def rows(folder, encoding):
    """(what, tokens) for a folder's documents, then its edits."""
    for path in sorted(glob.glob(os.path.join(folder, "**", "*.json"), recursive=True)):
        if os.path.basename(path) == "edits.json":
            continue
        with open(path, encoding="utf-8") as file:
            document = json.load(file)
        yield os.path.relpath(path, folder) + " (minified)", len(encoding.encode(minified(without_notes(document))))
    edits_path = os.path.join(folder, "edits.json")
    if os.path.exists(edits_path):
        with open(edits_path, encoding="utf-8") as file:
            edits = json.load(file)
        for name, edit in edits.items():
            if name == "note":
                continue
            yield "edit: " + name, len(encoding.encode(minified(without_notes(edit))))


def main():
    """Prints one markdown table per folder."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folders", nargs="+", help="prototype folders")
    arguments = parser.parse_args()
    try:
        import tiktoken
    except ImportError:
        sys.exit("tiktoken is missing: pip install -r tests/perf/requirements.txt")
    encoding = tiktoken.get_encoding("o200k_base")
    for folder in arguments.folders:
        print(f"\n{os.path.basename(os.path.normpath(folder))}\n")
        print("| What | Tokens (o200k) |\n|---|---|")
        for what, count in rows(folder, encoding):
            print(f"| {what} | {count:,} |")


if __name__ == "__main__":
    main()
