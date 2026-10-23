# composition: features merged into the screen

A document with features keeps each one whole in its record (`/features/<name>`). The runtime, the rules and the
checker work on the composed screen, where every declaration has its full name, the views are children of their
points and imports are resolved.

| File | Holds |
|---|---|
| `Composition.*` | The enabled features merged into the screen, and what kept a feature out |
| `ComposedVersions.*` | The document's version control as the runtime sees it, with the features merged in |
| `ContributionPoints.*` | The `points` section, typed exports, a feature's needs and an app's offers |
| `CompositionProof.*` | The proof that a feature from another app fits a host: each check with its reasons |
| `Contributions.*` | Commands, keybindings, menus and the `context` variable (RFC 0001) |

A feature that doesn't fit is left out, with a problem naming its place, and the rest of the screen is built.

Tests: `tests/unit/features.cpp`, `contributions.cpp`. Fuzz target: `composition`.
