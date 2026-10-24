# Version control: a history and approvals for a live app

AffrescoUI apps can be changed while they run, by the app itself, by the user, or by outside programs such as an AI agent.
Version control is what makes that safe enough to use: **who may change what, who approves it, what happened, and how to
go back.** It is built into the engine, so every app gets it.

It has two halves:

- **The history:** every applied change is a commit named by its SHA-256 (`a3f9c21` for short). `main` is the live
  branch, what the app shows, and it only grows. A **proposal** is a branch (`proposal/<n>`) its author can keep
  updating; accepting it makes a merge commit whose author is the proposer and whose committer is the reviewer. `log`,
  `diff`, `revert`, `restore` and names like `HEAD~1` work as their names say, and a run of commits exports as a change
  set ("plugin").
- **The approvals:** `allow`, `ask` and `deny` rules like `agent(#board)` in the screen file, where deny beats ask beats
  allow and anything else asks; modes `default`, `acceptEdits` and `plan`, set by the reviewer; **Always allow**, which
  accepts and allows that kind of change until a reviewer revokes it; and **Reject** with a reason, which the agent hears
  and can propose again.

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

- A rule is `author` or `author(pattern)`.
  - **author**: `*` (everyone), a channel name (`agent` covers `agent/claude-code`), or a full name.
  - **pattern**: what the rule covers, with everything inside it. A JSON Pointer (`/colors`, `/state/cards`), a node
    (`#board`, `#board/children`), with `*` for one segment (`/actions/*`). A trailing `/**` means the same as none.
    No pattern covers the whole screen.
- **Precedence:** a matching `deny` refuses the revision. Otherwise a matching `ask` sends it to
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

### Budgets
`permissions.budgets` caps how much one author changes at once: the most nodes it may add, remove and change in one
revision, and in one proposal, which is judged whole when it merges.

```json
"budgets": [
  { "author": "agent", "added": 20, "removed": 5, "changed": 50 },
  { "author": "role:design", "changed": 10, "over": "ask" }
]
```

- `author` is a rule's author without a pattern: `*`, a channel, a full name, or a role (`role:design`, below).
- Nodes are counted by id, before and after, in `root`, `screens` and `templates`: a node is changed when its own
  keys differ (not its children), however the patch was written.
- Over a budget the change is refused (`"over": "deny"`, the default) or sent to review (`"over": "ask"`). The reason
  names the budget: `over the budget "agent: 20 added, 5 removed, 50 changed": 21 new nodes, at most 20`.
- A budget applies to changes that would apply on their own; a change that already goes to review is judged by its
  reviewers. Deny rules still come first.
- At most 64 budgets; each limit is between 0 and 100,000.

### Data rules
`permissions.data` says which paths in `state` and in the sections an author may read, write, or never touch:

```json
"data": [
  { "author": "agent", "read": ["/state/cards", "/state/board"], "write": ["/state/cards"],
    "never": ["/state/token"] }
]
```

- `read`: if given, the only paths of their part (`/state`, `/colors`...) the author reads; its `write` paths are
  readable too. `write`: if given, the only paths of their part it changes. `never`: paths it neither reads nor
  changes. Paths are JSON Pointers, `#id` nodes or `*` patterns, as in rules; state is at `/state`.
- **Writes are judged by effect**, like every rule: a change to a path outside `write`, or to a `never` path, is
  refused with the rule (`data rule: 'agent' may never touch /state/token`), even when the patch never names it.
- **Reads are masked.** An author whose reads are limited gets the screen and the state without what it may not read
  (array elements read as `null`, so positions stay). It can't ask for a diff, export, render, evidence or blame of
  the whole screen, and proposals it sees list no changes. A revision that names an unreadable path (a `test` or a
  `copy` from it would tell its value) is refused.
- At most 64 data rules, each with at most 64 paths of at most 256 bytes.

### Policy tests
A `*.policy.json` file beside a screen holds example revisions, their authors and what they should get:

```json
{
  "screen": "index.screen.json",
  "roles": { "agent/claude-code": "design" },
  "cases": [
    { "name": "the agent recolours a card", "revision": { "author": "agent", "set": { "#card1/color": "#1e293b" } },
      "expect": "allow" },
    { "name": "21 new panels", "revision": { "author": "agent", "add": { "...": {} } }, "expect": "deny",
      "reason": "over the budget" }
  ]
}
```

`affrescoui policy test <files> [--json]` judges each revision against the screen as it is (nothing is applied) and
compares `allow`, `ask` or `deny`, and a part of the reason if given. Exit codes are those of `affrescoui test`: 0 all
passed, 1 a case failed or a file is malformed. CI runs every `*.policy.json` in the repository, and each template has
one. The MCP server explains a denial with the rule that matched.

### Roles
A role says what an author is in this run for (`design`, `data`, `builder`), and rules, budgets and data rules can
name it as `role:design`.

