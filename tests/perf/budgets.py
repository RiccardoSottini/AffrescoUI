"""The budgets of BENCHMARKS.md (tests/perf/budgets.json): each one measured, checked against its target, and
written down. Used by run.py --budgets.

    python3 tests/perf/run.py --budgets                 # measures every budget here (release build), prints a table
    python3 tests/perf/run.py --budgets --markdown      # the tables of BENCHMARKS.md
    python3 tests/perf/run.py --budgets --record        # writes the measured values into budgets.json
    python3 tests/perf/run.py --budgets --from DIR      # CI: the instructions and tokens already measured in DIR

Sources: "bench" budgets are affrescoui_bench workloads (wall clock here; instructions under callgrind in CI), the
memory ones each in a process of its own; "tokens" budgets are counted with tokens.py's functions (tiktoken's
o200k_base) and the oracle's tasks in benchmarks/agents/tasks. A budget fails CI only above its ciThreshold: a number
in what CI measures (tokens, or instructions per operation), "pending" until a Linux callgrind run sets it, or null
with a reason. A miss stays a miss: the targets are never moved, and a missed token budget's threshold is the value
it was recorded at, so it can only get better.
"""

import datetime
import glob
import json
import os
import platform
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BUDGETS = os.path.join(HERE, "budgets.json")
TO_MILLISECONDS = {"ns": 1e-6, "us": 1e-3, "ms": 1.0}


def load():
    """budgets.json, as it is."""
    with open(BUDGETS, encoding="utf-8") as file:
        return json.load(file)


def bench_results(bench, names):
    """Runs affrescoui_bench on some workloads (names are prefixes): their results by name. A workload that fails
    (its error on stderr) has no result, so the budgets it feeds come from the others, or are "not measured"."""
    process = subprocess.run([bench, "--only", ",".join(names)], stdout=subprocess.PIPE, text=True)
    if process.returncode != 0:
        print(f"run.py: affrescoui_bench failed on some of {','.join(names)} (exit status {process.returncode}); "
              "the budgets use the others", file=sys.stderr)
    return {result["name"]: result for result in map(json.loads, filter(str.strip, process.stdout.splitlines()))}


def measure_bench(bench, targets):
    """Every bench budget's workloads, wall clock: the memory budgets' each in a process of its own (the process's
    memory is what they report), the others together."""
    alone = sorted({name for target in targets if target.get("field") == "residentBytes" for name in target["workloads"]})
    together = sorted({name for target in targets if target["source"] == "bench" for name in target["workloads"]} -
                      set(alone))
    results = bench_results(bench, together) if together else {}
    for name in alone:
        results.update(bench_results(bench, [name]))
    return results


def workload_value(result, field, unit):
    """One result's field in a budget's unit: times to ms (the field "processMs" of the idle second to a percentage of
    it), bytes as they are."""
    if field == "processMs":
        return result[field] / 1000.0 * 100.0  # of one second
    if unit == "ms":
        return result[field] * TO_MILLISECONDS[result["unit"]]
    return result[field]


def bench_value(target, results):
    """A bench budget's value: its field of each matching workload, aggregated; and the workload it came from."""
    values = [(workload_value(result, target["field"], target["unit"]), name) for name, result in results.items()
              if any(name == prefix or (prefix.endswith("/") and name.startswith(prefix))
                     for prefix in target["workloads"]) and target["field"] in result]
    if not values:
        return None, None
    if target["aggregate"] == "median":
        return statistics.median(value for value, _ in values), None
    return max(values)


def token_counts(encoding):
    """Every token count tokens.py makes, by name, with this checkout's edits."""
    sys.path.insert(0, HERE)
    import tokens  # tests/perf/tokens.py
    return dict(tokens.counts(ROOT, os.path.join(HERE, "edits"), encoding))


def guide_topics(mcp, count):
    """The format guide's largest topic, as get_format_guide gives it: (tokens, title)."""
    with open(os.path.join(ROOT, "SCREEN_FORMAT.md"), encoding="utf-8") as guide:
        text = guide.read()
    return max((count(part), title) for title, part in mcp.guide_sections(text))


def feature_requests(mcp, count):
    """The oracle's largest feature: (output tokens, task), over the tasks of the "feature" and "forge" levels, its
    submit_revision arguments as the server's compact JSON."""
    sizes = []
    for path in sorted(glob.glob(os.path.join(ROOT, "benchmarks", "agents", "tasks", "*.json"))):
        with open(path, encoding="utf-8") as file:
            task = json.load(file)
        if task["level"] in ("feature", "forge"):
            sizes.append((sum(count(mcp.compact(call)) for call in task["reference"]), task["id"]))
    return max(sizes) if sizes else (None, None)


def token_values(encoding):
    """Each token budget's value and where it comes from, by id."""
    found = token_counts(encoding)
    sys.path.insert(0, HERE)
    import tokens
    mcp = tokens.load_mcp(ROOT)

    def count(text):
        return len(encoding.encode(text, disallowed_special=()))

    def largest(suffix, exclude=()):
        return max((value, name.split("/")[1]) for name, value in found.items()
                   if name.endswith(suffix) and name.split("/")[1] not in exclude)

    edits = [value for name, value in found.items() if "/edit/" in name]
    return {"template-outline": largest("/outline", ("forge",)),
            "forge-outline": (found["tokens/forge/outline"], "forge"),
            "typical-edit": (statistics.median(edits), f"median of {len(edits)} canonical edits"),
            "reread": largest("/reread"),
            "guide-topic": guide_topics(mcp, count),
            "feature-request": feature_requests(mcp, count)}


