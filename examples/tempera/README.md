# Tempera

An image editor over `modules/imaging`: layers and groups, masks and selections, brushes, filters with a live preview,
adjustment layers and colour management, on sparse copy-on-write tiles. It is a folder of Lisp that the stock
`affrescoui_launcher` runs, with nothing compiled for it.

**It is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"), because it has behaviour beyond one
operation at a time: the dialogs' numbers, grouping and ungrouping, cropping to a ratio, adjustments and the panels' lists
are functions in `lisp/tempera.lisp`. Each operation of the image step stays an action in `parts/commands.lisp`, which the
functions run, so an agent's revision may wire a button to an operation exactly as before, and the slide sorter an agent
installs runs the same actions. Not to the functions that run an operation by a name they are given (`op`): the revision
rules count a `run` of a computed name as reaching anything, where one written out reaches what it names. `lisp/` is its only source, with no `json/` beside it, and
`tools/notation/lisp_examples.py` leaves it alone.

```sh
./build/affrescoui_launcher examples/tempera/lisp/index.screen.lisp --socket out/s   # opens the sample
./build/affrescoui test examples/tempera/lisp/index.scenarios.json                   # the 62 scenarios (ctest: scenarios)
```

It opens a generated sample (1200 × 800: Sky, Sun and Hills), drawn by `onLoad`.

Its look is its own: sage-green verdaccio panels as rounded cards on a darker ground, an ochre accent, pill tabs and
title bar menus, the tool's name in the options bar, and the panel icon strip at the window's far edge. A control that
can't work yet is shown disabled, its tooltip saying "not available yet".

## How it is split

| File | What it holds |
| --- | --- |
| `lisp/index.screen.lisp` | The window, fonts, colours, permissions, the `images` section (the document `doc`) and the initial state |
| `lisp/tempera.lisp` | The program: the dialogs, groups, crops, adjustments and the panels' lists, as functions and commands |
| `lisp/parts/commands.lisp` | The actions that are one operation of the image step, the sample, the menu commands, the tool keys and the filter watches |
| `lisp/parts/workspace.lisp` | The menu bar, the options bar, the tool bar, the canvas (a `raster` node) with rulers and guides, and the status bar |
| `lisp/parts/panels.lisp` | Navigator, Layers, History, Properties/Adjust, Colour/Palette, and the New dialog |
| `lisp/parts/dialogs.lisp` | The dialogs that take numbers (Free Transform, Image Size, Canvas Size, Rename Layer, Fill) and Close's question |
| `lisp/index.scenarios.json` | 62 scenarios, run headless (they stay JSON) |
| `PARITY.md` | What a daily user of a professional editor finds, row by row, and how far Tempera has come, round by round |
| `lisp/sorter/` | Sinopia's slide sorter installed into Tempera: its adapter, a broken one, and an end-to-end test with a scripted agent |

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
