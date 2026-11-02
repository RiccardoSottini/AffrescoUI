# Modello: a 3D modeller in Lisp

Modello is a 3D modeller written only as screen documents and a program: no C++ of its own. The stock host runs it:

```
./build/affrescoui_launcher examples/modello/lisp/index.screen.lisp [--socket DIR]
```

**Its `lisp/` is a real Lisp program** (RFC 0005; SCREEN_FORMAT.md, "The Lisp language"): Undo and Redo, deleting, hiding and
showing objects, putting the selection on the 3D cursor and clearing a transform are functions and commands in
`lisp/modello.lisp`, where one function changes the selected nodes that a list of steps had to change one path at a time.
The tools that follow the pointer, the editors and what reaches outside (opening, saving, exporting) stay actions in the
parts. `json/` beside it is the same app written in JSON, also by hand: the Launcher's Settings choose which one opens, the same scenarios check both, and `tools/notation/lisp_examples.py` leaves both alone.

Its layout and keys follow the conventions of desktop 3D modellers, so a daily user of such a modeller finds what they expect where they
expect it: the top bar with its menus and workspace tabs, the 3D viewport with its header, toolbar, navigation gizmo
and sidebar, the outliner, the properties editor, the timeline and the status bar. `PARITY.md` lists what is there and
what isn't yet.

The viewport is the library's `scene` node (Z up, a 3D cursor, a pivot, a gizmo and typed transforms); everything else
is the format's own nodes and the built-in kit. Every change, by a person or an agent, is a revision, so undo, history
and review cover the scene and the workspace alike.

| File | What it holds |
|---|---|
| `lisp/index.screen.lisp` | The window, fonts, colours (dark and light), the state and the areas |
| `lisp/modello.lisp` | The program: Undo and Redo, and the edits of the selected objects |
| `lisp/parts/commands.lisp` | The other actions, the keys and the menus |
| `lisp/parts/viewport.lisp` | The 3D viewport, its header, toolbar, navigation gizmo and sidebar |
| `lisp/parts/editors.lisp` | The top bar, outliner, properties editor, timeline and status bar |
| `lisp/parts/shader.lisp` | The shader editor's nodes and materials |
| `lisp/index.scenarios.json` | The scenarios, run headless (they stay JSON) |

## Keys

| Keys | What they do |
|---|---|
| `g`, `r`, `s`, then `x`/`y`/`z` (Shift: a plane), digits, Enter or Escape | Move, rotate, scale |
| `shift+a`, `x` or Delete, `shift+d`, `h`, `shift+h`, `alt+h` | Add, delete, duplicate, hide, reveal |
| `a`, `alt+a`, `mod+i` | Select all, none, the inverse |
| Numpad 1, 3, 7 (Ctrl: the opposite side), 5, 0, period; Home | Views, projection, camera, frame |
| Middle drag, Shift+middle drag, wheel; Shift+right click | Orbit, pan, zoom; place the 3D cursor |
| `mod+z`, `mod+shift+z`, `n`, `t`, `i`, Space | Undo, redo, sidebar, toolbar, keyframe, play |
| `mod+n`, `mod+o`, `mod+s` | A new scene, open one, save it (JSON, in a file you pick) |

## Samples

For glTF import, from the Khronos glTF Sample Assets, each with its licence and README in its folder, and listed in
`cmake/dependencies.json` (892 KiB in all):

| Sample | Licence |
|---|---|
| `AnimatedCube` | CC0 1.0, © 2017 UX3D |
| `BoxAnimated` | CC BY 4.0, © 2017 Cesium |
| `MetalRoughSpheresNoTextures` | CC0 1.0 |

## Tests

- `index.scenarios.json`: what the keys do, played headless (`./build/affrescoui test
  examples/modello/lisp/index.scenarios.json`, and in ctest's `scenarios`).
- `tests/render/scenarios/modello.json`: its frames in the rendering check.
- `tests/unit/scene3d*.cpp`: the scene module's maths, scene JSON, glTF loading and limits, picking, gizmos.
- `tests/perf/modello.cpp` (`affrescoui_perf_modello`, built with `AFFRESCOUI_BUILD_BENCHMARKS`): a 200,000-triangle
  scene orbited, within 16.7 ms a frame, and picking, within 8 ms; with `AFFRESCOUI_PERF_BUDGETS` set, a miss fails.
- `tests/fuzz/gltf.cpp`: the glTF loader, fuzzed.
