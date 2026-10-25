# Tempera

An image editor over `modules/imaging`: layers and groups, masks and selections, brushes, filters with a live preview,
adjustment layers and colour management, on sparse copy-on-write tiles. It is a folder of JSON that the stock
`affrescoui_launcher` runs, with nothing compiled for it.

```sh
./build/affrescoui_launcher examples/tempera/index.screen.json --socket out/s   # opens the sample
./build/affrescoui test examples/tempera/index.scenarios.json                   # the 35 scenarios (ctest: scenarios)
```

It opens a generated sample (1200 × 800: Sky, Sun and Hills), drawn by `onLoad`.

## How it is split

| File | What it holds |
| --- | --- |
| `index.screen.json` | The window, fonts, colours, permissions, the `images` section (the document `doc`) and the initial state |
| `parts/commands.json` | The actions, each a list of `image` steps, the menu commands, the tool keys and the filter watches |
| `parts/workspace.json` | The menu bar, the options bar, the tool bar, the canvas (a `raster` node) with rulers and guides, and the status bar |
| `parts/panels.json` | Navigator, Layers, History, Properties/Adjust, Colour/Palette, and the dialogs |
| `index.scenarios.json` | 35 scenarios, run headless |
| `sorter/` | Sinopia's slide sorter installed into Tempera: its adapter, a broken one, and an end-to-end test with a scripted agent |

The pixels stay in the library. The `images` section declares `doc`, and the state `doc` mirrors it after every step:
the layers (top first) with their thumbnails (`doc/layer/<id>`), the history, the selection's bounds, the colours, the
tool and its options, and the view. The canvas is a `raster` node that draws `doc` and sends a pointer to its tool.
Scenarios drive it through the `pointer` action, in document pixels.

## Agents

The screen allows `studio` peers and asks the agent: an agent's revision waits as a proposal until a `studio` peer or
the user accepts it. The filter panel an agent adds only runs `showFilter`, which sets `state.filter`; the screen's own
watches on `state.filter` preview, apply and discard the filter. A picture an agent draws comes as an asset of its
revision (SVG or PNG) with a `run` that opens it; only the person accepts a revision with assets, and then it opens as
a new image (the scenario `tempera-opens-an-agents-picture`).

## Keys

| Keys | Command |
| --- | --- |
| V M ⇧M L ⇧L W C B E S G ⇧G I H Z | Tools: move, marquee, ellipse, lasso, polygon, wand, crop, brush, eraser, clone, gradient, fill, eyedropper, hand, zoom |
| D, X, Q | Default colours, swap them, quick mask |
| `,` `.` | Smaller or larger brush |
| ⌥⌫ | Fill with the foreground |
| ⌘N ⌘O ⌘S ⌘⌥⇧W | New, open, save, export PNG |
| ⌘Z ⌘⇧Z | Undo, redo |
| ⌘A ⌘D ⌘⇧I | Select all, deselect, inverse |
| ⌘⇧N ⌘J ⌘G ⌘⌥G | New layer, duplicate, group, clipping mask |
| ⌘↑ ⌘↓ | Move the layer up or down |
| ⌘Y | Proof colours |
| ⌘= ⌘- ⌘0 ⌘1 | Zoom in, zoom out, fit, 100% |
| ⌘R | Rulers |
