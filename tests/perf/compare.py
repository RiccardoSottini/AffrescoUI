#!/usr/bin/env python3
"""Compares two sets of results (base and head), writes the tables, and fails if an instruction count grew too much.

    python3 tests/perf/compare.py --base out/base --head out/head [--threshold 3] [--allow] [--summary file.md]

Each directory holds *.jsonl files from run.py and tokens.py, one result per line. Results are matched by name, and
the ones only the base has are listed. Only
instruction counts (unit "instructions") are gated: a workload whose count grows by more than --threshold percent
fails, unless --allow is given (the pull request has the perf-change label). Wall clock, allocations, memory and
tokens are reported, not gated.
"""

import argparse
import glob
import json
import os
import sys


def load(directory):
    """Every result in a directory's *.jsonl files, by name."""
    results = {}
    for path in sorted(glob.glob(os.path.join(directory, "*.jsonl"))):
        with open(path, encoding="utf-8") as file:
            for line in file:
                if line.strip():
                    result = json.loads(line)
                    results[result["name"]] = result
    return results


def number(value):
    """A value for a table: whole numbers with separators, others with three significant decimals."""
    if value is None:
        return "–"
    if float(value).is_integer():
        return f"{int(value):,}"
    return f"{value:,.3f}" if abs(value) < 100 else f"{value:,.1f}"


def change(base, head):
    """The change from base to head, in percent (infinite from zero), or None if there is no base to compare with."""
    if base is None or head is None:
        return None
    if base == 0:
        return 0.0 if head == 0 else float("inf") if head > 0 else float("-inf")
    return (head - base) / abs(base) * 100.0


def percent(value):
    """A change for a table."""
    if value is None:
        return "new"
    return f"{value:+.2f}%" if abs(value) != float("inf") else "from 0"


def rows(base, head, names, field="value"):
    """Table rows (name, unit, base, head, change) for some results, and the changes by name."""
    lines, changes = [], {}
    for name in names:
        before = base.get(name, {}).get(field)
        after = head[name].get(field)
        changes[name] = change(before, after)
        unit = head[name]["unit"] if field == "value" else field
        lines.append(f"| `{name}` | {unit} | {number(before)} | {number(after)} | {percent(changes[name])} |")
    return lines, changes


def section(title, base, head, names, field="value"):
    """A titled table, and its changes."""
    if not names:
        return [], {}
    lines, changes = rows(base, head, names, field)
    header = [f"#### {title}", "", "| Workload | Unit | Base | Head | Change |", "|---|---|---:|---:|---:|"]
    return header + lines + [""], changes


def report(base, head, threshold, allow):
    """The markdown report, and the workloads whose instruction counts grew by more than the threshold."""
    gated = [name for name, result in head.items() if result["unit"] == "instructions"]
    timed = [name for name, result in head.items() if result["unit"] in ("ns", "us", "ms")]
    tokens = [name for name, result in head.items() if result["unit"] == "tokens"]

    lines = ["### Benchmarks (tests/perf)", ""]
    if not any(base.get(name) for name in gated):
        lines += ["The base has no benchmark results: head's numbers are below, with nothing to compare them with.", ""]
    instructions, changes = section(f"Instructions per operation (callgrind; gated at +{threshold:g}%)", base, head,
                                    gated)
    regressions = [name for name in gated if changes.get(name) is not None and changes[name] > threshold]
    lines += instructions
    lines += section("Allocations per operation (counted under callgrind)", base, head, gated, "allocations")[0]
    lines += section("Bytes kept per operation (counted under callgrind)", base, head, gated, "retainedBytes")[0]
    lines += section("Time (wall clock, or the main thread's CPU time for frames; not gated: shared runners are noisy)",
                     base, head, timed)[0]
    lines += section("Tokens (o200k_base, a stand-in for Claude's tokenizer)", base, head, tokens)[0]
    removed = [name for name in base if name not in head]
    if removed:  # a workload removed or renamed is no longer gated: say so instead of dropping it silently
        lines += ["**Measured on the base but not on the head** (removed or renamed, so not compared):", ""]
        lines += [f"- `{name}`" for name in removed] + [""]

    if regressions:
        verdict = "accepted: the pull request has the `perf-change` label" if allow else "the job fails"
        lines += [f"**{len(regressions)} workload(s) grew by more than {threshold:g}% in instructions ({verdict}):**", ""]
        lines += [f"- `{name}`: {percent(changes[name])}" for name in regressions] + [""]
    elif gated:
        lines += [f"No workload grew by more than {threshold:g}% in instructions.", ""]
    return "\n".join(lines), regressions


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", required=True, help="the base's results (a directory of *.jsonl)")
    parser.add_argument("--head", required=True, help="the head's results")
    parser.add_argument("--threshold", type=float, default=3.0, help="largest growth in instructions, in percent")
    parser.add_argument("--allow", action="store_true", help="report growth without failing (perf-change label)")
    parser.add_argument("--summary", help="also append the report to this file (e.g. $GITHUB_STEP_SUMMARY)")
    args = parser.parse_args()

    base, head = load(args.base), load(args.head)
    if not head:
        sys.exit(f"compare.py: no results in {args.head}")
    text, regressions = report(base, head, args.threshold, args.allow)
    print(text)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as summary:
            summary.write(text + "\n")
    if regressions and not args.allow:
        for name in regressions:
            print(f"::error title=Benchmarks::{name} grew by more than {args.threshold:g}% in instructions")
        sys.exit(1)


if __name__ == "__main__":
    main()
