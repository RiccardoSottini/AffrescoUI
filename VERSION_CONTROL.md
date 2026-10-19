# Version control: Git for a live app, with approvals like Claude Code's

AffrescoUI apps can be changed while they run, by the app itself, by the user, or by outside programs such as an AI agent.
Version control is what makes that safe enough to use: **who may change what, who approves it, what happened, and how to
go back.** It is built into the engine, so every app gets it.

It borrows from two places:

- **Git** for the history: content-addressed commits, a `main` branch, proposals as branches that get merged, `log`,
  `diff`, `revert`, `restore`.
- **Claude Code's tool permissions** for the approvals: `allow` / `ask` / `deny` rules, permission modes, and "Yes,
  and don't ask again".

| Git | AffrescoUI |
|---|---|
| a commit, named by its hash | every applied change is a commit, named by its SHA-256 (`a3f9c21` for short) |
| `main` | the live branch: what the app shows. It only grows: no reset, no force-push |
| a pull request from a branch | a **proposal**: branch `proposal/<n>`, which its author can keep pushing to |
| `git merge --no-ff` | accepting a proposal: a merge commit whose author is the agent and whose committer is the reviewer |
| `git log`, `git diff`, `git revert`, `git restore --source`, `HEAD~1` | the same ideas, same names |
| `git format-patch` / `git am` | change sets ("plugins") |

| Claude Code | AffrescoUI |
|---|---|
| `allow`, `ask`, `deny` rules like `Bash(npm test)` | rules like `agent(#board)` in the screen file |
| deny beats ask beats allow; anything else asks | the same |
| modes `default`, `acceptEdits`, `plan` | the same names, set by the reviewer |
| "Yes, and don't ask again for …" | **Always allow**: accept, and allow that kind of change until a reviewer revokes it |
| "No, and tell Claude what to do differently" | **Reject** with a reason; the agent hears it and can propose again |

## Revisions
A revision is one change to the running screen:

```json
{ "author": "mcp/claude-code", "message": "Add a filter to the task list", "parent": "a3f9c21",
  "add": { "#taskList/children/0": { "id": "filter", "type": "textarea", "bind": "state.filter" } },
  "set": { "/state/filter": "" } }
```

- Changes are written as **compact edits**: `set` (replace, or add where nothing is yet), `add` (insert; `/-` appends),
  `remove` (a list of paths) and `move` (from → to), applied in that order. Or as standard **JSON Patch (RFC 6902)**
  in `patch`, applied first, when the order of operations matters. Either way they are stored as plain JSON Patch, so
  history, diffs, reverts and change sets have one form.
- Operations apply one at a time. Paths can address nodes by **id** (`#taskList/...`, or `#filter` for the node
  itself), so they survive layout changes, even nodes an earlier operation in the same revision added. `/state/...`
  changes live data; everything else changes the document (layout, styles, actions, components, screens, fonts,
  colors).
- **Atomic:** if anything is wrong, nothing changes, and the error names the operation and the place:
  `revision.patch[0] (set #nope/color).path: no node with id 'nope'`.
- **`parent`** (optional): the revision it was based on, as a number or a commit id. If `main` has moved on, it is
  rejected as a conflict.
- **`proposal`** (optional): push onto your own open proposal instead of opening a new one.

## Commits and `main`
Every applied change is a commit on `main`. A commit records the whole document, its parents, the author (who wrote
the change), the committer (who applied it: the author, or the reviewer who accepted it), the time, the message and
the patch (compact edits written out as JSON Patch). Its id is the **SHA-256** of all of that, parents included, so an id stands for exactly one
history.

