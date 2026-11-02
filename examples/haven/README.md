# Haven, a smart-home dashboard

Rooms in a tree by floor, a floor plan, scenes, lights with their colours, climate, blinds, media, automations, schedules, an energy page with charts, security and notifications, a settings page, light and dark themes, in English, Spanish and French. The `smart-home` template: an app with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_launcher examples/haven/lisp/index.screen.lisp
./build/affrescoui test examples/haven/lisp/index.scenarios.json
```

**Its `lisp/` is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"), as it has real behaviour: scenes over every
light of a room or every room, device calls that take a while, the alarm's code, a simulated service, automations edited in
place. `json/` beside it is the same app written in JSON, also by hand: the Launcher's Settings choose which one opens, the same scenarios check both, and `tools/notation/lisp_examples.py` leaves both alone. The screen
and its parts say what is shown and keep their keys as data; `lisp/haven.lisp` says what happens: every action is a function
or a command, and the handlers name them.

| File | What it is |
|---|---|
| `PARITY.md` | What it covers of the app it is modelled on, area by area |
| `assets/images/` | The pictures the screen shows |
| `lisp/app.lisp` | The app: its entry, its capabilities, its models and seed data |
| `lisp/index.screen.lisp` | The screen document: the window, the theme, the settings, the state, the keys, the layout |
| `lisp/haven.lisp` | The program: every function and command (rooms and lights, scenes, devices, security, automations, the cards) |
| `lisp/automations.lisp` | The automation graph, the schedules, security, notifications and the simulated service (included) |
| `lisp/plan.lisp`, `lisp/moreinfo.lisp`, `lisp/edit.lisp` | The floor plan and the media card; a light's, a cover's or a device's more-info; arranging the cards |
| `lisp/shell.lisp`, `lisp/settings.lisp`, `lisp/areas.lisp`, `lisp/energy.lisp`, `lisp/history.lisp` | The sidebar and the alarm keypad; the settings pages; the rooms, energy and history pages |
| `lisp/locales/` | Its text, one file per language: English, Spanish and French |
| `lisp/index.policy.lisp` | Its approval rules' example revisions (`affrescoui policy test`) |
| `lisp/index.scenarios.json` | Its scenarios, which stay JSON (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the plan and the automations; components with `use` and `with`.
- **Widgets:** the `canvas` floor plan (rooms dragged and picked), the `graph` node editor for automations with a minimap, the `timeline` for schedules (clips dragged in whole hours), the `colorPicker` in a modal dialog, a tree of rooms, a table, bar and line charts, sliders, toggles, toasts.
- **A simulated service:** an `every` timer that moves each room towards its target and reports motion while away; device calls that take a while, with progress.
- **Commands:** titled actions in the command palette (Cmd+K or Cmd+Shift+P) and shortcuts: pages (Cmd+1 to Cmd+4, Cmd+,), rooms (Alt+Up and Down), the target (Up and Down), the theme (Cmd+T), the language (Cmd+L), security (Cmd+Shift+A), notifications (Cmd+Shift+N).
- **Overlays:** the notifications popover, the modal colour dialog, the command palette; Escape or a click outside closes them.
- **Settings** that persist (`settings` with a schema): language, the Away temperature, device notifications, motion alerts, the simulated devices; the theme and the security mode persist too.
- **Themes and locales:** light and dark tokens; English, Spanish and French string tables.
- **Accessibility:** roles, names, live regions, toolbars and lists with arrow keys, the focus kept in modal overlays.
- **Layouts** from 1040 × 640 (compact cards, the sidebar's energy card left out) to full screen.

## How it holds together

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its program or its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, the settings and the security mode persist; nothing else.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document and its program on its UI thread. The program's definitions are the file's; a definition an agent changes keeps the agent's authority, however the person reaches it (SECURITY.md, "The Lisp language").
- **Extension points:** rooms, devices and scenes are entries of `state`; the plan, the automations and the strings are included files, each changed on its own; a language is a file in `locales/`.
- **Assets:** the pictures in `images/` are simple vector drawings made for this project, under the repository's Apache-2.0 licence. The fonts' licences are with them in `../shared/fonts`.
