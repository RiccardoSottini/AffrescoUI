# sGLGUI MCP server

Lets an AI agent (Claude, or any [Model Context Protocol](https://modelcontextprotocol.io) client) read and change a
running sGLGUI app:

```
MCP client  ──MCP (JSON-RPC, stdio)──▶  sglgui_mcp.py  ──JSON-lines stream──▶  app --stdio  ──▶  engine
```

The client starts this server; the server starts the app (with `--stdio`) and owns its stream. It needs only the
Python standard library (3.9+).

## Set up

Claude Code:
```sh
claude mcp add sglgui -- python3 /path/to/sGLGUI/mcp/sglgui_mcp.py -- /path/to/sGLGUI/build/chatbot_demo
```

Other clients (JSON configuration):
```json
{ "mcpServers": { "sglgui": { "command": "python3",
  "args": ["/path/to/sGLGUI/mcp/sglgui_mcp.py", "--", "/path/to/sGLGUI/build/chatbot_demo"] } } }
```

Anything after `--` is the app to start. Any app built with the examples' options works, e.g.
`-- /path/to/build/screen_viewer /path/to/my.screen.json`.

## Tools

| Tool | What it does |
|---|---|
| `get_screen` | The document, live state, revision, pending proposals and the agent's author name |
| `get_format_guide` | The screen format specification (`SCREEN_FORMAT.md`) |
| `submit_revision` | A JSON Patch change: applied, proposed for review, or rejected with the reason |
| `get_history` | Every revision: number, author, message, time |
| `restore_revision` | Go back to an earlier revision (a new revision; follows the permissions) |
| `get_events` | What happened since the last call: user events (e.g. chat messages) and decisions on proposals |
| `export_changes` | The revisions after a point as a change set ("plugin") |

There are no tools to accept or reject proposals: the user decides, in the app. The agent's changes are authored as
`mcp/<client name>` and follow the screen's `permissions` for `mcp` (see [GOVERNANCE.md](../GOVERNANCE.md)).
