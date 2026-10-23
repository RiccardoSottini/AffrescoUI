# Tempera

An image editor built with AffrescoUI over `modules/imaging`: layers and groups, masks and selections, brushes,
filters with a live preview, adjustment layers and colour management, on sparse copy-on-write tiles.

```sh
cmake -S . -B build -G Ninja && cmake --build build
./build/affrescoui_tempera [--socket DIR] [--data DIR]
./build/affrescoui_tempera_scenarios [--out DIR] examples/tempera/tempera.scenarios.json   # --out: draws and saves the captures
```

It opens a generated sample (1200 × 800: Sky, Sun and Hills).

## How it is split

| File | What it holds |
| --- | --- |
| `tempera.screen.json` | The window, fonts, colours, permissions and the initial state |
| `parts/commands.json` | One action per native, the menu commands, the tool keys and the filter watches |
| `parts/workspace.json` | The menu bar, the options bar, the tool bar, the canvas with rulers and guides, and the status bar |
| `parts/panels.json` | Navigator, Layers, History, Properties/Adjust, Colour/Palette, and the dialogs |
| `Session.cpp`, `Natives.cpp`, `NativesEdit.cpp`, `Tools.cpp` | The host: the document, the tools and the `tempera.*` natives |
| `main.cpp`, `scenarios_main.cpp`, `Tempera.hpp` | The program, the scenarios runner and the natives' declarations |
| `tempera.scenarios.json` | 30 scenarios, run headless |
| `agent-filter-panel.revision.json` | The revision an agent proposes in the scenario `tempera-agent-adds-a-filter-panel` |

The pixels stay in the host. The screen's state holds `state.doc`, a snapshot that every native returns: the layers
(top first) with their thumbnails, the history, the selection's bounds, the colours, the tool and its options, and the
view.

The canvas is a `surface` node that the host's compositor draws. A pointer on it reaches the active tool. Scenarios drive
it through the `pointer` action, in document pixels.

## Agents

The screen allows the Studio and asks the agent: an agent's revision waits as a proposal until the Studio or the user
accepts it. A revision may not call natives. So the filter panel the agent adds only runs `showFilter`, which sets
`state.filter`; the screen's own watches on `state.filter` preview, apply and discard the filter.

## Keys

| Keys | Command |
| --- | --- |
| V M ⇧M L ⇧L W C B E S G ⇧G I H Z | Tools: move, marquee, ellipse, lasso, polygon, wand, crop, brush, eraser, clone, gradient, fill, eyedropper, hand, zoom |
| D, X, Q | Default colours, swap them, quick mask |
| `,` `.` | Smaller or larger brush |
| ⌥⌫ | Fill with the foreground |
| ⌘N ⌘O ⌘S ⌘⇧E | New, open, save, export PNG |
| ⌘Z ⌘⇧Z | Undo, redo |
| ⌘A ⌘D ⌘⇧I | Select all, deselect, inverse |
| ⌘⇧N ⌘J ⌘G ⌘⌥G | New layer, duplicate, group, clipping mask |
| ⌘↑ ⌘↓ | Move the layer up or down |
| ⌘Y | Proof colours |
| ⌘= ⌘- ⌘0 ⌘1 | Zoom in, zoom out, fit, 100% |
| ⌘R | Rulers |
