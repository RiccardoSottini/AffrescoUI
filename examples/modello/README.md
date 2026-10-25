# Modello: a 3D modeller in JSON

Modello is a 3D modeller written only as screen documents: no C++ of its own. The stock host runs it:

```
./build/affrescoui_launcher examples/modello/index.screen.json [--socket DIR]
```

Its layout and keys follow the conventions of desktop 3D modellers, so a daily user of such a modeller finds what they expect where they
expect it: the top bar with its menus and workspace tabs, the 3D viewport with its header, toolbar, navigation gizmo
and sidebar, the outliner, the properties editor, the timeline and the status bar. `PARITY.md` lists what is there and
what isn't yet.

The viewport is the library's `scene` node (Z up, a 3D cursor, a pivot, a gizmo and typed transforms); everything else
is the format's own nodes and the built-in kit. Every change, by a person or an agent, is a revision, so undo, history
and review cover the scene and the workspace alike.

| File | What it holds |
|---|---|
| `index.screen.json` | The window, fonts, colours (dark and light), the state and the areas |
| `parts/commands.json` | The actions, the keys and the menus |
| `parts/viewport.json` | The 3D viewport, its header, toolbar, navigation gizmo and sidebar |
| `parts/editors.json` | The top bar, outliner, properties editor, timeline and status bar |

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
  examples/modello/index.scenarios.json`, and in ctest's `scenarios`).
- `tests/render/scenarios/modello.json`: its frames in the rendering check.
- `tests/unit/scene3d*.cpp`: the scene module's maths, scene JSON, glTF loading and limits, picking, gizmos.
- `tests/perf/modello.cpp` (`affrescoui_perf_modello`, built with `AFFRESCOUI_BUILD_BENCHMARKS`): a 200,000-triangle
  scene orbited, within 16.7 ms a frame, and picking, within 8 ms; with `AFFRESCOUI_PERF_BUDGETS` set, a miss fails.
- `tests/fuzz/gltf.cpp`: the glTF loader, fuzzed.
