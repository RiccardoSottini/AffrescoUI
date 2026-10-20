# Benchmarks

Measures what a change costs in speed, memory and tokens, the same way on a laptop and in CI.

- `affrescoui_bench` runs the workloads below through the library's public API. It prints one JSON object per
  workload.
- `tokens.py` counts the tokens of what agents read and write.
- In CI, `.github/workflows/perf.yml` runs both on a pull request's base and head, and puts the tables in the job
  summary. It fails when a workload's instruction count grows by more than 3%.

## Running them

```sh
cmake -S . -B build -G Ninja -DAFFRESCOUI_BUILD_BENCHMARKS=ON   # off by default
cmake --build build --target affrescoui_bench
python3 tests/perf/run.py                        # every workload, wall clock: about 5 s
python3 tests/perf/run.py --only update,frame    # the workloads whose names start with these
python3 tests/perf/run.py --skip load --runs 3   # leave some out; fewer runs
python3 tests/perf/run.py --callgrind --skip frame   # instruction counts (Linux, valgrind installed before the build)

pip install --require-hashes -r tests/perf/requirements.txt   # tiktoken, for the token counts only
python3 tests/perf/tokens.py --table
```

`run.py` only passes its options to `./build/affrescoui_bench`, which you can also run yourself. The workloads that need
a window open hidden ones, so a display is needed, like the rendering check (`xvfb-run` on a Linux server).

Compare two checkouts as CI does: write each one's results to its own directory, then compare them.

```sh
python3 tests/perf/run.py --out out/before/bench.jsonl        # on the base
python3 tests/perf/run.py --out out/after/bench.jsonl         # on your change
python3 tests/perf/compare.py --base out/before --head out/after
```

On a laptop the wall clock changes by a few percent from run to run. Repeat a run before you trust a small change.

## Output

Each line is one workload:

```json
{"name":"update/400","value":24421.667,"unit":"us","runs":15,"operations":1,"allocations":747768.0,"retainedBytes":32.0}
```

| Field | Meaning |
|---|---|
| `name` | The workload. The part before the first `/` is its group |
| `value` | The median over `runs` measured runs, per operation, in `unit` |
| `unit` | `ns`, `us` or `ms` of wall clock. Frames use the main thread's CPU time. Under callgrind it is `instructions` |
| `runs` | How many measured runs. One warm-up run comes before them |
| `operations` | How many operations one run does. `value`, `allocations` and `retainedBytes` are per operation |
| `allocations` | Calls to `operator new` per operation (median) |
| `retainedBytes` | Bytes allocated during the run and still allocated after it, per operation. This is the memory a run keeps |

`allocations.cpp` replaces the global `operator new` to count the last two fields. Allocations through `malloc`, such
as the OpenGL driver's, aren't counted.

## The workloads

