# Benchmarks

What AffrescoUI promises in speed, memory and tokens, how each promise is measured, and what was measured. A target
missed is written as missed: the targets don't move, the code does.

## Method

- **Where.** Every number below was measured on an Apple M4 Pro, macOS 27.0.1, on 2026-10-22: the budgets at commit
  `1d60d3f`, the agent benchmark at commit `8b367ca` (the same code; that commit adds only the recorded budgets). The
  picks ("What a pick saves") and the latest oracle runs were measured on the same machine on 2026-11-15, at commit
  `6f3b7584`. Other machines give other numbers.
- **Build.** Release (`-O3`), the benchmark alone:
  `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DAFFRESCOUI_BUILD_BENCHMARKS=ON -DAFFRESCOUI_BUILD_TESTS=OFF -DAFFRESCOUI_BUILD_EXAMPLES=OFF`.
- **Workloads.** `affrescoui_bench` (`tests/perf`, described in `tests/perf/README.md`): each workload runs once to
  warm up, then `runs` times, and reports the median per operation, in wall-clock time (frames: the main thread's CPU
  time). Typing and scrolling time each operation and report the 99th percentile. The memory budgets run each in a
  process of its own and read the whole process's memory after the last run (its footprint on macOS, its resident set
  on Linux). Idle CPU is every thread's CPU time over one second of a template's event loop, with nothing
  happening, for every template, in a process of its own.
