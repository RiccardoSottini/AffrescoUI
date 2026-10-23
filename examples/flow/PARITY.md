# Flow: parity

Flow is modelled on Linear, with a board that feels like Trello's. It is a JSON screen only. Its icons are Lucide's
and all of its data is invented. This page lists what a daily user would reach for and how far Flow has got.

| Area | What a daily user reaches for | Flow | Notes |
| --- | --- | --- | --- |
| Sidebar | workspace, search, new issue, My issues, the team, its issues, its boards, help | done | `sidebar.json` |
| List view | issues grouped by status, a row each: priority, id, status, title, label, due, estimate, assignee | done | `list.json`; ids are `FLO-n` |
| List view | select a row, Enter to edit, context menu, drag a row to another group | done | |
| Issue page | full page, title, description, properties side panel, activity | done | `issue.json`; double-click or Cmd+Enter opens, Escape closes |
| Board | columns by status, cards with id, priority and assignee, drag and drop | done | cards show id, assignee, title, then priority, label, due and estimate chips; Space peeks in the side panel |
| Cmd+K | one palette for commands and going to an issue | done | Cmd+K opens the command palette; Cmd+P goes to a card |
| Keyboard | C new, J/K move, single-key property changes | partial | C, J/K, S, P, A, I (assign to me) and / work; X multi-select and G-chords missing |
| Cycles | current cycle, scope and progress | missing | |
| Projects | a project list, status and progress | missing | |
| Inbox | notifications, read and unread | missing | |
| Roadmap | projects on a timeline | partial | the Timeline view |
