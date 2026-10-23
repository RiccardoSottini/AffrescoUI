# sections: a document's top-level sections

`SectionRegistry` (`SectionRegistry.hpp`) holds the top-level sections a screen document can have: how each is
checked, whether it is required, and whether it is a capability. The built-in sections never change, so one
read-only registry is shared by every runtime.

| File | Holds |
|---|---|
| `Sections.cpp` | The registry, and the sections that aren't capabilities |
| `CapabilitySections.cpp` | `channels`, `services`, `persist` and the other sections that grant something |
| `DesignTokens.*` | `tokens`: design values named once and resolved for every theme, once per document change |

A capability section grants something (programs to talk to, files to write), so a revision can never change it
(module README, "Invariants"). A section's check throws `DocumentError` naming the place.

Tests: `tests/unit/registries.cpp`, `rules_sections.cpp`, `tokens.cpp`.
