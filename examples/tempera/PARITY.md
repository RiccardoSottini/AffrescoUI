# Tempera parity matrix

Tempera is modelled on a professional raster image editor (Photoshop). This matrix lists the areas and workflows a
daily user of that kind of editor expects, each `done`, `partial` or `missing`, with the gap in one line. Every round
takes the gaps a daily user would notice first. Tempera keeps its own name, the Lucide icons and invented sample data.

## Counts

| Round | done | partial | missing |
| --- | --- | --- | --- |
| 0 (start) | 9 | 22 | 27 |

## Workspace

| Area | Status | Gap |
| --- | --- | --- |
| Menu bar: File, Edit, Image, Layer, Type, Select, Filter, View, Window, Help | partial | Fewer menus and items than expected; Type and Window missing |
| Options bar that changes with the tool | partial | Present for a few tools only |
| Tools bar on the left, one column, grouped with flyouts (long press) | partial | No flyouts; groups shown as separate buttons |
| Foreground/background swatches with swap and reset, quick mask button | partial | Swatches present; quick mask button missing |
| Document tabs: name, zoom, layer, mode ("@ 100% (Layer 1, RGB/8)") | missing | No document tab strip |
| Rulers and guides | partial | Rulers and guides; no dragging guides out of the rulers |
| Dark grey pasteboard, checkerboard for transparency | done | |
| Status bar: zoom field and document size | partial | Zoom shown; no editable zoom field or document size |
| Panels docked right in tabbed groups, collapsible to icons | missing | Panels stacked, no tabs, no icon strip |
| Light and dark interface, locales, keyboard focus, accessible names | partial | Keep and extend every round |

## Panels

| Panel | Status | Gap |
| --- | --- | --- |
| Layers: thumbnails, eye, locks, blend mode, opacity and fill | partial | No fill, no lock icons row |
| Layers: groups with disclosure, masks with link, adjustment layers, fx | partial | No link icon between layer and mask; fx badge missing |
| Layers: bottom buttons link, fx, mask, adjustment, group, new, delete | partial | Some buttons missing |
| Properties (context-sensitive) | partial | Shows layer only |
| Adjustments (icon grid) | missing | |
| History (states and snapshots) | partial | States only, no snapshots |
| Color and Swatches | partial | Basic |
| Brushes (presets) | missing | |
| Channels | missing | |
| Paths | missing | |
| Navigator | done | |
| Info (pointer position, colour under the pointer) | missing | |
| Character and Paragraph | missing | |

## Tools

| Tool or workflow | Status | Gap |
| --- | --- | --- |
| Brush: size, hardness, opacity, flow, round outline cursor, `[` `]` | partial | Keys are `,` `.`; no outline cursor |
| Eraser | done | |
| Pencil | missing | |
| Marquee rectangle and ellipse, Shift/Alt add/subtract/intersect, marching ants | partial | Modifier combinations incomplete |
| Lasso and polygonal lasso | done | |
| Magic wand with tolerance, quick selection | partial | No quick selection |
| Select all, deselect, inverse, feather, transform selection | partial | No transform selection |
| Move with auto-select | partial | No auto-select |
| Free transform (Cmd+T): handles, rotate, skew, commit and cancel | missing | |
| Crop with overlay grid and aspect presets | partial | No presets, no rule-of-thirds grid |
| Gradient tool with editor | partial | No gradient editor |
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
| Keyboard shortcuts as the counterpart's | partial | Most single-key tools; several menu shortcuts missing |
| Assistant chat add-on (shared) | missing | Waiting for the shared add-on |
