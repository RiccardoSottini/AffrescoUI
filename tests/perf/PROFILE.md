# Where the time goes in the missed budgets

[BENCHMARKS.md](../../BENCHMARKS.md) misses three budgets: one state change in 1 ms, a revision applied and
reconciled in 2 ms, and an idle app at 0% CPU. This profile measures where their time goes. It changes no library code.
It gives each guess about the cause a verdict with a number, and ranks what to change by measured share.

**Machine.** An Apple M4 Pro, macOS 27, Release build (`-O3`, warnings as errors). The machine was shared with other
work. The final measurement pair below ran at a load average of 1.7–1.8; the profiles ran at loads of 6–14.

**Commit.** `5ac551c` (master) with this branch's workloads. **Date.** 2026-11-16.

**How.**

- Medians, allocations and retained bytes come from `affrescoui_bench` (two runs each, below).
- Shares come from macOS `sample`: 10 s of one workload running in a loop, about 7,000 main-thread samples.
- A share is inclusive: a function counts with everything it calls, so the rows of a table overlap.
- The idle wakes come from a temporary trace of the event loop and of `Window::setTimer`, not committed.
- The per-template counts of H4 come from a temporary counter in `Reconciler::update`, not committed.
- Anything not measured is marked *estimate*.

Linux and CI can repeat the instruction view with `run.py --callgrind --only update/400 --annotate update/400 --out
out/perf.jsonl` (README.md).

## The numbers

Two runs, one after the other. Allocations and panels were the same in both runs.

| Workload | Run 1 | Run 2 | Δ | Allocations | Panels |
|---|---|---|---|---|---|
| `update/14` | 1.051 ms | 1.064 ms | 1.3% | 27,650 | |
| `update/100` | 2.756 ms | 2.797 ms | 1.4% | 79,276 | |
| `update/400` | 8.828 ms | 8.770 ms | 0.7% | 261,074 | |
| `update/read/14` · `/100` · `/400` | 1.058 · 2.765 · 8.743 ms | 1.084 · 2.797 · 9.100 ms | ≤3.9% | 27,681 · 79,307 · 261,105 | 493 · 1,275 · 4,000 |
| `update/record/14` · `/100` · `/400` | 3.429 · 9.759 · 33.04 ms | 3.484 · 9.638 · 33.47 ms | ≤1.6% | 75,793 · 229,534 · 769,103 | 493 · 1,275 · 4,000 |
| `update/animation/400` | 8.975 ms | 8.979 ms | 0.0% | 261,107 | 4,000 |
| `update/panels-50000` | 0.614 ms | 0.599 ms | 2.4% | 358 | |
| `update/panels-50000-without-layout` | 15.8 µs | 19.3 µs | 18% | 358 | |
| `update/app-50000` | 159 µs | 128 µs | 20% | 1,891 | 52,602 |
| `update/app-50000-without-layout` | 142 µs | 125 µs | 12% | 1,891 | 52,602 |
| `update/computed-unrelated/400` | 237 µs | 245 µs | 3.6% | 7,538 | 402 |
| `update/parameter/400` | 297 µs | 295 µs | 0.7% | 3,137 | 402 |
| `update/lisp-row/25000` | 615 µs | 600 µs | 2.5% | 371 | 50,002 |
| `update/list-append-25000` | 64.7 ms | 90.2 ms | 39% | 1,250,404 | 50,002 |
| `update/list-insert-front-25000` | 13.1 s | 23.3 s | 78% | 1,790,457 | 50,002 |
| `update/list-remove-front-25000` | 0.63 s | 0.86 s | 35% | 825,318 | 50,002 |
| `update/list-reverse-25000` | 14.6 s | 20.7 s | 42% | 875,364 | 50,002 |
| `revision/state-only/400` | 66.65 ms | 66.42 ms | 0.3% | 1,216,642 | |
| `revision/one-label/14` · `/100` · `/400` | 21.08 · 28.03 · 52.70 ms | 20.64 · 27.87 · 52.76 ms | ≤2.1% | 316,777 · 448,841 · 911,499 | |
| `revision/kanban/add-column` · `add-filter` · `move-card` | 35.47 · 35.09 · 35.47 ms | 35.32 · 35.32 · 35.33 ms | ≤0.7% | about 577,000 | |
| `revision/smart-home/add-room` | 33.59 ms | 33.94 ms | 1.0% | 555,611 | |
| `revision/habit-tracker/add-field` | 33.67 ms | 34.15 ms | 1.4% | 476,547 | |
| `revision/code-editor/add-terminal-line` | 21.79 ms | 21.86 ms | 0.3% | 374,346 | |
| `revision/music-player/add-field` | 19.41 ms | 18.97 ms | 2.2% | 313,304 | |
| `revision/sign-in/*` (3 edits) | 14.59–14.79 ms | 14.46–14.72 ms | ≤1.0% | about 203,500 | |
| `revision/launcher/*` (3 edits) | 5.79–6.06 ms | 5.83–6.06 ms | ≤0.7% | about 87,000 | |
| `budgets/idle` (Kanban; processMs 5.35) | 5.06 ms | 5.22 ms | 2.9% | 26,576 | |
| `budgets/idle-kanban` (processMs 6.85) | 5.85 ms | 5.99 ms | 2.3% | 26,884 | |
| `budgets/idle-music-player` (processMs 3.96) | 2.84 ms | 2.86 ms | 0.6% | 9,433 | |
| `budgets/idle-<the other six>` | 0.54–0.75 ms | 0.55–0.80 ms | ≤11% | 99–251 | |
| `build/kanban` | 14.46 ms | 14.38 ms | 0.6% | 208,241 | 471 |

