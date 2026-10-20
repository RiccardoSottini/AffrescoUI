#!/usr/bin/env python3
"""What an agent reads when a list is a host data model rather than state (RFC 0002, "Host data models").

    pip install -r tests/perf/requirements.txt
    python3 rfcs/prototypes/0002-services/model_read.py

Compares, in tiktoken o200k tokens: 10,000 file entries kept in the state (what get_screen would send today) with
the summary of a model of 1,204,331 entries (its count, the shape of a row and three sample rows).
"""

import json

import tiktoken


def tokens(value, encoding):
    """The tokens of a value as minified JSON."""
    return len(encoding.encode(json.dumps(value, separators=(",", ":"))))


def main():
    """Prints both counts."""
    encoding = tiktoken.get_encoding("o200k_base")
    rows = [{"name": f"file{i}.cpp", "path": f"src/module{i // 50}/file{i}.cpp", "depth": 2} for i in range(10000)]
    summary = {"models": {"files.tree": {"count": 1204331,
                                         "shape": {"name": "text", "path": "text", "depth": "number",
                                                   "folder": "boolean"},
                                         "sample": rows[:3]}}}
    print(f"10,000 rows in the state: {tokens(rows, encoding):,} tokens")
    print(f"a model of 1,204,331 rows, summarised: {tokens(summary, encoding):,} tokens")


if __name__ == "__main__":
    main()
