# properties: what node types share

`Properties` (`Properties.*`) applies a compiled node's evaluated values to its panel, by slot: the behaviour keys,
the look, the layout and the properties each type registered. The rest of the folder is the property sets types
share and the readers of their values.

| File | Holds |
|---|---|
| `Properties.*` | Applying a node's values to its panel |
| `PropertyValues.*` | Readers for values that need nothing but the value: lengths, alignments, cursors, placements, radii |
| `CommonProperties.cpp` | The properties every panel has, its shape, and overlays |
| `LayoutProperties.*` | The flexible and grid layout keys, and the sets rows, columns, stacks and grids take |
| `AccessibilityProperties.cpp` | The states a node gives assistive technologies besides its role and name |
| `TransformProperties.cpp` | `rotate`, `scale`, `skew`, `translate`, `transformOrigin` |
| `TextFieldProperties.cpp` | `secret`, `maxLength`, `accept` and the other form keys of text inputs |
| `IconProperty.cpp`, `RunsProperty.cpp` | A label's `icon`, and its rich text `runs` |

A reader takes the value and its place, and throws `DocumentError` naming the place for a value it can't read.

Tests: `tests/unit/layout*.cpp`, `accessibility*.cpp`, `transforms.cpp`, `richtext.cpp`, `form_inputs.cpp`,
`shapes.cpp`, `updates.cpp`.