- Revision **numbers** are positions on `main`: `0` is the file as loaded.
- A revision can be named by its number, its commit id or any unique prefix of 4+ characters, `main` / `HEAD`,
  `HEAD~2` (two back on `main`), or `proposal/3` (that proposal's newest commit).
- `main` is append-only. Undo, restore and revert all add commits, so the history is an **audit log** nobody can
  rewrite, including the reviewer.
- The live state (`/state`) is data, not code: commits keep the document, and state changes aren't undone.

## Permissions: allow, ask, deny
The screen file decides, in a `permissions` section that no revision can change:

```json
"permissions": {
	"allow": ["studio"],
	"ask": ["agent(/actions)"],
	"deny": ["*(#statusBar)"],
	"reviewers": ["studio"]
}
```

- A rule is `author` or `author(pattern)`, like Claude Code's `Tool(specifier)`.
  - **author**: `*` (everyone), a channel name (`agent` covers `agent/claude-code`), or a full name.
  - **pattern**: what the rule covers, with everything inside it. A JSON Pointer (`/colors`, `/state/cards`), a node
    (`#board`, `#board/children`), with `*` for one segment (`/actions/*`). A trailing `/**` means the same as none.
    No pattern covers the whole screen.
- **Precedence, as in Claude Code:** a matching `deny` refuses the revision. Otherwise a matching `ask` sends it to
  review. Otherwise it applies only if `allow` rules (and grants, below) cover *everything* it changes. Anything else
  is reviewed. Without a `permissions` section every change applies and anyone may review.
- **Rules judge what a change does, not how its patch is written.** The engine compares the screen before and after.
  A protected node that ends up identical wasn't changed, even if its siblings moved. A change to it is caught even
  when the patch replaces a whole parent list. And a change outside the allowed areas is caught even if the patch
  never names it.
- **Locked nodes:** `"locked": true` on a node is short for `deny: ["*(#id)"]`. Deny rules hold for everyone,
  including the app's own changes and the reviewer.
- **Protected controls:** no revision may change `permissions`, or add or alter steps that control changes (`accept`,
  `reject`, `preview`, `endPreview`, `restore`, `revert`, `undo`, `patch`). Otherwise an agent could hide an "accept"
  inside a button and approve itself.
- **Protected capabilities:** no revision may change `channels` (the programs the screen talks to) or `persist` (what
  it writes to disk), or add or alter `call`, `send` and `connect` steps. An agent can rearrange how the app uses the
  outside world, never give it new access to it.
- **Authors are assigned by the channel.** A program on a channel is that channel's author (`studio`, `agent`),
  whatever it calls itself. The host decides that, e.g. by which socket a peer connects to. The name a peer gives
  itself is kept after a `/` for display (`agent/claude-code`), and it is chosen by the peer. Give rules to channel
  names: those are the trusted part.
- **Reviewers** (`reviewers`) are the authors who may decide on proposals, set the mode and revoke grants. Without a
  `permissions` section anyone may; with one, only those listed.

## Modes
A reviewer sets how much they are asked, like Claude Code's permission modes (`affrescoui_host --mode`, the Studio's
switch, or `setMode` over the live protocol):

| Mode | What happens |
|---|---|
| `default` | The rules decide. A change no rule allows waits for review. |
| `acceptEdits` | A change that would wait only because no rule allows it is applied. `ask` and `deny` rules still hold. (`--auto-accept`) |
| `plan` | Authors who aren't reviewers can only propose: their `allow` rules and grants are suspended. |

Proposals already waiting stay until someone decides on them. The mode is kept with the history: `affrescoui_host`
starts in the mode it had when it quit, unless `--mode` (or `--auto-accept`) says otherwise.

## Review: proposals are branches
A revision that needs review becomes a **proposal**, the equivalent of a pull request:

1. The author is told `proposed` (with the proposal number and its head commit) and keeps working. Every peer gets the
   new list of proposals (`review`): who, the title, how many commits and operations, **what it touches** (`#board`,
   `/state/columns`), **its changes as lines** a person can read (`set #statusBar/color to @topBar`,
   `add at #card/children/-: {...}`, `remove #badge`), and why it waits.
2. The author can **push** more commits to it (`"proposal": 3` in a revision), e.g. when the person asks for more
   while it is still open. The branch is rebuilt on the current `main` each time.
3. A reviewer can **preview** it (the running app shows exactly what accepting would produce, then goes back) and
   **diff** it (the JSON Patch accepting would apply).
