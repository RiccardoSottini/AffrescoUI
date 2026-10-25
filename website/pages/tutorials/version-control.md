# Version control and permissions

Every change to a running screen, from a person, an agent or the app itself, is a **revision**: compact edits that are
checked, judged by the screen's permission rules, and committed to an append-only history. The full design is in
[the version-control reference](../reference/version-control.md); this page walks through it.

## Rules in the screen file

```json
--8<-- "recipes/permissions.screen.json"
```

- `allow` lets the person's own client (`studio`) change anything, and an agent change the colours and the `#notes`
  node.
- `ask` sends an agent's change to the actions to review, even where `allow` would cover it.
- `deny` refuses any change to `#footer`, from anyone. `"locked": true` on the node says the same.
- `reviewers` are the authors who may accept or reject proposals, set the mode and revoke grants.

A matching `deny` wins, then `ask`; otherwise a revision applies only if `allow` rules cover everything it changes, and
anything else waits for review. Rules judge what a change does (the screen before and after), not how its patch is
written. No revision can change `permissions`, add steps that accept or reject changes, or give the screen new access
to the outside world (`channels`, `persist`, `call`, `send`, `connect`).

Authors are named by the channel a peer connects on (`studio` for the person's client, `agent` for agents), not by
what the peer calls itself.

## Revisions

```json
{ "message": "Warmer accent", "parent": 3,
  "set": { "/colors/accent": "#ea580c", "#notes/text": "Changed by an agent" } }
```

- `set`, `add` (`/-` appends), `remove` and `move`, or a JSON Patch in `patch`.
- Paths are JSON Pointers, or start at a node's id (`#notes/text`).
- A revision is atomic: if one operation is wrong, nothing changes, and the error names the operation and gives a hint.
- It is checked like a screen file: a reference to something the screen doesn't define is refused before review.
- `parent` makes it a conflict if `main` has moved on since.

## Modes

| Mode | What happens |
|---|---|
| `default` | The rules decide. A change no rule allows waits for review. |
| `acceptEdits` | A change that would wait only because no rule allows it applies. `ask` and `deny` still hold. |
| `plan` | Authors who aren't reviewers can only propose: their `allow` rules and grants are suspended. |

`affrescoui_launcher --mode acceptEdits` sets it for one run; a reviewer can set it over the live protocol, and the
mode a reviewer sets is kept with the history.

## History

Each commit has a SHA-256 id, its author, the committer (who applied or approved it), a message and its patch. Name a
revision by number, commit id prefix, `HEAD~2` or `proposal/3`. Revert, undo and restore add commits, so the history
is an audit log nobody can rewrite. Blame tells which commit last changed a node or a value. A screen opened from a
file keeps its history on disk (`~/.affrescoui`, `%APPDATA%\affrescoui` on Windows, or `--data DIR`), so a restart
brings back `main`, the open proposals, the grants and the mode.

## Change sets

The commits after a point can be exported and applied to another copy of the same screen, as a plugin:

```sh
affrescoui export app.screen.json --from 0 --name "task filter" -o task-filter.json
affrescoui import other/app.screen.json task-filter.json
affrescoui_launcher app.screen.json --plugin task-filter.json
```

Importing is all or nothing, checked against the deny rules, recorded as `plugin:<name>`, and applied only once.

Next: [services and connections](services-connections.md).
