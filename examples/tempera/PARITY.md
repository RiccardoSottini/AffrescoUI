# Tempera parity matrix

Tempera is modelled on a professional raster image editor (Photoshop). This matrix lists the areas and workflows a
daily user of that kind of editor expects, each `done`, `partial` or `missing`, with the gap in one line. Every round
takes the gaps a daily user would notice first. Tempera keeps its own name, the Lucide icons and invented sample data.

## Counts

| Round | done | partial | missing |
| --- | --- | --- | --- |
| 0 (start) | 9 | 22 | 27 |
| 1 | 15 | 25 | 19 |
| 2 | 15 | 28 | 17 |

## Workspace

| Area | Status | Gap |
| --- | --- | --- |
| Menu bar: File, Edit, Image, Layer, Type, Select, Filter, View, Window, Help | done | The ten menus with the counterpart's items and shortcuts; items without behaviour yet are disabled |
| Options bar that changes with the tool | done | Every tool has its row, with the tool's icon at the left |
| Tools bar on the left, one column, grouped with flyouts (long press) | done | Groups open on a right click (no long press); tools not built yet show disabled |
| Foreground/background swatches with swap and reset, quick mask button | done | |
| Document tabs: name, zoom, layer, mode ("@ 100% (Layer 1, RGB/8)") | partial | One document: the tab has its ×, but closing the only document isn't available yet |
| Rulers and guides | partial | Zero at the image's corner, minor ticks, click to add a guide; no dragging guides out of the rulers |
| Dark grey pasteboard, checkerboard for transparency | done | |
| Status bar: zoom field and document size | done | One information item at a time, chosen from its › menu |
| Search (commands), workspace switcher, screen modes | partial | Searches the commands only; one workspace; standard and full screen with menu bar |
| Panels docked right in tabbed groups, collapsible to icons | partial | Tabbed groups and an icon strip (History, Navigator); groups can't be dragged, resized or collapsed |
| Light and dark interface, locales, keyboard focus, accessible names | partial | Keep and extend every round |

## Panels

| Panel | Status | Gap |
| --- | --- | --- |
| Layers: thumbnails, eye, locks, blend mode, opacity and fill | partial | Fill shown, not editable; transparency and artboard locks disabled; the bottom layer isn't a locked Background |
| Layers: filter by kind | partial | Pixel, adjustment and group filters; type and shape layers don't exist yet |
| Layers: groups with disclosure, masks with link, adjustment layers, fx | partial | Disclosure isn't clickable; no fx |
| Layers: bottom buttons link, fx, mask, adjustment, group, new, delete | partial | Link and fx disabled |
| Properties (context-sensitive) | partial | Collapsible Transform (selection or image box, read-only), Align (disabled) and Quick Actions; adjustment controls |
| Adjustments (icon grid) | done | |
| History (states and snapshots) | partial | States only, no snapshots |
| Color, Swatches, Gradients and Patterns | partial | Foreground/background chips and a square with a horizontal hue strip; no patterns |
| Brushes (presets) | partial | Presets, size, hardness and spacing in the options bar's brush picker; no Brushes panel |
| Channels | partial | Composite and channel rows, display only |
| Paths | missing | An empty panel until the pen tool exists |
| Navigator | done | |
| Info (pointer position, colour under the pointer) | missing | |
| Character and Paragraph | missing | |

## Tools

| Tool or workflow | Status | Gap |
| --- | --- | --- |
| Brush: size, hardness, opacity, flow, round outline cursor, `[` `]` | partial | Keys are `,` `.`: the format's shortcuts don't name the bracket keys yet; no outline cursor |
| Eraser | done | |
| Pencil | missing | Shown disabled in the brush group |
| Marquee rectangle and ellipse, Shift/Alt add/subtract/intersect, marching ants | partial | Modifier combinations incomplete |
| Lasso and polygonal lasso | done | |
| Magic wand with tolerance, quick selection | partial | No quick selection |
| Select all, deselect, inverse, feather, transform selection | partial | No transform selection |
| Move with auto-select | partial | No auto-select |
| Free transform (Cmd+T): handles, rotate, skew, commit and cancel | missing | |
| Crop with overlay grid and aspect presets | partial | No presets, no rule-of-thirds grid |
| Gradient tool with editor | partial | Inline editor and presets; reverse, dither and modes disabled |
| Paint bucket and fill dialog | partial | No fill dialog |
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
| Color Balance, Black & White, Invert, Threshold | missing | Some exist as filters only |
| Gaussian Blur, Unsharp Mask, Add Noise, Motion Blur with live preview | partial | Preview exists; not every filter |

## Image and file

| Item | Status | Gap |
| --- | --- | --- |
| New document dialog with presets | missing | |
| Canvas size, image size, rotate canvas | missing | |
| Flatten, merge down | missing | |
| Export as PNG/JPEG | partial | PNG only |
| Open and save its own format | done | |
| Undo/redo with the History panel | done | |
| Keyboard shortcuts as the counterpart's | partial | Tools, menus, F12, Shift+F6; no bracket keys |
| Assistant chat add-on (shared) | missing | Waiting for the shared add-on |
