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
  `wait_for_request` tool (for any MCP client). A request from inside the app gets a "working" note right away, and a request is never an approval:
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
   ./build/affrescoui_launcher examples/flow/index.screen.json &   # the Kanban board; its assistant chat asks and reviews
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

Options: `--socket DIR` if the host listens elsewhere (`affrescoui_launcher --socket DIR`), and `--launch "command"` to start
the host if none is running, e.g. `--launch "/path/to/build/affrescoui_launcher"`.

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
3. `--socket DIR` and `AFFRESCOUI_SOCKET_DIR` work as on the other platforms. `--launch` takes a Windows command line
   as it is, e.g. `--launch "C:\path\to\build\Release\affrescoui_launcher.exe"`.

## Tools

| Tool | What it does |
|---|---|
| `wait_for_request` | The next request from inside the app or from a client on `studio.sock` (`request_id`, `from`, `text`, `origin`: `"app"` or `"studio"`), waiting for it if needed |
| `reply` | Answer a request where it was asked; `done: false` sends a progress note and keeps it open |
| `get_screen` | The document (with its permission rules), live state, revision and head commit, open proposals, mode, grants and the agent's author name. Later calls return only what changed (`full` for everything). `outline` gives the node tree, one line per node: its address, type, text, `bind`, `onClick`, `repeat` and `when`, without the styling. `at` gives one node (`"#id"` or a JSON Pointer). `depth` limits both: outline lines say `+12` for what is below, and a node's deeper children become their addresses |
| `find_nodes` | Nodes by `type` (or component), `text` (in their text, placeholder or tooltip, any case) or `property` (with a `value`), one outline line each with its parent |
| `get_catalog` | What the screen's format offers: node types and properties, events, steps, functions, and the app's registered types |
| `get_format_guide` | The screen format specification (`SCREEN_FORMAT.md`), a part at a time: its introduction and topics, or one `topic` (a heading or a word of one), or `"all"` |
| `submit_revision` | A JSON Patch change: applied (a commit), proposed (a branch for review), or rejected with the reason. `proposal` pushes onto the agent's open proposal |
| `render_screen` | A picture (PNG image block) of main, or of an open proposal applied (`proposal`), at `scale` 1 or 2: the agent checks its work before the reviewer does. Drawn offscreen, so the person's window doesn't change. Read-only |
| `get_evidence` | An open proposal's evidence, gathered by the host (VERSION_CONTROL.md, "Evidence"): the pictures before and after (PNG image blocks, at `scale` 1 or 2), then one line of JSON with the changed `region`, the `behaviour` lines, the `scenarios` results and the changed `texts`. Read-only |
| `run_scenarios` | Plays the screen's own scenarios (`file`: a `*.scenarios.json` shipped next to the screen, and no other file; SCREEN_FORMAT.md, "Scenarios") on main as it runs now, or with an open proposal applied (`proposal`), or the agent's own `scenarios` on that screen; `only` picks one. Answers in a few lines: the totals, then `FAILED <scenario>, step <n>: <what>`. Runs the `affrescoui` tool (`$AFFRESCOUI_CLI`, the `PATH`, or `build/`) headless on a copy: nothing changes. Read-only |
| `get_history` | main's commits, like `git log`: number, short id, author, committer (who approved it), message, parents |
| `get_diff` | A JSON Patch between two revisions (`HEAD~1`, ids, numbers), or what an open proposal would change |
| `get_blame` | Who last changed a node (`"#board"`, `"#board/text"`) or a path (`"/colors/accent"`), and when, like `git blame`: the commit's revision, id, author, committer, message and time |
| `revert_revision` | Undo one revision and keep the later ones, like `git revert` (follows the permissions) |
| `restore_revision` | Bring the whole screen back to an earlier revision, like `git restore` (follows the permissions) |
| `get_events` | What happened since the last call: decisions on proposals, commits by anyone, mode and grant changes, emitted events, errors |
| `export_changes` | The revisions after a point as a change set ("plugin") |

Prompt: `serve_studio` (work through the people's requests one after another; the name is kept for existing clients).

**Reading little.** Every node has an id (SCREEN_FORMAT.md, "Every type"), so an agent addresses nodes by name. The
cheap way through a request: `get_screen` with `outline=true` (and `depth`), or `find_nodes`; then `at="#id"` for the
nodes to change; `get_format_guide` with the `topic` it needs. Measured with `tests/perf/tokens.py` (o200k_base), an
outline is 316 to 584 tokens for the templates (a full read 1,548 to 4,197), an outline to depth 2 is 87 to 305, a
re-read after a commit about 20, and the guide's contents 583 (the whole guide 9,221).

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
| `replies.py` | What the agent is told of the host's replies and of messages the server can't use |
| `repository.py` | Where the guide and the `affrescoui` tool are found |

**Queries are tested** without a host: `python3 mcp/test_affrescoui_mcp.py` (ctest `mcp`).

There are no tools to accept or reject proposals, set the mode or revoke grants: only reviewers (the person, in the
app or a client on `studio.sock`, as the template's `permissions.reviewers` says) can, and the host refuses those
messages from `agent`. See
[VERSION_CONTROL.md](../VERSION_CONTROL.md).