4. The reviewer **accepts** it: a merge commit lands on `main`, still checked against the deny rules, with the agent
   as author and the reviewer as committer. Or **Always allow**: accept it and add a **grant** (below). Or **reject**
   it with a reason.
5. Every peer, the author included, hears the decision (`applied` / `rejected` with the proposal number).

Who reviews is up to the application. In the examples it is the person, in the **AffrescoUI Studio** (a separate
client). A screen can also have its own review controls (`accept`, `reject`, `preview` steps).

### "Don't ask again": grants
Accepting with **Always allow** (`"dontAskAgain": true`) adds a rule for exactly what the proposal touched, for its
author's channel: e.g. `agent(/state/columns)` and `agent(#board)`. From then on, changes from that channel that stay
inside those areas apply without asking.

- Grants are never written into the screen. They are kept with the history, so for a screen opened from a file they
  survive a restart ("History on disk", below). Reviewers see and **revoke** them.
- They are allow rules, so `deny` and `ask` rules still win, and `plan` mode suspends them.
- Not every proposal can be remembered. A proposal can't be remembered if it touches parts an `ask` rule covers, or
  if it changes something as broad as the whole layout (`/root`) or a whole section (`/actions`).

## What keeps it safe
- **Nobody approves their own change.** Agents aren't reviewers, the MCP server has no accept tool, revisions can't
  create review controls, and a reviewer can't accept a proposal from its own channel.
- **You accept what you saw.** Over the live protocol, `accept` must name the proposal's head commit. If the author
  pushed since, the approval is refused and the proposal stays open for another look.
- **Deny is final.** Deny rules are checked when a revision arrives *and* again when it is merged, against the
  current `main`.
- **The history can't be rewritten.** `main` only grows, and commit ids chain every commit to its parents.
- **No flooding:** a channel can have at most 16 open proposals.
- **Merges never guess:** a proposal that no longer applies to `main` is closed with the reason, and a revert whose
  parts changed later is refused as a conflict.

## History
- **Log:** every commit on `main` with its author, committer, message, time and parents (two for a merged
  proposal). It is available to the screen (`history`) and to agents.
- **Diff** any two revisions, or a proposal against `main`.
- **Revert** a revision: a new commit with its inverse, keeping everything after it (`git revert`). If a later
  commit changed the same parts, it is refused as a conflict. **Undo** reverts the latest revision.
- **Restore** a revision: a new commit that makes the whole screen equal to it again (`git restore --source`).
- Reverts and restores made by agents follow the same permissions as any other change.

## History on disk
A screen opened from a file (`Screen::fromFile`, which `affrescoui_host` and the Studio use) keeps its history in
`~/.affrescoui/<file name>.history/`, next to its `persist` state file. Restarting the app brings back `main`, the
open proposals, the grants and the mode. A screen loaded from a string (`fromString`) keeps its history in memory
only, as before, unless `Screen::setHistoryDirectory` (or `vcs::Repository::openHistory`) gives it a folder. Either
is called once, before the first change, so a screen from `fromFile` keeps the folder it was given.

```
kanban.history/
  lock                  held by the program that writes this history
  refs.json             main's head, the open proposals, the grants, the mode, and a checksum
  objects/e1/6ecc8085…  commits and documents, each named by its SHA-256
```

