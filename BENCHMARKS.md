# Benchmarks

What AffrescoUI promises in speed, memory and tokens, how each promise is measured, and what was measured. A target
missed is written as missed: the targets don't move, the code does.

## Method

- **Where.** Every number below was measured on an Apple M4 Pro, macOS 27.0.1, on 2026-10-22: the budgets at commit
  `1d60d3f`, the agent benchmark at commit `8b367ca` (the same code; that commit adds only the recorded budgets). Other
  machines give other numbers.
- **Build.** Release (`-O3`), the benchmark alone:
  `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DAFFRESCOUI_BUILD_BENCHMARKS=ON -DAFFRESCOUI_BUILD_TESTS=OFF -DAFFRESCOUI_BUILD_EXAMPLES=OFF`.
- **Workloads.** `affrescoui_bench` (`tests/perf`, described in `tests/perf/README.md`): each workload runs once to
  warm up, then `runs` times, and reports the median per operation, in wall-clock time (frames: the main thread's CPU
  time). Typing and scrolling time each operation and report the 99th percentile. The memory budgets run each in a
  process of its own and read the whole process's memory after the last run (its footprint on macOS, its resident set
  on Linux). Idle CPU is every thread's CPU time over one second of the Kanban template's event loop, with nothing
  happening.
- **Worst case.** A budget that covers several workloads (every template's revisions, two sizes of state change) is
  the slowest of them, named in the table.
- **Tokens.** Counted with tiktoken's `o200k_base` on what the MCP server sends or receives (`tests/perf/tokens.py`):
  outlines, re-reads after one commit, the format guide's topics, the canonical edits of `tests/perf/edits`, the
  reference revisions of the agent benchmark's feature tasks, and each desktop kit component's use (`tests/perf/kit.json`).
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
  budget's threshold is the value it was recorded at, so it can only get better.

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

## Agents

The agent benchmark (`benchmarks/agents/README.md`): 34 tasks a person might ask of a running screen, at three levels
(12 edits, 17 features in a template, 5 features in Forge across its files), each given to a client that changes the
screen through the MCP server, then checked by the task's scenarios, its unchanged nodes and, where the request says how
it is written, the definitions or actions it must leave. The oracle submits each task's reference revisions: it checks
the tasks themselves, and CI runs it on every change. A model has been run once, on the tasks that compare Lisp with
JSON (below), not on the others.

| Date | Client | Model | Versions | Machine | Success | Median tokens in / out | Median tool calls |
|---|---|---|---|---|---:|---:|---:|
| 2026-10-22 | oracle | none | oracle 1, AffrescoUI 8b367ca | Apple M4 Pro, macOS 27.0.1 | 24/24 (edit 11/11, feature 8/8, forge 5/5) | n/a / n/a | 1.0 |
| 2026-11-02 | oracle | none | oracle 1, AffrescoUI 064a5355 (with changes) | Apple M4 Pro, macOS 27.0.1 | 34/34 (edit 12/12, feature 17/17, forge 5/5) | n/a / n/a | 1.0 |
| 2026-11-02 | claude-code | claude-sonnet-5-5 | 2.1.312 (Claude Code), AffrescoUI 064a5355 (with changes) | Apple M4 Pro, macOS 27.0.1 | 11/11 (edit 2/2, feature 9/9), the 11 tasks below only | 179,548 / 1,022 | 7 |

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