- **Worst case.** A budget that covers several workloads (every template's revisions, two sizes of state change) is
  the slowest of them, named in the table.
- **Tokens.** Counted with tiktoken's `o200k_base` on what the MCP server sends or receives (`tests/perf/tokens.py`):
  outlines, re-reads after one commit, the format guide's topics, the canonical edits of `tests/perf/edits`, the
  reference revisions of the agent benchmark's feature tasks, each desktop kit component's use (`tests/perf/kit.json`),
  and what an agent reads first when a person points at a node (`tests/perf/picks.json`, read with `affrescoui source`).
- **Measuring and recording.**

  ```sh
  python3 tests/perf/run.py --build build-release --budgets              # measures, prints a table
  python3 tests/perf/run.py --build build-release --budgets --record     # and writes the values into budgets.json
  python3 tests/perf/run.py --build build-release --budgets --markdown   # the tables below
  ```

  The targets, the values recorded (with machine, commit and date) and the CI thresholds are in
  `tests/perf/budgets.json`.
- **In CI.** The job "Benchmarks (callgrind)" counts instructions under callgrind on Linux, where wall clock is too
  noisy to compare, and fails when a budget is above its threshold: tokens for the token budgets, instructions per
  operation for the others. A threshold is `pending` until a Linux callgrind run sets it; wall-clock and percentile
  budgets (idle, frame, typing, scrolling) are not enforced there, with the reason in `budgets.json`. A missed token
  budget's threshold is the value it was recorded at, so it can only get better. `budgets.py --propose-thresholds
  --from <folder>` turns a Linux callgrind run into thresholds for the pending budgets that are met.
- **Idle in CI.** Idle is not enforced as a time: an idle second is a wall-clock measure, and a loaded runner would
  decide it. It is enforced as counts by `tests/unit/window/idle.cpp`: in an idle second of every template, no update,
  no frame, and no wake of the event loop but the template's own timers (3 at most) and the quit.
- **Gains held.** `tests/perf/locks.json` holds the ratios that keep the cost of a change flat as the screen grows
  (`update/400` against `update/14`, and so on); CI checks them on every run, beside the 3% gate.

## Budgets

### Speed and memory

| Budget | Target | Measured | | Worst case | CI fails above |
|---|---:|---:|---|---|---|
| Idle CPU: 0% | 0.0% | 1.2% | **missed** | budgets/idle | not enforced |
| A full frame: 1 ms at most | 1 ms | 0.438 ms | met | frame/14 | not enforced |
| One state change: 1 ms at most at any size, a 50,000-panel screen included | 1 ms | 3.09 ms | **missed** | update/400 | pending |
| A revision applied and reconciled: 2 ms at most | 2 ms | 3.23 ms | **missed** | revision/kanban/add-button | pending |
| Typing in a 100,000-line file: 8 ms at most from key to frame, at p99 | 8 ms | 3.37 ms | met | editor/type-100k | not enforced |
| Scrolling a 1,000,000-line file at 120 Hz (8.33 ms a frame at p99) | 8.33 ms | 2.05 ms | met | editor/scroll-1m | not enforced |
| Opening a 50 MB file: 1 s at most | 1,000 ms | 246 ms | met | budgets/open-50mb | pending |
| Opening a 50 MB file: memory at most 1.5 times the file plus 50 MB | 128.6 MB | 213.4 MB | **missed** | budgets/open-50mb | pending |
| Forge cold start to interactive: 300 ms at most | 300 ms | 36.4 ms | met | budgets/forge-cold-start | pending |
| Forge with 10 tabs and a terminal: 150 MB at most | 150.0 MB | 188.8 MB | **missed** | budgets/forge-10-tabs | pending |
| Quick open over 100,000 files: 30 ms at most per keystroke | 30 ms | 2.5 ms | met | fuzzy/100000 | pending |
| Expanding a 100,000-node tree: 4 ms at most | 4 ms | 0.163 ms | met | views/tree-expand-100k | pending |

### Tokens (o200k_base)

| Budget | Target | Measured | | Worst case | CI fails above |
|---|---:|---:|---|---|---|
| A template's outline: 600 tokens at most | 600 | 1,272 | **missed** | studio | 1,272 tokens |
| Forge's outline: 4,000 tokens at most | 4,000 | 2,169 | met | forge | 4,000 tokens |
| A typical edit: 40 output tokens at most (the median canonical edit) | 40 | 24 | met | median of 35 canonical edits | 40 tokens |
| A re-read after one commit: 150 tokens at most | 150 | 29 | met | forge | 150 tokens |
| The format guide: 1,500 tokens at most per topic | 1,500 | 1,669 | **missed** | Top level | 1,669 tokens |
| A whole feature request: 2,000 output tokens at most (the oracle's feature tasks) | 2,000 | 174 | met | forge-focus | 2,000 tokens |
| What an agent reads first for a request that points: 1,500 tokens at most | 1,500 | 2,603 | **missed** | sinopia-json thumbnails | not enforced |
| A desktop kit component's use: 120 output tokens at most (tests/perf/kit.json) | 120 | 75 | met | toolbar | 120 tokens |


### The misses

- **Idle CPU** is 1.2% of a core, not 0%: some 12 ms of CPU time in an idle second, across every thread.
- **A state change** takes 3.09 ms at its worst (`update/400`), against 1 ms.
- **A revision** takes 3.23 ms at its worst (`revision/kanban/add-button`), against 2 ms. Seven canonical edits of
  `tests/perf/edits` no longer apply to the templates and were not measured: the three `change-colour` edits that set
  `/colors/...` (the templates now have `tokens`), `kanban/add-filter`, `music-player/add-playlist`, and both
  `sign-in` edits, which the sign-in screen's permissions reject.
- **Opening a 50 MB file** holds 213.4 MB, against 128.6 MB (1.5 times the file plus 50 MB). An earlier run of the same
  build measured 168.2 MB: this number varies from run to run, and both miss.
- **Forge with 10 tabs and a terminal** holds 188.8 MB, against 150 MB.
- **A template's outline** is 1,272 tokens at its largest (the Studio), against 600.
- **The format guide's** largest topic ("Top level") is 1,669 tokens, against 1,500.
- **What an agent reads first for a request that points** is 2,603 tokens at its largest (`#thumbnails` in Sinopia's
  JSON notation), against 1,500. A JSON pick holds the node's whole subtree, and `#thumbnails` is a container of 346
  lines; the same node in the Lisp notation is a definition that calls named views, 383 tokens. The other 51 picks are
  within the budget, the next largest 858 tokens.