- **Objects, like Git's.** Each commit is one file named by its id, holding everything the id covers: the parents,
  author, committer, time, message, patch, and the id of its document (its tree). Each document is a file of its own,
  named by the SHA-256 of its JSON, so commits that leave the document as it was (a change to the live state, a merge
  of a branch that main hadn't passed) share one. The commits written are those a ref reaches when
  the history is saved: `main`'s and the open proposals'. Once no ref reaches them (a rejected proposal, or a branch
  before it was rebuilt), their files stay but are never read again: nothing deletes them yet, as in Git before its
  garbage collection runs.
- **Atomic writes.** Every file is written to a temporary file, synced to disk, then renamed over the old one, and
  the rename is synced too (its folder). Commits are written before the refs that name them, so a crash leaves the
  history as it was before the change or after it.
  A write that fails is logged; the change stays in memory, and the next change tries again. Nothing is written that
  loading would refuse (a file over 64 MiB, or JSON nested more than 256 levels deep, which a screen file may be):
  the history on disk then stays where it was, and the error says why.
- **Verified on load.** Every commit and document read is hashed again and must match its name. The refs file must match its
  checksum, and each proposal's branch must lead from its head to its base on `main`. A damaged history is never
  loaded: the app stops with an error that names the file, e.g.
  `AffrescoUI history: …/objects/e1/6ecc8085…: its content doesn't match its SHA-256: the file is damaged`. Moving
  the folder aside starts a new history, and keeps the old one for inspection.
- **One writer.** The program that writes a history holds its `lock`, which the system releases if the program ends.
  Another program that opens the same screen file meanwhile loads the same history but never writes it. A warning
  says so (`in use by another program (process 3495)`), and its own changes stay in memory.
- **When the screen file changes.** A history belongs to the version of the file it began with: its revision 0. If the
  file differs (a template update, a hand edit, a changed `include`), the old history is moved aside, whole, to
  `<file name>.history.replaced-<UTC time>/`, a warning says where, and a new history starts from the file as it is
  now. Customisations are never replayed onto a file they weren't made for.
- **What it doesn't keep.** The live state is data, not history: after a restart it starts from the document's
  `state`, and then the `persist` paths are loaded from the state file. A preview ends when the app quits.
- **Cost.** On the Kanban template, saving adds about 0.6–1 ms to each change (two synced files), each commit takes
  about 10 KB on disk (the whole document, as in memory), and loading 200 commits takes about 45 ms.

## Change sets: customisations as plugins
The commits on `main` after a point can be exported as a **change set**:

```json
{ "format": "affrescoui-changes", "version": 1, "name": "task filter", "from": 0, "to": 3, "base": "35331cd…",
  "head": "e6636b0…",
  "revisions": [ { "author": "mcp/claude-code", "committer": "studio", "message": "Add a filter to the task list",
                   "commit": "e6636b0…", "patch": [ ... ] } ] }
```

Change sets are portable to other copies of the same screen (id-based paths make them robust to layout differences).
Importing one is all or nothing, still checked against the deny rules, and recorded as `plugin:<name>`. This is the
building block for the application-layer vision: start from a rich template, customise it by talking to it, and
share the result as a plugin others can install.

## Where it lives in the API

| Concept | C++ (`affresco::Screen`, or `VersionControl` via `versions()`) | Live protocol | MCP tool |
|---|---|---|---|
| Submit a change | `submitRevision` (permission-checked), `applyRevision` (the app's own) | `revision` | `submit_revision` |
| Proposals | `proposals`, `previewProposal`, `acceptProposal`, `rejectProposal`; `proposalDiff`, `accept(id, Approval)` | `proposed`, `review`; reviewers: `preview`, `endPreview`, `accept` (`head`, `dontAskAgain`), `reject` | push with `proposal` (no accept tool) |
| Modes and grants | `setMode`; `grants`, `revokeGrant` | reviewers: `setMode`, `revoke` | (none) |
| History | `log`, `revert`, `undo`, `restore`, `submitRestore`; `resolve`, `diff` | `getHistory` (`limit`), `diff`, `revert`, `undo`, `restore`, `committed` | `get_history`, `get_diff`, `revert_revision`, `restore_revision` |
| Change sets | `exportChanges`, `importChanges` | `exportChanges` | `export_changes` |
| History on disk | `fromFile` does it; `setHistoryDirectory`; `vcs::Repository::openHistory` | (none) | (none) |
| In the screen file | `permissions` (`allow`, `ask`, `deny`, `reviewers`), `locked`, steps `accept`/`reject`/`preview`/`endPreview`/`restore`/`revert`/`undo`, variables `proposals`/`history`/`preview`/`mode` | | |
