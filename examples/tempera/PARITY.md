# Tempera parity matrix

Tempera is a professional raster image editor. This matrix lists the areas and workflows a
daily user of that kind of editor expects, each `done`, `partial` or `missing`, with the gap in one line. Every round
takes the gaps a daily user would notice first. Tempera keeps its own name, the Lucide icons and invented sample data.

## Counts

| Round | done | partial | missing |
| --- | --- | --- | --- |
| 0 (start) | 9 | 22 | 27 |
| 1 | 15 | 25 | 19 |
| 2 | 15 | 28 | 17 |
| 3 (a folder of files) | 18 | 27 | 17 |
| 4 (every control works) | 25 | 28 | 11 |
| 5 (tools and fill) | 28 | 26 | 10 |
| 6 (keys, Shift and Alt, Auto-Select) | 30 | 24 | 10 |
| 7 (the square and centre, the histogram) | 31 | 23 | 10 |

## Workspace

| Area | Status | Gap |
| --- | --- | --- |
| Menu bar: File, Edit, Image, Layer, Type, Select, Filter, View, Window, Help | done | The ten menus with the counterpart's items and shortcuts; items without behaviour yet are disabled |
| Options bar that changes with the tool | done | Every tool has its row, after a pill with the tool's icon and name |
| Tools bar on the left, one column, grouped with flyouts (long press) | done | Groups open on a right click (no long press); tools not built yet show disabled |
| Foreground/background swatches with swap and reset, quick mask button | done | |
| Document tabs: name, layer, zoom, mode ("Untitled 1 — Layer 1 · 100% · RGB/8") | partial | One document at a time: its × and Cmd+W ask before losing changes, then leave an empty Untitled document |
| Rulers and guides | partial | Zero at the image's corner, minor ticks, click to add a guide; no dragging guides out of the rulers |
| Dark grey pasteboard, checkerboard for transparency | done | |
| Status bar: zoom field and document size | done | One information item at a time, chosen from its › menu |
| Search (commands), workspace switcher, screen modes | partial | Searches the commands only; one workspace; standard and full screen with menu bar |
| Panels docked right in tabbed groups, collapsible to icons | partial | Rounded groups with pill tabs and an icon strip (History, Navigator); groups can't be dragged, resized or collapsed |
| Light and dark interface, locales, keyboard focus, accessible names | partial | Keep and extend every round |

## Panels

| Panel | Status | Gap |
| --- | --- | --- |
| Layers: thumbnails, eye, locks, blend mode, opacity and fill | partial | Fill shown, not editable; transparency and artboard locks disabled; the bottom layer isn't a locked Background |
| Layers: filter by kind | partial | Pixel, adjustment and group filters; type and shape layers don't exist yet |
| Layers: groups with disclosure, masks with link, adjustment layers, fx | partial | Group Layers (Cmd+G) and Ungroup (Shift+Cmd+G); the chevron folds a group; no fx |
| Layers: bottom buttons link, fx, mask, adjustment, group, new, delete | partial | Link and fx disabled |
| Properties (context-sensitive) | partial | Transform (W, H, X, Y, angle, flips) and Align edit the active pixel layer; Quick Actions; adjustment controls; no Distribute |
| Adjustments (icon grid) | done | |
| History (states and snapshots) | partial | States only, no snapshots |
| Colour, Palette, Gradients and Patterns | partial | Foreground/background chips and a square with a horizontal hue strip; no patterns |
| Brushes (presets) | partial | Presets, size, hardness and spacing in the options bar's brush picker; no Brushes panel |
| Channels | partial | Composite and channel rows, display only |
| Paths | missing | An empty panel until the pen tool exists |
| Navigator | done | |
| Info (pointer position, colour under the pointer) | missing | |
| Character and Paragraph | missing | |

## Tools

