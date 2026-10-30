# Haven, a smart-home dashboard

Rooms in a tree by floor, a floor plan, scenes, lights with their colours, climate, blinds, media, automations, schedules, an energy page with charts, security and notifications, a settings page, light and dark themes, in English, Spanish and French. The `smart-home` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_launcher examples/haven/json/index.screen.json
./build/affrescoui test examples/haven/json/index.scenarios.json
```

| File | What it is |
|---|---|
| `automations.json` | The automation graph, the schedules, security, notifications and the simulated service (included) |
| `images/` | The pictures the screen shows |
| `plan.json` | The floor plan, the media card and the colour dialog (included) |
| `index.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `index.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `index.screen.json` | The screen document |
| `strings.json` | Its string tables: English, Spanish and French |

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

- **Permissions:** `studio` peers (the person's client) change it directly; an agent's change waits for the person's review (on its card in the assistant chat, or from a `studio` peer) (a change to its actions always does, even under a grant), and a budget caps how many nodes an agent changes at once (20 added, 5 removed, 50 changed); no one may change the status bar. The theme, the settings and the security mode persist; nothing else.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** rooms, devices and scenes are entries of `state`; the plan, the automations and the strings are included files, each changed on its own; a language is a table in `strings.json`.
- **Assets:** the pictures in `images/` are simple vector drawings made for this project, under the repository's Apache-2.0 licence. The fonts' licences are with them in `../shared/fonts`.
