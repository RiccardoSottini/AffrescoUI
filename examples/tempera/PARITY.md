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
| Marquee rectangle and ellipse, Shift/Alt add/subtract/intersect, marching ants | partial | Modifier combinations incomplete |
| Lasso and polygonal lasso | done | |
| Magic wand with tolerance, quick selection | partial | No quick selection |
| Select all, deselect, inverse, feather, transform selection | partial | No transform selection |
| Move with auto-select | partial | No auto-select |
| Free transform (Cmd+T): handles, rotate, skew, commit and cancel | partial | A dialog: move, scale (flip with a negative), any angle, OK and Cancel; no on-canvas handles, no skew |
| Crop with overlay grid and aspect presets | partial | Thirds while dragging; the ratios crop the largest centred part, but don't hold a drag to that shape |
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
| Levels with histogram | partial | Dialog without histogram |
| Curves | partial | |
| Hue/Saturation, Brightness/Contrast | partial | |
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