The idle rows are the main thread's CPU time in one second; `processMs` counts every thread.

- **Memory.** `build/kanban` retains 3,959,402 bytes for 471 panels: **8,406 bytes a panel**. `revision/one-label`
  retains 2,096 bytes a revision. The updates retain nothing.
- **Agreement.** Two runs agree within 10%, except these:
  - `app-50000` and `panels-50000-without-layout` (16–159 µs) differ by up to 31 µs;
  - three idle templates at about 0.6 ms differ by up to 11%;
  - the four 25,000-row list workloads differ by 35–78%. They ran last, at a load average of 16–21, and each takes one
    run of up to 23 s, so they give the order of magnitude only. Their allocations were the same in both runs.
- **Idle runs alone.** Run with other workloads in the same process, idle reads 25–93 ms: the earlier workloads'
  windows stay open in the same event loop. `budgets.py` already runs it alone. `--only budgets/idle` also runs every
  `budgets/idle-<template>`, which adds about 8 s.
- **Failing workloads.** 17 `revision/<template>/<edit>` workloads of the templates written as programs fail on master,
  before this branch, so `run.py --only revision` exits 1. Each fails with *"X is built by the program and written in
  the document differently"*. The edit writes the built tree (`root.children`, `tokens…`, `styles…`), which a
  program's revision can't change. One of them is `revision/kanban/add-button`, the workload `budgets.json` recorded
  3.23 ms from.
