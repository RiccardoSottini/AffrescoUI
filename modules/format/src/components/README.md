# components: the lookup of the components and tokens a document doesn't define

The format holds no component and no theme of its own. When a document's `"use"` names a component its own
`components` doesn't define, or its colours name a token its `tokens` doesn't, the format asks the installed
`ComponentLibrary` (`affresco/core/ComponentLibrary.hpp`): in a stock program, the kits and themes of
`modules/themes`, which `affresco::Screen` and the other entry points install. `Library.hpp` is that lookup, for the
builder (`nodes/Builder.cpp`), the checker (`checker/Checker.cpp`, `checker/CheckerNodes.cpp`), the design tokens
(`sections/DesignTokens.cpp`), the reference (`reference/ReferenceTables.cpp`, the `kitTokens` table) and the
control scan (`rules/RulesControl.cpp`, through each component's `runsActions`).

A document's own component always wins over the library's of its name. Without a library installed, a document has
only its own components and tokens.

Tests: `tests/unit/themes.cpp`; the kits' own are listed in `modules/themes/README.md`.
