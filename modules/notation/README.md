# notation: the notations a document is written in

`notation` turns text into the JSON tree of a document and back. A document can be written in **JSON**, as it always
was, or in the **Lisp notation**: S-expressions that are only another spelling of the very same tree. Nothing is ever
evaluated: the Lisp notation is data, never code (no functions, no macros, no `eval`). The module knows text and
JSON only; it doesn't know screens, nodes' meanings, revisions or files' places. Target: `affrescoui::notation` (uses
`core`, for its JSON type and limits, and nlohmann::json).

```lisp
; Flow's insights panel
(panel #insights :color transparent
  (node #burndownTitle :style caption :anchors {:left 0 :top 0} :text "{{t('burndown.title')}}")
  (column #list :gap 4))
```

is

```json
{ "type": "panel", "id": "insights", "color": "transparent", "children": [
	{ "id": "burndownTitle", "style": "caption", "anchors": { "left": 0, "top": 0 }, "text": "{{t('burndown.title')}}" },
	{ "type": "column", "id": "list", "gap": 4 } ] }
```

The spelling rules are in SCREEN_FORMAT.md, "The Lisp notation".

## Public header (`include/affresco/notation/`)

| Header | Offers |
|---|---|
| `Notation.hpp` | `Notation` (`Json`, `Lisp`); `notationOf(file)` (`.lisp` is Lisp, anything else JSON), `notationNamed`, `nameOf`; `read(text, notation)`, `write(value, notation)`; `readDocument(text, file)` and `jsonText(text, file)`, the one way documents on disk are read; `NotationError` (a `DocumentError` with `line()`, `column()` and `message()`); `maxLispBytes` |

## Invariants

- **The same tree, exactly.** `read(write(x))` is `x` for every value, key order included, so a document converted to
  the Lisp notation and back is the same document, and a commit made from it has the same hash. Every key keeps its
  place: a node's head is its `type` only when `type` is its first key; otherwise the head is `node` and `:type` is
  written where it is.
- **Canonical.** `write` depends only on the value: the same value always gives the same text, so `write(read(t))` is
  stable, and `affrescoui fmt` has one layout to give a Lisp file. Comments (`;`) are dropped when a file is converted;
  a `note` key keeps its text.
- **JSON is untouched.** `read(text, Json)` is `parseUntrusted(text)`: the same parser, the same errors, the same
  limits. `readDocument` and `jsonText` give JSON files to it as they are.
- **Version control never sees Lisp.** History, commits, patches, blame and permissions are JSON (RFC 6902). A peer's
  text in the Lisp notation becomes JSON before anything else reads it (channel's `convert` message).
- **Bounded.** A Lisp text is at most `maxLispBytes` (64 MiB) and nests at most `maxNesting` (256) levels, counted as
  JSON counts them (a node's children are a list inside it: two levels). The reader refuses a container before it
  would go deeper, so its recursion is bounded whatever the text; `write` refuses a deeper value. Numbers are JSON's,
  at most `maxNumberLength` characters, and one no double holds (`1e400`) is refused. Objects of many keys read in
  time proportional to their size (a hash set past 16 keys), as `parseUntrusted` does. Strings in quotes are read by
  nlohmann::json's own string reader: JSON's escapes, no control characters, valid UTF-8. Any other text is a
  `NotationError`; nothing crashes (fuzzed: `tests/fuzz/notation.cpp`).

## Errors

`NotationError` names the place: `AffrescoUI screen: parts/board.lisp: line 3, column 8: expected ')' to close the
node opened at line 1, column 1` (the file when `readDocument` read it; columns count characters). It is a
`DocumentError`, so whatever already reports a document's errors reports it.

## Files

```
include/affresco/notation/Notation.hpp  the notations, reading and writing
src/
  Notation.cpp      which notation a file is in; reading and writing in either; NotationError
  Lisp.hpp          the Lisp reader's and writer's entry points
  Words.hpp         what both agree on: bare words, reserved words, the head `node`
  LispReader.cpp    text into the tree: a token at a time, one call per container, bounded
  LispWriter.cpp    the tree into canonical text: one line when it fits in 120 columns, else one member per line
CMakeLists.txt      the affrescoui_notation target
```

## Recipes

**Read a document file.** `notation::readDocument(text, file)`: the screen module reads screens, includes and an
app's entry and parts with it, the automation module its scenarios' screens, the command-line tool what it checks.
A reader that takes JSON text (`Screen::fromString`) gets `notation::jsonText(text, file)`.

**Convert a file.** `affrescoui convert board.screen.json` prints it in the Lisp notation; `--to json` and `-o FILE`
choose the other way and a file.

**Let a peer read and write Lisp.** The `convert` message (SCREEN_FORMAT.md, "Peers and the live protocol"):
`{"type": "convert", "notation": "lisp", "value": ...}` answers the value's `text`, and with `text` instead, its
`value`. The MCP server's `get_screen` and `submit_revision` take `notation: "lisp"` through it.