- **Only the host or a reviewer sets a role** (`setRole`), never a revision, and nobody sets the role of their own
  channel. An agent can't give itself one.
- Roles last for the run; a policy test file gives its own (`"roles"`). `role:` never covers a reviewer's decisions
  or a grant.

```json
{ "type": "setRole", "id": 7, "author": "agent/claude-code", "role": "design" }
```

### Leases
Several agents at once claim the parts they work on, so one doesn't undo another's work:

```json
{ "type": "lease", "id": 8, "author": "ada", "target": "/state/columns/0", "seconds": 300 }
{ "type": "leased", "id": 8, "target": "/state/columns/0", "seconds": 300 }
```

- `target` is an `#id` or a JSON Pointer; `seconds` is 1 to 3600 (300 by default). Asking again renews it;
  `release` ends it, and so does the holder's connection closing or the time running out.
- **A revision that changes another holder's part is refused, naming the holder:** `permission denied: 'agent/ada'
  holds a lease on /state/columns/0; wait for it to end`. Judged by effect, like every rule. A part already leased by
  someone else, or overlapping one, can't be leased.
- **A lease can't reach beyond its holder's rules:** a part the holder may not change at all is refused. One name holds
  leases on one connection only.
- A reviewer ends anyone's lease (`endLease`, with `target`). Every `review` message carries `coordination`: who is
  connected and their role, the leases with the seconds left, and the roles. A reviewer's client shows it, and ends a
  lease or gives a role with `endLease` and `setRole`; the MCP server has `lease_part`, `renew_lease` and `release_part`.
- At most 256 peers and 256 leases, 16 per holder and 16 per connection; targets are at most 256 bytes, holder names 64.

### Usage
Usage summaries say which controls people use, so whoever improves a screen can see what runs, what fails and what
nobody touches. They are **off by default**: `affrescoui_host --usage` switches them on, and the host asks the person
once, on the terminal, whether to count (without a terminal or a "yes", nothing is counted; the answer is kept).

- Counted per node id and action (`onClick`, `onClick:addCard`): runs and errors, the first and last time; the screens
  navigated to; and, when asked, the controls (nodes with an id and a handler or a `bind`) never used.
- **Never content:** ids as the document writes them, action and screen names, counts and times. No text, values,
  state or pictures. Kept only on the device, in `<data folder>/<screen>.usage.json` beside the "persist" state; counts
  not made for 30 days are dropped.
- Bounded: 1,024 node and action pairs, 64 screens, names of 1 to 64 printable bytes, counts up to 10^9.
- **Only a reviewer reads them,** and only while they are on: `getUsage` answers `{ "type": "usage", "usage": {...} }`,
  `clearUsage` forgets the counts (the consent stays). An agent can't read them, nor grant itself that. The MCP
  server has `get_usage`.

## Modes
A reviewer sets how much they are asked with a mode (`affrescoui_host --mode`, or `setMode` over the live
protocol):

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

Who reviews is up to the application. In the examples it is the person, on the proposal's card in the app's
assistant chat, or in any client connected on `studio.sock` (author `studio`). A screen can also have its own review
controls (`accept`, `reject`, `preview` steps).

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

- `approvals`: how many reviewers must approve (1 when it is missing). Reviewers count once per channel: "studio/anna"
  and "studio/bob" are one approval, since the channel names its own users.
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
  "main", "proposal"}]`, paths from the nearest node, e.g. `#title/text`), for a reviewer to see side by side. It
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
- `texts` lists each text the proposal changes, added or removes, by node, for a diff view that shows them side by
  side.

**The host gathers all of it itself**, when asked, from the proposal as it is now: a revision that carries
`evidence` or `behaviour` is refused, so an author can never supply or forge it. Gathering draws two pictures, so it
counts against the same rate as `render`.

### Inspector
A reviewer can turn on the host's **inspector** (`{"type": "inspect", "on": true}`): a
debug overlay on the screen's window. For the panel under the pointer it shows its node, bounds, bindings as written
(`{{state.count}}`, never the value) and handlers, and the commit that last changed it (`blame`); then the last frame's
time and draw calls, the last update's time and panels rebuilt and created, and the latest events: clicks and scrolls
by node, keys such as Enter or Escape (never a typed character), sends by channel (never the message), commits and
decisions. It is off by default, a peer that may not review can't turn it on, and the overlay is never in a picture
drawn for a peer. The host answers `done`, or `rejected` with no window or no inspector. In C++:
`Screen::setInspector()` and `Screen::inspectorTrace()`.

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
A screen opened from a file (`Screen::fromFile`, which `affrescoui_host` uses) keeps its history in
`~/.affrescoui/<file name>.history/`, next to its `persist` state file. Restarting the app brings back `main`, the
open proposals, the grants and the mode. A screen loaded from a string (`fromString`) keeps its history in memory
only, as before, unless `Screen::setHistoryDirectory` (or `vcs::Repository::openHistory`) gives it a folder. Either
is called once, before the first change, so a screen from `fromFile` keeps the folder it was given.

