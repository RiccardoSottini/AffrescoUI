# packages: shared versions of an app

A package is a version of an app in one file, ending in `.affresco.json`
([schema/affrescoui-package.schema.json](../../../../schema/affrescoui-package.schema.json)): the version's source
document, the release it was made from (its app, name, release, notation and tree) with the JSON Patch that turns the
version back into it, and the state keys and assets the version adds. A package of kind `changes` is for a person who
has the app; one of kind `app` also holds the app's files. The format is declared in
`include/affresco/format/Package.hpp`.

| File | Holds |
|---|---|
| `Package.cpp` | The format and its checks: a package read (`parsePackage`, `readPackage`) and written (`packageJson`), the tree of a document (`documentTree`), the base it was made from (`baseOf`), and the summary the person reviews (`packageSummary`) |
| `PackageExport.cpp` | Writing a package (`buildPackage`, declared in `include/affresco/format/PackageFiles.hpp`): the files an app's manifest reaches (`appFiles`), and the scan for the person's paths and keys (`scrubProblems`). `Screen::exportPackage` gives it the parts `Repository::versionParts()` finds in the history |
| `PackageInstall.cpp` | What an install proposes |

Limits: a package at most 16 MiB, 1,024 files of at most 4 MiB each, a name 80 characters, a description 2,000; the
person's trusted publishers at most 64 KiB and 256 publishers. A document's tree is `sha256:` and the hex SHA-256 of
its canonical JSON (RFC 8785), so the same document has the same tree whatever the order of its keys.

A package is untrusted input: it is bounded and checked whole before anything reads it, and installing one is only
ever a proposal, which the person reviews and accepts, limited by the capabilities they grant.

Tests: `tests/unit/packages_format.cpp`, `tests/unit/packages.cpp` (reading), `tests/unit/packages_export.cpp`
(writing). Fuzzed: `package`.
