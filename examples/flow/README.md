# Flow, a Kanban board

Boards as tabs, swimlanes, a table and a timeline of every card, a card drawer, a burndown and an activity feed, drag and drop with a keyboard alternative, undo, Cmd+P and Cmd+Shift+P. The `kanban` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/flow/kanban.screen.json
./build/affrescoui test examples/flow/kanban.scenarios.json
```

| File | What it is |
|---|---|
| `board.json` | Its board logic, included: undo, the activity feed, work-in-progress limits, keyboard moves, the drawer's edits, presence |
| `drawer.json` | The card drawer, included: notes, due date, labels, assignees, checklist, comments, attachments |
| `kanban.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `kanban.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `kanban.screen.json` | The screen document |
| `strings.json` | Its string tables: English, Italian and Arabic |
| `views.json` | Its other views, included: insights, swimlanes, the timeline, the settings |

`affrescoui_host` opens it when it is given no screen.

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the board logic, the drawer and the views; components with `use` and `with`.
- **State and expressions:** computed values, `watch`, `fuzzy` search, date functions (`dateMs`, `formatDate`, `daysBetween`, `addDays`).
- **Widgets:** tabs, a virtualised table with sorting and resizable columns, the `timeline` (clips dragged to change dates or owner), the `calendar` date picker, a line chart, rich-text runs with links, the markdown `editor`, checkboxes, tooltips, toasts.
- **Drag and drop** between columns and swimlanes, and the same moves by keyboard (Alt+arrows, Cmd+arrows, the card menu).
- **Commands:** titled actions in the command palette (Cmd+Shift+P), go to a card (Cmd+P), shortcuts, `enablement`, a context menu.
- **Overlays:** the members popover, the modal date picker and settings sheet, the edit dialog.
- **Settings** that persist (`settings` with a schema): language, work-in-progress warnings, first day of the week.
- **Themes and locales:** light and dark tokens; English, Italian and Arabic string tables with plurals, right to left with mirrored icons.
- **Accessibility:** roles, names, live regions, toolbars with arrow keys, the focus kept in modal overlays.
- **Shared editing:** a document-server channel for presence and other people's revisions in the activity feed.
- **Layouts** from 1040 × 640 (icon-only buttons with names and tooltips) to full screen.

## How it holds together

- **Permissions:** Studio peers change it directly; an agent's change waits for a Studio reviewer (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, column sizes, view, swimlanes and settings persist; the cards are the document's.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** boards and columns are entries of `state`; the board logic, the drawer and the other views are included files; a language is a table in `strings.json`.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
