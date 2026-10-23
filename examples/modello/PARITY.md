# Modello's parity list

Modello is a 3D modeller. This list says what a daily user of such a modeller finds, and
what isn't there yet. Modello is JSON only: `./build/affrescoui_host examples/modello/modello.screen.json` runs it.

`done`: it works as such a user expects. `partial`: it is there, with a difference the note says. `missing`: not yet.

## Layout

| What | Status | Note |
|---|---|---|
| Top bar: File, Edit, Render, Window, Help menus | partial | the menus hold a few commands each |
| Workspace tabs (Layout, Modeling, ... Scripting) | partial | drawn; only Layout is a workspace |
| Scene and view layer pickers | partial | drawn, one scene |
| 3D viewport header: editor type, mode, View, Select, Add, Object | done | |
| Header: orientation, pivot, snapping, proportional editing | done | proportional is a toggle only |
| Header: visibility, gizmos, overlays, X-ray, four shading modes | partial | X-ray and look development show as solid |
| Navigation gizmo (drag to orbit, click an axis) | done | |
| Zoom, pan, camera and orthographic buttons | done | |
| Left toolbar | done | Select, Cursor, Move, Rotate, Scale, Transform, Annotate, Measure, Add Cube |
| N sidebar: Item, Tool, View | partial | Item shows the transform; Tool and View are short |
| View name and the active object's collection | done | |
| Outliner: collections, objects, data icons | done | |
| Outliner: selectable, eye and camera toggles | done | |
| Properties editor: tab strip | done | |
| Properties: Object tab panels (Transform, Delta Transform, ...) | partial | only Transform has content |
| Timeline: play controls, frame, start, end | done | |
| Timeline: keyframe diamonds | done | |
| Status bar: mouse hints, counts, version | done | |
| Light and dark themes | done | Window › Toggle Light and Dark Theme |
| Camera and light glyphs in the viewport | done | a frustum with its up triangle; a lamp with a line to the floor |

## Object mode

| What | Status | Note |
|---|---|---|
| Click to select, Shift+click to add | done | |
| A, Alt+A, Ctrl+I | done | |
| G, R, S with X/Y/Z locks and Shift for planes | done | |
| Typed values (digits, minus, backspace) | done | |
| Enter or click confirms, Escape or right click cancels | done | |
| Shift+A Add menu (meshes, lights, camera, empty) | done | at the viewport's centre |
| X and Delete | done | |
| Shift+D duplicate, then move | done | |
| H, Shift+H, Alt+H | done | |
| Numpad 1/3/7, Ctrl for the opposite, 5, 0, period, 2/4/6/8, +/- | done | |
| Home (frame all) | done | |
| Middle drag orbits, Shift+middle pans, wheel zooms | done | |
| 3D cursor (Shift+right click) | done | |
| Pivot point (median, cursor, active) | done | |
| Snapping | done | |
| Ctrl+Z, Ctrl+Shift+Z | done | |
| F3 search | done | over every command, with its key |
| Adjust Last Operation panel, F9 | partial | Move, Resize and Add adjust; Rotate shows its name only; F9 opens it in place |
| Z and ~ pie menus | partial | Z opens a shading popup, not a pie |
| Right click object context menu | done | |
| Editor-type switcher | partial | the menu is drawn; editors don't switch yet |
| Area split and join | missing | the areas resize |
| I inserts a keyframe, Space plays | done | |

## Edit mode

| What | Status | Note |
|---|---|---|
| Tab into edit mode | partial | the mode switches; mesh editing isn't there |
| 1/2/3 vertex, edge, face select | missing | |
| E extrude, I inset, Ctrl+R loop cut, Ctrl+B bevel | missing | |

## Other workspaces and files

| What | Status | Note |
|---|---|---|
| Shading workspace: node editor, material slots | missing | |
| Import and export glTF | missing | |
| Save and open | missing | |
| Render window | missing | |
| Chat add-on | done | Window › Assistant, Ctrl+Shift+A |
| Keyboard access and accessibility names | partial | names on every control; Tab order not tuned |

## Counts

Round 1: 31 done, 10 partial, 11 missing.
