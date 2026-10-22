# The agent benchmark

How well an AI agent changes a running AffrescoUI screen through the MCP server (`mcp/affrescoui_mcp.py`): each task
is a request a person might make, given to a client, and checked by scenarios played on the screen the client leaves.
The results go into the "Agents" section of `BENCHMARKS.md`.

## Tasks

`tasks/*.json`, one per task, at three levels:

- `edit`: one change to a template (a label, a color, a value in the state);
- `feature`: a small feature in a template (a new label with an expression, an action, a key);
- `forge`: a feature in Forge, across its feature files (a button in the title bar wired to an action, a link on the
  welcome page, a status bar item).

Each task has:

| Field | What it is |
|---|---|
| `id` | its name, which is also its file's |
| `level` | `edit`, `feature` or `forge` |
| `screen` | the starting screen, from the repository root |
| `request` | what the client is asked, in plain words; it names the ids the scenarios look for |
| `scenarios` | the scenarios (`SCREEN_FORMAT.md`, "Scenarios") that pass once the request is done |
| `unchanged` | the nodes that the change must leave exactly as they were |
| `reference` | the revisions that do it (`submit_revision` arguments), which the oracle submits |

The reference's output tokens also give the "feature request" budget of `tests/perf/budgets.json`: its largest
reference at the `feature` and `forge` levels.

## Method

For each task, `run.py`:

1. starts a host (`affrescoui run <screen> --mode acceptEdits`) on a socket folder of its own, under the system's
   temporary folder, readable by this user only, with a reviewer connected to its `studio.sock` that accepts each
   proposal (the screens send changes to actions to review);
2. plays the scenarios on the starting screen: for the oracle they must fail, so that they test the request;
3. gives the request to the client, connected through `mcp/affrescoui_mcp.py --socket <that folder>`;
4. reads the screen back from the host and plays the scenarios on it (`affrescoui test --document`), with the live
   state;
5. compares each `unchanged` node with the starting screen's.

A task succeeds when its scenarios pass and every `unchanged` node is unchanged. Recorded per task: success, the
failures, tokens in (input, cached included) and out, tool calls, revisions and seconds; per run: the client, its
version, the model, the commit, the machine (processor and system), the date and the medians.

## Clients

- `oracle`: submits each task's `reference`, one MCP call per revision. It needs no model and no secret, and must pass
  every task: CI runs it (job "Agent benchmark (oracle)" in `.github/workflows/ci.yml`), so a task, its scenarios and
  the code can't drift apart.
- `claude-code`: Claude Code headless (`claude -p`, stream-json output), with only the `affrescoui` MCP server
  (`--strict-mcp-config`) and its other tools denied, so every change goes through the server. Its counts come from its
  stream's events.

API keys: the client reads its own (e.g. `ANTHROPIC_API_KEY`) from the environment. `run.py` never reads, writes or
passes on a key: the results hold the counts only, the client's error output is discarded, and nothing of the
environment is recorded. No key belongs in this repository.

## Running

Build first (`cmake -S . -B build -G Ninja && cmake --build build`), then, under a display:

```sh
python3 benchmarks/agents/run.py --client oracle                          # every task; exits 1 unless all pass
python3 benchmarks/agents/run.py --client claude-code --model <model>     # a model, through Claude Code
python3 benchmarks/agents/run.py --only forge-                            # the tasks whose ids start so
python3 benchmarks/agents/run.py --markdown benchmarks/agents/results/*.json   # the Agents table of BENCHMARKS.md
```

Results go to `results/<client>-<date>.json` unless `--out` says otherwise. `--timeout` bounds a client's seconds per
task (900 by default).