| Tool or workflow | Status | Gap |
| --- | --- | --- |
| Brush: size, hardness, opacity, flow, round outline cursor, `[` `]` | done | `[` `]` (and `,` `.`) step the size; the raster draws the brush's outline under the pointer |
| Eraser | done | |
| Pencil | done | A hard painting tool (Shift+B); the brush keeps its own hardness |
| Marquee rectangle and ellipse, Shift/Alt add/subtract/intersect, marching ants | done | Shift, Alt or both at the press add, subtract or intersect (marquee, lasso, polygon, wand); during a drag Shift keeps a square and Alt draws from the centre |
| Lasso and polygonal lasso | done | |
| Magic wand with tolerance, quick selection | partial | No quick selection |
| Select all, deselect, inverse, feather, transform selection | partial | No transform selection |
| Move with auto-select | done | The Auto-Select check picks the topmost layer painted under the press |
| Free transform (Cmd+T): handles, rotate, skew, commit and cancel | partial | A dialog: move, scale (flip with a negative), any angle, OK and Cancel; no on-canvas handles, no skew |
| Crop with overlay grid and aspect presets | partial | Thirds while dragging; Shift keeps a square and Alt crops from the centre; the ratios crop the largest centred part, but don't hold a drag to that shape |
| Gradient tool with editor | partial | Inline editor and presets; reverse, dither and modes disabled |
| Paint bucket and fill dialog | done | Edit > Fill (Shift+Backspace): foreground, background, black, grey or white |
| Eyedropper | done | |
| Clone stamp (Alt-click source) | done | |
| Healing brush and patch | missing | |
| History brush | missing | |
| Blur, sharpen, smudge | missing | |
| Dodge, burn, sponge | missing | |
| Pen and path selection | missing | |
| Type tool, editable type layers | missing | |
| Shapes as vector layers | missing | |
| Hand (and space to pan), zoom | done | |

## Adjustments and filters

| Item | Status | Gap |
| --- | --- | --- |
| Levels with histogram | done | An adjustment layer; Properties draws the image's tones above its sliders, measured again as the image changes |
| Curves | partial | The tones show behind the curve; one curve for all channels |
| Hue/Saturation, Brightness/Contrast | partial | Both as adjustment layers; no Colorize or colour ranges |
| Color Balance, Black & White, Invert, Threshold | done | As adjustment layers; Black & White is a Hue/Saturation layer at no saturation |
| Gaussian Blur, Unsharp Mask, Add Noise, Motion Blur with live preview | partial | Preview exists; not every filter |

## Image and file

| Item | Status | Gap |
| --- | --- | --- |
| New document dialog with presets | done | Width, height, bit depth and six size presets |
| Canvas size, image size, rotate canvas | done | Canvas size with an anchor, image size keeping proportions, quarter and half turns, flips; size changes forget the history |
| Flatten, merge down | done | Also merge visible and delete hidden layers |
| Export as PNG/JPEG | done | Through the library's image step |
| Open and save its own format | done | |
| Open a picture an agent proposes | done | An SVG or PNG asset in a revision, opened by its `run` once the person accepts it |
| Undo/redo with the History panel | done | |
| Copy, cut, paste, clear; layer via copy and via cut | done | Through the image's clipboard; paste centres a new layer |
| Rename, align and arrange layers | done | Rename from a dialog; align to the selection or the image |
| Keyboard shortcuts as the counterpart's | partial | Tools, menus, F12, Shift+F6, `[` `]`, the adjustments (Cmd+L, M, U, B, I) and fills; not every menu command has its key |
| Assistant chat add-on (shared) | done | The shared add-on, in Tempera's colours |

## Left for later

Every row above still partial or missing, and every control still disabled, with what it needs.
Priority: P1 a daily user meets it, P2 occasional, P3 rare. "JSON" means Tempera's files alone; "Library" names the
general feature the image step or a node needs first.

