# AffrescoUI MCP server

Connects an AI agent (Claude Code, or any [Model Context Protocol](https://modelcontextprotocol.io) client) to a
running AffrescoUI host, **in both directions**:

```
requests   affrescoui_studio ──"request"──▶ affrescoui_host ──agent.sock──▶ affrescoui_mcp.py ──channel event──▶ AI agent
                                                                                   (or wait_for_request)
changes    AI agent ──tools──▶ affrescoui_mcp.py ──agent.sock──▶ affrescoui_host ──revision──▶ rendering engine
                                                                       └──review, reply──▶ affrescoui_studio
```

- **Into the agent:** a person types a request in the AffrescoUI Studio ("add a dark theme"). The host passes it on to
  this server, which pushes it into the agent's session as a Claude Code **channel event**, and also keeps it for the
  `wait_for_request` tool (for any MCP client).
- **Out of the agent:** the agent reads the screen and sends revisions (JSON Patch) with the tools. They go through
  the host's permission rules: for the templates they become proposals a reviewer merges or rejects in the Studio,
  unless the mode is `acceptEdits` or the reviewer said "don't ask again" for that kind of change. The agent answers the person with `reply`.

The server connects to the host's `agent.sock`, so its changes are authored as `agent/<client name>`. It needs only the
Python standard library (3.9+). The socket directory is found the way the host finds it:
1. `--socket DIR`;
2. else `$AFFRESCOUI_SOCKET_DIR`;
3. else `/tmp/affrescoui-<uid>` on macOS and Linux, and `%LOCALAPPDATA%\affrescoui\sockets` on Windows.

## Set up

1. Build and start the host (and, to send requests and review, the Studio):
   ```sh
   cmake -S . -B build && cmake --build build
   ./build/affrescoui_host &        # the Kanban board template
   ./build/affrescoui_studio &      # the client
   ```
2. Register the server with Claude Code:
   ```sh
   claude mcp add affrescoui -- python3 /path/to/AffrescoUI/mcp/affrescoui_mcp.py
   ```
   From a release, the server is also a Python package (`mcp/pyproject.toml`; [RELEASING.md](../RELEASING.md)):
   `pipx install affrescoui-mcp`, then `claude mcp add affrescoui -- affrescoui-mcp`.
3. Start Claude Code so requests are pushed into the session (channels are a research preview; a server you add
   yourself needs the development flag):
   ```sh
   claude --dangerously-load-development-channels server:affrescoui
   ```
   Requests from the Studio now arrive as `<channel source="affrescoui" request_id="request-1" sender="studio">`.

Without channels (or with another MCP client), ask the agent to use the `serve_studio` prompt, or just tell it to
"wait for requests from the AffrescoUI Studio and handle them": it loops on `wait_for_request`.

Options: `--socket DIR` if the host listens elsewhere (`affrescoui_host --socket DIR`), and `--launch "command"` to start
the host if none is running, e.g. `--launch "/path/to/build/affrescoui_host"`.

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
1. Build and start the host and the Studio (Windows 10 version 1803 or later). By default they meet in
   `%LOCALAPPDATA%\affrescoui\sockets`, a folder in your profile that only you (and administrators) can open:
   ```powershell
   cmake -S . -B build
   cmake --build build --config Release
   Start-Process .\build\Release\affrescoui_host.exe
   Start-Process .\build\Release\affrescoui_studio.exe
   ```
2. Register the server with Claude Code. Use `py` (the Python launcher) or `python`, since `python3` is often missing
   on Windows:
   ```powershell
   claude mcp add affrescoui -- py C:\path\to\AffrescoUI\mcp\affrescoui_mcp.py
   ```
   In a JSON configuration, double the backslashes: `"args": ["C:\\path\\to\\AffrescoUI\\mcp\\affrescoui_mcp.py"]`.
3. `--socket DIR` and `AFFRESCOUI_SOCKET_DIR` work as on the other platforms. `--launch` takes a Windows command line
   as it is, e.g. `--launch "C:\path\to\build\Release\affrescoui_host.exe"`.

## Tools

| Tool | What it does |
|---|---|
| `wait_for_request` | The next request from the Studio (`request_id`, `from`, `text`), waiting for it if needed |
| `reply` | Answer a request in the Studio |
| `get_screen` | The document (with its permission rules), live state, revision and head commit, open proposals, mode, grants and the agent's author name. Later calls return only what changed (`full` for everything). `outline` gives the node tree, one line per node: its address, type, text, `bind`, `onClick`, `repeat` and `when`, without the styling. `at` gives one node (`"#id"` or a JSON Pointer). `depth` limits both: outline lines say `+12` for what is below, and a node's deeper children become their addresses |
| `find_nodes` | Nodes by `type` (or component), `text` (in their text, placeholder or tooltip, any case) or `property` (with a `value`), one outline line each with its parent |
| `get_catalog` | What the screen's format offers: node types and properties, events, steps, functions, and the app's registered types |
| `get_format_guide` | The screen format specification (`SCREEN_FORMAT.md`), a part at a time: its introduction and topics, or one `topic` (a heading or a word of one), or `"all"` |
| `submit_revision` | A JSON Patch change: applied (a commit), proposed (a branch for review), or rejected with the reason. `proposal` pushes onto the agent's open proposal |
| `render_screen` | A picture (PNG image block) of main, or of an open proposal applied (`proposal`), at `scale` 1 or 2: the agent checks its work before the reviewer does. Drawn offscreen, so the person's window doesn't change. Read-only |
| `get_history` | main's commits, like `git log`: number, short id, author, committer (who approved it), message, parents |
| `get_diff` | A JSON Patch between two revisions (`HEAD~1`, ids, numbers), or what an open proposal would change |
| `revert_revision` | Undo one revision and keep the later ones, like `git revert` (follows the permissions) |
| `restore_revision` | Bring the whole screen back to an earlier revision, like `git restore` (follows the permissions) |
| `get_events` | What happened since the last call: decisions on proposals, commits by anyone, mode and grant changes, emitted events, errors |
| `export_changes` | The revisions after a point as a change set ("plugin") |

Prompt: `serve_studio` (work through the Studio's requests one after another).

**Reading little.** Every node has an id (SCREEN_FORMAT.md, "Every type"), so an agent addresses nodes by name. The
cheap way through a request: `get_screen` with `outline=true` (and `depth`), or `find_nodes`; then `at="#id"` for the
nodes to change; `get_format_guide` with the `topic` it needs. Measured with `tests/perf/tokens.py` (o200k_base), an
outline is 316 to 584 tokens for the templates (a full read 1,548 to 4,197), an outline to depth 2 is 87 to 305, a
re-read after a commit about 20, and the guide's contents 583 (the whole guide 9,221).

**Queries are tested** without a host: `python3 mcp/test_affrescoui_mcp.py` (ctest `mcp`).

There are no tools to accept or reject proposals, set the mode or revoke grants: only reviewers (the Studio, as the
template's `permissions.reviewers` says) can, and the host refuses those messages from `agent`. See
[VERSION_CONTROL.md](../VERSION_CONTROL.md).
