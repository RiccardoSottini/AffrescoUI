# Benchmarks

Measures what a change costs in speed, memory and tokens, the same way on a laptop and in CI.

- `affrescoui_bench` runs the workloads below through the library's public API. It prints one JSON object per
  workload.
- `tokens.py` counts the tokens of what agents read and write.
- In CI, the `Benchmarks (callgrind)` job of `.github/workflows/ci.yml` runs both on a pull request's base and head, and puts the tables in the job
  summary. It fails when a workload's instruction count grows by more than 3%.
- `scaling.py` measures how each area's cost grows with its n, and fails when it grows faster than the baseline's
  ("Scaling", below).

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
| `fuzzy/100000` | A quick pick's filter: `fuzzySort` over 100,000 path-like texts, keeping the best 50. Roadmap item 8.3 asks for 10 ms or less |
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
| `layout/5000` | The same with 770 cards: 5,066 panels, more than the 5,000 that roadmap item 4.5 sets 4 ms of layout for |
| `layout/5000/change` | On that screen, one card's title changes, then the layout: only what the change moves is laid out again |
| `frame/<cards>` | Laying out and drawing a full frame of the Kanban template with 14 or 400 cards (`Window::invalidate()`: every panel laid out, recorded and drawn again). This is the main thread's CPU time; the GPU's work isn't counted |
| `frame/blink/400` | On the 400-card board, the text field's caret appears or disappears (its focus changes, as a blink would), then a frame: only that field is recorded and drawn again. The main thread's CPU time, presenting the frame included |
| `frame/blink/400/drawn` | The same, without presenting the frame (`detail::renderWithoutPresenting`): what the frame costs the library, the windowing system's own part left out. Roadmap item 7.8 asks for 0.1 ms or less |
| `views/tree-expand-100k` | A folder of 100,000 items opened or closed in a tree (each run does both): its rows rebuilt, none drawn |
| `views/list-scroll-100k` | A list of 100,000 rows scrolled to a new place, then a frame: only the rows in view are drawn |
| `views/table-sort-100k` | A table of 100,000 rows sorted by a column of numbers, descending or ascending (each run does both) |
| `views/table-scroll-100k` | That table scrolled to a new place, then a frame |

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

## Scaling

A 3% threshold catches a change that makes one size slower. It misses a change that makes the cost grow faster with
the size of what is worked on, which small templates hide until a large app meets it. `scaling.py` measures that:

```sh
pip install --require-hashes -r tests/perf/requirements.txt   # tiktoken
python3 tests/perf/scaling.py                                 # wall time, allocations, memory, tokens
python3 tests/perf/scaling.py --callgrind                     # and instructions (Linux, valgrind)
python3 tests/perf/scaling.py --size large                    # the nightly sizes
```

- `affrescoui_bench --scaling small` runs each area at 5 sizes that double; `--scaling large` at 7. The workloads are
  named `scaling/<area>/<n>` (`scaling_screens.cpp` needs a window, `scaling_engine.cpp` doesn't). `scaling.py` adds
  the agent areas, measured with the MCP server's own functions.
- For each area and metric, the least-squares line through (log n, log cost) gives the exponent k of cost ~ n^k. The
  metrics are instructions (callgrind, all but the frames), time (wall clock; CPU time for the frames), allocations,
  retained memory and tokens. A metric with a value of 0 (e.g. nothing kept) has no exponent.
- `scaling-baseline.json` gives each area its n and its target class (`O(1)`, `O(log n)`, `O(n)`, `O(n log n)`,
  `O(n^2)`), and holds the exponents recorded on Linux with callgrind, per size set (`--record` rewrites them and keeps
  the targets).
- The gate judges the metrics that barely move between runs: instructions, allocations, retained memory and tokens.
  It fails when an exponent is more than 0.15 above the baseline's, or when an area has no target. The `perf-change`
  label (`--allow`) reports a rise without failing; a missing target still fails. Time is reported only.
- An area meets its target when its first metric of instructions, tokens and time has a k at most 0.15 above the
  class's (0, 1 or 2), or 0.3 above it for the classes with a log. Missing it is reported, not failed: the baseline records where the library is today.
- Pull requests run the small sizes (`ci.sh`); the large ones run nightly (`.github/workflows/nightly.yml`). The job
  summary has a table: each area's n, n range, exponent, target and whether it is met, then every metric's exponent.

The areas:

| Area | n | Target |
|---|---|---|
| `update` | Kanban cards | O(1): one state change, its reconciliation and layout |
| `layout`, `frame` | Kanban cards | O(n): a full layout after a resize; a full frame |
| `frame/damage` | Kanban cards | O(1): a frame that redraws one changed region |
| `load`, `build` | Kanban cards | O(n): parse, validate and build; build only |
| `revision` | Kanban cards | O(1): the same small patch to a growing document |
| `checker` | Kanban cards | O(n): the checker's findings and validation |
| `history/commit` | commits | O(1): a commit, and the memory it keeps |
| `history/restore`, `revert`, `diff` | commits | O(1) |
| `history/blame`, `history/load` | commits | O(n): blame searches the whole history; loading reads it |
| `expressions/list`, `sort` | list items | O(n): filter, map and sum; O(n log n): a sort |
| `text/measure`, `wrap`, `shape` | characters | O(n): Latin, Greek and Cyrillic words with icons |
| `editor/insert`, `delete`, `caret` | lines | O(1): one key in an editor of n lines |
| `peer` | strings in a message | O(n): a peer's revision parsed, checked and committed |
| `agent/outline`, `agent/find` | nodes | O(n): the outline's tokens; `find_nodes`' time |
| `agent/reread` | nodes | O(1): the tokens of a re-read after one commit |
| `tokens/document` | nodes | O(n): the minified document (O(1) tokens per node) |

And the adversarial shapes: `adversarial/fonts` (characters that alternate between a font and its fallback),
`adversarial/deep` (panels nested n deep), `adversarial/line` (one word of n characters, no place to break),
`adversarial/siblings` (a stack of n labels) and `adversarial/keys` (one change in a state of n keys).

A new area needs its target in `scaling-baseline.json`, then `--record` on Linux with `--callgrind` for both size
sets.

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

## Gains locked in

A 3% gate compares the head with its base, so a gain lost over many small steps would pass. `locks.json` holds the
gains that must stay: each lock says a workload costs at most `atMost` times another workload of the same run, in the
same unit. `compare.py` checks them on the head's results and fails the job like a growth in instructions (the
`perf-change` label lets it pass too).

| Lock | At most | The gain |
|---|---:|---|
| `layout/5000/change` / `layout/5000` | 0.05 | incremental layout: one change lays out only what it moves |
| `frame/blink/400/drawn` / `frame/400` | 0.5 | damage regions: a blinking caret redraws only its region |
| `frame/blink/400` / `frame/400` | 0.8 | an unchanged frame reuses the cached draw list |

When set, the ratios were 0.007, 0.21 and 0.46; the limits leave room for noisy runners. The compiled screens and the
expression cache are held by the scaling gate instead (`update`, `revision` and `expressions/*`, above). Where layout,
images, icons and partial redraws put each node is held by the scenario suites in `tests/render/screens/` (ctest
`scenarios`), so a faster path can't change what the tree holds.
