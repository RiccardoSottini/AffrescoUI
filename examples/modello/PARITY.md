# Modello's parity list

Modello is a 3D modeller. This list says what a daily user of such a modeller finds, and
what isn't there yet. Modello is JSON only: `./build/affrescoui_launcher examples/modello/index.screen.json` runs it.

`done`: it works as such a user expects. `partial`: it is there, with a difference the note says. `missing`: not yet.

## Layout

| What | Status | Note |
|---|---|---|
| Top bar: File, Edit, Render, Window, Help menus | done | new, open, save, export glTF; undo, redo, copy, paste; render; workspaces, theme, assistant |
| Workspace tabs (Layout, Model, ... Script) | partial | 5 of 11 work (Layout, Model, Materials, Animate, Render), Ctrl+Page Up and Page Down step through them; the rest are disabled with a reason |
| Scene and view layer pickers | partial | the names can be edited; one scene and one layer |
| 3D viewport header: editor type, mode, View, Select, Add, Object | done | |
| Header: orientation, pivot, snapping, proportional editing | done | proportional is a toggle only |
| Header: visibility, gizmos, overlays, X-ray, four shading modes | done | X-ray sees through; look development lights with a studio light or the scene's |
| Navigation gizmo (drag to orbit, click an axis) | done | |
| Zoom, pan, camera and orthographic buttons | done | |
| Left toolbar | done | Select, Cursor, Move, Rotate, Scale, Transform, Annotate, Measure, Add Cube |
| N sidebar: Item, Tool, View | done | fields, locks, the tool's settings, the view's lens and clipping |
| View name and the active object's collection | done | |
| Outliner: collections, objects, data icons | done | several collections: New Collection, M moves objects between them, each with its own toggles |
| Outliner: selectable, eye and camera toggles | done | |
| Properties editor: tab strip | done | |
| Properties: Object tab panels (Transform, Transform Offset, ...) | partial | Transform and Visibility edit; the others are disabled with a reason |
| Properties: Material, World, Data, Render, Output, Scene, Collection, Tool tabs | done | browse, add and unlink materials; base colour, metallic, roughness; the world colour; a light's or a camera's settings; exposure; the frame range; search |
| Timeline: play controls, frame, start, end | done | |
| Timeline: keyframe diamonds | done | |
| Keyframes animate (playback, next and previous key, dragging keys) | done | translation, rotation and scale, in frame order |
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
| Adjust Last Operation panel, F9 | done | Move, Rotate, Resize and Add adjust; F9 opens it in place |
| Z and ~ pie menus | done | Z: shading and X-ray; ~: the views |
| Right click object context menu | done | |
| Editor-type switcher | done | an editor swaps with the area that shows it; those Modello lacks are disabled with a reason |
| Area split and join | missing | the areas resize |
| I inserts a keyframe, Space plays | done | |
| Ctrl+C and Ctrl+V copy and paste objects | done | |
| Alt+G, Alt+R, Alt+S clear; Shift+S snap menu | done | |
| Ctrl+Space maximizes an area | done | |
| M moves to a collection | done | to an existing one or a new one |
| Parenting (Ctrl+P, Alt+P) | missing | |

## Edit mode

| What | Status | Note |
|---|---|---|
| Tab into edit mode | partial | the mode switches; mesh editing isn't there |
| 1/2/3 vertex, edge, face select | missing | |
| E extrude, I inset, Ctrl+R loop cut, Ctrl+B bevel | missing | |

## Other workspaces and files

| What | Status | Note |
|---|---|---|
| Shading workspace: node editor, material slots | done | the Shader Editor adds, links, sets and deletes nodes of the material's graph, undoable |
| Import and export glTF | partial | export only (File › Export glTF, with the keyframes as an animation; no textures yet) |
| Save and open | done | Ctrl+S and Ctrl+O, and the File menu: the scene as JSON in a file the person picks, checked when it opens, undoable |
| Render window | done | F12: the camera's view, lit, without markers; Save As Image |
| Chat add-on | done | Window › Assistant, Ctrl+Shift+A |
| Keyboard access and accessibility names | partial | names on every control; Tab order not tuned |

## Counts

Round 1: 31 done, 10 partial, 11 missing.

Round 2 (a folder of files): 35 done, 11 partial, 6 missing (34, 11 and 7 before it). Save and open.

Round 3 (every control works): 49 done, 6 partial, 4 missing (35, 11 and 6 before it, over 52 rows; 7 rows added). Keyframes animate, the properties tabs, the Shader Editor, the render window, collections, pie menus, glTF export.
