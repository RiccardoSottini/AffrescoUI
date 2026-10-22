#!/usr/bin/env python3
"""How each area's cost grows with its n (tests/perf/README.md, "Scaling"). Runs affrescoui_bench --scaling, which
measures each area at 5 sizes that double (7 at night), measures what agents read with the MCP server's own functions,
fits a line to log(cost) against log(n), and checks each exponent k against the area's target and the baseline.

    pip install --require-hashes -r tests/perf/requirements.txt          # tiktoken, for the agent areas
    python3 tests/perf/scaling.py                                        # wall time, allocations, memory, tokens
    python3 tests/perf/scaling.py --callgrind                            # and instructions (Linux, valgrind)
    python3 tests/perf/scaling.py --callgrind --record                   # write today's exponents as the baseline
    python3 tests/perf/scaling.py --size large                           # the nightly sizes

The metrics: instructions (under callgrind; the frames, which callgrind makes very slow with software OpenGL, are not
counted), time (the wall clock, or the main thread's CPU time for the frames), allocations, retained memory and
tokens. Instructions, allocations, retained memory and tokens barely change from run to run, so the gate judges them:
it fails when an exponent is more than 0.15 above the baseline's (unless --allow: the perf-change label), or when an
area has no target in tests/perf/scaling-baseline.json. Time is reported only. Whether an area meets its target is
reported, not failed: the baseline records where the library is, the targets where it should be.
"""

import argparse
import json
import math
import os
import sys
import time

import run
import tokens

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BASELINE = os.path.join(HERE, "scaling-baseline.json")
TOLERANCE = 0.15
LIMITS = {"O(1)": 0.15, "O(log n)": 0.3, "O(n)": 1.15, "O(n log n)": 1.3, "O(n^2)": 2.15}  # largest k per target
GATED = ("instructions", "allocations", "retained", "tokens")
METRICS = ("instructions", "time", "allocations", "retained", "tokens")


def split_name(name):
    """A scaling workload's area and n: "scaling/text/shape/256" is ("text/shape", 256)."""
    area, _, size = name.removeprefix("scaling/").rpartition("/")
    return area, int(size)


def collect(results, measured, metric_of_value):
    """Adds a run's results to measured[area][metric][n]: the value as metric_of_value(result) names it (None leaves
    it out), allocations and retained memory per operation."""
    for name, result in results.items():
        area, size = split_name(name)
        metrics = measured.setdefault(area, {})
        value_metric = metric_of_value(result)
        if value_metric:
            metrics.setdefault(value_metric, {})[size] = result["value"]
        metrics.setdefault("allocations", {})[size] = result["allocations"]
        metrics.setdefault("retained", {})[size] = result["retainedBytes"]


def run_bench(bench, size, callgrind):
    """Every scaling workload of the program: the wall clock (CPU time for the frames), and with callgrind the
    instructions of all but the frames, whose allocations and memory then come from the same run."""
    measured = {}
    passed = ["--scaling", size]
    if callgrind:
        results, status = run.run_callgrind(bench, passed + ["--skip", "scaling/frame"])
        if status != 0:
            sys.exit(f"scaling.py: affrescoui_bench failed under callgrind (exit status {status})")
        collect(results, measured, lambda result: "instructions")
    process = run.subprocess.run([bench] + passed, stdout=run.subprocess.PIPE, text=True)
    if process.returncode != 0:
        sys.exit(f"scaling.py: affrescoui_bench failed (exit status {process.returncode})")
    for name, result in run.read_results(process.stdout).items():
        area, size_n = split_name(name)
        metrics = measured.setdefault(area, {})
        metrics.setdefault("time", {})[size_n] = result["value"]
        if not callgrind or area.startswith("frame"):
            metrics.setdefault("allocations", {})[size_n] = result["allocations"]
            metrics.setdefault("retained", {})[size_n] = result["retainedBytes"]
    return measured


def node_list(count):
    """A screen of n labels, each in its own row with an id, the shape agents read in large screens."""
    rows = [{"type": "row", "id": f"row-{i}", "children": [{"type": "label", "id": f"item-{i}", "text": f"Item {i}"}]}
            for i in range(count)]
    return {"format": "affrescoui-screen", "version": 2, "root": {"type": "stack", "id": "list", "children": rows}}


def best_time(call, repeats=5):
    """The fastest of a few calls, in microseconds."""
    fastest = math.inf
    for _ in range(repeats):
        start = time.perf_counter()
        call()
        fastest = min(fastest, time.perf_counter() - start)
    return fastest * 1e6


def agent_areas(sizes, encoding):
    """What agents read, at screens of n nodes (n labels in n rows): the outline's tokens, find_nodes' time, the
    re-read after one commit (get_screen's changes) and the whole minified document, in tokens."""
    mcp = tokens.load_mcp(ROOT)
    measured = {"agent/outline": {}, "agent/find": {}, "agent/reread": {}, "tokens/document": {}}

    def count(text):
        return len(encoding.encode(text, disallowed_special=()))

    for size in sizes:
        nodes = 2 * size + 1
        document = node_list(size)
        rename = {"set": {"#item-0/text": "Renamed"}}
        measured["agent/outline"].setdefault("tokens", {})[nodes] = count(mcp.outline(document))
        measured["agent/outline"].setdefault("time", {})[nodes] = best_time(lambda: mcp.outline(document))
        measured["agent/find"].setdefault("time", {})[nodes] = best_time(
            lambda: mcp.find_nodes(document, text=f"Item {size - 1}"))
        changes = tokens.reread(mcp, document, rename)
        measured["agent/reread"].setdefault("tokens", {})[nodes] = count(mcp.compact({"changes": changes}))
        measured["tokens/document"].setdefault("tokens", {})[nodes] = count(mcp.compact(document))
    return measured


