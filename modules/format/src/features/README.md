# features: features from catalogues (RFC 0004)

A feature is a part of a screen published on its own: a manifest with what it adds, what it exports and what it
imports. Publishers sign their catalogues and packages; a host trusts some publishers and installs or upgrades a
feature as a revision, which version control reviews like any other.

| File | Holds |
|---|---|
| `FeatureManifest.*` | Releases and their ranges, a manifest's shape, and its `features`, `exports` and `imports` |
| `FeaturePrograms.*` | A feature's program: its names, its build alone, what its code reaches, its hooks and advice |
| `FeatureCatalog.*` | Catalogue v0: a signed index of features and packages, and a release's file fetched from its folder |
| `FeatureSignatures.*` | base64url, canonical JSON (RFC 8785), Ed25519 signatures and SHA-512 digests |
| `Features.cpp` | `Features` (`include/affresco/format/Features.hpp`): trusted publishers, catalogues, revisions |

Everything read here is untrusted until its signature is checked against a trusted publisher, and bounded before
that: a manifest of at most 1 MiB, an index of 8 MiB and 10,000 features. A local catalogue's paths stay inside its
folder. An index may also list `packages`: versions of an app (VERSION_CONTROL.md, "Sharing a version: packages"), each
release pinned by digest and size, which the `packages` service installs as a file from disk (`catalogue/README.md`). A feature can't bring a capability section, and one that uses a control step is installed by the host, not by a
revision.

Tests: `tests/unit/features.cpp`, `features_programs.cpp`, `signatures.cpp`, `catalogue.cpp`. Fuzz targets: `feature_manifest`, `catalog_index`, `signature`.
