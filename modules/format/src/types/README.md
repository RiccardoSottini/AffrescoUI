# types: the type registry and the node types

`TypeRegistry` holds the node types a runtime can build: each type's factory, its properties (name, kind of value,
whether states and `transition` may set them), its apply function, and a widget's value hooks, events and part in
drag and drop. Each runtime has its own registry, since applications add to it (`Runtime::registerType`).

| File | Holds |
|---|---|
| `TypeRegistry.*` | The registry: adding, finding and creating types |
| `Types.cpp` | The core types (panel, label, textarea, stack, row, column, grid, outlet) and `registerBuiltInTypes()`, which registers every built-in type in catalog order |
| `EditorType.cpp`, `SyntaxHighlighter.cpp` | `editor`, and the highlighter its `grammar` installs (declared in `include/affresco/format/SyntaxHighlighter.hpp`) |
| `DiffType.cpp`, `ImageType.cpp`, `SurfaceType.cpp`, `SplitType.cpp`, `TabsType.cpp`, `ListType.cpp` | `diff`, `image`, `surface`, `split`, `tabs`, and `list`, `tree` and `table` |

Types from the other folders (`controls`, `creative`) register through `registerBuiltInTypes()` too.

## Adding a node type

1. A file with a `register<Name>Type(TypeRegistry&)` that adds the type, its properties and its apply function.
2. A call in `registerBuiltInTypes()`, at the place the catalog should list it.
3. Its keys in `reference/ReferenceNodeTypes.cpp`, then `affrescoui schema` for the schema and SCREEN_FORMAT.md's
   tables. `tests/unit/reference.cpp` fails while the registry and the reference differ.

An apply function gets the values the runtime evaluated, by name, and the place to name in an error. A widget's
events reach the runtime only through the sink its type's `listen` is given, which runs one of the node's handlers.

Tests: `tests/unit/registries.cpp`, `types.cpp`, `editor*.cpp`, `syntax.cpp`, `diff.cpp`, `image_nodes.cpp`,
`lists.cpp`, `tables.cpp`, `split_tabs.cpp`.
