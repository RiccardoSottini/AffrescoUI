# Haven, a smart-home dashboard

Rooms in a tree by floor, a floor plan, scenes, lights with their colours, climate, blinds, media, automations, schedules, an energy page with charts, security and notifications, a settings page, light and dark themes, in English, Italian and Arabic. The `smart-home` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/haven/smart-home.screen.json
./build/affrescoui test examples/haven/smart-home.scenarios.json
```

| File | What it is |
|---|---|
| `automations.json` | The automation graph, the schedules, security, notifications and the simulated service (included) |
| `images/` | The pictures the screen shows |
| `plan.json` | The floor plan, the media card and the colour dialog (included) |
| `smart-home.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `smart-home.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `smart-home.screen.json` | The screen document |
| `strings.json` | Its string tables: English, Italian and Arabic |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## What it shows

- **Screen documents in parts:** `include` of the plan and the automations; components with `use` and `with`.
- **Widgets:** the `canvas` floor plan (rooms dragged and picked), the `graph` node editor for automations with a minimap, the `timeline` for schedules (clips dragged in whole hours), the `colorPicker` in a modal dialog, a tree of rooms, a table, bar and line charts, sliders, toggles, toasts.
- **A simulated service:** an `every` timer that moves each room towards its target and reports motion while away; device calls that take a while, with progress.
- **Commands:** titled actions in the command palette (Cmd+K or Cmd+Shift+P) and shortcuts: pages (Cmd+1 to Cmd+4, Cmd+,), rooms (Alt+Up and Down), the target (Up and Down), the theme (Cmd+T), the language (Cmd+L), security (Cmd+Shift+A), notifications (Cmd+Shift+N).
- **Overlays:** the notifications popover, the modal colour dialog, the command palette; Escape or a click outside closes them.
- **Settings** that persist (`settings` with a schema): language, the Away temperature, device notifications, motion alerts, the simulated devices; the theme and the security mode persist too.
- **Themes and locales:** light and dark tokens; English, Italian and Arabic string tables, Arabic right to left.
- **Accessibility:** roles, names, live regions, toolbars and lists with arrow keys, the focus kept in modal overlays.
- **Layouts** from 1040 × 640 (compact cards, the sidebar's energy card left out) to full screen.
