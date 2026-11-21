# The language's reference

The language is a documented subset of Clojure (RFC 0006, `rfcs/0006-clojure-dialect.md`): what it has means what it
means in Clojure, and what it leaves out says what to write instead. The module's own page (`../README.md`) describes the
core, the library and how it runs; these pages describe the language area by area, each written by the package that adds
that area.

| Page | What it covers | Where the area lives |
|---|---|---|
| `forms.md` | The everyday forms: macros such as `when-let`, `if-let`, `->`, `->>`, `cond->` | `src/PreludeForms.cpp`, `words/Forms.hpp` |
| `functions.md` | The further functions on numbers, collections and maps | `src/PreludeFunctions.cpp`, `words/Functions.hpp` |
| `strings.md` | `clojure.string`, written `str/` | `src/CoreStrings.cpp`, `words/Text.hpp` |
| `reader.md` | The reader's forms: `#(...)`, `#?(...)`, `#_`, `^metadata`, characters, and Clojure's ways to write numbers | `src/ReaderForms.cpp`, `words/Reader.hpp` |
| `sets.md` | Sets, `#{...}`, the functions on them, and `clojure.set`, written `set/` | `src/CoreSets.cpp`, `words/Sets.hpp` |
| `namespaces.md` | Qualified names, aliases, `ns` | `src/Namespaces.cpp`, `words/Namespaces.hpp` |
| `bindings.md` | Which forms a template's `~form` lowers to a binding the expression machine runs | `src/CompilerLowering.cpp` |
| `style.md` | How a program is written in the dialect, as every example's is: what a change keeps, which form to choose, the traps, `ns` and keywords, the places to keep an old spelling, and how to check an app (the idiom check's lock, the tree's comparison) | `tools/notation/lisp_idioms.py`, `lisp_tree.py`, `idioms/` |
| `differences.md` | What differs from Clojure on purpose, and what to write instead | RFC 0006, "Intended differences" |

A page appears when its package does. Until then, the module's page and RFC 0006 are the reference.
