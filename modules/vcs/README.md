# vcs: version control

`vcs` keeps a JSON document and its live state under version control, like Git, with approvals like Claude Code's
(`VERSION_CONTROL.md`). `Repository` implements `core`'s `VersionControl`: content-addressed commits (SHA-256), an
append-only `main`, proposals as branches merged with merge commits, revert, restore, diff and log, change sets, and
the permission rules (`allow`, `ask`, `deny`), modes and "don't ask again" grants. It knows nothing about what the
document describes: the `DocumentRules` it is given say what is valid, where `#id` nodes are, and who may change
what. So the same module can keep any JSON document. Target: `affrescoui::vcs` (uses `core` and nlohmann/json).

## Public headers (`include/affresco/vcs/`)

| Header | Offers |
|---|---|
| `Repository.hpp` | `Repository`: the `VersionControl` implementation; `openHistory` (the history on disk) and `OpenedHistory`; `maxOpenProposals` (16 per channel) |

Everything else a user needs is in `core/VersionControl.hpp` (the interface and its value types) and
`core/DocumentRules.hpp`.

## Invariants

- **`main` only grows.** Restore, revert and undo are new commits, so the history is an audit log that can't be
  rewritten.
- **A commit id stands for exactly one history:** the SHA-256 covers the document, the parents, the author and
  committer, the time and the patch.
- **Every change is atomic:** a revision applies whole, or nothing changes (`RevisionStatus::Rejected` says why).
- **History is RFC 6902.** Compact edits (`set`, `add`, `remove`, `move`) are normalised to JSON Patch before they
  are stored.
- **Rules are judged by a change's effect** (the document before and after), never by the paths its patch happens to
  use: deny beats ask beats allow, and what no rule allows asks.
- **An author never approves its own change,** and an approval names the head the reviewer saw: if the branch moved
  since, the approval is refused.
- **The document rules' `checkChange` runs on every change,** so what the format forbids for everyone (widening a
  capability, for example) is refused even for a reviewer.
- **On disk,** objects are verified against their hashes when they are loaded, each distinct document is stored once,
  a file larger than 64 MiB is refused, and one program at a time writes a history folder (a lock the system releases
  if the program dies). In memory, every commit still holds a copy of the document.

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
- **Coming:** history in proportion to the patches (no document copy per commit) and blame (from 4.8a).

## Files

```
include/affresco/vcs/Repository.hpp   the Repository class
src/
  Repository.cpp            commits, main, proposals, review, permissions in use, change sets
  RepositoryHistory.cpp     the history on disk: writing and loading objects and refs
  HistoryStore.hpp / .cpp   the history folder: the lock, atomic writes, bounded reads
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
