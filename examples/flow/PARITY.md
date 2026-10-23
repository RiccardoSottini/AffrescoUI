# Flow: parity

Flow is modelled on Linear, with a board that feels like Trello's. It is a JSON screen only. Its icons are Lucide's
and all of its data is invented. This page lists what a daily user would reach for and how far Flow has got.

| Area | What a daily user reaches for | Flow | Notes |
| --- | --- | --- | --- |
| Sidebar | workspace, search, new issue, My issues, the team, its issues, its boards, help | done | `sidebar.json` |
| List view | issues grouped by status, a row each: priority, id, status, title, label, due, estimate, assignee | done | `list.json`; ids are `FLO-n` |
| List view | select a row, Enter to edit, context menu, drag a row to another group | done | |
| Issue page | full page, title, description, properties side panel, activity | done | `issue.json`; double-click or Cmd+Enter opens, Escape closes; only the applied labels show, + adds one |
| Board | columns by status, cards with id, priority and assignee, drag and drop | done | cards show id, assignee, title, then priority, label, due and estimate chips; Space peeks in the side panel |
| Filter and display | view tabs, a Filter menu, a Display menu with the layouts, a new-issue composer | done | All issues, Active and Backlog tabs; Cmd+F opens a search field; F opens Filter; Display picks board, list, table or timeline and swimlanes; C or a column's + opens the composer in that status, with a description, status, priority and assignee chips and Create more |
| Workspace | a workspace menu with settings, theme and language, a facepile, toasts with Undo | done | |
| Cmd+K | one palette for commands and going to an issue | done | Cmd+K lists commands (the selected issue's first) and issues together; Cmd+P searches issues only |
| Keyboard | C new, J/K move, single-key property changes, G-chords | done | C, J/K, S, P, A, I (assign to me), /, Cmd+Z; G then I, A, V or P |
| Multi-select | X, Shift+J/K, a bar for bulk status, priority, assignee and delete | done | `select.json`; Cmd+A selects every shown issue, Escape clears, each bulk change is one undo step |
| Cycles | current cycle, scope and progress | done | `pages.json`; scope, started and completed from the board, a burn-up, upcoming and past cycles; G then V |
| Projects | a project list, status and progress | partial | health, lead, target date and progress; G then P. The project page with milestones and updates is missing |
| Inbox | notifications, read and unread | done | unread count in the sidebar, J/K, U read or unread, H snooze, Enter opens the issue; G then I |
| Roadmap | projects on a timeline | partial | the Timeline view |