### What a pick saves

When a person points at a node and asks, the agent reads the pick first: the pick's line (the node, its type, the
repeats it is in) and where the node is written (`getSource`). Without a pick it reads what it would to find the node:
the outline and the format guide's top level. Two nodes per example, in its Lisp and its JSON notation (Calamaio has
only Lisp, so it is left out), counted with `o200k_base` by `python3 tests/perf/tokens.py --table`. The first read
without a pick is the JSON notation's for both rows, as an outline is the same either way.

Over the 52 picks the ratio (with a pick over without) has a median of 0.054, at the smallest 0.015 (`#chat-input` in
Arcadia's JSON) and at the largest 0.312 (`#thumbnails` in Sinopia's JSON). It is a measure, not a target.

| Example | Pointed at | With a pick | First read without | Ratio |
|---|---|---:|---:|---:|
| arcadia-lisp | #app-body | 858 | 14,727 | 0.058 |
| arcadia-lisp | #chat-input | 446 | 14,727 | 0.030 |
| arcadia-json | #app-body | 462 | 14,727 | 0.031 |
| arcadia-json | #chat-input | 226 | 14,727 | 0.015 |
| code-editor-lisp | #openFile | 420 | 1,858 | 0.226 |
| code-editor-lisp | #command | 309 | 1,858 | 0.166 |
| code-editor-json | #openFile | 300 | 1,858 | 0.162 |
| code-editor-json | #command | 175 | 1,858 | 0.094 |
| flow-lisp | #cardTitle | 216 | 6,054 | 0.036 |
| flow-lisp | #checkText | 330 | 6,054 | 0.054 |
| flow-json | #cardTitle | 211 | 6,054 | 0.035 |
| flow-json | #checkText | 168 | 6,054 | 0.028 |
| forge-lisp | #editor-split | 316 | 6,096 | 0.052 |
| forge-lisp | #explorer | 271 | 6,096 | 0.044 |
| forge-json | #editor-split | 209 | 6,096 | 0.034 |
| forge-json | #explorer | 340 | 6,096 | 0.056 |
| haven-lisp | #label-light-name | 531 | 9,146 | 0.058 |
| haven-lisp | #alarmCode | 272 | 9,146 | 0.030 |
| haven-json | #label-light-name | 147 | 9,146 | 0.016 |
| haven-json | #alarmCode | 193 | 9,146 | 0.021 |
| launcher-lisp | #keyCombo | 398 | 1,893 | 0.210 |
| launcher-lisp | #cardName | 491 | 1,893 | 0.259 |
| launcher-json | #keyCombo | 177 | 1,893 | 0.093 |
| launcher-json | #cardName | 129 | 1,893 | 0.068 |
| modello-lisp | #right-areas | 299 | 5,540 | 0.054 |
| modello-lisp | #material-name | 525 | 5,540 | 0.095 |
| modello-json | #right-areas | 196 | 5,540 | 0.035 |
| modello-json | #material-name | 197 | 5,540 | 0.036 |
| orrery-lisp | #info-name | 392 | 2,076 | 0.189 |
| orrery-lisp | #sky-place | 220 | 2,076 | 0.106 |
| orrery-json | #info-name | 123 | 2,076 | 0.059 |
| orrery-json | #sky-place | 183 | 2,076 | 0.088 |
| sign-in-lisp | #shortcut-title | 325 | 4,572 | 0.071 |
| sign-in-lisp | #rule-length | 242 | 4,572 | 0.053 |
| sign-in-json | #shortcut-title | 194 | 4,572 | 0.042 |
| sign-in-json | #rule-length | 168 | 4,572 | 0.037 |
| sinopia-lisp | #thumbnails | 383 | 8,353 | 0.046 |
| sinopia-lisp | #outline | 243 | 8,353 | 0.029 |
| sinopia-json | #thumbnails | 2,603 | 8,353 | 0.312 |
| sinopia-json | #outline | 768 | 8,353 | 0.092 |
| streak-lisp | #label-cell | 327 | 3,725 | 0.088 |
| streak-lisp | #label-summary | 300 | 3,725 | 0.081 |
| streak-json | #label-cell | 288 | 3,725 | 0.077 |
| streak-json | #label-summary | 132 | 3,725 | 0.035 |
| tempera-lisp | #main | 695 | 6,217 | 0.112 |
| tempera-lisp | #adjustments | 306 | 6,217 | 0.049 |
| tempera-json | #main | 170 | 6,217 | 0.027 |
| tempera-json | #adjustments | 336 | 6,217 | 0.054 |
| tempo-lisp | #label-track-title | 514 | 3,385 | 0.152 |
| tempo-lisp | #collectionName | 505 | 3,385 | 0.149 |
| tempo-json | #label-track-title | 147 | 3,385 | 0.043 |
| tempo-json | #collectionName | 124 | 3,385 | 0.037 |

