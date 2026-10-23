"""AffrescoUI MCP server: connects an AI agent to a running AffrescoUI host, in both directions.

    requests   affrescoui_studio ──"request"──▶ affrescoui_host ──agent.sock──▶ this server ──channel event──▶ AI agent
                                                                                    (or wait_for_request)
    changes    AI agent ──tools (submit_revision, reply, ...)──▶ this server ──agent.sock──▶ affrescoui_host ──▶ screen

The host (examples/host/host.cpp) renders a screen and doesn't care where changes come from. A person asks for a change in
plain words in the Studio; the request reaches the agent through this server; the agent reads the screen, sends
revisions (JSON Patch), and replies. Revisions follow the screen's permission rules (allow / ask / deny, like
Claude Code's): in the example templates the agent's changes become proposals the person reviews in the Studio,
unless the mode is acceptEdits or the reviewer said "don't ask again" for that kind of change. A proposal is a
branch the agent can keep pushing to. There is no tool to accept or reject: that is the reviewer's decision.

Requests reach the agent two ways:
  - pushed, as Claude Code channel events (research preview: start Claude Code with
    `--dangerously-load-development-channels server:affrescoui`);
  - pulled, with the wait_for_request tool, which works with any MCP client.

Usage:
    python3 mcp/affrescoui_mcp.py [--socket DIR] [--launch "command"]
    claude mcp add affrescoui -- python3 /path/to/AffrescoUI/mcp/affrescoui_mcp.py

DIR is where the host listens: --socket, else $AFFRESCOUI_SOCKET_DIR, else /tmp/affrescoui-<uid> on macOS and Linux
and %LOCALAPPDATA%\\affrescoui\\sockets on Windows. With --launch, the command is started if no host is running. Only
the Python standard library is needed (3.9+). The host is reached through a Unix domain socket (AF_UNIX), which no
released CPython offers on Windows yet: there the server starts, but its tools say why they can't reach the host.
"""

# The package's modules, one concern each (see README.md). Every name the server had when it was one file is
# still here, for the scripts and tests that import it.
from .document import (
    compact, diff, changes_since, by_id, find_node, trees, walk_nodes, node_type, short, summary, TYPE_WORDS,
    describe_type, state_types, outline, locate, pruned, find_nodes, pointer_parts, apply_patch,
)
from .guide import GUIDE_TOPIC, guide_sections, format_guide
from .host import TIMEOUT, MAX_EVENTS, NO_UNIX_SOCKETS, log, default_socket_dir, check_socket_dir, connect_unix, Host
from .replies import scenarios_report, DENIALS, explain_denial, describe, screen_info, malformed, client_name
from .repository import ROOT, guide_path, find_cli, repository_path
from .server import PROTOCOLS, UnknownTool, coordination_tool, Server, main
from .tools import object_schema, TOOLS, COORDINATION_TOOLS, PROMPTS, SERVE_PROMPT, INSTRUCTIONS

__all__ = [
    "compact", "diff", "changes_since", "by_id", "find_node", "trees", "walk_nodes", "node_type", "short", "summary",
    "TYPE_WORDS", "describe_type", "state_types", "outline", "locate", "pruned", "find_nodes", "pointer_parts",
    "apply_patch", "GUIDE_TOPIC", "guide_sections", "format_guide", "TIMEOUT", "MAX_EVENTS", "NO_UNIX_SOCKETS", "log",
    "default_socket_dir", "check_socket_dir", "connect_unix", "Host", "scenarios_report", "DENIALS", "explain_denial",
    "describe", "screen_info", "malformed", "client_name", "ROOT", "guide_path", "find_cli", "repository_path",
    "PROTOCOLS", "UnknownTool", "coordination_tool", "Server", "main", "object_schema", "TOOLS", "COORDINATION_TOOLS",
    "PROMPTS", "SERVE_PROMPT", "INSTRUCTIONS",
]
