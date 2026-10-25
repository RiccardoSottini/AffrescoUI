# An agent changes a running app

The host renders a screen and listens for peers on local sockets: `studio.sock` for the person's own client and
`agent.sock` for agents. The MCP server connects an agent to `agent.sock` **in both directions**: requests typed in
the app's assistant are pushed into the agent's session, and the agent's changes go back to the host as revisions,
which the permission rules judge.

## Set up

Start the host on an app, with a socket folder of your own (it keeps parallel sessions apart; without `--socket` the
host and the server meet in `/tmp/affrescoui-<uid>`):

```sh
./build/affrescoui_launcher examples/flow/index.screen.json --socket out/s
```

Register the MCP server with Claude Code, on the same folder, and start Claude Code with channels so requests arrive
in the session:

```sh
claude mcp add affrescoui -- python3 /path/to/AffrescoUI/mcp/affrescoui_mcp.py --socket out/s
claude --dangerously-load-development-channels server:affrescoui
```

Channels are a research preview of Claude Code. Without them, or with another MCP client, tell the agent to wait for
requests from the app: it loops on the `wait_for_request` tool. The server needs only the Python standard library.
Details: [`mcp/README.md`](https://github.com/RiccardoSottini/AffrescoUI/blob/master/mcp/README.md).

## Ask for a change

Open the board's assistant (the round button in its bottom right corner) and type *“add a Blocked column between In
progress and In review”*. The agent reads the screen (`get_screen`, `find_nodes`), writes a revision and sends it
with `submit_revision`. A revision is compact edits on paths. The board's lanes are state, so the heart of this one
adds an entry to `state.lanes`:

```json
{ "message": "Add a Blocked column",
  "add": { "/state/lanes/launch/2": { "name": "Blocked", "color": "#dc2626" } } }
```

Paths can also name nodes by id, such as `#title/text`.

It is checked before anyone sees it: a path to a node that doesn't exist, or a reference the screen doesn't define, is
refused with the place and a hint, and nothing changes. The agent can draw the result (`render_screen`) and play the
screen's scenarios (`run_scenarios`) to prove it, then answer you with `reply`.

## Review it in the app

The board's rules allow the person's own edits, ask about an agent's changes to actions and deny any change to the
status bar. A change from an agent that no rule allows waits for review, so it arrives as a **proposal**: a branch you
preview live in the app while the agent keeps refining it. It comes back as a card in the assistant's conversation:

- **Accept** merges it into `main`; the app updates in place, keeping focus and typing.
- **Reject** with a reason: the agent hears the reason and can propose again.
- A reviewing client can also accept with **Always allow**, which stops asking about that part of the screen until
  you revoke the grant.

Every commit keeps its author and approver, and any commit can be reverted or restored (`get_history`,
`revert_revision` and `restore_revision` over MCP). Start the host with `--mode acceptEdits` to let changes that no
rule asks about apply as they arrive.

Next: [version control and permissions](version-control.md). Every tool is in [the MCP reference](../reference/mcp.md).