## Agents

The agent benchmark (`benchmarks/agents/README.md`): 62 tasks a person might ask of a running screen, at four levels
(21 edits, 26 features in a template, 5 features in Forge across its files, 10 uses of an app through its commands),
each given to a client that changes the screen, or runs the app's commands, through the MCP server, then checked by the
task's scenarios, its unchanged nodes and, where the request says how it is written, the definitions or actions it must
leave; a `use` task also by its runs, replayed on the starting screen, and by the screen staying as it was. The oracle submits each task's reference revisions: it checks
the tasks themselves, and CI runs it on every change. A model has been run once, on the tasks that compare Lisp with
JSON (below), not on the others.

| Date | Client | Model | Versions | Machine | Success | Median tokens in / out | Median tool calls |
|---|---|---|---|---|---:|---:|---:|
| 2026-10-22 | oracle | none | oracle 1, AffrescoUI 8b367ca | Apple M4 Pro, macOS 27.0.1 | 24/24 (edit 11/11, feature 8/8, forge 5/5) | n/a / n/a | 1.0 |
| 2026-11-02 | oracle | none | oracle 1, AffrescoUI 064a5355 (with changes) | Apple M4 Pro, macOS 27.0.1 | 34/34 (edit 12/12, feature 17/17, forge 5/5) | n/a / n/a | 1.0 |
| 2026-11-15 | oracle | none | oracle 1, AffrescoUI 6f3b7584 (with changes) | Apple M4 Pro, macOS 27.0.1 | 50/50 (edit 19/19, feature 26/26, forge 5/5) | n/a / n/a | 1.0 |
| 2026-11-15 | oracle (pointed) | none | oracle 1, AffrescoUI 6f3b7584 (with changes) | Apple M4 Pro, macOS 27.0.1 | 8/8 (edit 6/6, feature 2/2), the point- tasks only | n/a / n/a | 1.0 |
| 2026-11-16 | oracle | none | oracle 1, AffrescoUI 45991cb6 (with changes) | Apple M4 Pro, macOS 27.0.1 | 62/62 (edit 21/21, feature 26/26, forge 5/5, use 10/10) | n/a / n/a | 1.0 |
| 2026-11-02 | claude-code | claude-sonnet-5-5 | 2.1.312 (Claude Code), AffrescoUI 064a5355 (with changes) | Apple M4 Pro, macOS 27.0.1 | 11/11 (edit 2/2, feature 9/9), the 11 tasks below only | 179,548 / 1,022 | 7 |

### Using an app against changing it

