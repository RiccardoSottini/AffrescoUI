# Modello's parity list

Modello is a 3D modeller. This list says what a daily user of such a modeller finds, and
what isn't there yet. Modello is a Lisp program: `./build/affrescoui_launcher examples/modello/lisp/index.screen.lisp` runs it.

`done`: it works as such a user expects. `partial`: it is there, with a difference the note says. `missing`: not yet.

## Layout

| What | Status | Note |
|---|---|---|
| Top bar: File, Edit, Render, Window, Help menus | done | new, open, save, export glTF; undo, redo, copy, paste; render; workspaces, theme, assistant |
| Workspace tabs | done | Modello's own five, as an icon segmented control: Stage, Shape, Surface (the Shader Editor under the viewport), Motion (a taller timeline) and Shot (rendered shading, no overlays); Ctrl+Page Up and Page Down step through them |
| Scene and view layer pickers | partial | the names can be edited; one scene and one layer |
| 3D viewport header: editor type, mode, View, Select, Add, Object | done | |
| Header: orientation, pivot, snapping, proportional editing | done | every pivot, Individual Origins too; every orientation but Gimbal (View follows the camera's axes); proportional editing (O) pulls the vertices near the moved ones along in Edit Mode |
| Header: visibility, gizmos, overlays, X-ray, four shading modes | done | X-ray sees through; look development lights with a studio light or the scene's |
| Navigation gizmo (drag to orbit, click an axis) | done | |
| Zoom, pan, camera and orthographic buttons | done | |
| Left toolbar | done | Select, Cursor, Move, Rotate, Scale, Transform, Annotate, Measure, Add Cube |
| N sidebar: Item, Tool, View | done | fields, locks, the tool's settings, the view's lens and clipping |
| View name and the active object's collection | done | |
| Outliner: collections, objects, data icons | done | several collections: New Collection, rename, delete, M moves objects between them, each with its own toggles |
| Outliner: selectable, eye and camera toggles | done | |
| Properties editor: tab strip | done | |
| Properties: Object tab panels (Transform, Transform Offset, ...) | partial | Transform, Visibility, Relations (the parent) and Viewport Display (solid, wire or bounds) edit; Collections, Transform Offset, Instancing, Motion Paths, Line Drawing and Custom Properties are disabled with a reason |
| Properties: Modifiers tab | done | mirror (X, Y or Z), array (its count) and subdivision (its levels), stacked, each removable, undoable; drawn live, and left out while the object is in Edit Mode |
| Properties: Texture tab | done | Open Image picks a PNG or JPEG as the material's base colour texture (kept in the scene, saved with it), Remove Image takes it off; a primitive maps it by its own coordinates, an edited mesh has none yet |
| Properties: Data › Normals | done | Shade Smooth on or off for a mesh (its primitive becomes an editable mesh) |
| Properties: Material, World, Data, Render, Output, Scene, Collection, Tool tabs | done | browse, add and unlink materials; base colour, metallic, roughness; blend mode (opaque, clip, blend) and alpha; the world colour; a light's or a camera's settings, a light's shadow; exposure; the frame range; the resolution (the render window takes its shape) and the frame rate (the timeline, playback and the export follow it); search |
| Timeline: play controls, frame, start, end | done | |
| Timeline: keyframe diamonds | done | |
| Keyframes animate (playback, next and previous key, dragging keys) | done | translation, rotation and scale, in frame order |
| Status bar: mouse hints, counts, version | done | |
| Light and dark themes | done | Window › Toggle Light and Dark Theme |
| Camera and light glyphs in the viewport | done | a frustum with its up triangle; a lamp with a line to the floor; light over a dark background, dark over a light one |
| Floor grid | done | a backdrop every surface covers, even below the floor |

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
| Editor-type switcher | done | an editor swaps with the area that shows it; every type works: the Image Editor shows the camera's render, kept up to date, and the Text Editor the scene's notes, saved with it |
| Area split and join | missing | the areas resize |
| I inserts a keyframe, Space plays | done | |
| Ctrl+C and Ctrl+V copy and paste objects | done | |
| Alt+G, Alt+R, Alt+S clear; Shift+S snap menu | done | |
| Ctrl+Space maximizes an area | done | |
| M moves to a collection | done | to an existing one or a new one |
| Parenting (Ctrl+P, Alt+P) | done | Ctrl+P, Alt+P, the Object menu, the context menu and the Relations panel; each keeps its place, turned and scaled as it was, however its old and new parents are turned or scaled; the outliner nests children; deleting a parent frees its children where they are |

## Edit mode

| What | Status | Note |
|---|---|---|
| Tab into edit mode | done | Tab or the mode menu, on the active mesh: its primitive becomes an editable mesh in the scene; its edges, vertices and selected faces are drawn over it, the status bar counts its vertices, edges and faces |
| 1/2/3 vertex, edge, face select | done | also three buttons in the header; a click picks, Shift adds or takes out, A and Alt+A select all or nothing; G, R and S move, turn and scale the selected vertices (with proportional editing) |
| E extrude, I inset, Ctrl+R loop cut, Ctrl+B bevel | partial | E extrudes the selected faces as a region, I insets them, Ctrl+R cuts the loop of quads across the selected edge, X deletes vertices or faces, G, R and S move, turn and scale the selection (the selected faces follow the pointer), each undoable; no bevel yet, and E extrudes a fixed half unit (G moves it on) |

## Other workspaces and files

| What | Status | Note |
|---|---|---|
| Shading workspace: node editor, material slots | done | the Shader Editor adds, links, sets and deletes nodes of the material's graph, undoable |
| Import and export glTF | done | File › Import glTF reads a .gltf with its data inside: its objects, materials and meshes join the scene, the meshes editable (no textures or animations from it yet); File › Export glTF writes the keyframes as an animation (no textures yet) |
| Save and open | done | Ctrl+S and Ctrl+O, and the File menu: the scene as JSON in a file the person picks, checked when it opens, undoable |
| Render window | done | F12: the camera's view, lit, without markers; Save As Image |
| Chat add-on | done | Window › Assistant, Ctrl+Shift+A |
| Keyboard access and accessibility names | partial | names on every control; Tab order not tuned |

## Counts

Round 1: 31 done, 10 partial, 11 missing.

Round 2 (a folder of files): 35 done, 11 partial, 6 missing (34, 11 and 7 before it). Save and open.

Round 3 (every control works): 49 done, 5 partial, 5 missing (35, 11 and 6 before it, over 52 rows; 7 rows added). Keyframes animate, the properties tabs, the Shader Editor, the render window, collections, pie menus, glTF export.

Round 4 (the revamp): 52 done, 4 partial, 4 missing (one row added: the floor grid). Modello's own workspaces, parenting,
Individual Origins and the View orientation, the Image and Text Editors, viewport display as wire or bounds, a light's shadow, a material's blend
mode and alpha, the output's resolution and frame rate; the grid drawn behind every surface, the object named once in
the properties, glyphs that contrast with the background, a solid shading whose faces read apart.

Round 5 (round 2 of the revamp): 58 done, 4 partial, 1 missing (three rows added: the Modifiers and Texture tabs, Data › Normals). Edit Mode on editable polygon meshes (`scene3d`'s `PolyMesh.hpp`
and the `mesh...` expression functions): vertex, edge and face selection, extrude, inset, loop cut, delete, proportional
editing; the modifier stack; Shade Smooth; glTF import; image textures; a turned or scaled parent keeping its child's place.

## Left for later

Every row above that is partial or missing, and every control Modello shows disabled with a tooltip. "JSON" means
Modello's files alone can do it; otherwise the general library feature it needs. P1: a daily user meets it; P2:
occasionally; P3: rarely.

| What | Priority | Needs |
|---|---|---|
| Ctrl+B bevel; E then moving with the pointer | P1 | library: a bevel operator in `PolyMesh.hpp`; JSON: the extrude starting a move of the pivot |
| Area split and join | P2 | JSON if the areas become a split tree in state; simpler with a library docking layout |
| Textures in the glTF export and on edited meshes; glTF import with textures and animations | P2 | library: the export embedding the scene's pictures; texture coordinates on polygon meshes; `gltfScene` keeping them |
| Material › Settings: shadow | P2 | library: shadow casting per material (blend mode and alpha are done) |
| Camera › Depth of Field | P2 | library: depth of field in the scene renderer (Light › Shadow and Mesh › Normals are done) |
| Keyboard access: Tab order | P2 | JSON: focus order through the areas (header, toolbar, outliner, properties, timeline) |
| Workspaces Sculpt, UV, Paint | P3 | library: sculpting, UV unwrapping and texture painting on editable meshes |
| Workspaces Compose, Nodes, Script | P3 | library: a compositor, geometry nodes, a script console (the last may never fit a JSON-only app) |
| Scene and view layer pickers, View Layer tab, outliner Scenes mode | P3 | JSON: a list of scenes and layers in state, each with its own nodes and toggles |
| Outliner Orphan Data mode | P3 | JSON: materials and meshes no object uses, listed from state |
| Orientation: Gimbal | P3 | library: gimbal axes from the object's Euler rotation in the gizmo (View is done) |
| Rotation mode (quaternion, axis angle) | P3 | JSON: other rotation fields converted to rotationDegrees |
| Properties pin | P3 | JSON: a pinned object id the panels read instead of the active one |
| Object › Collections (link to several) | P3 | JSON: a list of collections per object instead of one |
| Object › Transform Offset, Custom Properties | P3 | JSON: delta transforms added in the displayed scene; free key-value fields per object |
| Object › Instancing, Motion Paths, Line Drawing | P3 | library: instancing, path lines through keyframe positions, line-art rendering in `scene` |
| Particles, Physics, Object Constraints tabs | P3 | library: particles and a physics step; constraints may be JSON (track-to as an expression) |
| Render › Sampling, Scene › Units, Tool › Workspace | P3 | library: a sampled renderer; JSON: unit labels on the fields; JSON: workspace settings |
