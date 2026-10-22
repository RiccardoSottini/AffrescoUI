# creative: the creative types

Node types for creative tools: `colorPicker`, `gradient` and `curves`, whose values are colours, gradients and curve
points (`CreativeValues.*`, shared with the expression functions that read them); `canvas`, `graph` and `timeline`,
views of items, nodes and links, or tracks and keyframes from the state or a host's model, with their cameras,
selections and edits; and `path`, SVG path data filled and stroked.

| File | Holds |
|---|---|
| `CreativeValues.*` | Colours as `#rrggbbaa`, gradient stops and curve points, read from JSON and written back |
| `ColorPickerType.cpp`, `GradientType.cpp`, `CurvesType.cpp` | The editors of those values |
| `CanvasType.cpp`, `GraphType.cpp`, `TimelineType.cpp` | The views, and the events their edits run |
| `PathType.cpp` | `path` |

An edit made in a view runs one of the node's handlers with the edit, through the sink its type's `listen` is
given.

Adding a type follows `types/README.md`, "Adding a node type".

Tests: `tests/unit/creative_controls.cpp`, `color_spaces.cpp`, `canvas.cpp`, `graph.cpp`, `timeline.cpp`,
`paths.cpp`.
