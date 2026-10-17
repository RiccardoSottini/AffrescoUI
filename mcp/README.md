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

The server connects to the host's `agent.sock` (in `/tmp/affrescoui-<uid>` by default), so its changes are authored as
`agent/<client name>`. It needs only the Python standard library (3.9+).

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

## Tools

| Tool | What it does |
|---|---|
| `wait_for_request` | The next request from the Studio (`request_id`, `from`, `text`), waiting for it if needed |
| `reply` | Answer a request in the Studio |
| `get_screen` | The document (with its permission rules), live state, revision and head commit, open proposals, mode, grants and the agent's author name |
| `get_format_guide` | The screen format specification (`SCREEN_FORMAT.md`) |
| `submit_revision` | A JSON Patch change: applied (a commit), proposed (a branch for review), or rejected with the reason. `proposal` pushes onto the agent's open proposal |
| `get_history` | main's commits, like `git log`: number, short id, author, committer (who approved it), message, parents |
| `get_diff` | A JSON Patch between two revisions (`HEAD~1`, ids, numbers), or what an open proposal would change |
| `revert_revision` | Undo one revision and keep the later ones, like `git revert` (follows the permissions) |
| `restore_revision` | Bring the whole screen back to an earlier revision, like `git restore` (follows the permissions) |
| `get_events` | What happened since the last call: decisions on proposals, commits by anyone, mode and grant changes, emitted events, errors |
| `export_changes` | The revisions after a point as a change set ("plugin") |

Prompt: `serve_studio` (work through the Studio's requests one after another).

There are no tools to accept or reject proposals, set the mode or revoke grants: only reviewers (the Studio, as the
template's `permissions.reviewers` says) can, and the host refuses those messages from `agent`. See
[VERSION_CONTROL.md](../VERSION_CONTROL.md).