| Workload | What one operation is |
|---|---|
| `expressions` | One evaluation of an expression from `tests/unit/expressions.cpp`. Every expression that succeeds is used, except the two that test the 10,000-item limits. Each run evaluates the table 100 times. Evaluation reuses the compiled expression, as a screen does |
| `commits` | One small commit (a label's text) on the Kanban template's history, 1,000 per run. `retainedBytes` is the memory each commit keeps |
| `text` | Measuring and wrapping 10,000 lines, in Roboto at 15 px, to 240 px |
| `history/commit/<n>` | One small commit (a label's text) on a Kanban history of 100 or 10,000 commits, 100 per run: the same cost at both sizes, and `retainedBytes` is what each commit keeps. Each run adds to the history |
| `history/blame` | Blame of a node no commit changed, on the 10,000-commit history: it reads every commit back to revision 0 |
| `history/diff` | A diff of two commits on the 10,000-commit history, the farthest from their snapshot: the worst rebuild |
| `history/load/10000` | Opening a folder holding a history of 10,000 commits (written once, unmeasured, as one change set) |
| `load/<template>` | Parsing, validating and building a template into a window. The window is opened before the run. Each run keeps its screen, so `retainedBytes` is the memory of a loaded screen |
| `build/<template>` | Building a template into a window (`Screen::build`, with its checker warnings). The window is opened and the screen read, its fonts loaded, before the run. `retainedBytes` is the memory of the panels built and what the runtime keeps for them: divide it by the screen's panels for the memory per panel |
| `update/<cards>` | One state change on the Kanban template, filled to 14, 100 or 400 cards, followed by its reconciliation and layout. The change renames the board (`set state.board`). The board is filled by copying the template's own 12 cards into its four columns in turn, so even `update/14` has two copies |
| `revision/<template>/<edit>` | One canonical edit from `edits/<template>.json`, applied as a revision: the commit, the reconciliation and the layout. Each edit has its own screen. Between runs the edit is taken back, unmeasured. The history still grows (see below) |
| `layout/400` | Laying out the largest screen, the Kanban template with 400 cards, after a resize |
| `frame/<cards>` | Laying out and drawing a full frame of the Kanban template with 14 or 400 cards. This is the main thread's CPU time; the GPU's work isn't counted |

Every workload is deterministic. The inputs are fixed, and screens load without a state file. Channels find no host.

The `revision` runs are not all alike, though. Taking an edit back is itself a commit: an `undo()`, then a revision
that resets the state. So each run adds two or three commits to the screen's history, and every update rebuilds the
`history` global from the whole log. A later run costs more than an earlier one. For example,
`revision/sign-in/rename-label` makes 5,536 allocations as the first measured run, and the median of 15 runs makes
8,056. A screen can't be discarded and loaded again without leaving its window behind, so there is no fresh history
for each run yet. Until then:

- the callgrind count (one run after the warm-up) and the wall-clock median (15 runs) are taken at different history
  lengths, so don't compare them with each other;
- `--runs` changes these workloads' values. Compare runs made with the same options.

CI compares base and head the same way, so the gate isn't affected.

## Tokens

`tokens.py` counts, for each template in `examples/assets/screens/`:

- `document`: the minified document, as the MCP server's first `get_screen` sends it;
- `outline`: `get_screen` with `outline=true`;
- `edit/<name>`: each canonical edit in `edits/<template>.json`, as an agent writes it for `submit_revision`.

It also counts `SCREEN_FORMAT.md` (`format-guide`), the guide that agents load.

The document and the outline come from the MCP server's own functions (`compact`, `outline`), so a change to what the
server sends shows up here. The counter is tiktoken's `o200k_base`. It stands in for Claude's tokenizer, which isn't
public, so compare ratios, not totals.

The canonical edits are typical agent requests: change a colour, rename a label, add a column or a filter, restyle, add
a button with its action, move a card, remove a node, and add an item or a node to each template. The `revision`
workloads apply every one of them, so an edit that no longer applies fails the benchmark instead of being counted.

## Adding a workload

1. Choose a group. `screens.cpp` holds the groups that need a window: `load`, `update`, `revision`, `layout` and
   `frame`. `engine.cpp` holds the ones that don't: `expressions`, `commits` and `text`, and `history.cpp` holds
   `history`. A new family is a new function that returns its `Workload`s, listed in `screenGroups()` or
   `engineGroups()`. In a new file, it also needs its own line in `CMakeLists.txt`.
2. Do the setup in that function: load the screen, build the inputs. It isn't measured.
3. Fill in the `Workload`:
   - its `name`;
   - its `unit` (pick one that gives values above 1);
   - its `runs` (enough for a stable median in a few seconds);
   - its `operations` per step;
   - a `step`: the measured work, and nothing else;
   - optionally a `prepare`, which gets each run ready (for example, it takes the last run's change back) and isn't
     measured.
   Steps must leave the screen as they found it, or prepare must put it back, so every run measures the same thing.
   If that can only be partly done, as with `revision`'s history, write down what changes from run to run.
4. Use only the public API (`affresco/affresco.hpp`), so the program builds against the base commit too.
   `expressions` is the one exception: it uses `Expression.hpp`, as the unit tests do.
5. Run it locally, then with `--callgrind` on Linux. Check that two callgrind runs give the same count.

Canonical edits are entries in `edits/<template>.json`: a name, then `submit_revision`'s arguments.

## Why instruction counts gate CI

Shared CI runners are noisy. Their wall clock changes by 10% or more from run to run, depending on the machine and its
neighbours, so a 3% threshold on time would fail at random. Callgrind counts the instructions the CPU executes. For the
same program and inputs, the count barely moves between runs, so a small growth is a real one.

- Only the measured run is counted. `run.py` starts the program with callgrind's instrumentation off, and the program
  turns it on around each measured run, through valgrind's `callgrind.h`. Startup and OpenGL's software driver run
  uninstrumented, and much faster.
- Instructions aren't time. Cache misses and the GPU's work don't show. That's why the frames are timed, and their
  CPU time is reported without gating them: callgrind makes the software driver too slow to count.
- `ci.sh` builds and measures the base and the head on the same runner, with the same flags. If the base has no
  benchmarks, the head's numbers are reported alone.

The job fails if a workload's instructions grow by more than 3%. When a slowdown is intended, the maintainer adds the
`perf-change` label to the pull request. The tables still show the growth, and the job passes.
