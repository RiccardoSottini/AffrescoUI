# The Studio's parity matrix

The Studio is modelled on GitHub Desktop and on GitHub's pull-request review. Its repository is the connected app's
document: main's history is the repository's history, a proposal is a pull request (author, message, changed lines,
evidence pictures), and accepting one approves and merges it, pinned to the head that was reviewed.

Status: **done** (a daily user finds it where they expect it and it works the same), **partial** (there, but looks or
works differently), **missing**. The Studio's own parts that its counterparts don't have are listed last.

| Area | Feature | In the Studio | Start | Now |
|---|---|---|---|---|
| Toolbar | Current repository drop-down: list, filter, add | The connections (hosts, document servers) | partial | partial |
| Toolbar | Current branch drop-down: filter, default, recent and other branches | main and the open proposals | missing | partial |
| Toolbar | Fetch / pull / push button with counts and "last fetched" | Refresh from the host, reconnect when offline | missing | partial |
| Sidebar | Changes and History tabs with counts | | partial | done |
| Changes | Changed files with check boxes and status icons; select all | Direct-change and permission drafts | missing | missing |
| Changes | Summary and description, "Commit to main" | Message of a direct change, apply | partial | partial |
| Changes | Nothing-changed state with suggestions | | partial | partial |
| Changes | Undo the last commit | Undo | partial | partial |
| History | Commits with avatar, summary, author and relative time | Main's revisions | partial | partial |
| History | The chosen commit: header, files and their diffs | A revision's changes | missing | missing |
| History | Revert a commit; restore | Revert, restore | partial | partial |
| Diff | Unified view: old and new line numbers, added and removed lines coloured | | missing | missing |
| Diff | Split view | | partial | partial |
| Diff | Hunk headers and expanding the context | | missing | missing |
| Diff | Hide whitespace changes; diff options | | missing | missing |
| Diff | Large-diff and binary placeholders | | missing | missing |
| Branches | Compare a branch with main | A proposal against main | missing | missing |
| Branches | Merge conflicts dialog | A proposal that conflicts with main | partial | partial |
| Branches | Stash changes | Set a draft aside | missing | missing |
| App | Menu bar (File, Edit, View, Repository, Branch, Help) | | missing | missing |
| App | Options: appearance and language | Settings | partial | partial |
| App | Repository settings | Mode, grants, permissions | partial | partial |
| App | Keyboard shortcuts (tabs, commit, lists) | | partial | partial |
| App | No-repository and offline states | Not connected | partial | partial |
| Pull request | Header: title, number, state, "wants to merge N commits into main" | A proposal | missing | missing |
| Pull request | Conversation, Commits, Checks, Files changed tabs with counts | | missing | missing |
| Pull request | Conversation: description and timeline of commits and reviews | | missing | missing |
| Pull request | Sidebar: reviewers, state of each | Who approved, what the policy needs | missing | missing |
| Pull request | Files changed: file tree, per-file +/− counts, "Viewed" | Changed nodes and sections | missing | missing |
| Pull request | Comments on a line, with suggestions | | missing | missing |
| Pull request | Review box: Comment, Approve, Request changes | Accept, reject | partial | partial |
| Pull request | Checks: what ran, with its results | Evidence: scenarios, pictures before and after | partial | partial |
| Pull request | Merge box states: conflicts, blocked, ready, merged | Refusals and conflicts | partial | partial |
| Pull request | Merge button with options | Accept, "always allow" | partial | partial |
| Pull request | Close the pull request | Reject, with a reason | partial | partial |
| Pull request | Check out the pull request | Preview | partial | partial |

**Counts at the start:** 0 done, 19 partial, 17 missing.
**Counts now:** 1 done, 20 partial, 15 missing.

## The Studio's own parts

Requests to the agents (the composer, which is the Studio's request client), the connection state, the approval mode
and rules, the activity log, the inspector of the connected app's nodes and the agents' leases and usage. They stay,
each where a daily user of the counterpart would look for something like it.
