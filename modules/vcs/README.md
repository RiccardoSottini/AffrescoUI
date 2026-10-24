# vcs: version control

`vcs` keeps a JSON document and its live state under version control, with approvals (`VERSION_CONTROL.md`). `Repository` implements `core`'s `VersionControl`: content-addressed commits (SHA-256), an
append-only `main`, proposals as branches merged with merge commits, revert, restore, diff and log, change sets, and
the permission rules (`allow`, `ask`, `deny`), modes and "don't ask again" grants, and blame. A commit keeps a delta
from its parent's document, with a whole document every 64th commit, so the history grows with the changes. It knows nothing about what the
document describes: the `DocumentRules` it is given say what is valid, where `#id` nodes are, and who may change
what. So the same module can keep any JSON document. Target: `affrescoui::vcs` (uses `core` and nlohmann/json).

## Public headers (`include/affresco/vcs/`)

| Header | Offers |
|---|---|
| `Repository.hpp` | `Repository`: the `VersionControl` implementation; `openHistory` (the history on disk) and `OpenedHistory`; `blame`; `maxOpenProposals` (16 per channel); `snapshotInterval` (64) |
| `Leases.hpp` | `Leases`: a part of a document (an `#id` or a pointer) one person holds for a while, bounded; `LeasedRules`: a document's rules plus a deny rule for everyone else per running lease |
| `Roles.hpp` | `Roles`: the role each author plays for this run (`design`, `data`, `builder`), set by the host or a reviewer, never by a revision; `ruleCovers`: whether a rule's author (`role:<name>` too) covers an author |
| `ChangeSetRepair.hpp` | `ChangeSetRepairs`: change sets installed with their request, base and proof, checked on install, and repaired by an agent through review; `RepairStatus`, `InstalledChangeSet` |
| `Presence.hpp` | `Presence`: who is on a document, where and doing what, bounded; shown, never trusted |

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
- **Proposals follow main** (`rebaseProposals`, `RepositoryReview.cpp`): when main moves, each open proposal rebases
  through `detail::merge` (`Merge.hpp`), keyed by node id. Approvals survive only an exact rebase (replaying its
  commits gives the merge); a merged rebase dismisses them; a conflict leaves the branch where it was, listed in
  `conflicts`, and blocks accepting it.
- **A review policy is met before a merge** (`DocumentRules::permissions().review`): approvals from distinct reviewers,
  more for behaviour changes (`DocumentRules::behaviourLines`), and one from an owner of each owned part changed. It
  is read from main when each approval arrives. Grants may carry `expires`; expired ones no longer allow.
- **Evidence is never the author's:** a revision carrying `evidence` or `behaviour` is refused; the host gathers both
  (VERSION_CONTROL.md, "Evidence").
- **The document rules' `checkChange` runs on every change,** so what the format forbids for everyone (widening a
  capability, for example) is refused even for a reviewer.
- **On disk** (history version 2), commits carry their deltas, documents are written for snapshots only, and every
  256 loose objects go into a pack named by its SHA-256, which the refs name before the loose files are deleted.
  Objects and packs are verified against their hashes when they are loaded, a file larger than 64 MiB is refused,
  and one program at a time writes a history folder (a lock the system releases if the program dies). Version 1
  histories still load.

## Assets, run steps and attachments

Files a revision brings (a picture an agent drew, a song the person attached), steps it runs once when it lands, and
files the person gives the app for agents to use (VERSION_CONTROL.md, "Assets"). The names, types and bounds are in
`core/Assets.hpp`; the API:

```cpp
// A revision, through submit(), propose(), commit(), judge(), or pushed onto a proposal
{ "author": "agent", "message": "A night sea to paint on",
  "assets": [ { "name": "moon-sea.svg", "type": "image/svg+xml", "text": "<svg ...>" },   // text: SVG, JSON, text
              { "name": "beat.mp3", "type": "audio/mpeg", "data": "<base64>" },            // bytes, in base64
              { "name": "beat.mp3", "attachment": "att-1-3f9c21aa0b4e" } ],                 // what the person attached
  "run": [ { "image": "open", "of": "canvas", "with": { "asset": "moon-sea.svg" } } ],   // once, on the live screen
  "set": { "#cover/file": "asset:moon-sea.svg" } }                                       // compact edits, as ever

// The working copy's assets (the proposal's while it is previewed): VersionControl, implemented by Repository
std::vector<affresco::AssetInfo> assets() const;                          // {name, type, size, sha256}, by name
std::shared_ptr<const std::string> assetBytes(std::string_view name) const; // nullptr: no such asset
// The person's files (an attach step, a drop, Screen::attach): never a revision or a peer
affresco::Attached attach(std::string_view name, std::string_view type, std::string bytes, json details);
std::optional<affresco::AttachmentInfo> attachment(std::string_view id) const; // {id, name, type, size, details}
static constexpr std::string_view Repository::personChannel = "user";      // the only reviewer of such proposals
// CommitInfo::run: the steps a commit ran (null if none); ProposalInfo::assets, ::run, ::person: what review shows
// DocumentRules hooks the format implements: personLines(before, after), checkRun(document, steps),
// checkAsset(name, type, bytes) -> details
```

