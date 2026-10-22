# scene3d: general 3D

`scene3d` is the 3D model behind Modello (`examples/modello`): maths, a scene of nodes with meshes, materials, lights
and cameras, keyframe animation, glTF 2.0 files in and out, the orbit camera, picking and gizmo maths, material node
graphs, and a PBR forward renderer that a screen shows in a `surface` node. The scene is small and lives in a
document's state as JSON; the vertices and pixels it names by id stay in an `Assets` store on the host. Target:
`affrescoui::scene3d` (uses `core` and `render`; cgltf for glTF, from `cmake/AffrescoUIGltf.cmake`).

## Public headers (`include/affresco/scene3d/`)

| Header | Offers |
|---|---|
| `Math.hpp` | Vectors, quaternions, column-major 4 by 4 matrices and translation-rotation-scale transforms; right-handed, y up |
| `Bounds.hpp` | Boxes, spheres, rays and the view frustum: what culling and picking test against |
| `Mesh.hpp` | Mesh and texture data on the CPU, their checks, normals and ray casts, and the primitives (cube, sphere, cylinder, torus, plane) |
| `Scene.hpp` | `Scene`: nodes in a hierarchy with meshes, materials, lights and cameras; `Assets`: the store of their heavy data |
| `SceneJson.hpp` | Scenes and animation clips as JSON, the form they take in a document's state |
| `Animation.hpp` | Tracks of a node's translation, rotation or scale, with step, linear or cubic keys, and clips of them |
| `Gltf.hpp` | `.gltf` and `.glb` loaded into a scene within `GltfLimits`, and a scene exported as `.gltf` |
| `Camera.hpp` | The orbit camera: orbit, pan, zoom, frame a box, snap to a view, perspective or orthographic; as JSON |
| `Picking.hpp` | The node under a ray (bounds first, then triangles) and the nodes inside a screen box, on the CPU |
| `Gizmo.hpp` | Which handle a ray points at, and what dragging it or typing a number does to a transform, with snapping |
| `MaterialGraph.hpp` | Material node graphs checked against a fixed set of operations, then assembled into GLSL from fixed snippets |
| `Viewport.hpp` | What a viewport shows, read from a surface's parameters, and the scene at a time of its animations |
| `Renderer.hpp` | `makeSceneRenderer`: the renderer as a `GpuSurface`, within a `GpuBudget`; `ViewportProbe`: what it drew and picked |

## Invariants

- **Heavy data never goes into JSON.** A scene names meshes and textures by id; the `Assets` store holds them, and the
  renderer re-uploads one when the store's data under its id changes.
- **A file is untrusted input.** A glTF file's counts and sizes are checked against `GltfLimits` before anything is
  read, and its accessors against their buffers; buffers and images come only from the GLB's chunk, `data:` URIs or
  relative files beside it without `..` that stay in its folder, never the network or an absolute path. Extensions it
  doesn't decode are skipped with a warning naming them. The loader is fuzzed (`tests/fuzz/gltf.cpp`).
- **A document never supplies shader source.** A material graph chooses operations and numbers; only the host's
  snippets and the numbers it formats reach the shader, between fixed parts of the renderer's own.
- **GPU memory stays within the budget** (`GpuBudget`, 512 MiB): past it, the least recently used meshes and textures
  are freed; what the frame being drawn uses is kept.
- **Pure where it can be.** Maths, picking, gizmo and viewport reading don't touch the GPU, so the renderer and the
  natives that pick and drag agree on what is on screen, and tests run headless.

## Threading and ownership

- **One thread at a time.** Scenes, assets and cameras are values used by one thread; the renderer and its
  `ViewportProbe` are used on the thread that renders, which owns the GL context.
- **The renderer shares the `Assets` store** (`std::shared_ptr<const Assets>`) with its host and owns its GPU meshes,
  textures and programs, freed with it.

## Extension points

- **A material operation:** its ports and GLSL snippet in `render/MaterialGraph.cpp`, checked like the others.
- **A primitive:** a generator in `scene/Primitives.cpp`.
- **A glTF feature:** read in `gltf/GltfConvert.cpp` after the loader's checks, with a limit in `GltfLimits` if it
  brings data of its own.
- **Something drawn over the scene:** an overlay in `render/RendererOverlays.cpp`.

## Sub-folders (`src/`)

| Folder | Holds |
|---|---|
| `math/` | Vectors, matrices and transforms (`Math.cpp`), bounds, rays and frusta (`Bounds.cpp`) |
| `scene/` | Meshes, primitives, scenes and their assets, their JSON, and animation |
| `gltf/` | Loading and checking files (`GltfLoad.cpp`), turning them into a scene (`GltfConvert.cpp`), exporting (`GltfExport.cpp`), and cgltf compiled once (`Cgltf.cpp`) |
| `viewport/` | The orbit camera, picking, gizmo maths and reading a viewport's parameters |
| `render/` | Material graphs, GL programs, GPU resources within the budget, the renderer (`Renderer.cpp`) and its overlays |

## Tests

`tests/unit/scene3d.cpp` (maths, scenes, glTF in and out, hostile files, rendering), `tests/unit/scene3d_modello.cpp`
and `tests/unit/scene3d_tools.cpp` (Modello's tools over the module), the fuzz target `tests/fuzz/gltf.cpp`, and
`tests/perf/modello.cpp`.
