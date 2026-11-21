# Tempera

An image editor over `modules/imaging`: layers and groups, masks and selections, brushes, filters with a live preview,
adjustment layers and colour management, on sparse copy-on-write tiles. It is a folder of Lisp that the stock
`affrescoui_launcher` runs, with nothing compiled for it.

**Its `lisp/` is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language" and "Writing an app as a
program"): its files by area, views that read as an outline, and what happens in functions and commands. Each operation
on the image is a function of `lisp/tempera.lisp` that runs the image step, its name written out
(`(image "moveLayer" :with args)`), so the revision rules judge exactly what it reaches; buttons, keys, menus, scenarios
and the slide sorter an agent installs run those functions by name, their arguments in `args`, as they ran the actions.
Its text is in `lisp/locales/` (English, Spanish and French; Settings choose). `json/` beside it is the same app written
in JSON, also by hand: the Launcher's Settings choose which one opens, the same scenarios check both, and
`tools/notation/lisp_examples.py` leaves both alone.

```sh
./build/affrescoui_launcher examples/tempera/lisp/index.screen.lisp --socket out/s   # opens the sample
./build/affrescoui test examples/tempera/lisp/index.scenarios.json                   # the 65 scenarios (ctest: scenarios)
```

It opens a generated sample (1200 × 800: Sky, Sun and Hills), drawn by `onLoad`.

Its look is its own: sage-green verdaccio panels as rounded cards on a darker ground, an ochre accent, pill tabs and
title bar menus, the tool's name in the options bar, and the panel icon strip at the window's far edge. A control that
can't work yet is shown disabled, its tooltip saying "not available yet".

## How it is split

| File | What it holds |
| --- | --- |
| `lisp/index.screen.lisp` | The files it includes, and `main`: the window, fonts, strings, permissions, and the window's parts in drawing order |
| `lisp/state.lisp`, `computed.lisp` | The initial state (the snapshot `doc`, the tools, the dialogs) and what is worked out from it |
| `lisp/image.lisp`, `offers.lisp` | The `images` section (the document `doc` and its tools' gestures); the exports and the point a feature binds to |
| `lisp/tempera.lisp` | What happens: the image's operations, the sample, the dialogs, groups, adjustments, filters' preview, the controls' handlers |
| `lisp/commands.lisp`, `keys.lisp`, `menus.lisp` | The commands with their localized titles, the shortcuts, and the menus |
| `lisp/controls.lisp`, `menu.lisp` | The components: icon buttons, sliders, fields, checks, pickers, and the pop-up menu |
| `lisp/menubar.lisp`, `options.lisp`, `tools.lisp`, `canvas.lisp` | The menu bar, the options bar, the tools bar, the canvas with rulers, guides and status bar |
| `lisp/panels.lisp`, `properties.lisp`, `layers.lisp`, `dialogs.lisp` | The dock and its panels, and the dialogs |
| `lisp/look.lisp`, `locales/` | The palette and styles; the text in English, Spanish and French |
| `lisp/index.scenarios.json` | 69 scenarios, an agent's proposal of a Filters panel to the program among them, run headless (they stay JSON); the slide sorter's proof plays them all |
| `PARITY.md` | What a daily user of a professional editor finds, row by row, and how far Tempera has come, round by round |
| `lisp/sorter/` | Sinopia's slide sorter installed into Tempera: its adapter, a broken one, and an end-to-end test with a scripted agent. The Lisp Tempera installs the sorter written as a program (`sorter.lisp`), the JSON one its JSON manifest |

The pixels stay in the library. The `images` section declares `doc`, and the state `doc` mirrors it after every step:
the layers (top first) with their thumbnails (`doc/layer/<id>`), the history, the selection's bounds, the colours, the
tool and its options, and the view. The canvas is a `raster` node that draws `doc` and sends a pointer to its tool.
Scenarios drive it through the `pointer` action, in document pixels.

Type and shape layers are the library's too: each keeps what it is drawn from (its text, size and style; its outline,
fill and stroke), shown in the snapshot's layer rows as `content`, and is drawn again whenever that changes, so it stays
sharp and editable until Layer > Rasterize makes it pixels. The Type tool types on the canvas into the layer it adds or
clicks, and Properties edits it too: its font family, size, style, tracking, and the kerning of the pair where typing
goes on. The pen's paths are the document's (`doc.paths`), drawn over the canvas while one is chosen in the Paths
panel, and filled, stroked, selected or made a shape layer from there or the options bar. The Pen adds an anchor on a
segment, deletes the one pressed on, and Alt converts a smooth anchor to a corner (a drag pulls a corner's handles);
Type > Create Work Path makes a type layer's letters paths. Select > Transform Selection puts handles on the
selection's box (the `selection-box` gesture), Free Transform (Cmd+T) on the active layer's (`layer-box`), and every
colour chip opens a picker beside it.

## Agents

The screen allows `studio` peers and asks the agent: an agent's revision waits as a proposal until a `studio` peer or
the user accepts it. The filter panel an agent adds only runs `showFilter`, which sets `state.filter`; the screen's own
watches on `state.filter` preview, apply and discard the filter. A picture an agent draws comes as an asset of its
revision (SVG or PNG) with a `run` that opens it; only the person accepts a revision with assets, and then it opens as
a new image (the scenario `tempera-opens-an-agents-picture`).

## Keys

| Keys | Command |
| --- | --- |
| V M ⇧M L ⇧L W ⇧W C B ⇧B E S G ⇧G R O ⇧O I H Z | Tools: move, marquee, ellipse, lasso, polygon, wand, quick selection, crop, brush, pencil, eraser, clone, gradient, fill, blur, dodge, burn, eyedropper, hand, zoom |
| T P A U ⇧U | Tools: type, pen, path selection, rectangle, ellipse (shapes) |
| D, X, Q, F | Default colours, swap them, quick mask, screen mode |
| `[` `]` (or `,` `.`) | Smaller or larger brush |
| ⌥⌫, ⌘⌫, ⇧⌫, ⌫ | Fill with the foreground or the background, Fill..., clear |
| ⌘X ⌘C ⌘⇧C ⌘V | Cut, copy, copy merged, paste |
| ⌘T ⌘⌥I ⌘⌥C | Free transform, image size, canvas size |
| ⌘N ⌘O ⌘S ⌘⇧S ⌘W ⌘⌥⇧W F12 | New, open, save, save as, close, export PNG, revert |
| ⌘Z ⌘⇧Z | Undo, redo |
| ⌘A ⌘D ⌘⇧D ⌘⇧I | Select all, deselect, reselect, inverse |
| ⌘⇧N ⌘J ⌘⇧J ⌘G ⌘⇧G ⌘⌥G | New layer, duplicate, layer via cut, group, ungroup, clipping mask |
| ⌘L ⌘M ⌘U ⌘B ⌘I ⌘⌥⇧B | Levels, Curves, Hue/Saturation, Colour Balance, Invert, Black & White (as adjustment layers) |
| ⌘E ⌘⇧E ⌘/ | Merge down, merge visible, lock the layer |
| ⌘↑ ⌘↓ ⌘⇧↑ ⌘⇧↓ | Move the layer up or down, to the front or the back |
| ⌘Y | Proof colours |
| ⌘= ⌘- ⌘0 ⌘1 | Zoom in, zoom out, fit, 100% |
| ⌘R ⌘F ⇧F6 | Rulers, search the commands, tools and layers, feather |