def exponent(points):
    """The slope of the least-squares line through (log n, log value): the k of cost ~ n^k. None when a value is not
    above zero (e.g. no memory kept), as a logarithm needs."""
    if len(points) < 2 or any(value <= 0 for value in points.values()):
        return None
    xs = [math.log(size) for size in points]
    ys = [math.log(value) for value in points.values()]
    mean_x, mean_y = sum(xs) / len(xs), sum(ys) / len(ys)
    spread = sum((x - mean_x) ** 2 for x in xs)
    return round(sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / spread, 2)


def fit(measured):
    """Each area's n range and exponent per metric."""
    fitted = {}
    for area, metrics in sorted(measured.items()):
        sizes = sorted({size for points in metrics.values() for size in points})
        fitted[area] = {"sizes": [sizes[0], sizes[-1]],
                        "exponents": {metric: exponent(metrics[metric]) for metric in METRICS if metric in metrics}}
    return fitted


def primary(exponents):
    """The metric an area's target is judged on: instructions, else tokens, else time."""
    for metric in ("instructions", "tokens", "time"):
        if exponents.get(metric) is not None:
            return metric
    return None


def judge(fitted, baseline, size):
    """The rows of the report and the failures: an area with no target, or a gated exponent more than TOLERANCE above
    the baseline's."""
    rows, failures = [], []
    areas = baseline.get("areas", {})
    recorded = baseline.get("exponents", {}).get(size, {})
    for area, found in fitted.items():
        target = areas.get(area, {}).get("target")
        if target not in LIMITS:
            failures.append(f"{area}: no target in {os.path.relpath(BASELINE, ROOT)}")
        metric = primary(found["exponents"])
        k = found["exponents"].get(metric) if metric else None
        met = "n/a" if k is None or target not in LIMITS else ("yes" if k <= LIMITS[target] else "**no**")
        for name in GATED:
            now, before = found["exponents"].get(name), recorded.get(area, {}).get(name)
            if now is not None and before is not None and now > before + TOLERANCE:
                failures.append(f"{area}: the {name} exponent is {now}, the baseline's {before} (+{TOLERANCE} at most)")
        rows.append((area, areas.get(area, {}).get("n", "?"), found, metric, k, target or "none", met))
    return rows, failures


def table(rows, size):
    """The job summary: one row per area."""
    lines = [f"### Scaling ({size} sizes)", "",
             "| Area | n | n range | k | Metric | Target | Met | k per metric (instr. / time / alloc. / memory / tokens) |",
             "|---|---|---|---:|---|---|---|---|"]
    for area, n, found, metric, k, target, met in rows:
        every = " / ".join("-" if found["exponents"].get(name) is None else str(found["exponents"][name])
                           for name in METRICS)
        span = f"{found['sizes'][0]:,}–{found['sizes'][1]:,}"
        lines.append(f"| {area} | {n} | {span} | {'-' if k is None else k} | {metric or '-'} | {target} | {met} | "
                     f"{every} |")
    missed = [row[0] for row in rows if row[6] == "**no**"]
    lines += ["", f"Areas that miss their target: {', '.join(missed) if missed else 'none'}.", ""]
    return "\n".join(lines)


def record(fitted, baseline, size):
    """The baseline with this run's gated exponents for these sizes; the areas and their targets stay as written."""
    exponents = {area: {name: found["exponents"][name] for name in GATED if found["exponents"].get(name) is not None}
                 for area, found in fitted.items()}
    baseline.setdefault("exponents", {})[size] = exponents
    with open(BASELINE, "w", encoding="utf-8") as out:
        json.dump(baseline, out, indent=2, ensure_ascii=False)
        out.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=os.path.join(ROOT, "build"), help="the build directory")
    parser.add_argument("--size", choices=("small", "large"), default="small", help="small: pull requests; large: night")
    parser.add_argument("--callgrind", action="store_true", help="count instructions with callgrind")
    parser.add_argument("--record", action="store_true", help="write the exponents to the baseline instead of judging")
    parser.add_argument("--allow", action="store_true", help="report exponents above the baseline without failing")
    parser.add_argument("--out", help="also write the fitted exponents here, as JSON")
    parser.add_argument("--summary", help="also append the report to this file (e.g. $GITHUB_STEP_SUMMARY)")
    args = parser.parse_args()

    bench = os.path.join(args.build, "affrescoui_bench")
    if not os.path.exists(bench):
        sys.exit(f"scaling.py: no {bench}: configure with -DAFFRESCOUI_BUILD_BENCHMARKS=ON and build affrescoui_bench")
    try:
        import tiktoken
    except ImportError:
        sys.exit("scaling.py: needs tiktoken: pip install -r tests/perf/requirements.txt")
    measured = run_bench(bench, args.size, args.callgrind)
    sizes = [64 * 2 ** i for i in range(7 if args.size == "large" else 5)]
    measured.update(agent_areas(sizes, tiktoken.get_encoding("o200k_base")))
    fitted = fit(measured)
    with open(BASELINE, encoding="utf-8") as file:
        baseline = json.load(file)

    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as out:
            json.dump(fitted, out, indent=2)
    if args.record:
        record(fitted, baseline, args.size)
    rows, failures = judge(fitted, baseline, args.size)
    report = table(rows, args.size)
    sys.stdout.write(report)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as summary:
            summary.write(report + "\n")
    for failure in failures:
        print(f"::error title=Scaling::{failure}")
    if failures and not (args.allow and all("no target" not in failure for failure in failures)):
        sys.exit(f"scaling.py: {len(failures)} failure(s)")


if __name__ == "__main__":
    main()
