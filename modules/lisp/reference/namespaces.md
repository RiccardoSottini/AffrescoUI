# Namespaces: qualified names, aliases and `ns`

A word with a `/` inside it names a namespace's function, and a program file's first form may be Clojure's
`(ns name (:require ...))`, which labels the file, gives it aliases of its own and names the files it needs. There is
one flat space of names: a namespace is a file's label, and `alias/name` is checked against it.

Where the code is: `modules/lisp/src/Namespaces.cpp` (the form, the aliases, the checks), `src/Document.cpp` (the form
taken out of a file's text), `modules/screen/src/Screen.cpp` (the files read); the tests in
`tests/unit/lisp_clojure_namespaces.cpp`, and a small app of three files in `tests/unit/fixtures/namespaces/`.

## Qualified names

`clojure.core/inc` is `inc`; `clojure.string/join`, written `str/join` or `string/join`, is the global of that name;
`clojure.set`'s are written `set/`. A qualified word is never a local, and a qualified form (`clojure.core/when`) is the
form. An unknown prefix is an error that lists the namespaces. `/` alone is division, and a word starting or ending with
`/` is a word of its own. A host adds its own namespaces with `Interpreter::addNamespace` and `addAlias`.

## The `ns` form

```clojure
(ns flow.parts.board
  "The board: its columns and their cards."          ; a docstring, optional
  (:require [clojure.string :as s]                   ; the language's: no file
            [clojure.set :refer [union]]
            [flow.parts.card :as card :refer [card-row]]
            ["../shared/charts.lisp" :as charts]      ; a file by its path
            flow.columns))                            ; a name alone
```

It comes once, as the file's first form (comments may come before it). Its name is words between dots. Each spec of
`(:require ...)` is a namespace's name, a file's path as a string, or a vector of either followed by `:as alias`,
`:refer [word...]`, or both.

- `[clojure.string :as s]` makes `s/trim` the same as `clojure.string/trim`, in that file only: an alias of one file is
  unknown in another.
- `:refer [join]` of `clojure.core`, `clojure.string` or `clojure.set` is taken only when the plain word already is the
  same function; otherwise the error says to write it qualified (`"trim" isn't a plain name here: write str/trim`).
- Any other namespace is a file of the program, added to the file's `include` after its own include forms, in order,
  each once: it loads as an include does.

## Which file a name requires

A string is a path relative to the requiring file, as an include's is. A dotted name is a path from the app's folder:
with two segments or more its first, the app's, is dropped and the rest are folders and the file; a `-` stays.

| Required | File |
|---|---|
| `flow.parts.board` | `parts/board.lisp` |
| `flow.board` | `board.lisp` |
| `board` | `board.lisp` |

The requiring file's own name says how deep it is: from `flow.parts.card`, `flow.columns` is `../columns.lisp`. A file
required by a dotted name must start with `(ns <that name>)`, else: `flow.parts.board is required, and
parts/board.lisp doesn't declare it`.

Two files that require each other are both read once. A file that several files require is read for each of them, as
an include is, and its definitions come once; at most 1,024 files are read for one screen. The files read are the
loader's, with its checks: in an app, a path that leaves the app's folder and the shared folder beside it
(`["../../x.lisp"]`, or a dotted name in a file that says it is deeper than it is) is refused.

## Where an error is

An error in a required or included file names that file, as the program calls it (`parts/board.lisp`), and the line
and column in it: whether the file doesn't read (`parts/board.lisp: line 7, column 28: expected a value, found ')'`),
a form in it can't be compiled, or a function in it fails as it runs, at the build or later in an event. So do the
entry file's own definitions, and through an include inside an include. The message names names and kinds, never
values. A definition a revision changed has no line in a file yet: its error counts lines from the definition's own
start, as before.

## One flat space of names

Names stay global: `board/render` is the global `render`, and a file that requires nothing still reaches it by its plain
name. The alias is a label checked against the file: `board/render` and `:refer [render]` are errors unless the file
`board` names defines `render`, as `board/render: "render" isn't defined in parts/board.lisp`. Two files can't define the
same name, as without `ns`.

## At the command line

`affrescoui repl` takes an `ns` form for the forms typed after it: `(ns a (:require [clojure.string :as s]))`, then
`(s/upper-case "a")` gives `"A"`. It loads no file: a spec that names one is refused with `a file is required by the app
that loads it`.

## What is refused

- Any clause but `(:require ...)`: `ns takes (:require ...) only`. `:refer :all`: `name what you refer`. More than 256
  specs in one form.
- `ns` anywhere but a file's first form, a second one, or as a definition's text: `ns is read with the file an app loads
  from its folder: no entry of a program, so a revision can't add one`. A definition named `ns`.
- A revision that adds, changes or removes a file's `ns` form: it decides which files the loader reads, so it comes only
  with the file. The form is kept with the file's entries under the reserved key `ns`, which no definition can take.
- `(ns clojure.string)` and the other names of the language's namespaces, for a file.
- `in-ns`, `require` as a function, `refer`, `alias` and `*ns*`: a file's namespace is fixed by its first form.
