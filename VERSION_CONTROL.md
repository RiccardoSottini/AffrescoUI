# Version control: who may change a live app, and how

sGLGUI apps can be changed while they run, by the app itself, by the user, or by outside programs such as an AI agent.
Version control is what makes that safe enough to use: **who may change what, who approves it, what happened, and how to
go back.** It is built into the engine, so every app gets it.

## Revisions: every change is a commit
A revision is one change to the running screen:

```json
{ "author": "mcp/claude-code", "message": "Add a filter to the task list", "parent": 7,
  "patch": [ { "op": "add", "path": "#taskList/children/0", "value": { "id": "filter", "type": "textarea" } },
             { "op": "add", "path": "/state/filter", "value": "" } ] }
```

- The patch is standard **JSON Patch (RFC 6902)**. Paths can address nodes by **id** (`#taskList/...`), so they
  survive layout changes. `/state/...` changes live data; everything else changes the document (layout, styles,
  actions, templates, fonts, colors).
- **Atomic:** if anything is wrong, nothing changes, and the error says exactly what and where.
- **`parent`** (optional) is the revision it was based on. If the screen has moved on, it is rejected as a conflict.

## Permissions: who may change what
The screen file decides, in a `permissions` section that no revision can change:

```json
"permissions": {
	"authors": { "studio": "allow", "app": "allow", "agent": "review", "*": "review" },
	"reviewers": ["studio"],
	"locked": ["#statusBar", "/actions/send", "/state/account"]
}
```

- **Policies per author:** `allow` (apply immediately), `review` (propose to the user) or `deny`. Lookup order:
  the exact author, then its channel name (`agent` for `agent/claude-code`), then `*`. Without a `permissions` section,
  everything is allowed.
- **Authors are assigned by the channel.** A program on a channel is that channel's author (`studio`, `agent`),
  whatever it calls itself; the host decides, e.g. by which socket a peer connects to. Its own name is only kept for
  display (`agent/claude-code`). Nothing can claim to be `app` or a reviewer.
- **Reviewers** (`reviewers`) are the authors who may decide on proposals and switch auto-accept over the live
  protocol, e.g. the person's client. Without a `permissions` section anyone may; with one, only those listed.
- **Locked parts** can never be changed by a revision: nodes listed by id (or marked `"locked": true`), any JSON
  Pointer in the document, or paths in the state.
- **Protected controls:** no revision may add or alter steps that control changes (`accept`, `reject`, `preview`,
  `endPreview`, `restore`, `undo`, `patch`, `call`). Otherwise an agent could hide an "accept" inside a button and
  approve itself.

## Review before apply
With `review`, a revision becomes a **proposal** instead of a change:

1. The author is told `proposed` and keeps working. Every peer is sent the new list of proposals (`review`), and the
   screen itself can show it (the `proposals` variable).
2. A reviewer can **preview** it: the running app shows the proposed version live, then goes back.
3. The reviewer **accepts** (it is committed as a revision, still checked against the rules) or **rejects** it.
4. Every peer, the author included, is told the decision (`applied` / `rejected` with the proposal id).

Who reviews is up to the application: in the examples it is the person, in the **sGLGUI Studio** (a separate
client); a screen can also have its own review controls (`accept`, `reject`, `preview` steps). Approving is never
the author's own decision: an agent isn't a reviewer, the MCP server has no accept tool, and revisions can't create
review controls.

### Auto-accept
A reviewer can switch **auto-accept** on (`setAutoAccept`, or `Screen::setAutoAccept` / `sglgui_host --auto-accept`):
revisions that would wait for review are applied right away instead. Everything else still holds: `deny` authors are
refused, locks and protected controls are enforced, and every change lands in the history, where it can be restored.
Proposals already waiting stay until someone decides on them.

## History as a feature
Every committed revision is kept with its author, message, time and patch:

- **Audit log:** what changed, who changed it, why, and when. It's available to the screen itself (`history`) and to
  agents.
- **Restore any revision:** going back creates a *new* revision equal to the old one, so the history only grows and
  nothing is lost. `undo` restores the previous one.
- Restores made by agents follow the same permissions as any other change.

## Change sets: customisations as plugins
The revisions after a point can be exported as a **change set**:

```json
{ "format": "sglgui-changes", "version": 1, "name": "task filter", "from": 0, "to": 3,
  "revisions": [ { "author": "mcp/claude-code", "message": "Add a filter to the task list", "patch": [ ... ] } ] }
```

Change sets are portable to other copies of the same screen (id-based paths make them robust to layout differences).
Importing one is all or nothing, still checked against the rules, and recorded as `plugin:<name>`. This is the
building block for the application-layer vision: start from a rich template, customise it by talking to it, and
share the result as a plugin others can install.

## Where it lives in the API

| Concept | C++ (`sgl::Screen`) | Stream protocol | MCP tool |
|---|---|---|---|
| Submit a change | `submitRevision` (permission-checked), `applyRevision` (app's own) | `revision` | `submit_revision` |
| Proposals | `proposals`, `previewProposal`, `acceptProposal`, `rejectProposal` | `proposed`, `review`; reviewers: `preview`, `endPreview`, `accept`, `reject` | (none: reviewers only) |
| Auto-accept | `setAutoAccept` | reviewers: `setAutoAccept` | (none) |
| History | `history`, `restore`, `submitRestore`, `undo` | `getHistory`, `restore`, `undo`, `committed` | `get_history`, `restore_revision` |
| Change sets | `exportChanges`, `importChanges` | `exportChanges` | `export_changes` |
| In the screen file | `permissions` (`authors`, `reviewers`, `locked`), `locked`, steps `accept`/`reject`/`preview`/`endPreview`/`restore`/`undo`, variables `proposals`/`history`/`preview` | | |
