# Quality gates

Every pull request shows, and CI enforces, that it doesn't make the C++ worse. The gates look only at what the
change touches, so existing code never blocks a pull request; the reports measure the whole repository, so later
clean-ups can pick their files.

| What | Checks | In CI |
|---|---|---|
| **Gate: clang-tidy** | No finding of [`.clang-tidy`](../../.clang-tidy) in a line the change adds or modifies | `Quality gate` fails |
| **Gate: function size** | No function the change touches is over 80 lines or complexity 15, unless it was already over and didn't grow | `Quality gate` fails |
| **Report: coverage** | Line, function and branch coverage of the unit tests, per module | Job summary, `coverage` artifact |
| **Report: whole repository** | The 20 longest and 20 most complex functions, tidy findings per module, check and file | Job summary, `quality-report` artifact |

The workflow is [`.github/workflows/quality.yml`](../../.github/workflows/quality.yml). It runs on every pull request
and on every push to `master`.

## The gates

**clang-tidy.** The gate runs clang-tidy on every C++ file the change touches (headers too) and keeps only the
findings on changed lines, like `clang-tidy-diff.py`. A finding on a line you touched counts, even if the line had it
before: fix it while you are there. A changed file that clang-tidy can't parse fails the gate, because otherwise it
would pass unchecked.

The checks are the families `bugprone-*`, `performance-*`, `cppcoreguidelines-*`, `modernize-*`, `readability-*` and
`misc-*`, with the checks that don't fit this codebase turned off, each with its reason in `.clang-tidy`. Formatting
stays with `.clang-format`, and what the compiler already warns about (`AFFRESCOUI_WARNINGS` in `CMakeLists.txt`)
isn't repeated.

**Function size.** [lizard](https://github.com/terryyin/lizard) measures every function in the files the change
touches, before and after:
- **length**: the lines from the function's first line to its closing brace, comments and blank lines included;
- **cyclomatic complexity**: one, plus one for each `if`, loop, `case`, `catch`, `&&`, `||` and `?:`.

A function the change touches fails if it is over a limit and either is new, or got longer or more complex on that
metric. A function that was already over and didn't get worse is listed, but passes: editing old code isn't blocked,
and making it worse is. A function whose parameters changed is still matched by its name, when the name is unique in
the file before and after the change; a new overload of an old function counts as new.

The limits are in [`thresholds.json`](thresholds.json): `function` is enforced, `aim` (60 lines, complexity 12, from
the project's aims) is reported. Thresholds tighten over time, when the maintainer decides: lower the numbers in
`thresholds.json` in a pull request of their own.

## The reports

**Coverage** comes from clang's source-based coverage of `affrescoui_unit_tests`, per module. A `.cpp` file that the
tests never call isn't linked into the test binary, so it isn't measured at all: the table counts the measured files
of each module next to the percentages. Files written in the build directory under `coverage/`:
- `coverage.json`: totals, per module (with the measured and unmeasured files) and per file; the form a coverage
  baseline compares with;
- `coverage.lcov`: the same data line by line, for any lcov tool;
- `html/`: the annotated sources.

**The whole-repository report**, `quality-report.json`, has the commit, the tool versions, the thresholds, the
function counts over the limit and the aim, the 20 longest and the 20 most complex functions, and every tidy finding
with its counts by module, check and file.

## Running them locally

You need CMake, Ninja, Python 3.9 or newer, and an LLVM with clang-tidy.
- **macOS:** `brew install llvm ninja`. Builds use Xcode's clang and its `llvm-cov`; clang-tidy comes from Homebrew.
- **Linux (Debian, Ubuntu):** `sudo apt install ninja-build clang-18 clang-tidy-18 llvm-18`, plus GLFW's X11 and
  Wayland headers (the list is in `quality.yml`).

```sh
tools/quality/run.sh gate             # what changed since HEAD left origin/master, uncommitted changes included
tools/quality/run.sh gate HEAD~3      # ... or since another commit
tools/quality/run.sh coverage         # builds and runs the unit tests with coverage
tools/quality/run.sh report           # the whole repository (with the coverage, if measured)
tools/quality/run.sh all              # all three
tools/quality/run.sh test             # the unit tests of these tools
```

The gate reads the changes from `git diff`, so a new file counts once it is added (`git add`).

Everything goes into `build-quality/` (or `$QUALITY_BUILD`): a Debug build with clang and coverage, its
`compile_commands.json`, and a Python virtual environment with lizard, installed from
[`requirements.txt`](requirements.txt) with `pip --require-hashes`. `LLVM_BIN` and `CLANG_TIDY` choose other tools,
and `QUALITY_JOBS` (default 4) how many clang-tidy runs go in parallel.

CI uses LLVM 18, the release Ubuntu 24.04 ships. A newer clang-tidy knows more checks, so it can find more locally
than CI does; CI's result is the one that counts.

## Files

| File | Does |
|---|---|
| `run.sh` | Finds the tools, configures the build, runs a command |
| `quality.py` | The commands: `gate`, `report`, `coverage` (Markdown, GitHub annotations, JSON) |
| `changes.py` | The lines each file changed since the base commit (`git diff -U0`) |
| `functions.py` | Function length and complexity with lizard; the size gate |
| `tidy.py` | Runs clang-tidy in parallel and collects its findings |
| `coverage.py` | Per-module coverage from `llvm-cov export` |
| `test_quality.py` | Unit tests of the above (`run.sh test`, in the `Quality gate` job) |
| `thresholds.json` | The limits, the aims, and the folders checked (`modules`, `examples`, `tests`) |
| `requirements.txt` | lizard and its dependencies, pinned by hash |
