# adapters: other formats and other apps

Two kinds of adapter live here.

**Other formats** (`include/affresco/format/Adapters.hpp`): pure functions that turn an A2UI 0.8 message stream or an
Adaptive Card 1.6 into a screen document, its warnings, and A2UI's later updates as revisions. An element the screen
has no equivalent for becomes a placeholder and a warning; malformed input is an error naming its place.

| File | Holds |
|---|---|
| `Adapters.cpp` | `detect` and `convertForeign` |
| `Layout.*` | What both converters share: the screen they write, its limits, ids, safe text and URLs, placeholders |
| `A2ui.*`, `A2uiComponents.cpp` | A2UI's messages and data model, and its components |
| `AdaptiveCards.*`, `AdaptiveCardActions.cpp` | The card's elements, and its inputs and actions |

**Adapters between apps** (`AdapterMapping.*`): a feature's needs bound to a host, directly or through a declarative
mapping (see `composition/`).

Inputs are untrusted: at most 1 MiB, 32 levels and 2,000 elements. Images are `https://` URLs without a user name, or
relative image files that can't leave the screen's folder; text with `{{` is kept as a literal, so a foreign
document can't run an expression.

Tests: `tests/unit/adapters.cpp`. Fuzz target: `adapters`.
