# imaging: layered images on tiles

`imaging` is the image model behind Tempera (`examples/tempera`): images of up to a gigapixel stored as sparse grids
of copy-on-write tiles, documents of layers, groups and adjustment layers with masks and a selection, brushes,
filters, colour management, an undo history by tile differences, PNG and JPEG files, and a project format read
lazily. The CPU compositor is the reference; the canvas composites the same layers on the GPU, tile by tile, within a
memory budget. Target: `affrescoui::imaging` (uses `core` and `render`; stb_image and stb_image_write for PNG and JPEG,
Little CMS 2 for ICC profiles, from `cmake/AffrescoUIColor.cmake`).

## Public headers (`include/affresco/imaging/`)

| Header | Offers |
|---|---|
| `Pixels.hpp` | The formats (`rgba8`, `rgba16f`, `mask8`), `Pixel` (straight-alpha floats), `tileSize` (256) and half floats |
| `Tile.hpp` | `Tile`: one 256 by 256 square of one format, shared by pointer until written |
| `Image.hpp` | `Image`: a sparse grid of tiles, loaded on first use from a `TileSource` within `Limits::cacheBytes`, with mips made on demand and kept current with each tile's version |
| `Document.hpp` | `Document`: pixel layers, groups and adjustment layers, masks, clipping, locks, the selection; edits as steps (`begin`, `editTile`, `commit`) and structure changes, undone and redone through its `History` |
| `History.hpp` | `History`: steps that keep only the tiles they changed, before and after, bounded by `undoSteps` and `undoBytes` (the oldest go first) |
| `Blend.hpp` | The 12 blend modes (normal, multiply, screen, overlay, soft light, darken, lighten, difference, hue, saturation, colour, luminosity), on the CPU and as GLSL, tested against each other |
| `Composite.hpp` | `Compositor` (a document flattened, one tile at a time at any mip level) and `Sampler` (pixels of a layer or of the flattened document) |
| `Adjustment.hpp` | The 8 adjustments: levels, curves, hue/saturation, colour balance, brightness/contrast, invert, threshold and gradient map |
| `Selection.hpp` | Shapes (rectangle, ellipse, lasso, polygon) with soft edges, combined by replace, add, subtract or intersect; the magic wand, colour range, invert, and the selection's bounds to the pixel |
| `Brush.hpp` | `BrushTip`, presets, and `Stroke`: stamps painted, erased or cloned along a path, with pressure, as one step |
| `Filter.hpp` | Gaussian blur, motion blur, sharpen, unsharp mask and add noise, and the selection's feather, tile by tile with a halo, for a preview of the visible tiles or the whole layer |
| `Gradient.hpp` | Linear, radial, angle, reflected and diamond gradients, and a fill by colour (the paint bucket) |
| `Color.hpp` | `ColorProfile` (sRGB, Display P3, a wide RGB, grey, or parsed from bytes), `ColorTransform`, soft proofing, and the 3D table the canvas converts to the display through |
| `Codec.hpp` | PNG and JPEG read into tiled images (16-bit PNG to `rgba16f`) with their embedded profile, and written |
| `Project.hpp` | The project file: a header, a JSON manifest, then each tile deflated; opened by reading the manifest only, each tile inflated on first use |
| `Residency.hpp` | `TileResidency`: which tiles are on the GPU, within `Limits::gpuBytes`, the least recently used evicted first |
| `Canvas.hpp` | `CanvasModel` and a `GpuSurface` that composites a document in passes, at the mip level of the zoom, for a screen's `surface` node |
| `Limits.hpp` | Every bound in one place: the rows of SECURITY.md's Limits |

## Invariants

- **Tiles are shared until written.** Copying an image, duplicating a layer or keeping a history step copies
  pointers; `writableTile` copies a shared tile first. So a step costs the tiles it changed, whatever the image's size.
- **Memory follows what is used, not the image.** Tiles not yet read stay in their file; loaded tiles and mips are
  evicted within `cacheBytes`, GPU tiles within `gpuBytes`, history within `undoBytes`.
- **The CPU is the reference.** The GPU compositor's blend modes and adjustments are tested pixel against pixel with
  the CPU's.
- **Input is bounded before it is read.** Image files, projects and ICC profiles are refused beyond `Limits` from
  their headers, before decoding; a damaged tile of a project reads as transparent. Projects and profiles have fuzz
  targets (`tests/fuzz/tempera_project.cpp`, `tests/fuzz/icc.cpp`).
- **Errors are `DocumentError`**, naming the bound or the fault.

## Threading and ownership

- **One thread at a time.** A `Document`, its images and its history are used by one thread at a time (the app's);
  nothing in the module locks. The canvas renders on the thread that owns the GL context, which is that thread.
- **A `Document` owns its images;** tiles are `std::shared_ptr<const Tile>`, shared by the images, mips and history
  steps that point at them, and freed with the last of them. A `TileSource` is owned by its image and reads its tiles
  on first use (`ProjectTileSource` keeps the project file's bytes alive by a shared pointer).
- **A `Stroke` is one step:** it begins on construction, commits on `finish`, and cancels when destroyed unfinished.
- **The host owns the `CanvasModel`; the canvas holds it weakly**, so a closed document leaves an empty canvas, and
  keeps its GPU tiles in a `TileResidency`, freed with the surface.

## Extension points

- **A blend mode or an adjustment:** the CPU function (`compose/Blend.cpp`, `compose/Adjustment.cpp`) and its GLSL
  twin (`gpu/CanvasShaders.cpp`), then a case in the GPU test that compares the two.
- **A filter:** a kernel in `filters/Kernels.cpp` that works on a tile and its halo, and its kind in `Filter.hpp`.
- **A storage for tiles:** a `TileSource` (`Image.hpp`) that loads a tile by key, or says it provides a mip level.
- **A bound:** a member of `Limits`, checked where the input is read, and its row in SECURITY.md.

## Sub-folders (`src/`)

| Folder | Holds |
|---|---|
| `tiles/` | Tiles, images and their mips (`Image.cpp`), and GPU residency (`Residency.cpp`) |
| `document/` | The document's layers, masks and selection (`Document.cpp`) and the undo history (`History.cpp`) |
| `compose/` | Blend modes, adjustments, and the CPU compositor and sampler (`Composite.cpp`) |
| `paint/` | Brushes, gradients and the bucket, selections, and the shape rasterizer they share (`Rasterize.cpp`) |
| `filters/` | Filter runs over tiles with a halo (`Filter.cpp`) and their kernels (`Kernels.cpp`) |
| `io/` | PNG and JPEG (`Codec.cpp`, over stb in `Stb.cpp`), colour profiles (`Color.cpp`) and the project file (`Project.cpp`, `ProjectJson.cpp`) |
| `gpu/` | The canvas surface (`Canvas.cpp`) and its shaders (`CanvasShaders.cpp`) |

## Tests

`tests/unit/imaging_*.cpp` (tiles, undo, blend on the CPU and the GPU, selections, colour, project, codec, brushes,
filters) and `tests/perf/tempera.cpp`: a 20,000 by 20,000 image opened, panned and painted, with budgets when
`AFFRESCOUI_PERF_BUDGETS` is set.
