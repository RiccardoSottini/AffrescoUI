# AffrescoUI MCP server

Connects an AI agent (Claude Code, or any [Model Context Protocol](https://modelcontextprotocol.io) client) to a
running AffrescoUI host, **in both directions**:

```
requests   the app's chat ──"request"──▶ affrescoui_launcher ──agent.sock──▶ affrescoui_mcp.py ──channel event──▶ AI agent
                                                                                (or wait_for_request)
changes    AI agent ──tools──▶ affrescoui_mcp.py ──agent.sock──▶ affrescoui_launcher ──revision──▶ rendering engine
                                                                       └──review, reply──▶ the app's chat
```

- **Into the agent:** a person types a request in the app itself ("add a dark theme"; a screen's `ask` step, e.g. the
  assistant add-on's chat), or in any client connected on the host's `studio.sock`. The host passes it on to this
  server, which pushes it into the agent's session as a Claude Code **channel event**, and also keeps it for the
  `wait_for_request` tool (for any MCP client). A request may carry `picks`, what the person pointed at with the
  Point button of the app's assistant (at most 4, cleaned to the keys of a pick record: node, type, pointer, bounds,
  bindings as written, source; never a value from the app's data; only on a request from inside the app); the channel
  event lists them after the text, one `Pointed at: ...` line each (without line breaks or control characters), and
  holds them in its `picks` meta. A request from inside the app gets a "working" note right away, and a request is never an approval:
  the person still reviews the agent's proposals.
- **Out of the agent:** the agent reads the screen and sends revisions (JSON Patch) with the tools. They go through
  the host's permission rules: for the templates they become proposals the person accepts or rejects on their card in
  the app's chat, unless the mode is `acceptEdits` or the reviewer said "don't ask again" for that kind of change. The
  agent answers the person with `reply`.

The server connects to the host's `agent.sock`, so its changes are authored as `agent/<client name>`. It needs only the
Python standard library (3.9+). The socket directory is found the way the host finds it:
1. `--socket DIR`;
2. else `$AFFRESCOUI_SOCKET_DIR`;
3. else `/tmp/affrescoui-<uid>` on macOS and Linux, and `%LOCALAPPDATA%\affrescoui\sockets` on Windows.

## Set up

1. Build and start the host on an app (with no file it opens the Launcher, which lists every app):
   ```sh
   cmake -S . -B build && cmake --build build
   ./build/affrescoui_launcher examples/flow/lisp/index.screen.lisp &   # the Kanban board; its assistant chat asks and reviews
   ```
2. Register the server with Claude Code:
   ```sh
   claude mcp add affrescoui -- python3 /path/to/AffrescoUI/mcp/affrescoui_mcp.py
   ```
   From a release, the server is also a Python package (`mcp/pyproject.toml`):
   `pipx install ./affrescoui_mcp-<version>-py3-none-any.whl` (from the release's files) or
   `pipx install "git+https://github.com/RiccardoSottini/AffrescoUI#subdirectory=mcp"`, then `claude mcp add affrescoui -- affrescoui-mcp`.
3. Start Claude Code so requests are pushed into the session (channels are a research preview; a server you add
   yourself needs the development flag):
   ```sh
   claude --dangerously-load-development-channels server:affrescoui
   ```
   Requests now arrive as `<channel source="affrescoui" request_id="request-1" sender="studio" origin="studio">`
   (`origin="app"` for a request typed inside the app).

Without channels (or with another MCP client), ask the agent to use the `serve_studio` prompt, or just tell it to
"wait for requests from the AffrescoUI app and handle them": it loops on `wait_for_request`.

Options: `--socket DIR` if the host listens elsewhere (`affrescoui_launcher --socket DIR`); `--launch PROGRAM` to start
the host if none is running, e.g. `--launch /path/to/build/affrescoui_launcher`, with each of its arguments in a
`--launch-arg ARG` of its own (never split from one string); and `--root DIR` (repeatable) for a folder outside the
repository and the working folder whose screens and scenarios the tools may read. The socket folder must be the
user's own and closed to others (`0700`).

Other clients (JSON configuration):
```json
{ "mcpServers": { "affrescoui": { "command": "python3", "args": ["/path/to/AffrescoUI/mcp/affrescoui_mcp.py"] } } }
```

## Windows

The host listens on a Unix domain socket (`AF_UNIX`), which Windows has supported since Windows 10 version 1803.
**Python can't use it yet:** no released CPython (3.9 to 3.15) has `socket.AF_UNIX` on Windows. The work to add it is
[python/cpython#77589](https://github.com/python/cpython/issues/77589), with an open pull request. So on Windows
the server starts and lists its tools, but every tool that needs the host (all but `get_format_guide`) answers that
this Python has no Unix domain sockets. Nothing else is needed for the day a Python with `socket.AF_UNIX` comes: the
server uses it as it is.

The setup, once the host builds on Windows (work in progress) and Python can connect:
1. Build and start the host (Windows 10 version 1803 or later). By default it and this server meet in
   `%LOCALAPPDATA%\affrescoui\sockets`, a folder in your profile that only you (and administrators) can open:
   ```powershell
   cmake -S . -B build
   cmake --build build --config Release
   Start-Process .\build\Release\affrescoui_launcher.exe
   ```
2. Register the server with Claude Code. Use `py` (the Python launcher) or `python`, since `python3` is often missing
   on Windows:
   ```powershell
   claude mcp add affrescoui -- py C:\path\to\AffrescoUI\mcp\affrescoui_mcp.py
   ```
   In a JSON configuration, double the backslashes: `"args": ["C:\\path\\to\\AffrescoUI\\mcp\\affrescoui_mcp.py"]`.
3. `--socket DIR` and `AFFRESCOUI_SOCKET_DIR` work as on the other platforms. `--launch` takes the program's
   path, e.g. `--launch C:\path\to\build\Release\affrescoui_launcher.exe`.

## Tools

| Tool | What it does |
|---|---|
| `wait_for_request` | The next request from inside the app or from a client on `studio.sock` (`request_id`, `from`, `text`, `origin`: `"app"` or `"studio"`), waiting for it if needed |
| `reply` | Answer a request where it was asked; `done: false` sends a progress note and keeps it open |
| `answer_repair` | Answer a repair request (a change set that no longer applies, or whose scenarios fail, on a new template) with the request redone as one JSON Patch: always a proposal, and the scenarios in `repair.proof` must pass on it |
| `get_screen` | The document (with its permission rules), live state, revision and head commit, open proposals, mode, grants and the agent's author name. Later calls return only what changed (`full` for everything). `outline` gives the node tree, one line per node: its address, type, text, `bind`, `onClick`, `repeat` and `when`, without the styling. `at` gives one node (`"#id"` or a JSON Pointer). `depth` limits both: outline lines say `+12` for what is below, and a node's deeper children become their addresses. `notation: "lisp"` writes the document or the node in the Lisp notation (SCREEN_FORMAT.md, "The Lisp notation"), converted by the host; what changed since a read stays a JSON Patch. A program reads as its definitions, each at `/program/<file>/<name>` as exact text with its comments, and the outline lists them; `built` adds the tree they build, which `outline`, `at` and `find_nodes` read anyway |
| `find_nodes` | Nodes by `type` (or component), `text` (in their text, placeholder or tooltip, any case) or `property` (with a `value`), one outline line each with its parent; a program's definitions by their form (`type: "defn"`) or their text |
| `get_models` | The data layer: each collection's model (its version and typed fields), how many records it holds, and the declared queries; never a record |
| `get_contributions` | What an app offers to features from other apps (its contribution points and typed exports) and the installable parts it ships, one line each |
| `check_composition` | Proves that a feature from another app fits the running screen (needs bound to exports, kinds, slots, ids, permissions, the composed document, its scenarios), changing nothing |
| `install_feature` | Installs a feature once `check_composition` proves it: one revision, reviewed like any other; for a program, both prove on its source (the host's `source`), so its definitions stay open to change |
| `get_catalog` | What the screen's format offers: node types and properties, events, steps, functions, and the app's registered types |
| `get_format_guide` | The screen format specification (`SCREEN_FORMAT.md`), a part at a time: its introduction and topics, or one `topic` (a heading or a word of one), or `"all"` |
| `submit_revision` | A JSON Patch change: applied (a commit), proposed (a branch for review), or rejected with the reason. In a program it sets a definition's text (`/program/<file>/<name>`) or one keyword's value in it (`.../<name>/:title`), and rules judge what that builds. `proposal` pushes onto the agent's open proposal. With `notation: "lisp"`, `revision` holds the change as Lisp text (`{:set {:"#title/text" Hello}}`), which the host converts to the same JSON before it is submitted: the history stays JSON |
| `render_screen` | A picture (PNG image block) of main, or of an open proposal applied (`proposal`), at `scale` 1 or 2: the agent checks its work before the reviewer does. Drawn offscreen, so the person's window doesn't change. Read-only |
| `get_evidence` | An open proposal's evidence, gathered by the host (VERSION_CONTROL.md, "Evidence"): the pictures before and after (PNG image blocks, at `scale` 1 or 2), then one line of JSON with the changed `region`, the `behaviour` lines, the `scenarios` results and the changed `texts`. Read-only |
| `run_scenarios` | Plays the screen's own scenarios (`file`: a `*.scenarios.json` shipped next to the screen, and no other file; SCREEN_FORMAT.md, "Scenarios") on main as it runs now, or with an open proposal applied (`proposal`), or the agent's own `scenarios` on that screen; `only` picks one. Answers in a few lines: the totals, then `FAILED <scenario>, step <n>: <what>`. Runs the `affrescoui` tool (`$AFFRESCOUI_CLI`, the `PATH`, or `build/`) headless on a copy: nothing changes. Read-only |
| `list_commands` | The app's commands, a line each: `name(arguments)  description  [badge]`, the badge `[ask]`, `[person]`, `[deny: why]` or `[disabled now]` (none when it runs at once); `<n> commands` first, at most 200 lines (`filter` narrows them; `all` adds the commands the agent may never run and each argument's description). Read-only |
| `run_command` | Runs a command (`name`, `arguments`) under the agent's own name and the app's run rules: what it changed in the state (one JSON Patch operation a line, at most 20), its errors, the records it wrote and `undo: undo_run(<n>)`; `picture` puts a picture of the app after it first. A run that waits for a yes is waited for up to `wait` seconds (60 by default, at most 600), then its outcome comes in `get_events`. A refusal is `refused: <why>` |
| `undo_run` | Puts back what a run changed in the state and the records (`run`): `undone run <n>` and the changes put back, or refused if something changed them since |
| `get_history` | main's commits, like `git log`: number, short id, author, committer (who approved it), message, parents |
| `get_diff` | A JSON Patch between two revisions (`HEAD~1`, ids, numbers), or what an open proposal would change; with `files`, the change file by file (`file` for a file's text, `notation` for JSON or Lisp) |
| `get_blame` | Who last changed a node (`"#board"`, `"#board/text"`) or a path (`"/colors/accent"`), and when, like `git blame`: the commit's revision, id, author, committer, message and time |
| `get_pick` | What the person pointed at and has not sent yet: each element's node, type, and the definition or file that builds it (the host's `getPicks`). Refused to a connection whose reads are limited. Read-only |
| `get_source` | Where a node is written (`at`: `"#id"`, a JSON Pointer or a definition's address): one line of JSON (address, file, line, notation, ...), then the definition's text in a program, or the node as JSON in a JSON document (the host's `getSource`). Refused to a connection whose reads are limited. Read-only |
| `revert_revision` | Undo one earlier revision and keep the later ones that don't depend on it, in one new revision, like `git revert` (follows the permissions). `dry_run` returns the plan (the host's `undoPlan`) and changes nothing: its `outcome` (`clean`, `choose`, `ready`, `blocked`), `head`, the `conflicts` (later changes built on it), the `later` commits they come from, the live `state` it undoes and keeps, and its `warnings`. Each conflict's `path` goes in `drop` (the later change is lost there) or `keep` (it stays, and that part is not undone), or `"all"`, with `head` from the plan; `then` is a JSON Patch applied after the undo, to keep what a later change built in another place. With `drop` or `then`, the undo is always a proposal the person reviews. A refusal on a conflict says to dry-run first |
| `restore_revision` | Bring the whole screen back to an earlier revision, like `git restore` (follows the permissions) |
| `get_events` | What happened since the last call: decisions on proposals, commits by anyone, mode and grant changes, emitted events, the outcome of runs that waited (`{"run", "command", "outcome", "detail"}`), `{"commands": "may have changed"}` once after commits, errors |
| `export_changes` | The revisions after a point as a change set ("plugin") |
| `lease_part` | Claims a part of the screen (`"#id"` or a JSON Pointer) for `seconds`: other authors' revisions that touch it are refused until it ends |
| `renew_lease` | Makes a lease the agent holds last `seconds` from now |
| `release_part` | Ends a lease the agent holds |
| `get_usage` | The screen's usage summaries (counts per node and action, screens visited, controls never used), when the host switched them on with the person's consent and the connection may review. Read-only |

Prompt: `serve_studio` (work through the people's requests one after another; the name is kept for existing clients).

**Undoing an earlier change** without losing what was built on it: look first, then undo with the choices.

```text
revert_revision {"revision": 12, "dry_run": true}                      -> {"outcome":"choose","head":"9f3c2a1","conflicts":[{"path":"#title/size",...}],...}
revert_revision {"revision": 12, "drop": ["#title/size"], "head": "9f3c2a1",
                 "then": [{"op": "replace", "path": "/styles/heading/size", "value": 28}]}  -> proposed as proposal 4 ...
```

**Reading little.** Every node has an id (SCREEN_FORMAT.md, "Every type"), so an agent addresses nodes by name. The
cheap way through a request: `get_screen` with `outline=true` (and `depth`), or `find_nodes`; then `at="#id"` for the
nodes to change; `get_format_guide` with the `topic` it needs. A request that points (`picks`) starts from each
pick's `source.address` with `get_source`, and reads the outline only if the change needs more. Measured with `tests/perf/tokens.py` (o200k_base), an
outline is 316 to 584 tokens for the templates (a full read 1,548 to 4,197), an outline to depth 2 is 87 to 305, a
re-read after a commit about 20, and the guide's contents 583 (the whole guide 9,221).

## Using the app

An agent can use a running app, not only change it: `list_commands` lists what the app can already do for the person
(its commands: SCREEN_FORMAT.md, "Commands and keybindings"), and `run_command` runs one, as the agent and never as
the person. The app's rules decide, in its `permissions.run` (VERSION_CONTROL.md): a run starts at once (`allow`), waits
for a reviewer's yes on its card in the app's chat (`ask`), waits for the person's own yes in the window, each time and
never remembered (`person`, for commands that write files, run terminals or send requests that write), or is refused
(`deny`). The server has no tool to allow or deny a run, and never retries one that was refused; a run never approves
a change. Names, titles and descriptions are the app's own text: each is shown on its line, escaped, after the name.

When a person asks for something the app can already do (move, add, play, open), the agent lists the commands and runs
one; when they ask for the app to be different, it sends a revision; when it isn't clear which, it asks. A host from
before commands answers `this host is older and has no commands`.

## Code

`affrescoui_mcp.py` is what clients run from a checkout; the server is the package next to it, `affrescoui_mcp/`
(also what the `affrescoui-mcp` wheel installs), one concern per module:

| Module | What it does |
|---|---|
| `server.py` | The MCP side: JSON-RPC over stdio, each tool call answered from the host or the `affrescoui` tool |
| `host.py` | The host side: where it listens (the socket folder, checked like the host checks it), the connection, the messages it sends |
| `tools.py` | The tools, prompt and instructions the server offers, as clients and the website's reference read them |
| `document.py` | A screen document read the way an agent needs it: compact JSON, changes since a read, outline, one node, search, JSON Patch |
| `guide.py` | `SCREEN_FORMAT.md` one topic at a time |
| `picks.py` | What the person pointed at (pick records), cleaned to a record's keys and told in plain lines |
| `commands.py` | Using the app: the commands listed a line each, a run's outcome and its changes in a few lines, undo |
| `replies.py` | What the agent is told of the host's replies and of messages the server can't use |
| `undo.py` | `revert_revision`'s choices, checked before the host is asked, and the undo's plan cut to what an agent needs |
| `repository.py` | Where the guide and the `affrescoui` tool are found |

**Queries are tested** without a host: `python3 mcp/test_affrescoui_mcp.py` (ctest `mcp`).

There are no tools to accept or reject proposals, set the mode or revoke grants: only reviewers (the person, in the
app or a client on `studio.sock`, as the template's `permissions.reviewers` says) can, and the host refuses those
messages from `agent`. See
[VERSION_CONTROL.md](../VERSION_CONTROL.md).