- **The manifest is part of the document**: `"assets": {"moon-sea.svg": {"type", "size", "sha256"}}` at the top level.
  A revision's `assets` become `add /assets/<name>` operations first in its stored patch, so commits, diffs, reverts,
  restores, proposals, previews, rebases and blame cover them as any other change; the commit id covers the manifest,
  so it covers every asset's SHA-256. A patch may also remove an asset or put back an entry (a revert, a restore),
  whose bytes the history already holds; an entry naming bytes it doesn't hold, or the wrong size or type, is refused.
- **Bytes are content-addressed blobs** (SHA-256 of the bytes), checked against their type when a revision brings them
  (`assetBytesProblem`, then `DocumentRules::checkAsset`), kept in memory, and on disk under `blobs/<ab>/<cd…>`, never
  packed. Read back, a blob is hashed again and refused if it doesn't match.
- **`run`** is kept in the commit (its id covers it), a proposal's `run` is its commits' in order, and the merge commit
  carries it: `onCommit` gives it (`CommitInfo::run`) to the host, which runs it once (the Screen does). A restore, a
  revert, a preview or loading the history never runs it.
- **Only the person accepts** a change that brings, removes or changes assets, carries `run`, or has
  `DocumentRules::personLines`: it is always a proposal (no allow rule, grant, `acceptEdits` or `plan` lets it
  through, and "don't ask again" can't remember it), and only a reviewer of the channel `personChannel` ("user": the
  screen's own review steps, `Screen::acceptProposal`) may accept it, never its own author. The app's own commits
  (`commit()`) and the person's own revisions aren't asked.
- **Attachments** wait in memory for this run only, at most `maxAttachments` and `maxAttachmentBytes` (the oldest go
  first); an asset naming one copies its bytes into the history. Nothing else keeps them.

## Threads and ownership

Main thread only. The repository has no threads of its own; peers reach it through `channel::Session`, which runs on
the main thread. File writes happen inside the change that causes them.

The `Repository` borrows its `DocumentRules` (they must outlive it) and owns everything else: the commits, the
proposals, the grants and the history folder's lock, which it holds until it is destroyed. Its observer is
borrowed too; `setObserver(nullptr)` before the observer goes.

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
include/affresco/vcs/
  Repository.hpp            the Repository class
  Leases.hpp, Roles.hpp, Presence.hpp, ChangeSetRepair.hpp   what several agents at once add
src/
  revisions/                a revision, from what it says to a commit on main
    Revisions.hpp / .cpp    what a revision says: author, message, patch; the commit time
    Budget.hpp / .cpp       what preparing one revision may cost
    Deltas.hpp / .cpp       exact deltas between documents, applying them safely, JSON Pointer segments
    Repository.cpp          commits and main; checking, judging and landing a revision
    RepositoryDocuments.cpp rebuilding a commit's document from a snapshot and deltas; blame; restore and revert
  review/                   proposals and what it takes to merge them
    RepositoryProposals.cpp opening, extending, previewing, accepting and rejecting proposals
    RepositoryReview.cpp    rebasing proposals onto a newer main, approvals and review policies, expiring grants
    Merge.hpp / .cpp        the three-way JSON merge, lists of nodes merged by id
    ChangeSetRepair.cpp     installing change sets with a proof, asking for repairs, re-recording them once accepted
  permissions/              who may change what
    Permissions.hpp / .cpp  how a rule sees a change: by its effect on the paths it covers
    Policy.hpp / .cpp       budgets and data rules: nodes counted by id, paths an author may read, write or never touch
    PolicyRepository.cpp    rules, reviewers and modes in use; what a read-limited author sees
    Roles.cpp               the roles of this run, and rules naming them
  peers/                    who is on the document
    Leases.cpp              parts of a document one person holds for a while
    LeasesRepository.cpp    who is connected, the parts they lease, and refusing changes to another holder's part
    Presence.cpp            who has the document open, and where
  history/                  the history on disk
    HistoryStore.hpp / .cpp the history folder: the lock, atomic writes, bounded reads, packs
    RepositoryHistory.cpp   writing and loading objects and refs, every hash checked
    RepositoryChanges.cpp   the log, and exporting, checking and importing change sets
  CompositionProofRevision.hpp / .cpp   the revision that installs a feature from another app
  Reasons.hpp               an error's reason without its prefix, for messages that wrap it
  Sha256.hpp                SHA-256 (forwards to core's Sha256.hpp)
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

**Add a field to commits or proposals.** Add it to `CommitInfo` or `ProposalInfo` (`core/VersionControl.hpp`) and to its
protocol JSON there; fill it in `revisions/Repository.cpp` (commits) or `review/RepositoryProposals.cpp` (proposals). If
it is part of what a commit is, it changes the hash: ask the maintainer first, because existing histories on disk would
no longer load.

**Add a permission behaviour.** Rules are judged in `Repository::evaluate` (`permissions/PolicyRepository.cpp`) using
`permissions/Permissions.cpp`. Changes to who may do what touch the security model: ask first, update
`VERSION_CONTROL.md` and `SECURITY.md`, and add unit tests to `tests/unit/vcs.cpp`.
