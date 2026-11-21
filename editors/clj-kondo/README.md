# A clj-kondo configuration for AffrescoUI Lisp

AffrescoUI Lisp ([LISP.md](../../LISP.md)) is a sandboxed subset of Clojure, so a Clojure linter reads an app's `.lisp`
files once it knows the language's definition forms and the words a host gives. This folder is that knowledge for
[clj-kondo](https://github.com/clj-kondo/clj-kondo): it reports the mistakes a Clojure linter finds in a function's
body (a wrong number of arguments, an unused binding, a misplaced docstring, a word defined nowhere) and stays quiet
about what is correct AffrescoUI Lisp.

| File | What it holds |
|---|---|
| `config.edn` | The configuration: the hooks for the definition forms, the host's words, the app's paths |
| `affrescoui/hooks.clj` | How each definition form reads for the linter (clj-kondo's `:macroexpand` hooks) |
| `examples.sh` | Lints the example apps, each app's code files in one run (see "Linting an example app") |
| `check.sh` | Checks the configuration: the samples give exactly their planted mistakes, two example apps none of the kinds it silences |
| `samples/` | Small programs written for the check, using every definition form: `notes.screen.lisp` with no `ns`, which includes `tally.lisp`, labelled `(ns app.tally)`; and `board/`, an app of three files whose entry requires the others with `ns` |

## Using it

clj-kondo picks a file's language from its extension, and `.lisp` isn't one of Clojure's, so pass `--lang clj`. It
also skips `.lisp` files in a folder it is given, so name the files:

```sh
clj-kondo --lang clj --config-dir editors/clj-kondo --lint my-app/lisp/*.lisp
```

Or copy this folder to `.clj-kondo/` at the top of the app (with `affrescoui/hooks.clj` beside `config.edn`), and the
linter and your editor's Clojure plugin find it on their own. clj-kondo writes a `.cache/` folder there, which
`.gitignore` leaves out. The VS Code extension ([editors/vscode](../vscode/README.md)) opens AffrescoUI Lisp files as
Clojure, so a Clojure plugin's linting reaches them.

## Linting an example app

```sh
editors/clj-kondo/examples.sh flow            # each finding as file:line: linter message
editors/clj-kondo/examples.sh --summary       # one line per app, for every app and shared
```

An app is a folder of `examples/`, and `shared` is `examples/shared/`; with none named, every one. An app's code files
are its `.lisp` files that start with a comment or a form, outside `locales/` and `seed/`: the files that start with
`{` are data. All of an app's code files are linted in one run. The script exits with 0 when there is no finding, 1
when there are, and 2 when clj-kondo isn't installed.

It leaves out a call of a name that another file of the app, or a file of `examples/shared/`, defines, and counts it
apart (`--summary`, "calls of names defined in other files"): clj-kondo resolves a plain name in its own file's
namespace only, where the language has one space of names for every file it loads (see "What a linter can't know
here").

## What it knows

- **The definition forms.** `defview` and `defcommand` read as functions: their parameters, their docstring and the code
  their templates hold (each `~form` and `~@form`) are linted as a `defn`'s, and the rest of a template, which is data,
  is not. `defscreen`, `defstate`, `defcomputed`, `defcustom`, `defstyle`, `defmenu`, `deftokens`, `defpart`,
  `defdefaults`, `defcomponent` and `defhook` read as definitions of their name whose body isn't code. `defkey`,
  `add-hook`, `remove-hook`, `advice` and `include` are known calls.
- **Paths of locals.** A dotted word whose first part is a local reads that local: `(fn [task] task.done)` uses `task`,
  in a `defn`, a `fn`, a `let` and the other binding forms as in a view, and `#(... %.title)` takes one argument.
- **`ns`.** A file may start with `(ns flow.board)`, a label, and an entry with
  `(ns flow.index (:require flow.state flow.board ["locales/en.lisp"]))`. The label names a file of the app's folder,
  not a Clojure source path, so its name isn't checked against the file's. A required name with no alias is a file
  the loader reads, which clj-kondo takes for a namespace loaded for its side effects, not an unused one; a string is
  a file's path. An alias (`[flow.cards :as cards]`) works as Clojure's: `cards/card-count` is checked against the
  file of that label when both are linted together.
- **Views.** Inside a view, `~(repeat [card cards :index i] ...)` binds `card` and `i`, `~(handler move c i)` uses
  `move`, `c` and `i`, and `~card.title` uses the local `card`.
- **The host's words.** The steps (`set`, `push`, `toggle`, `run`, `invoke`...), the expression functions (`trim`,
  `daysBetween`, `t`...), the app's variables (`state`, `settings`, `theme`...), and the language's own words that
  Clojure lacks (`truthy?`, `or-else`, `join`...). `set`, `find`, `repeat`, `empty`, `time` and `type` are the host's
  here, not Clojure's: inside a call of one, its arguments aren't checked against Clojure's.
- **Paths.** A dotted word nothing defines is a path of the app (`state.count`, `computed.left`, `event.key`, `item.name`),
  never an unresolved symbol. The fixed aliases `str/` and `set/` need no `require`.
- **Computed values.** A `defcomputed` whose form is a list is linted as that code; one whose value is a text or data
  isn't.
- **Both kinds of file.** A file with no `ns` and a file with `(ns app.name)` read the same.

## What a linter can't know here

- **Whether a path exists.** `state.cuont` is a dotted word like any other to the linter. `affrescoui check` knows the
  app's state, computed values and data, and says so with the line and the column.
- **The app's other files, by their plain names.** Every file the language loads shares one space of names, with an
  `ns` or without, and a file reaches another's function by its plain name. clj-kondo resolves a plain name in its
  file's own namespace, so such a call is an unresolved symbol to it, even with the files linted together.
  `examples.sh` and `check.sh` leave those out by the names the app's files define; `affrescoui check` reads the
  whole app from its entry. A call through an alias (`cards/card-count`) is resolved and checked.
- **An id written on its own.** `#rooms`, an id in the Lisp notation, is a tagged literal to Clojure's readers. Write
  the id after the type, `column#rooms`, or as `:id "rooms"` in files you lint.
- **A macro an app defines.** A call of an app's own `defmacro` is read as a call: the names its arguments define are
  known to the scripts (each word after a form whose name starts with `def`), not to the linter.
- **Arity across a step's callbacks.** A function a handler or a step names (`:onClick save`, `:then done`) gets its
  parameters by name when it runs; the linter doesn't check those.
- **Code inside a definition that isn't a view or a command.** A `defcomputed`'s form, a `defcustom`'s keys and a
  `defscreen`'s template aren't linted; `affrescoui check` builds and checks them.

## Checking the configuration

```sh
editors/clj-kondo/check.sh
```

It lints `samples/` (`notes.screen.lisp` with `tally.lisp`, then `board/`, each in one run) and
`examples/streak/lisp` and `examples/flow/lisp`, prints what it ran, and exits with 1 on a difference: the samples must
give exactly the six mistakes planted in them (marked `planted`), and the apps none of the findings the configuration
is meant to silence. In both, a call of a name another of the app's files defines is left out, as `examples.sh` does.
The samples' locals read only through a path (`column.done`) must not be reported. It exits with 2 when clj-kondo
isn't installed. Nothing in CI runs it: clj-kondo isn't a dependency of the project.