The ten `use-` tasks ask for something done in Flow, Streak or Tempo (move a card, give it to someone and make it urgent,
add a card; mark a habit done on a day, with an amount, add a habit; play, queue and like a song), which the apps'
commands do: the client lists them (`list_commands`) and runs them (`run_command`), under each app's run rules, and the
screen stays as it was (`"commits": 0`). Two `change-` tasks sound alike but change the screen (`"runs": 0`). The oracle
passes all twelve (2026-11-16, above): 11 runs, two of them asked and allowed by the benchmark's reviewer. No model
has been run on them yet.

### Pointing against describing

The eight `point-` tasks ask for one change to the Kanban template, in its Lisp and its JSON notation, two ways: by
describing where it goes ("Show the titles of the cards on the board in the theme's accent colour.") and by pointing at
it ("Show card titles like this one in the theme's accent colour.", with the pick of `#cardTitle`). The changes are a
card's title colour, a column's name in capitals, the Filter button's tooltip (three edits) and a click on the owner in
a card's side panel that filters the board (a feature). `run.py --point` asks them by pointing; `--markdown` on runs of
both ways gives one row per task, described → pointed.

The oracle passes all eight both ways (2026-11-15, above): the tasks, their scenarios and the pointed requests agree.
No model has been run on them yet.

### Lisp against JSON (RFC 0005, stage 6)

The same requests, made of a program's definitions and of JSON. Calamaio is both: its kill ring and its loop are a
program (`killring.lisp`, `loop.lisp`), its other commands JSON actions, so each pair is one app, one scenario and one
kind of change, and each request says which way it is written (the task's `present` checks it). Streak is a program;
its pair is `habit-notes-key`, the same command added as a JSON action. One run of each task, Claude Code with Sonnet
(`claude-sonnet-5-5`), so a few thousand tokens either way are noise.

| Change | JSON task | Lisp task | Passed, JSON / Lisp | Tokens in, JSON / Lisp | Tokens out, JSON / Lisp | Tool calls, JSON / Lisp |
|---|---|---|---:|---:|---:|---:|
| A command's title | `json-calamaio-title` | `lisp-calamaio-title` (the keyword edit) | 1/1 / 1/1 | 167,312 / 159,651 | 697 / 632 | 5 / 5 |
| A body: a message changed | `json-calamaio-body` | `lisp-calamaio-body` | 1/1 / 1/1 | 126,505 / 126,524 | 616 / 695 | 4 / 3 |
| A new command on a key | `json-calamaio-command-key` | `lisp-calamaio-command-key` | 1/1 / 1/1 | 185,091 / 179,548 | 1,202 / 1,179 | 8 / 8 |
| After a save: a step, or a hook | `json-calamaio-hook` | `lisp-calamaio-hook` | 1/1 / 1/1 | 253,816 / 222,385 | 1,229 / 1,279 | 9 / 9 |
| A new command on a key (Streak) | `habit-notes-key` | `lisp-streak-command-key` | 1/1 / 1/1 | 237,178 / 188,481 | 1,622 / 1,022 | 12 / 7 |
| A body: a toast added (Streak) | | `lisp-streak-body` | / 1/1 | / 170,041 | / 1,000 | / 6 |

Every task passed, both ways. Tokens and calls are within what one run per task can tell apart, but for Streak's new
command, where the JSON task took 12 calls to the Lisp task's 7: one run, so not yet a difference. This says agents
write Lisp as accurately as JSON on these changes; it doesn't say they do better, so Lisp isn't recommended to agents
over JSON on these numbers. More runs per task, and other models, would.

Since then Calamaio is a program only, so its JSON tasks are gone. The JSON side of each pair is now on an app written
both ways, not measured with a model yet: `json-streak-body`, `json-streak-command-key` and `json-streak-hook` (paired
with `lisp-streak-body`, `lisp-streak-command-key` and `lisp-streak-hook`, an `advice`), and `json-flow-title` with
`lisp-flow-title`, as Flow's palette shows a command's title and Streak's doesn't.