| Item | Pri | Needs |
| --- | --- | --- |
| Type tool, editable type layers (and the toolbar's Text Tool) | P1 | Library: a text layer kind, laid out and drawn into the composite, editable in place |
| Free transform: on-canvas handles, skew | P1 | Library: transform handles in the raster overlay that preview and commit `transformBox`, and a skew term |
| Document tabs: several documents open | P1 | Library: a raster node that switches the image it shows by name; then JSON tabs over `images` |
| Keyboard shortcuts: the remaining menu commands | P2 | JSON: a `keys` entry per command still without one |
| Rulers: drag a guide out of a ruler | P2 | Library: a drag from the ruler that publishes a guide position (the raster node's rulers); then JSON |
| Panels: drag, resize, collapse groups | P2 | Library: dockable panel groups in the kit (general, every app with panels) |
| Search beyond the commands; saved workspaces | P3 | JSON: search layers and help too; keep panel layouts in state |
| Light and dark interface, locales, focus, names | P2 | JSON: a light token set, and keep each new control named and focusable |
| Layers: editable Fill, lock transparency, a locked Background | P2 | Library: a per-layer fill opacity and a transparency lock that painting honours; the locked Background is JSON |
| Layers: fx; Layer Style menu rows (Blending Options, Bevel & Emboss, Stroke, Inner Shadow, Outer Glow, Drop Shadow); the fx button | P2 | Library: layer effects in the compositor, kept on the layer and edited from a dialog |
| Layers: link button | P3 | Library: linked layers that move and transform together |
| Layers: filter by type and shape | P3 | Type and shape layers (above and below) |
| Properties: Distribute | P3 | Library: several selected layers at once (multi-select in the image step) |
| History snapshots | P2 | Library: named snapshots kept in the image's history |
| Colour: patterns; New Fill Layer (Solid Colour, Gradient, Pattern) | P2 | Library: fill layers as a layer kind (solid, gradient, tiled image), re-rendered at any size |
| Brushes panel; the options bar's Brush settings button | P3 | JSON: a panel over the existing presets and options |
| Channels: show one channel, hide a channel | P2 | Library: a channel view mask in the canvas display |
| Info panel (pointer position, colour under it) | P2 | Library: the raster node publishing the pointer's document position and the composite colour there |
| Paths panel; Pen and Path Selection tools; Type > Create Work Path | P2 | Library: vector paths on the document (pen editing, stroke and fill, to a selection) |
| Shapes as vector layers (the Rectangle Tool); Layer > Rasterize | P2 | Library: shape layers built on the vector paths |
| Character and Paragraph; Type > Panels, Rasterize Type Layer | P3 | Type layers |
| Quick selection | P2 | Library: an edge-aware brush selection |
| Transform Selection; Select > Reselect | P2 | Library: `transformBox` on the selection mask, and keeping the last selection |
| Crop ratios that hold a drag to their shape | P2 | Library: a ratio option on the crop drag, next to the Shift square in `stretchBox` |
| Gradient: Reverse, Dither, Transparency checks | P2 | Library: those three flags on the gradient fill |
| Healing brush and patch (Healing Tool) | P2 | Library: a texture-matching brush and patch |
| Blur, sharpen, smudge (Blur Tool) | P2 | Library: brush modes that filter or drag pixels under the tip |
| History brush (Restore Brush Tool) | P3 | Library: painting from a history state (needs snapshots) |
| Dodge, burn, sponge (Dodge Tool) | P3 | Library: brush modes that lighten, darken or desaturate |
| Brush: pressure for size and opacity, smoothing, angle, airbrush | P2 | Library: brush options mapping pressure to size and opacity, stroke smoothing, a tip angle and build-up |
| Anti-alias checks (marquee, fill) | P3 | Library: an anti-alias flag on selection edges and fills (always on now) |
| Curves per channel | P2 | Library: per-channel curves in the adjustment (the curves node already shows channels); then JSON |
| Hue/Saturation: Colorize and colour ranges | P3 | Library: a colorize mode and per-range hue in the adjustment |
| Filters: more kinds | P2 | Library: further filter kinds (high pass, median, lens blur); each then a JSON dialog |
| Edit > Stroke | P2 | Library: stroking the selection's outline |
| Image > Mode | P2 | Library: converting the document's format (8/16 bit, greyscale) |
| Image > Trim | P3 | Library: the bounds of the non-transparent pixels, then a crop (JSON) |
| File > Print | P3 | The host: printing isn't offered to screens |
| Edit the toolbar (the tools bar's … button) | P3 | JSON: a dialog that reorders `state.tools` |
