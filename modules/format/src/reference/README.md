# reference: what the format is, for people and agents

The reference describes every key a node, a section or a step can have (`KeyReference`): its kind of value and one
line of documentation. From it and the registries come the format's JSON Schema (`schema()`), SCREEN_FORMAT.md's
tables (`referenceTables()`) and the catalog agents read (`catalog()`).

| File | Holds |
|---|---|
| `Reference.*` | `KeyReference`, and the schema built from the reference |
| `ReferenceNodes.cpp` | The keys every node can have |
| `ReferenceNodeTypes.cpp` | The keys each node type adds |
| `ReferenceNodeEvents.cpp` | The events, and the keys of forms and carousels |
| `ReferenceSections.cpp` | The sections, the steps and the shared definitions |
| `ReferenceTables.cpp` | SCREEN_FORMAT.md's tables, written between their markers |
| `ReferenceImages.cpp` | The tables about images: the `image` step's operations, and the names their values take |
| `Catalog.*` | The catalog: types, steps, sections, functions and models |

`tests/unit/reference.cpp` checks that every key the registries have is in the reference and the reference has no
other, so a new key needs a line here. `affrescoui schema --doc SCREEN_FORMAT.md` writes the tables again, and
`--check` says whether they are out of date.

Tests: `tests/unit/reference.cpp`, `catalog.cpp`, `registries.cpp`.
