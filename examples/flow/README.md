# Flow, an issue tracker

Boards in the sidebar, an inbox, sprints and projects, a grouped list, swimlanes, a table and a timeline of every card, a card drawer, a burndown and an activity feed, drag and drop with a keyboard alternative, undo, Cmd+P and Cmd+Shift+P. The `kanban` template: an app with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_launcher examples/flow/lisp/index.screen.lisp
./build/affrescoui test examples/flow/lisp/index.scenarios.json
```

**It is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"), as it has real behaviour: undo, moves
between columns and swimlanes, bulk changes, filters and the inbox, with the same logic used from many places. `lisp/` is its
only source, with no `json/` beside it, and `tools/notation/lisp_examples.py` leaves it alone. The screen and its parts say
what is shown and keep their keys as data; `lisp/flow.lisp` says what happens: every action is a function or a command, and
the handlers name them.

| File | What it is |
|---|---|
| `PARITY.md` | What it covers of the app it is modelled on, area by area |
| `lisp/app.lisp` | The app: its entry, its capabilities, its models and seed data |
| `lisp/index.screen.lisp` | The screen document: the window, the theme, the state, the computed values, the menus, the keys, the layout |
| `lisp/flow.lisp` | The program: every function and command (cards, undo, filters, the timeline, marking, the inbox) |
| `lisp/board.lisp` | The board's settings, state and keys, the timeline's watch, presence from the host's channel |
| `lisp/drawer.lisp` | The card drawer, included: notes, due date, labels, assignees, checklist, comments, attachments |
| `lisp/issue.lisp`, `lisp/pages.lisp`, `lisp/select.lisp` | The issue page; the inbox, sprints and projects; marking several cards |
| `lisp/list.lisp`, `lisp/sidebar.lisp`, `lisp/views.lisp` | The list view; the sidebar; the insights, swimlanes, the timeline, the settings |
| `lisp/locales/` | Its text, one file per language: English, Spanish and French |
| `lisp/index.policy.lisp` | Its approval rules' example revisions (`affrescoui policy test`) |
| `lisp/index.scenarios.json` | Its scenarios, which stay JSON (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |

The Launcher lists it first, and the release's checks draw it: `affrescoui_launcher examples/flow/lisp/index.screen.lisp`.

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the board logic, the drawer and the views; components with `use` and `with`.
- **State and expressions:** computed values, `watch`, `fuzzy` search, date functions (`dateMs`, `formatDate`, `daysBetween`, `addDays`).
- **Widgets:** tabs, a virtualised table with sorting and resizable columns, the `timeline` (clips dragged to change dates or owner), the `calendar` date picker, a line chart, rich-text runs with links, the markdown `editor`, checkboxes, tooltips, toasts.
- **Drag and drop** between columns and swimlanes, and the same moves by keyboard (Alt+arrows, Cmd+arrows, the card menu).
- **Commands:** titled actions in the command palette (Cmd+Shift+P), go to a card (Cmd+P), shortcuts, `enablement`, a context menu.
- **Overlays:** the members popover, the modal date picker and settings sheet, the edit dialog.
- **Settings** that persist (`settings` with a schema): language, work-in-progress warnings, first day of the week.
- **Themes and locales:** light and dark tokens; English, Spanish and French string tables with plurals.
- **Accessibility:** roles, names, live regions, toolbars with arrow keys, the focus kept in modal overlays.
- **Shared editing:** a document-server channel for presence and other people's revisions in the activity feed.
- **Layouts** from 1040 × 640 (icon-only buttons with names and tooltips) to full screen.

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its program or its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, column sizes, view, swimlanes and settings persist; the issues are records of the `issues` model (a status for the column, an order in it), seeded from the document, read through a query per column (`data.lane0`...).
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document and its program on its UI thread. The program's definitions are the file's; a definition an agent changes keeps the agent's authority, however the person reaches it (SECURITY.md, "The Lisp language").
- **Extension points:** boards and columns are entries of `state`; the board logic, the drawer and the other views are included files; a language is a file in `locales/`.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