The data folder is `~/.affrescoui` unless `AFFRESCOUI_DATA_DIR` names another (`Screen::defaultDataDirectory`), and
`affrescoui_host` takes `--data DIR`. Parallel sessions should each use a folder of their
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

- **Objects.** Each commit is one object named by its id, holding everything the id covers (the parents,
  author, committer, time, message, patch, and the id of its document, its tree), then its delta (below). A document
  is an object of its own, named by the SHA-256 of its JSON, written only for a snapshot; commits that leave the
  document as it was (a change to the live state, a merge of a branch that main hadn't passed) share it. The commits
  written are those a ref reaches when the history is saved: `main`'s and the open proposals'. Once no ref reaches
  them (a rejected proposal, or a branch before it was rebuilt), they stay but are never read again: nothing deletes
  them yet.
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
{ "format": "affrescoui-changes", "version": 2, "name": "task filter", "from": 0, "to": 3, "base": "35331cd…",
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

## Self-repairing change sets
A change set can carry what it was made for, the template it was made on, and how to tell it still works:

```json
{ "format": "affrescoui-changes", "version": 2, "name": "blocked column", "revisions": [ ... ],
  "request": "Add a Blocked column before Done",
  "template": { "name": "flow/index.screen.json", "commit": "35331cd…", "tree": "9f1c…" },
  "proof": { "scenarios": [ { "name": "blocked", "steps": [ { "expect": "...", "equals": "Blocked" } ] } ] } }
```

These fields travel with the change set when it is exported and imported, and its signature covers them (it covers
the whole change set but `signature`). A host installs such a change set with `ChangeSetRepairs::install`
(`Screen::installChanges`): if its patch no longer applies to the template, or its scenarios fail on the result, the
change set is marked **needs repair** with the reason, left off, and the rest of the screen is untouched. The host
then asks the peers for a repair: a `request` of kind `repair` (the MCP server's `wait_for_request` returns it) with
the request in words, the old and the new base, what failed, the revisions and the proof. The agent answers with
one revision (`{"type": "repair", "request": id, "revision": {...}}`, the MCP tool `answer_repair`); the host checks
it applies and passes the proof, then **always** opens it as a proposal, whatever the mode, and the permission rules
check it as any other revision. Once a reviewer accepts it, the change set is re-recorded on the new base, with
`repairedFrom` naming the change set it replaces (its SHA-256), its old base and the proposal; it carries no
signature until its publisher signs it again. One repair runs at a time; a change set whose repair is refused,
rejected or fails is asked for again, at most three times, and then gives up until a reviewer retries it
(`{"type": "repair", "retry": name}`). `{"type": "repair"}` alone answers with
every installed change set's status (`{"type": "repairs", "changeSets": [...]}`), which is also sent to every peer
when it changes.

## Features: signed packages
A feature (RFC 0004) is a signed manifest that a catalogue lists. Its whole lifecycle is revisions on
`/features/<name>`, so the history, blame, undo and the permission rules cover it like any other change:
- **install** and **upgrade** go through the `features` service (`features.install {name, version}`,
  `features.upgrade {name}`): it fetches the manifest, checks its digest and signature against a publisher the host
  trusts, and submits the record as a revision by `features`, which the permissions review like any author's;
- **enable**, **disable** and **remove** are plain revisions: `replace /features/<name>/enabled`, `remove
  /features/<name>`.

An upgrade never changes the publisher, and a feature can never widen a capability or bring a control step.

## Installing across apps
A feature from another app (a part another app ships, such as Sinopia's slide sorter) installs the same way, with its
needs bound to what the host offers ([Screen format](SCREEN_FORMAT.md), "Features from another app"):
- **prove**: `affrescoui check --compose <screen> <feature> [--adapter <file>]`, the MCP tool `check_composition`, or
  `features.compose {manifest, adapter, source}` binds the needs, checks the composed document and plays the
  scenarios. A failed proof installs nothing and names each unmet need;
- **install**: a proven composition is one revision on `/features/<name>` (or `/features`, when the section is
  missing), its message naming the proof (`Install slideSorter from app:sinopia (proof: slideSorter 1.0.0 from
  app:sinopia, passed, ...)`) and its record carrying the bindings and the proof with the base commit. It is reviewed
  under the document's `permissions` like any author's: Tempera proposes what `features` and `agent` write, and a
  reviewer accepts it;
- **after**: history and blame show the install, and undo or `remove /features/<name>` takes it out. Signatures and
  trusted publishers apply unchanged to a signed manifest.

Over the channel, the revision's author is the connection's: an agent's install through MCP (`install_feature`) is the
agent's, an in-screen `features.compose` is `features`'. A person can ask an agent to install from another app (a
`request`): it reads the contributions, writes the adapter, submits it, and gives the proof in its reply.

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
