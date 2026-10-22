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
- **Atomic:** if anything is wrong, nothing changes, and the error names the operation, the place and a hint:
  `revision.patch[0] (set #nope/color).path: no node with id 'nope' (did you mean '#note'?)`.
- **Ids in the commit:** a node the revision adds without an `id` gets a readable one (`label-blocked`), and a node
  that replaces another keeps that one's id. The stored patch has them, so history, diffs, reverts and change sets
  always show them. A revision that gives a node an id another node has is rejected (the checker's finding).
  `DocumentRules::nameNodes` and `pathHint` are the format's hooks for both.
- **Checked:** a revision that refers to something the screen doesn't define (a state path, a variable, an action, a
  style, a component or its parameter, a font, a colour, a screen), or whose expressions don't parse, is rejected
  before any reviewer sees it, with each place and a hint: `revision: the checker found: root.children[2].text:
  'state.cardz' doesn't exist (did you mean 'state.cards'?)`. Only what the revision brings in counts
  ([SCREEN_FORMAT.md](SCREEN_FORMAT.md), "Errors").
- **`parent`** (optional): the revision it was based on, as a number or a commit id. If `main` has moved on, it is
  rejected as a conflict.
- **`proposal`** (optional): push onto your own open proposal instead of opening a new one.

## Commits and `main`
Every applied change is a commit on `main`. A commit records the document (kept as a delta from its parent's: see
"History in proportion to the changes"), its parents, the author (who wrote
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
  inside a button and approve itself. Nor may it move one to another trigger (from a button into a timer), or add a
  way to run one: a `run` or a handler that names an action with control steps, an action outside the document (the
  application's native actions), or a computed `run` (`{ "run": "{{state.name}}" }`).
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

Proposals already waiting stay until someone decides on them. The mode a reviewer sets is kept with the history, and
the next start uses it. `affrescoui_host`'s `--mode` and `--auto-accept` apply to that run only
(`Screen::setModeForThisRun`), so a one-off `--auto-accept` never sticks.

## Review: proposals are branches
A revision that needs review becomes a **proposal**, the equivalent of a pull request:

1. The author is told `proposed` (with the proposal number and its head commit) and keeps working. Every peer gets the
   new list of proposals (`review`): who, the title, how many commits and operations, **what it touches** (`#board`,
   `/state/columns`), **its changes as lines** a person can read (`set #statusBar/color to @topBar`,
   `add at #card/children/-: {...}`, `remove #badge`), and why it waits.
2. The author can **push** more commits to it (`"proposal": 3` in a revision), e.g. when the person asks for more
   while it is still open. The branch is rebuilt on the current `main` each time.
3. A reviewer can **preview** it (the running app shows exactly what accepting would produce, then goes back) and
   **diff** it (the JSON Patch accepting would apply). While a proposal is previewed, control steps (`call`, `send`,
   `connect`, `patch`, review and history) run only from the user's own input, never from a timer or a handler that
   fires by itself (`onLoad`, `onShow`, `watch`): the proposal's document runs, but it can't act for the user.
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
- **Grants can expire.** An accept can say how long its grants last (`"for": "8h"`: minutes `m`, hours `h` or days
  `d`, at most a year), and a review policy can cap it (`grantsLast`, below). Each grant then carries `expires`
  (UTC); from that time on it no longer allows, and changes like it are proposed again. Without either, a grant lasts
  until a reviewer revokes it.

### Review policies
A screen says what a proposal needs before it merges in `permissions.review`, like a repository's branch protection
and `CODEOWNERS`:

```json
"permissions": {
  "reviewers": ["studio", "human"],
  "review": {
    "approvals": 1,
    "behaviourApprovals": 2,
    "owners": { "#board": ["human/anna"], "/colors": ["studio"] },
    "grantsLast": "8h"
  }
}
```

- `approvals`: how many reviewers must approve (1 when it is missing).
- `behaviourApprovals`: how many when the proposal changes **behaviour**: an action, a shortcut, a watch, `onLoad`, a
  computed value, or a node's handlers (`onClick`...), `keys`, `every`, `check`, `when`, `repeat`, `drag` or bindings.
  Each proposal lists those changes as lines (`behaviour`: "adds onClick of #title", "changes the action 'save'").
- `owners`: parts of the screen and who owns them. A proposal that changes a part needs, besides the count, an approval
  from one of its owners (an author or a channel name).
- `grantsLast`: the longest a "don't ask again" grant lasts.

Each approval counts once per reviewer, and never from the author's own channel. Until the policy is met, an accept
records the approval and the proposal stays open; every peer sees `approvals`, `policy` (what it needs, e.g. "2
approvals (it changes behaviour), one from an owner of #board (human/anna)") and `reason` (what is missing). The
approval that completes the policy merges it. The policy is the one `main` has when the approval arrives, judged on
the change as it would merge then, so neither an older policy nor a rebase lowers the bar.

### Rebasing
When `main` moves on (a commit, an accepted proposal, an import), every open proposal rebases itself onto the new
head with a **three-way merge**: of the document it was built on, `main`'s document now, and its own. Objects merge key
by key, and lists of nodes merge **by id**, so two peers who change, add or remove different nodes never conflict,
wherever the nodes moved. A list reordered differently on both sides, or a value both sides changed differently, is a
conflict.

- **Exact rebase.** If replaying the proposal's commits on the new `main` gives what the merge gives, its commits are
  replayed as they were (new ids, same changes). Approvals stay, and the head a reviewer saw (pinned in the approval,
  "You accept what you saw") still counts: what merges is what was reviewed, plus `main`'s own changes.
- **Merged rebase.** If the commits no longer give the merge (e.g. an operation that adds at a position that has moved),
  the branch becomes one commit with the merge's changes, "Rebase onto ...". That is a change nobody reviewed:
  **its approvals are dismissed**, and an approval naming an older head is refused. Reviewers look again.
- **Conflict.** The proposal stays where it was, and lists its conflicts (`conflicts`: `[{"path", "reason", "base",
  "main", "proposal"}]`, paths from the nearest node, e.g. `#title/text`), which the Studio shows side by side. It
  can't be accepted until its author pushes again (which rebuilds the branch on the current `main`); it can be
  rejected.

### Evidence
A reviewer can ask the host for a proposal's **evidence** (`{"type": "getEvidence", "proposal": 3, "scale": 1}`), and
gets it back as `evidence`:

```json
"evidence": {
  "before": {"format": "png", "data": "...", "width": 1280, "height": 800},
  "after": {"format": "png", "data": "...", "width": 1280, "height": 800},
  "region": [x, y, w, h],
  "behaviour": ["adds onClick of #title"],
  "scenarios": [{"name": "opens", "passed": true, "message": ""}],
  "texts": [{"node": "#title", "original": "Tasks", "modified": "Today", "language": "text"}]
}
```

- `before` and `after` are the screen drawn on `main` and as accepting would leave it, off screen; `region` is the
  smallest rectangle (in those pictures' pixels) holding every pixel that differs, `[0, 0, 0, 0]` when none does.
- `behaviour` is the proposal's lines of behaviour change (as in "Review policies").
- `scenarios` are the results of the screen's scenarios (`<name>.scenarios.json` beside the screen) played on the
  proposal's document, headless: those that need no pictures. The host decides whether to play any.
- `texts` lists each text the proposal changes, added or removes, by node, for a diff view (the Studio shows them side
  by side).

**The host gathers all of it itself**, when asked, from the proposal as it is now: a revision that carries
`evidence` or `behaviour` is refused, so an author can never supply or forge it. Gathering draws two pictures, so it
counts against the same rate as `render`.

## What keeps it safe
- **Nobody approves their own change.** Agents aren't reviewers, the MCP server has no accept tool, revisions can't
  create review controls, and a reviewer can't accept a proposal from its own channel.
- **You accept what you saw.** Every accept names the proposal's head commit: over the live protocol (`head`), in an
  `accept` step (`"head"`) and through `Screen::acceptProposal(id, head)`. If the author pushed since, the approval
  is refused and the proposal stays open for another look.
- **A rebase never carries an approval further than what was reviewed.** It keeps approvals only when the proposal's
  changes are exactly the ones reviewed; otherwise it dismisses them. A conflicted proposal can't be accepted.
- **Deny is final.** Deny rules are checked when a revision arrives *and* again when it is merged, against the
  current `main`.
- **The history can't be rewritten.** `main` only grows, and commit ids chain every commit to its parents.
- **No flooding:** a channel can have at most 16 open proposals.
- **A revision costs in proportion to its size.** Its operations are applied before any rule judges them, so they are
  bounded first: at most 10,000 operations (its compact edits and its JSON Patch together; a proposal with all its
  commits too), at most 100,000 values copied by its `copy` operations in all (a copy of a list into itself doubles
  it), and no object given more than 10,000 members. A screen file may hold a wider object: a revision may change
  it, not widen it.
- **Evidence comes from the host.** Pictures, behaviour lines and scenario results are gathered by the host from the
  proposal itself; a revision can't carry any.
- **Merges never guess:** a proposal that no longer applies to `main` is closed with the reason, and a revert whose
  parts changed later is refused as a conflict.

## History
- **Log:** every commit on `main` with its author, committer, message, time and parents (two for a merged
  proposal). It is available to the screen (`history`) and to agents.
- **Diff** any two revisions, or a proposal against `main`.
- **Revert** a revision: a new commit with its inverse, keeping everything after it (`git revert`). It undoes what the
  commit changed in the document and, in the run that made it, in the live state (the live state starts again with
  every run, so after a restart only the document is reverted). If a later commit, or an action, changed the same
  parts, it is refused as a conflict. **Undo** reverts the latest revision.
- **Restore** a revision: a new commit that makes the whole screen equal to it again (`git restore --source`).
- Reverts and restores made by agents follow the same permissions as any other change.
- **Blame** a node or a value (`git blame`): the commit on `main` that last changed it, with its author, committer
  (who applied or approved it), message and time. The target is a node (`#board`, or `#board/text` inside it) or a
  path (`/colors/accent`). A commit changes a node if it changes the node, something inside it, or where it is (moved,
  or replaced along with what holds it); an item added or removed before it in a list only moves it. A path under
  `/state` gives the last commit whose patch reached that part of the live state (actions that change the state
  aren't commits). What no commit changed gives revision 0.

## History on disk
A screen opened from a file (`Screen::fromFile`, which `affrescoui_host` and the Studio use) keeps its history in
`~/.affrescoui/<file name>.history/`, next to its `persist` state file. Restarting the app brings back `main`, the
open proposals, the grants and the mode. A screen loaded from a string (`fromString`) keeps its history in memory
only, as before, unless `Screen::setHistoryDirectory` (or `vcs::Repository::openHistory`) gives it a folder. Either
is called once, before the first change, so a screen from `fromFile` keeps the folder it was given.

The data folder is `~/.affrescoui` unless `AFFRESCOUI_DATA_DIR` names another (`Screen::defaultDataDirectory`), and
`affrescoui_host` and `affrescoui_studio` take `--data DIR`. Parallel sessions should each use a folder of their
own, as they do for sockets: one history per file name lives there, so two copies of a template that differ would
keep moving each other's history aside. `fromFile(path, dataDirectory, Screen::History::InMemory)` keeps the
history in memory only. The host does that for a plain screenshot run (`AFFRESCOUI_SCREENSHOT` without
`AFFRESCOUI_SCREENSHOT_AFTER`), which draws the file as it is and leaves no history behind.

```
kanban.history/
  lock                  held by the program that writes this history
  refs.json             main's head, the open proposals, the grants, the mode, the change sets applied, the packs,
                        a checksum
  objects/e1/6ecc8085…  commits and documents not yet packed (loose), each named by its SHA-256
  packs/9b2f04c1…       loose objects packed together, named by the SHA-256 of the file
```

- **Objects, like Git's.** Each commit is one object named by its id, holding everything the id covers (the parents,
  author, committer, time, message, patch, and the id of its document, its tree), then its delta (below). A document
  is an object of its own, named by the SHA-256 of its JSON, written only for a snapshot; commits that leave the
  document as it was (a change to the live state, a merge of a branch that main hadn't passed) share it. The commits
  written are those a ref reaches when the history is saved: `main`'s and the open proposals'. Once no ref reaches
  them (a rejected proposal, or a branch before it was rebuilt), they stay but are never read again: nothing deletes
  them yet, as in Git before its garbage collection runs.
- **Packs.** Objects are first written loose, a file each. Once 256 are loose, they are written together into a pack,
  one line per object (`<id> <JSON>`), which the refs then name; only after the refs are on disk do the loose files
  go. Opening a file costs far more than reading a small one, so a history reads a few files, not one per commit.
- **Atomic writes.** Every file is written to a temporary file, synced to disk, then renamed over the old one, and
  the rename is synced too (its folder). Commits are written before the refs that name them, so a crash leaves the
  history as it was before the change or after it. On macOS a sync only reaches the drive, whose cache may write in
  any order, so the refs are written after a barrier (`F_BARRIERFSYNC`). A power cut there may lose the latest
  changes, but never leaves refs that name a lost commit. (`F_FULLFSYNC` would keep them too, at about 4 ms per file.)
  A write that fails is logged; the change stays in memory, and the next change tries again. Nothing is written that
  loading would refuse (a file over 64 MiB, or JSON nested more than 256 levels deep, which a screen file may be):
  the history on disk then stays where it was, and the error says why.
- **Verified on load.** Every commit and document read is hashed again and must match its name, and so must every
  pack. A delta isn't covered by its commit's id (it says how the commit is kept, not what it is), so the document
  that deltas rebuild is hashed and must match the commit's tree: `main`'s head when the history loads, any other
  commit when it is rebuilt. The refs file must match its checksum, and each proposal's branch must lead from its head
  to its base on `main`. A damaged history is never loaded: the app stops with an error that names the file, e.g.
  `AffrescoUI history: …/objects/e1/6ecc8085…: its content doesn't match its SHA-256: the file is damaged`. Moving
  the folder aside starts a new history, and keeps the old one for inspection.
- **Checked again, as if written by hand.** Hashes prove a file isn't damaged, not that a review made it. So `main`'s
  head must pass the same check as a revision against revision 0 (no new channels, `persist`, permissions or control
  steps); grants must be the narrow `<channel>(<one node or area>)` rules that **Always allow** gives; and what each
  open proposal touches, and what **Always allow** would grant for it, are worked out again from its branch instead
  of read from `refs.json`.
- **One writer.** The program that writes a history holds its `lock`, which the system releases if the program ends.
  Another program that opens the same screen file meanwhile loads the same history but never writes it. A warning
  says so (`in use by another program (process 3495)`), and its own changes stay in memory.
- **Private.** The folder is made readable by its owner only (`0700`), and every file in it is created `0600`, never
  through a symbolic link left in its place. A history folder that is a symbolic link, or on POSIX belongs to another
  user (in a shared data folder), is neither read nor written: the history stays in memory, and a warning says why
  (`is a symbolic link, not a folder`, `belongs to another user`).
- **When the screen file changes.** A history belongs to the version of the file it began with: its revision 0. If the
  file differs (a template update, a hand edit, a changed `include`), the old history is moved aside, whole, to
  `<file name>.history.replaced-<UTC time>/`, a warning says where, and a new history starts from the file as it is
  now. Customisations are never replayed onto a file they weren't made for.
- **What it doesn't keep.** The live state is data, not history: after a restart it starts from the document's
  `state`, and then the `persist` paths are loaded from the state file. A preview ends when the app quits.
- **Versions.** This is version 2 of the history (`"version"` in `refs.json`): deltas, snapshots and packs. A
  version 1 history, where every commit's document is an object of its own, still loads, and goes on as version 2;
  what it wrote stays as it was. A program older than version 2 refuses a version 2 history (it isn't a version 1
  history), and changes nothing in it.
- **Cost.** Measured on the Kanban template (macOS, Apple silicon, Release), small commits (a label's text):
  - a change takes about 1 ms with the history in a folder, against 0.26 ms in memory; most of it is waiting for the
    disk (the macOS barrier is about 0.4 ms);
  - 10,000 commits take 6.8 MB on disk, 675 bytes per commit (with version 1: 103 MB, 10,259 bytes per commit, in
    20,004 files);
  - loading them takes 114 ms (with version 1: 2.2 s).

## History in proportion to the changes
A commit doesn't keep a copy of its document. It keeps its **delta**: a JSON Patch from its first parent's document
to its own, worked out exactly (keys in the same order, 1 and 1.0 apart), so that applied to the parent it gives the
document its id names. Lists keep their common start and end, so adding or removing one item is one operation. Every
64th commit along first parents (and revision 0) is also a **snapshot**: it keeps the whole document. Any other
commit's document is rebuilt from the nearest snapshot before it, with at most 63 deltas, and checked against its
tree. `main`'s head is the working copy, so commits, merges and previews never rebuild anything; diffs, restores,
reverts and loading do.

The interval was chosen by measurement on the Kanban template (10,000 small commits; memory kept per commit in
memory, the worst rebuild as a diff of two commits, and loading from disk):

| Snapshot every | Memory per commit | Worst diff | Load of 10,000 |
|---|---|---|---|
| 16 | 3,614 B | 175 µs | 225 ms |
| 32 | 2,463 B | 202 µs | 161 ms |
| **64** | **2,079 B** | **230 µs** | **123 ms** |
| 128 | 1,696 B | 307 µs | 104 ms |
| 256 | 1,312 B | 463 µs | 96 ms |

Each commit kept 39 KB before (a copy of the document): 10,000 commits held 411 MB, and now 21 MB. A commit takes the
same time at commit 100 and at commit 10,000 (248 and 247 µs), and blame reads every commit back to revision 0 in
2.7 ms. The benchmarks are the `history` workloads in `tests/perf`.

## Change sets: customisations as plugins
The commits on `main` after a point can be exported as a **change set**:

```json
{ "format": "affrescoui-changes", "version": 1, "name": "task filter", "from": 0, "to": 3, "base": "35331cd…",
  "head": "e6636b0…",
  "revisions": [ { "author": "mcp/claude-code", "committer": "studio", "message": "Add a filter to the task list",
                   "commit": "e6636b0…", "patch": [ ... ] } ] }
```

Change sets are portable to other copies of the same screen (id-based paths make them robust to layout differences).
Importing one is all or nothing, still checked against the deny rules, and recorded as `plugin:<name>`. The history
also keeps the id of each change set it applied (the SHA-256 of its JSON; `hasImported`), so
`affrescoui_host --plugin x.json` applies a plugin once: on a later start it says so and skips it. This is the
building block for the application-layer vision: start from a rich template, customise it by talking to it, and
share the result as a plugin others can install.

## Where it lives in the API

| Concept | C++ (`affresco::Screen`, or `VersionControl` via `versions()`) | Live protocol | MCP tool |
|---|---|---|---|
| Submit a change | `submitRevision` (permission-checked), `applyRevision` (the app's own) | `revision` | `submit_revision` |
| Proposals | `proposals`, `previewProposal`, `acceptProposal`, `rejectProposal`; `proposalDiff`, `accept(id, Approval)` | `proposed`, `review`; reviewers: `preview`, `endPreview`, `accept` (`head`, `dontAskAgain`), `reject` | push with `proposal` (no accept tool) |
| Modes and grants | `setMode`, `setModeForThisRun`; `grants`, `revokeGrant` | reviewers: `setMode`, `revoke` | (none) |
| History | `log`, `revert`, `undo`, `restore`, `submitRestore`; `resolve`, `diff`, `blame` | `getHistory` (`limit`), `diff`, `blame` (`target`), `revert`, `undo`, `restore`, `committed` | `get_history`, `get_diff`, `get_blame`, `revert_revision`, `restore_revision` |
| Change sets | `exportChanges`, `importChanges`, `hasImported` | `exportChanges` | `export_changes` |
| History on disk | `fromFile` does it; `setHistoryDirectory`; `vcs::Repository::openHistory` | (none) | (none) |
| In the screen file | `permissions` (`allow`, `ask`, `deny`, `reviewers`), `locked`, steps `accept`/`reject`/`preview`/`endPreview`/`restore`/`revert`/`undo`, variables `proposals`/`history`/`preview`/`mode` | | |
