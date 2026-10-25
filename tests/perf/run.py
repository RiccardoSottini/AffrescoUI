#!/usr/bin/env python3
"""Runs affrescoui_bench and writes its results, one JSON object per line (tests/perf/README.md).

    python3 tests/perf/run.py                          # wall clock, every workload
    python3 tests/perf/run.py --only update,frame      # some of them
    python3 tests/perf/run.py --callgrind --skip frame # instruction counts (Linux, valgrind installed)

With --callgrind, the program runs under callgrind with instrumentation off, and turns it on only for each workload's
measured run; each run's counts are dumped under the workload's name. The value becomes the instructions per
operation. Instruction counts barely change from run to run, so CI compares them instead of the wall clock.

With --budgets, it measures the budgets of BENCHMARKS.md instead (budgets.py, budgets.json): --markdown prints their
tables, --record writes the values into budgets.json, and --from DIR checks CI's measurements in DIR against the
thresholds, failing above one.
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import tempfile

os.environ.setdefault("AFFRESCOUI_HIDDEN", "1")  # its programs never show a window (CONTRIBUTING.md); =0 shows them

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def bench_arguments(args):
    """The options passed through to affrescoui_bench."""
    passed = []
    for name in ("only", "skip", "runs", "scaling"):
        value = getattr(args, name)
        if value is not None:
            passed += ["--" + name, str(value)]
    return passed


def read_results(text):
    """The JSON objects of the program's output, one per line, by name, in order."""
    results = {}
    for line in text.splitlines():
        if line.strip():
            result = json.loads(line)
            results[result["name"]] = result
    return results


def read_dumps(prefix):
    """Instructions per dump, by the name it was dumped under ("Client Request: <name>")."""
    counts = {}
    for path in glob.glob(prefix + "*"):
        name, total = None, None
        with open(path, encoding="utf-8") as dump:
            for line in dump:
                if line.startswith("desc: Trigger: Client Request:"):
                    name = line.split(":", 3)[3].strip()
                elif line.startswith(("summary:", "totals:")):
                    total = int(line.split()[1])
        if name is not None and total is not None:
            counts[name] = total
    return counts


def run_callgrind(bench, passed):
    """Runs the program under callgrind: its results, with the value replaced by instructions per operation."""
    with tempfile.TemporaryDirectory() as directory:
        prefix = os.path.join(directory, "callgrind.out")
        command = ["valgrind", "--tool=callgrind", "--instr-atstart=no", "--callgrind-out-file=" + prefix,
                   bench, "--callgrind"] + passed
        process = subprocess.run(command, stdout=subprocess.PIPE, text=True)
        results = read_results(process.stdout)
        counts = read_dumps(prefix)
    for name, result in results.items():
        if name not in counts:
            sys.exit(f"run.py: callgrind wrote no counts for {name}")
        result.update(value=round(counts[name] / result["operations"]), unit="instructions")
    return results, process.returncode


def run_budgets(args, bench):
    """--budgets: measures the budgets (or checks a folder of CI's measurements against them) and prints them."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import budgets
    loaded = budgets.load()
    if args.from_folder:
        failures = budgets.check(loaded, args.from_folder)
        for failure in failures:
            print("BUDGET EXCEEDED " + failure)
        sys.exit(1 if failures else 0)
    values = budgets.measure(loaded, bench)
    if args.record:
        budgets.record(loaded, values)
    if args.markdown:
        sys.stdout.write(budgets.markdown(loaded, values))
    else:
        for row in budgets.rows(loaded, values):
            print("  ".join(str(cell) for cell in row))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=os.path.join(ROOT, "build"), help="the build directory")
    parser.add_argument("--only", help="workloads whose names start with one of these (comma-separated)")
    parser.add_argument("--skip", help="leave out the workloads whose names start with one of these")
    parser.add_argument("--runs", type=int, help="measured runs for every workload, instead of each one's own")
    parser.add_argument("--scaling", choices=("small", "large"), help="the scaling workloads instead (scaling.py)")
    parser.add_argument("--callgrind", action="store_true", help="count instructions with callgrind")
    parser.add_argument("--out", help="write the results here instead of to stdout")
    parser.add_argument("--budgets", action="store_true", help="measure the budgets of BENCHMARKS.md (budgets.json)")
    parser.add_argument("--markdown", action="store_true", help="with --budgets: print BENCHMARKS.md's tables")
    parser.add_argument("--record", action="store_true", help="with --budgets: write the values into budgets.json")
    parser.add_argument("--from", dest="from_folder", help="with --budgets: check CI's measurements in this folder")
    args = parser.parse_args()

    bench = os.path.join(args.build, "affrescoui_bench")
    if args.budgets and args.from_folder:
        run_budgets(args, bench)
    if not os.path.exists(bench):
        sys.exit(f"run.py: no {bench}: configure with -DAFFRESCOUI_BUILD_BENCHMARKS=ON and build affrescoui_bench")
    if args.budgets:
        return run_budgets(args, bench)
    if args.callgrind:
        results, status = run_callgrind(bench, bench_arguments(args))
    else:
        process = subprocess.run([bench] + bench_arguments(args), stdout=subprocess.PIPE, text=True)
        results, status = read_results(process.stdout), process.returncode

    lines = "".join(json.dumps(result) + "\n" for result in results.values())
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as out:
            out.write(lines)
    else:
        sys.stdout.write(lines)
    if status != 0:
        sys.exit(f"run.py: affrescoui_bench failed (exit status {status})")


if __name__ == "__main__":
    main()
