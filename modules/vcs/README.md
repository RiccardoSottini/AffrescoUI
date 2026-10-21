# vcs: version control

`vcs` keeps a JSON document and its live state under version control, like Git, with approvals like Claude Code's
(`VERSION_CONTROL.md`). `Repository` implements `core`'s `VersionControl`: content-addressed commits (SHA-256), an
append-only `main`, proposals as branches merged with merge commits, revert, restore, diff and log, change sets, and
the permission rules (`allow`, `ask`, `deny`), modes and "don't ask again" grants, and blame. A commit keeps a delta
from its parent's document, with a whole document every 64th commit, so the history grows with the changes. It knows nothing about what the
document describes: the `DocumentRules` it is given say what is valid, where `#id` nodes are, and who may change
what. So the same module can keep any JSON document. Target: `affrescoui::vcs` (uses `core` and nlohmann/json).

## Public headers (`include/affresco/vcs/`)

| Header | Offers |
|---|---|
| `Repository.hpp` | `Repository`: the `VersionControl` implementation; `openHistory` (the history on disk) and `OpenedHistory`; `blame`; `maxOpenProposals` (16 per channel); `snapshotInterval` (64) |

Everything else a user needs is in `core/VersionControl.hpp` (the interface and its value types) and
`core/DocumentRules.hpp`.

## Invariants

- **`main` only grows.** Restore, revert and undo are new commits, so the history is an audit log that can't be
  rewritten.
- **A commit id stands for exactly one history:** the SHA-256 covers the document, the parents, the author and
  committer, the time and the patch. Not the delta, which says how the commit is kept: a document rebuilt from
  deltas is checked against the commit's tree.
- **A delta rebuilds its document exactly** (`detail::delta`, `Deltas.hpp`): keys in order, 1 and 1.0 apart, which
  nlohmann's `json::diff` doesn't. main's head is the working copy, so commits never rebuild; diffs, restores, reverts
  and loading do, from at most 63 deltas.
- **A revert undoes what the commit changed,** in the document and, in the run that made it, in the live state; it is
  refused if a later commit or an action changed those parts since.
- **Every change is atomic:** a revision applies whole, or nothing changes (`RevisionStatus::Rejected` says why).
- **A revision is bounded before it is applied** (`detail::RevisionBudget`, `Budget.hpp`): 10,000 operations (a
  proposal with all its commits too), 100,000 values copied by `copy` operations, and objects of at most 10,000
  members. Its operations run before any permission rule judges them, so whatever they cost, any peer can make the
  app pay it; and an object finds a key by looking at its members one by one.
- **A history folder is private and its owner's:** files `0600` (never through a symbolic link), the folder `0700`;
  one that is a symbolic link, or another user's, is neither read nor written.
- **History is RFC 6902.** Compact edits (`set`, `add`, `remove`, `move`) are normalised to JSON Patch before they
  are stored, and so are the ids the document rules give to nodes an operation adds without one
  (`DocumentRules::nameNodes`): the stored patch is what was applied, so replaying it gives the same ids.
- **A refused operation says where:** the error names the operation, the path as written and resolved, and the
  document rules' hint (`DocumentRules::pathHint`, "did you mean '#title'?").
- **A revision's size is checked first** (`DocumentRules::checkRevision`), before any operation is applied.
- **Rules are judged by a change's effect** (the document before and after), never by the paths its patch happens to
  use: deny beats ask beats allow, and what no rule allows asks.
- **Some changes are always reviewed:** after the deny rules, a change the document rules list in
  `DocumentRules::reviewLines` (a new step that writes files or runs a program) becomes a proposal, whatever the allow
  rules, grants or mode say; the reason lists them. The app's own changes and merges aren't reviewed again.
- **An author never approves its own change,** and an approval names the head the reviewer saw: if the branch moved
  since, the approval is refused.
- **The document rules' `checkChange` runs on every change,** so what the format forbids for everyone (widening a
  capability, for example) is refused even for a reviewer.
- **On disk** (history version 2), commits carry their deltas, documents are written for snapshots only, and every
  256 loose objects go into a pack named by its SHA-256, which the refs name before the loose files are deleted.
  Objects and packs are verified against their hashes when they are loaded, a file larger than 64 MiB is refused,
  and one program at a time writes a history folder (a lock the system releases if the program dies). Version 1
  histories still load.

## Threads

Main thread only. The repository has no threads of its own; peers reach it through `channel::Session`, which runs on
the main thread. File writes happen inside the change that causes them.

## Errors

- Inside, a malformed revision or a refused change throws `DocumentError` naming the place
  (`revision.patch[2].path: ...`). The public calls turn it into a result: `RevisionResult` with `Rejected` and an
  `error`, a `Decision` with an `error`, or `std::optional<std::string>` (empty when it worked) from `commit`,
  `accept`, `preview` and `importChanges`.
- An invalid document given to the constructor, or a damaged history given to `openHistory`, throws `DocumentError`
  (`AffrescoUI history: <file>: <problem>`): there is nothing to fall back to.
- A history that can't be written is logged, and the repository keeps working in memory
  (`OpenedHistory::memoryOnly` says why).

## Extension points

- **Any document format:** implement `DocumentRules` (`core`) and give it to the `Repository`.
- **Observers:** `setObserver` (one `VersionObserver`: the renderer), and the signals `onCommit`, `onDecision` and
  `onReviewChanged` for everyone else.

## Files

```
include/affresco/vcs/Repository.hpp   the Repository class
src/
  Repository.cpp            commits, main, proposals, review, permissions in use, change sets
  RepositoryHistory.cpp     the history on disk: writing and loading objects and refs
  RepositoryDocuments.cpp   rebuilding a commit's document from a snapshot and deltas; blame; the live state's undo
  Deltas.hpp / .cpp         exact deltas between documents, applying them safely, JSON Pointer segments
  HistoryStore.hpp / .cpp   the history folder: the lock, atomic writes, bounded reads, packs
  Permissions.hpp / .cpp    how a rule sees a change: by its effect on the paths it covers
  Reasons.hpp               an error's reason without its prefix, for messages that wrap it
  Sha256.hpp / .cpp         SHA-256
CMakeLists.txt              the affrescoui_vcs target and its sources
```

## Recipes

**Keep any JSON document under version control.**

```cpp
MyRules rules;                                  // a DocumentRules for your format
affresco::vcs::Repository repository(rules, document);
repository.openHistory(dataDirectory / "notes.history"); // optional: keep it across restarts
const affresco::RevisionResult result = repository.submit(affresco::parseUntrusted(R"({
	"author": "agent", "message": "Rename", "patch": [{"op": "replace", "path": "/title", "value": "Notes"}]})"));
```

**Add a field to commits or proposals.** Add it to `CommitInfo` or `ProposalInfo` (`core/VersionControl.hpp`) and to
its protocol JSON there; fill it in `Repository.cpp`. If it is part of what a commit is, it changes the hash: ask the
maintainer first, because existing histories on disk would no longer load.

**Add a permission behaviour.** Rules are judged in `Repository::evaluate` (`Repository.cpp`) using
`Permissions.cpp`. Changes to who may do what touch the security model: ask first, update `VERSION_CONTROL.md` and
`SECURITY.md`, and add unit tests to `tests/unit/vcs.cpp`.
