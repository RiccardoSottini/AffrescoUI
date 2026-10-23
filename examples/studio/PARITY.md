# The Studio's parity matrix

The Studio is modelled on GitHub Desktop and on GitHub's pull-request review. Its repository is the connected app's
document: main's history is the repository's history, a proposal is a pull request (author, message, changed lines,
evidence pictures), and accepting one approves and merges it, pinned to the head that was reviewed.

Status: **done** (a daily user finds it where they expect it and it works the same), **partial** (there, but looks or
works differently), **missing**. The Studio's own parts that its counterparts don't have are listed last.

| Area | Feature | In the Studio | Start | Now |
|---|---|---|---|---|
| Toolbar | Current repository drop-down: list, filter, add | The connections (hosts, document servers) | partial | partial |
| Toolbar | Current branch drop-down: filter, default, recent and other branches | A filter, then main under "Default branch" and the open proposals under "Proposals"; no recent group | missing | partial |
| Toolbar | Fetch / pull / push button with counts and "last fetched" | Refresh from the host, reconnect when offline | missing | partial |
| Sidebar | Changes and History tabs with counts | Proposals with its count, History; rows of two lines (message, then number, date and author); text tabs, no icons | partial | partial |
| Changes | Changed files with check boxes and status icons; select all | Direct-change and permission drafts | missing | missing |
| Changes | Summary and description, "Commit to main" | Message of a direct change, apply | partial | partial |
| Changes | Nothing-changed state with suggestions | | partial | partial |
| Changes | Undo the last commit | Undo | partial | partial |
| History | Commits with avatar, summary, author and relative time | Main's revisions, two lines: message, then author, date, short id, approver; no avatar | partial | partial |
| History | The chosen commit: header, files and their diffs | A revision's changed sections against its parent | missing | partial |
| History | Revert a commit; restore | Revert, restore | partial | partial |
| Diff | Unified view: old and new line numbers, added and removed lines coloured | A proposal's sections, as formatted JSON | missing | partial |
| Diff | Split view | Unified or split, old on the left and new on the right, remembered | partial | partial |
| Diff | Hunk headers and expanding the context | Headers; the whole file on a click, not a few lines at a time | missing | partial |
| Diff | Hide whitespace changes; diff options | Unified and split as buttons and Hide whitespace as a check box above the files, not a menu | missing | partial |
| Diff | Large-diff and binary placeholders | | missing | missing |
| Branches | Compare a branch with main | From the branch menu: the proposal's Files changed page | missing | partial |
| Branches | Merge conflicts dialog | A proposal that conflicts with main | partial | partial |
| Branches | Stash changes | Set the direct change aside and restore it; one stash | missing | partial |
| App | Menu bar (File, Edit, View, Repository, Branch, Help) | The system's menu bar, each item a command (disabled when it can't run); titles in English only, none drawn where there is no system bar | missing | partial |
| App | Options: appearance and language | Settings | partial | partial |
| App | Repository settings | Mode, grants, permissions | partial | partial |
| App | Keyboard shortcuts (tabs, commit, lists) | | partial | partial |
| App | No-repository and offline states | Not connected, retrying every 2 s | partial | partial |
| Pull request | Header: title, number, state, "wants to merge N commits into main" | A proposal, with a green "Open" pill; main and the branch in code type (conflicts show in the merge box) | missing | partial |
| Pull request | Conversation, Commits, Checks, Files changed tabs with counts | Checks are the evidence; under the Studio's own tabs | missing | partial |
| Pull request | Conversation: description and timeline of commits and reviews | Opened, changes, pushed, approvals, reviews, policy, conflicts; a comment box at the end | missing | partial |
| Pull request | Sidebar: reviewers, state of each | Who approved or asked for changes, what the policy needs; only in wide windows | missing | partial |
| Pull request | Files changed: file tree, per-file +/− counts, "Viewed" | Per-file counts and five blocks sized as GitHub does, collapse, Viewed; no tree | missing | partial |
| Pull request | Comments on a line, with suggestions | | missing | missing |
| Pull request | Review box: Comment, Approve, Request changes | "Review changes" on Files changed opens it: a text, the three kinds stacked with a line each, Submit review; comments and change requests go to the agents as requests | partial | partial |
| Pull request | Checks: what ran, with its results | Evidence: each scenario with a pass or fail mark, pictures before and after | partial | partial |
| Pull request | Merge box states: conflicts, blocked, ready, merged | On Conversation only: ready, review required, changes requested, checks failed, blocked (the refusal), conflicts (grey, with Merge proposal disabled), merging; a merged notice; no Resolve conflicts button | partial | partial |
| Pull request | Merge button with options | "Merge proposal", green when ready, disabled while conflicts, change requests or failed checks hold it; "Always allow" under it; no squash or rebase | partial | partial |
| Pull request | Close the pull request | "Close proposal", grey, beside Comment in the comment box, asks first | partial | partial |
| Pull request | Check out the pull request | Preview, with a Previewing pill; the panel keeps its colours | partial | partial |

**Counts at the start:** 0 done, 19 partial, 17 missing.
**Counts now:** 0 done, 33 partial, 3 missing.

## The Studio's own parts

Requests to the agents (the composer, which is the Studio's request client), the connection state, the approval mode
and rules, the activity log, the inspector of the connected app's nodes and the agents' leases and usage. They stay,
each where a daily user of the counterpart would look for something like it.
