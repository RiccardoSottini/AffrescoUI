# Tempera

An image editor over `modules/imaging`: layers and groups, masks and selections, brushes, filters with a live preview,
adjustment layers and colour management, on sparse copy-on-write tiles. It is a folder of JSON that the stock
`affrescoui_launcher` runs, with nothing compiled for it.

```sh
./build/affrescoui_launcher examples/tempera/index.screen.json --socket out/s   # opens the sample
./build/affrescoui test examples/tempera/index.scenarios.json                   # the 61 scenarios (ctest: scenarios)
```

It opens a generated sample (1200 × 800: Sky, Sun and Hills), drawn by `onLoad`.

Its look is its own: sage-green verdaccio panels as rounded cards on a darker ground, an ochre accent, pill tabs and
title bar menus, the tool's name in the options bar, and the panel icon strip at the window's far edge. A control that
can't work yet is shown disabled, its tooltip saying "not available yet".

## How it is split

| File | What it holds |
| --- | --- |
| `index.screen.json` | The window, fonts, colours, permissions, the `images` section (the document `doc`) and the initial state |
| `parts/commands.json` | The actions, each a list of `image` steps, the menu commands, the tool keys and the filter watches |
| `parts/workspace.json` | The menu bar, the options bar, the tool bar, the canvas (a `raster` node) with rulers and guides, and the status bar |
| `parts/panels.json` | Navigator, Layers, History, Properties/Adjust, Colour/Palette, and the New dialog |
| `parts/dialogs.json` | The dialogs that take numbers (Free Transform, Image Size, Canvas Size, Rename Layer, Fill) and Close's question |
| `index.scenarios.json` | 61 scenarios, run headless |
| `PARITY.md` | What a daily user of a professional editor finds, row by row, and how far Tempera has come, round by round |
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
| V M ⇧M L ⇧L W C B ⇧B E S G ⇧G I H Z | Tools: move, marquee, ellipse, lasso, polygon, wand, crop, brush, pencil, eraser, clone, gradient, fill, eyedropper, hand, zoom |
| D, X, Q, F | Default colours, swap them, quick mask, screen mode |
| `[` `]` (or `,` `.`) | Smaller or larger brush |
| ⌥⌫, ⌘⌫, ⇧⌫, ⌫ | Fill with the foreground or the background, Fill..., clear |
| ⌘X ⌘C ⌘⇧C ⌘V | Cut, copy, copy merged, paste |
| ⌘T ⌘⌥I ⌘⌥C | Free transform, image size, canvas size |
| ⌘N ⌘O ⌘S ⌘⇧S ⌘W ⌘⌥⇧W F12 | New, open, save, save as, close, export PNG, revert |
| ⌘Z ⌘⇧Z | Undo, redo |
| ⌘A ⌘D ⌘⇧I | Select all, deselect, inverse |
| ⌘⇧N ⌘J ⌘⇧J ⌘G ⌘⇧G ⌘⌥G | New layer, duplicate, layer via cut, group, ungroup, clipping mask |
| ⌘L ⌘M ⌘U ⌘B ⌘I ⌘⌥⇧B | Levels, Curves, Hue/Saturation, Colour Balance, Invert, Black & White (as adjustment layers) |
| ⌘E ⌘⇧E ⌘/ | Merge down, merge visible, lock the layer |
| ⌘↑ ⌘↓ ⌘⇧↑ ⌘⇧↓ | Move the layer up or down, to the front or the back |
| ⌘Y | Proof colours |
| ⌘= ⌘- ⌘0 ⌘1 | Zoom in, zoom out, fit, 100% |
| ⌘R ⌘F ⇧F6 | Rulers, search the commands, feather |