- **Recorded values.** `budgets.json` records `update/400` at 3.09 ms and the revisions at 3.23 ms. Both are from
  `1d60d3f`, before the Kanban template became a program (#115), so they measure another screen.

## The budgets and their misses

| Budget | Measured (worst) | Miss | Accounted for below |
|---|---|---|---|
| One state change ≤1 ms (`update/400`, `update/panels-50000`) | `update/400` 8.8 ms; `panels-50000` 0.61 ms (met) | 7.8 ms | 96% of the time: the miss and more |
| A revision ≤2 ms (`revision/`) | `revision/state-only/400` 66.5 ms; `one-label/400` 52.7 ms; the Kanban edits 35.3 ms | 33–65 ms | 97–100% of the time |
| Idle 0% CPU (`budgets/idle`) | 5.1 ms of CPU a second (0.5%) | 5.1 ms/s | 94% (one timer) |

## One state change

### `update/400` (8.8 ms): renaming the board, on 400 cards

Nothing on the screen reads `state.board`, so this is what any state change costs on this screen.

| Function | Share |
|---|---|
| `Reconciler::update` | 94.2% |
| `Reconciler::reconcileChildren` | 88.4% |
| `Builder::instances`: repeats expanded again | 80.3% |
| `DocumentContext::variable`: built variables | 48.5% |
| the `menus` variable's builder (`addContributionVariables` `$_3`) | 47.2% |
| the `commands` builder (`$_2`, inside `menus`) | 46.7% |
| the `keybindings` builder (`$_1`, inside `commands`) | 23.5% |
| `DocumentContext::computedValue` | 21.6% (17.2 of it inside `commands`) |
| json destroy · json copy · `operator new` | 29.5% · 26.0% · 23.2% |
| `Value::toJson`: expanded lists turned into JSON | 12.3% |

The stages add up to 96.3% of the time:

| Stage | Share |
|---|---|
| The three repeats expanded on every update (H4) | 80.3% |
| Old items freed in `reconcileChildren` | 4.4% |
| `Reconciler::watch` | 4.6% |
| The layout | 5.6% |
| `updateLocale` | 1.3% |
| `updateTheme` | 0.1% |

The three repeats are the card menu and the column menu, over `menus.card` and `menus.column`, and one more. They
read a built variable, so every update builds `menus` again, then `commands`, then `keybindings`. Within that build:

- `commandEnabled` evaluates every action's `enabled`, 17.4%. Those expressions read computed values that filter the
  cards, so their cost grows with the board.
- `addBindings` walks every reconciler record, 17.6%.

### `update/14` (1.05 ms) and `update/read/400` (8.7–9.1 ms)

These have the same shape:

| Function | `update/14` | `update/read/400` |
|---|---|---|
| `Reconciler::update` | 95.8% | 93.2% |
| `Builder::instances` | 78.2% | 78.9% |
| `menus` | 53.7% | 46.4% |
| `commands` | 51.7% | 46.0% |
| `keybindings` | 20.6% | 22.9% |
| `computedValue` | 13.2% | 22.2% |
| `Reconciler::watch` | 1.9% | 4.0% |
| `globals` | 1.3% | 0.2% |
| layout | 4.0% | 6.6% |

A probe label in an overlay changing costs exactly what renaming the unread board costs, to within 4%: the fixed cost
of an update.

### `update/record/400` (33 ms): one card's record written

| Function | Share |
|---|---|
| `Reconciler::update` | 93.7% |
| `Builder::instances` | 67.7% |
| `menus` | 40.5% |
| `Reconciler::reconcileChild` | 29.5% |
| `computedValue` | 27.6% |
| `detail::choose` · `detail::filter` (computed values over the records) | 26.0% · 25.1% |
| `DataLayer`, 85% of it in `data::Store::run` | 14.2% |
| `ReadSet` | 2.8% |
| layout | 1.8% |

A record write costs 3.8 times a state write of the same screen (33.0 against 8.7 ms). The built variables are built
twice for each record write, once for each state write.

### `update/panels-50000` (0.61 ms) and `update/app-50000` (0.13–0.16 ms): both meet the budget

| Function | `panels-50000` | Function | `app-50000` |
|---|---|---|---|
| `Panel::layoutChildren` | 96.2% | `Reconciler::reconcileChildren` | 87.7% |
| `Panel::hasSameInputs` | 39.3% | `Reconciler::reconcileChild` | 56.9% |
| `Reconciler::update` | 2.9% | `Builder::instances` | 33.5% |
| `DocumentContext::globals` | 1.6% | json destroy | 22.4% |
| `Builder::values` | 1.7% | `Reconciler::updateKept` | 15.2% |
| | | json `operator==` | 12.7% |
| | | `ReadSet` | 6.7% |
| | | `DocumentContext::globals` | 6.4% |
| | | `Reconciler::reusable` | 4.0% |

On 50,000 panels the update itself takes 16–19 µs. The layout walks every panel to check its inputs, and that is 96%
of `panels-50000`.

### The 25,000-row lists

| Function | `list-insert-front` (13–23 s) | `list-append` (65–90 ms) |
|---|---|---|
| `Reconciler::reusable`: the key scan | 97.3% | 3.7% |
| `Reconciler::make` | 0.6% | |
| `Reconciler::updateKept`, which updates every kept row again | | 53.3% |
| `Builder::instances` | | 21.1% |
| `Reconciler::refreshItems` | | 19.8% |
| `Reconciler::gatherReads` | | 15.2% |

## A revision applied and reconciled

### The revision path, stage by stage

Shares of the whole revision, from `Repository::commit`, which is 98.7–99.8% of the step:

| Stage | `one-label/400` (52.7 ms, JSON) | `state-only/400` (66.5 ms, program) | `kanban/move-card` (35.3 ms, program) |
|---|---|---|---|
| **prepare** | **31.4%** | **41.7%** | **63.2%** |
| · its document and state copies (json copy constructors) | 1.7% | about 1.5% | 1.4% |
| · `Rules::judgeChange` | 23.2% | 28.4% | 48.3% |
| · · `check` (the checker's walk) | 18.8% | | 20.1% |
| · · `checkServiceUses` (`usesIn`, `addedUses`) | 0.7% | | 19.0% |
| · · `controlsIn` | 3.6% | | 6.1% |
| · `judgeProgram`: the program built twice in `judged` | none | 5.4% | 7.6% |
| · `Rules::validate`, one program build included | 5.3% | about 5.9% | 4.6% |
| **land** | **65.8%** | **55.0%** | **33.5%** |
| · the reconcile (`Reconciler::update`) | 59.3% | 46.6% | 20.3% |
| · · `menus` built again | 17.9% | 13.9% | 3.0% |
| · · panels applied again (`applyValues`) | 11.9% | | 3.1% |
| · storing (`store`: `dump`, then `sha256`, then `delta`) | 4.3% | 5.2% | 8.0% |
| · `refreshBuilt` | none | 1.8% | 2.9% |
| · `Engine::documentChanged` | 1.3% | 0.9% | 1.3% |
| the `Prepared` result freed | 1.4% | 2.0% | 2.9% |
| the observer's steps, layout included | 1.3% | 1.2% | 0.1% |
| **accounted for** | **99.9%** | **97.4%** | **99.8%** |

- **Program builds.** `lisp::buildDocument` builds the program three times in a revision of a program: twice in
  `judged`, before and after, and once in `validate`. Together they take 5.2% of `state-only` and 8.2–9.2% of the
  Kanban edits.
- **Judging a state change.** The checker walks the whole document even when the revision only sets a state key:
  28.4% of `state-only`.
- **The reconcile.** In `one-label/400` it takes 31 ms, 3.5 times the 8.8 ms of a state update on the same board.
  `reconcileChild` is 30.3% here and 4.3% in `update/400`: panels are applied again.

### Top 10, by inclusive share

| `one-label/400` | | `state-only/400` | | `kanban/move-card` | |
|---|---|---|---|---|---|
| `Repository::land` | 65.8% | `Repository::land` | 55.0% | `Repository::prepare` | 63.2% |
| `Reconciler::update` | 59.3% | `Reconciler::update` | 46.6% | `Rules::judgeChange` | 48.3% |
| `reconcileChildren` | 55.9% | `reconcileChildren` | 44.0% | `Repository::land` | 33.5% |
| `Repository::prepare` | 31.4% | `Repository::prepare` | 41.7% | `Reconciler::update` | 20.3% |
| `reconcileChild` | 30.3% | `Rules::judgeChange` | 28.4% | `check` | 20.1% |
| `Builder::instances` | 30.2% | `reconcileChild` | 24.4% | `checkServiceUses` | 19.0% |
| `Rules::judgeChange` | 23.2% | `Builder::instances` | 23.9% | `checker::Checker::run` | 18.9% |
| json destroy | 20.7% | json destroy | 19.6% | `lisp::buildDocument` | 8.8% |
| json copy | 19.8% | json copy | 18.1% | `Repository::store` | 8.0% |
| `check` | 18.8% | `operator new` | 17.1% | `Repository::judgeProgram` | 7.6% |

`kanban/add-filter` and `kanban/add-column` match `move-card` to within 1.4 points in every row of the stage table.

## Idle

### The wakes, per template

| Template | Wakes a second | What wakes the loop | CPU a second | Without the timer |
|---|---|---|---|---|
| Kanban (`budgets/idle`) | 1.5 | the bench's own quit post (1 a second, 0.05 ms); `Screen::Impl::retry`'s 2,000 ms timer for the `team` channel (6 ms each time) | 5.1–6.0 ms | 0.28–0.29 ms; allocations fall from 13,297 to 12 (6-run median) |
| Music player | 2 | the root's `every` 1,000 ms timer, which runs `tick` (2.0–2.5 ms busy); `ImageCache` posts | 2.8 ms | 0.30–0.37 ms; allocations fall from 9,036 to 13 |
| Launcher | 1 | the bench's quit post; a 30 s `ServiceCalls` timer; otherwise it waits with no timeout | 0.57–0.61 ms | |
| The other five | 1 | the bench's quit post | 0.54–0.80 ms | |

- **Where the main thread was.** In 10 s of Kanban idle, 7,874 of its 7,888 samples were inside
  `_glfwWaitEventsTimeoutCocoa`. Of the 14 others, 7 drew a frame (`copyToWindow`, a framebuffer blit) and 5 ran
  `Reconciler::update`.
- **The other threads.** None ran library code:
  - 4 AppKit and GCD workqueue threads, plus 9 short-lived ones, all in `__workq_kernreturn`;
  - the NSEventThread in `mach_msg`;
  - the bench's stopper threads (one a run) in `sleep_for`.
- **The floor.** What is left, 0.3–0.8 ms a second, is the bench's own: its thread, its quit post and the first frame
  of each run.
- **The median aliases.** The Kanban's retry comes every 2 s, but each measured run is 1 s, and there are 3 runs. So
  whether the median second holds a retry depends on where the runs fall.
  - In these runs it did: 26,576 allocations is one update's worth (`update/14` makes 27,650).
  - With 6 runs the median falls between the two kinds of second: 2.8 ms and 13,297 allocations.

## The hypotheses

| | Hypothesis | Verdict | Number |
|---|---|---|---|
| H1 | Every update rebuilds the globals, the history log included, and twice with a `theme` | **Too small to matter** | `DocumentContext::globals` 0.2–1.8% of the updates (6.4% of the 0.13 ms `app-50000`, about 8 µs); `updateTheme` 0.1–0.6% |
| H2 | A `watch` on a computed value computes it again, compares it and copies it on every update | **Too small to matter** | `Reconciler::watch` 1.9–4.6%; `update/computed-unrelated/400` (a 400-item computed list a watch names) 0.24 ms, within budget |
| H3 | `window.menuBar` rebuilds `commands` and `menus` after every state change | **Confirmed**, as the largest cost | The `menus` build is 47.2% of `update/400`, 53.7% of `update/14`, 40.5% of `update/record/400`, 17.9% of `revision/one-label/400`. Its readers are the two context menus' repeats, which read `menus.*`. `commands` is 46.7% and `keybindings` 23.5% inside it |
| H4 | Nodes that read a built variable, `nodes.<id>`, `now()`/`today()`, or have `alwaysApply`, are visited every update | **Confirmed** | The Kanban's 3 always-expanded repeats are 80.3% of `update/400`. Per template: see below |
| H5 | A write to a record asks for a full update (`DataLayer::watch`) | **Confirmed** | `update/record/400` is 33.0 ms, against 8.7 ms for `update/read/400`: 3.8×. `DataLayer::watch` bumps the state version, so every computed value and built variable is stale, and the built variables are built twice a write. `Store::run` is 12% |
| H6 | A keyed insert that isn't at the end rebuilds every later row, and each row scans the rows after it | **Confirmed** | `list-insert-front-25000` 13–23 s, 97.3% of it in `Reconciler::reusable`'s scan; `list-reverse-25000` 15–22 s. The rebuilds themselves (`make`, `forgetSubtree`, `adopt`) are 1.1% |
| H7 | Expanding a list again allocates a scope and copies the item per row | **Confirmed, a moderate share** | In `list-append-25000`, `Builder::instances` is 21.1%: json copies 9.3% and scopes (`extend`, `new`) about 9%. Larger: `updateKept`, which updates every kept row again, 53.3% |
| H8 | A subtree's read set above 32 places merges until it meets every change | **Too small to matter** | `ReadSet::collapse` 1 sample of about 7,000 in each profile (under 0.1%); all of `ReadSet` 1.9–6.7% |
| H9 | After every update the screen builds the persisted state and compares it | **Not measured: 0 samples** | The bench loads its screens with `fromString`, which has no state file, so `save()` returns at its first line. *Estimate*: small, since the Kanban persists 5 keys. A workload with a state file would measure it |
| R1 | A revision copies the document and the state three times | **Too small to matter** | json copy constructors under `prepare`: 1.4–1.7% of the revision |
| R2 | A program is built into its tree several times per revision | **Confirmed, 8–12%** | 3 builds a revision (2 in `judged`, 1 in `validate`): `lisp::buildDocument` 5.2–9.2%, plus `refreshBuilt` 1.8–3.0%. *Estimate*: building the unchanged "before" document is a third of `judged`'s 6–7% |
| R3 | The whole result is validated and judged | **Confirmed** | `judgeChange` and `validate`: 28.5% of `one-label/400`, about 34% of `state-only/400`, 52.9% of `kanban/move-card`. Half of the Kanban's is `check` (20.1%), half `checkServiceUses` (19.0%) |
| R4 | Every compiled node is dropped and every panel applied again | **Confirmed for the panels; the drop is small** | `Engine::documentChanged` (the compiled nodes, `forgetVersion`) is 0.9–1.3%. The reconcile after a revision is 59.3% of `one-label/400` (31 ms, 3.5× a state update); `applyValues` 11.9% there, against 1.0% in `update/400` |
| R5 | Colours, fonts, strings, services, sounds and windows are read again whatever the revision touched | **Too small to matter** | `updateLocale`, `updateTheme` and `readDerived` together are 2.4–3% of `one-label/400` and 6.6% of `move-card`; fonts, services, sounds and windows 0 samples |
| I1 | A declared channel with `reconnect` and no host retries on a timer, runs `<name>:closed`, and that action's write of an unchanged value runs an update | **Confirmed**: 94% of the idle miss | The Kanban's `team` channel retries every 2,000 ms. `team:closed` runs `teamClosed`, which sets `state.presence` to `{}`, its value already, and a full update follows (6 ms, 26,600 allocations). Without `:reconnect`, idle falls from 5.1 to 0.29 ms a second |
| I2 | A thread wakes on a timed wait (`Lines.hpp`, `Sockets.cpp`) | **Refuted** | No library thread ran in 10 s of idle. Both timed waits exist but don't run while idle: `Lines.hpp:67` waits 100 ms only while a backlog is full, and `Sockets.cpp:83` waits 1 s in a channel's destructor |

**Also measured: an `every` timer whose action writes nothing still runs an update.** The music player's `tick` does
nothing while `state.isPlaying` is false. Yet each second the built variables are built twice, and taking the timer out
brings idle from 2.84 to 0.30–0.37 ms.

**H4, per template.** The number of reconciler records whose values or expansion are evaluated again on every update:

| Template | Records | Values read always | Expansions read always |
|---|---|---|---|
| Kanban | 471 | 25 | 3 |
| Habit tracker | 1,518 | 22 | 7 |
| Code editor | 425 | 18 | 3 |
| Sign-in | 179 | 21 | 3 |
| Smart home | 339 | 15 | 3 |
| Calamaio | 319 | 11 | 4 |
| Music player | 231 | 8 | 2 |
| Launcher | 134 | 4 | 1 |

## What this says to do

Ranked by measured share of a missed budget.

1. **Don't build the built variables again on every update, or don't let a menu's repeat read them on every update.**
   - `menus` is 47% of a state change (H3), and the three repeats over built variables are 80% (H4).
   - Building `menus` once per change to the commands, keys or what their `enabled` reads would take `update/400` from
     8.8 ms to about 1.8 ms (*estimate*: 80% off).
   - The same build is 14–18% of a revision's reconcile.
2. **Judge only what a revision changed.**
   - `judgeChange` and `validate` are 29–53% of a revision (R3).
   - A revision that sets only a state key still walks the whole document: 28% of `state-only/400`.
   - `checkServiceUses` walks the uses before and after: 19% of the Kanban edits.
3. **Reconcile a revision like a state change.** After a revision, panels are applied again (R4): the reconcile costs
   3.5 times a state update, 59% of `one-label/400`. Keeping the compiled nodes of what didn't change would bring it
   near item 1's cost.
4. **Write nothing when a value doesn't change, and don't update after an action that wrote nothing.**
   - This is 94% of the Kanban's idle CPU (I1) and 88% of the music player's.
   - Either fix brings both to the bench's floor of about 0.3 ms a second.
5. **Make a record write a partial update.** It costs 3.8 times a state write (H5), because `DataLayer::watch` makes
   every computed value and built variable stale.
6. **Find a keyed row by a map, not a scan.**
   - Inserting at the front of 25,000 rows takes 13–23 s, 97% of it in the scan (H6).
   - On append, every kept row is updated again: 53% of `list-append` (H7).
   - Not a budget workload yet, but an order of magnitude past any.
7. **Build a program once per revision.** Three builds are 8–12% of a program's revision (R2).
8. **Too small to change for these budgets:** H1, H2, H8, R1, R5 and I2. H9 isn't reached by the bench.
