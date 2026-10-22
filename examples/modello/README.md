# Modello: a 3D application in JSON

Modello is a demo of a creative tool on the library: a 3D modeller and scene editor whose whole workspace is one screen
document, `modello.screen.json`, around a PBR viewport drawn by the `scene3d` module. People and agents change the
workspace and the scene while it runs, and every change is a revision, so undo, history and review cover both. It uses
only the library's public API. It is an example, not part of the library.

```
./build/affrescoui_modello [--socket DIR] [--data DIR]
```

Like `affrescoui_host`, it listens for the Studio and an agent in `DIR` (`studio.sock`, `agent.sock`). glTF files are
imported from the bundled samples and from `--data DIR`'s `imports/`; the scene is exported to its `exports/`.

## What it has

| Part | What it does |
|---|---|
| Viewport | The host's `scene` surface (`makeSceneRenderer`): metallic-roughness PBR with directional, point and spot lights, one of which casts shadows, solid, material and wireframe shading, a grid and axes, and the selection's outline. Its camera orbits, pans and zooms with the mouse, frames the selection (`f`), switches to orthographic, and jumps to the front, side and top views |
| Selection | A click picks the nearest mesh (Shift adds to the selection); a drag on empty space selects by box; the outliner selects too |
| Transforms | A gizmo to drag (move, rotate, scale; world or local space; with snapping), and the keys: `g`, `r` and `s` start a move, a turn or a scale, `x`, `y` and `z` keep it to an axis, digits type the amount, Enter confirms and Escape cancels |
| Outliner and properties | The scene's tree (hidden objects greyed); the selection's name, location and scale, and a light's intensity, typed |
| Materials | Base colour, metallic and roughness, and a node graph (`graph`) of fixed operations (colour, number, mix, multiply, add, scale, checker, output) that the host checks and assembles from fixed GLSL snippets |
| Timeline | The scene's animation clips as tracks of keyframes (`timeline`): play, scrub, add a key, move a key |
| Files | Import `.gltf` and `.glb` (`mod+i`), export the scene as `.gltf` (`mod+e`) |
| Edit | Add (cube, sphere, cylinder, torus, plane, sun, point and spot lights, camera, empty), duplicate (`mod+d`), delete, undo (`mod+z`) |
| Review | A proposal from an agent waits in a review bar above the status bar, to accept or reject |
| Themes | Dark and light, as design tokens |

The scene and its animations are the document's own computed values (`computed.scene`, `computed.animations`). The
natives (`modello.*`, in `Modello*.cpp`) only compute: they return patches, and the document's steps commit them.

| Native | What it computes |
|---|---|
| `modello.camera`, `modello.pick`, `modello.boxSelect` | The camera after a drag, a scroll or a view key; what a click or a box selects (`ModelloView.cpp`) |
| `modello.gizmoHit`, `modello.gizmoDrag`, `modello.numeric` | Which gizmo handle is under the pointer, and the transform a drag or a typed amount gives |
| `modello.add`, `modello.duplicate`, `modello.remove`, `modello.stats` | Scene edits as patches, and the counts the status bar shows (`ModelloEdit.cpp`) |
| `modello.outline`, `modello.checkGraph`, `modello.materialNode` | The outliner's rows, and a material graph checked and edited |
| `modello.tracks`, `modello.setKey`, `modello.moveKey` | The timeline's tracks, and keyframes added or moved |
| `modello.samples`, `modello.import`, `modello.load`, `modello.export` | The files that can be imported, a file imported, the imports' meshes loaded at start, the scene exported (`ModelloFiles.cpp`) |

## Files and safety

Files are read only from `samples/` and the data folder's `imports/`, by relative paths without `..` that stay inside
them once links are resolved; exports are written only to `exports/`, under a sanitised name. The glTF loader checks
every count and size against its limits before reading anything, reads buffers only from the `.glb` itself, `data:`
URIs or relative files without `..` (never a network URI), and skips extensions it doesn't decode (Draco and meshopt
compression, KTX2 textures) with a warning naming them. A document never supplies shader source. SECURITY.md,
"Modello's 3D files", lists the limits.

## Samples

From the Khronos glTF Sample Assets, each with its licence and README in its folder, and listed in
`cmake/dependencies.json` (892 KiB in all):

| Sample | Licence |
|---|---|
| `AnimatedCube` | CC0 1.0, © 2017 UX3D |
| `BoxAnimated` | CC BY 4.0, © 2017 Cesium |
| `MetalRoughSpheresNoTextures` | CC0 1.0 |

## Tests

- `modello.scenarios.json`: 35 scenarios of the workspace, from `modello-scene` to an agent's panel added through a
  reviewed revision (`modello-agent-panel`) and one rejected. `tests/unit/scene3d_modello.cpp` plays them headless
  with Modello's natives: `./build/affrescoui_unit_tests -sf="*scene3d*"`.
- `tests/unit/scene3d*.cpp`: the module's maths, scene JSON, glTF loading and limits, picking, gizmos, material graphs.
- `tests/perf/modello.cpp` (`affrescoui_perf_modello`, built with `AFFRESCOUI_BUILD_BENCHMARKS`): a 200,000-triangle
  scene orbited, within 16.7 ms a frame, and picking, within 8 ms; with `AFFRESCOUI_PERF_BUDGETS` set, a miss fails.
- `tests/fuzz/gltf.cpp`: the glTF loader, fuzzed.