def machine():
    """The machine measured on, in a few words (its processor and system): nothing that names a person."""
    if sys.platform == "darwin":
        chip = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], stdout=subprocess.PIPE, text=True)
        return f"{chip.stdout.strip()}, macOS {platform.mac_ver()[0]}"
    return f"{platform.machine()}, {platform.system()} {platform.release()}"


def commit():
    """The commit measured, short, marked when the checkout has changes."""
    head = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], stdout=subprocess.PIPE, text=True)
    dirty = subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "--untracked-files=no"],
                           stdout=subprocess.PIPE, text=True)
    return head.stdout.strip() + (" (with changes)" if dirty.stdout.strip() else "")


def measure(budgets, bench):
    """Every budget's value now: (value, where it came from) by id, None where it has no value (a workload this build
    lacks, or no oracle tasks yet)."""
    targets = budgets["targets"]
    results = measure_bench(bench, targets) if any(target["source"] == "bench" for target in targets) else {}
    try:
        import tiktoken
    except ImportError:
        sys.exit("run.py --budgets: needs tiktoken: pip install --require-hashes -r tests/perf/requirements.txt")
    found = token_values(tiktoken.get_encoding("o200k_base"))
    return {target["id"]: bench_value(target, results) if target["source"] == "bench" else found[target["id"]]
            for target in targets}


def shown(value, unit):
    """A value as BENCHMARKS.md writes it."""
    if value is None:
        return "not measured"
    if unit == "bytes":
        return f"{value / 1e6:,.1f} MB"
    if unit == "percent":
        return f"{value:.1f}%"
    if unit == "tokens":
        return f"{value:,.0f}"
    return f"{value:,.3g} ms" if value < 100 else f"{value:,.0f} ms"


def met(value, target):
    """Whether a value meets its target, compared as it is shown (a percentage to one decimal, the 0% of idle)."""
    if value is None:
        return None
    return (round(value, 1) if target["unit"] == "percent" else value) <= target["limit"]


def threshold(target):
    """A budget's CI threshold, as BENCHMARKS.md writes it."""
    value = target["ciThreshold"]
    if value is None:
        return "not enforced"
    return value if isinstance(value, str) else f"{value:,} {'tokens' if target['source'] == 'tokens' else 'instructions'}"


def rows(budgets, values, recorded=False):
    """One row per budget: the budget, its target, its value, met or MISSED, from where, its CI threshold. With
    recorded, the values written in budgets.json."""
    lines = []
    for target in budgets["targets"]:
        if recorded:
            entry = target["measured"] or {}
            value, source = entry.get("value"), entry.get("from")
        else:
            value, source = values[target["id"]]
        verdict = {True: "met", False: "**missed**", None: "not measured"}[met(value, target)]
        lines.append((target["budget"], shown(target["limit"], target["unit"]), shown(value, target["unit"]),
                      verdict, source or "", threshold(target)))
    return lines


def markdown(budgets, values, recorded=False):
    """The tables of BENCHMARKS.md: speed and memory, then tokens, then where and when they were measured."""
    heading = "| Budget | Target | Measured | | Worst case | CI fails above |\n|---|---:|---:|---|---|---|\n"
    tables = []
    for source, title in (("bench", "Speed and memory"), ("tokens", "Tokens (o200k_base)")):
        chosen = {"targets": [target for target in budgets["targets"] if target["source"] == source]}
        body = "".join("| " + " | ".join(str(cell) for cell in row) + " |\n" for row in rows(chosen, values, recorded))
        tables.append(f"### {title}\n\n{heading}{body}")
    return "\n".join(tables)


def record(budgets, values):
    """Writes each value into budgets.json's "measured", with the machine, the commit and the date."""
    where, which, when = machine(), commit(), datetime.date.today().isoformat()
    for target in budgets["targets"]:
        value, source = values[target["id"]]
        if value is not None:
            rounded = round(value, 3) if isinstance(value, float) else value
            target["measured"] = {"value": rounded, "from": source, "machine": where, "commit": which, "date": when}
    with open(BUDGETS, "w", encoding="utf-8") as file:
        file.write(json.dumps(budgets, indent=2) + "\n")


def check(budgets, folder):
    """CI: each budget with a numeric threshold against what CI measured in a folder (callgrind.jsonl's instructions
    per operation; the tokens counted now). The failures, as lines."""
    with open(os.path.join(folder, "callgrind.jsonl"), encoding="utf-8") as file:
        counted = {result["name"]: result for result in map(json.loads, filter(str.strip, file))}
    tokens = None
    failures = []
    for target in budgets["targets"]:
        if not isinstance(target["ciThreshold"], (int, float)):
            continue
        if target["source"] == "tokens":
            if tokens is None:
                import tiktoken
                tokens = token_values(tiktoken.get_encoding("o200k_base"))
            value, source = tokens[target["id"]]
        else:
            value, source = bench_value({**target, "field": "value", "unit": "instructions"}, counted)
        if value is None:
            failures.append(f"{target['id']}: not measured in {folder}")
        elif value > target["ciThreshold"]:
            failures.append(f"{target['id']}: {value:,.0f} ({source}) above its threshold {target['ciThreshold']:,}")
        else:
            print(f"budget {target['id']}: {value:,.0f} ({source}), threshold {target['ciThreshold']:,}")
    return failures
