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
| `check.sh` | Checks the configuration: the samples give exactly their planted mistakes, two example apps none of the kinds it silences |
| `samples/` | Two small programs written for the check, one with no `ns` and one with `(ns app.tally)`, using every definition form; without the `ns` line they build |

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

## What it knows

- **The definition forms.** `defview` and `defcommand` read as functions: their parameters, their docstring and the code
  their templates hold (each `~form` and `~@form`) are linted as a `defn`'s, and the rest of a template, which is data,
  is not. `defscreen`, `defstate`, `defcomputed`, `defcustom`, `defstyle`, `defmenu`, `deftokens`, `defpart`,
  `defdefaults`, `defcomponent` and `defhook` read as definitions of their name whose body isn't code. `defkey`,
  `add-hook`, `remove-hook`, `advice` and `include` are known calls.
- **Views.** Inside a view, `~(repeat [card cards :index i] ...)` binds `card` and `i`, `~(handler move c i)` uses
  `move`, `c` and `i`, and `~card.title` uses the local `card`.
- **The host's words.** The steps (`set`, `push`, `toggle`, `run`, `invoke`...), the expression functions (`trim`,
  `daysBetween`, `t`...), the app's variables (`state`, `settings`, `theme`...), and the language's own words that
  Clojure lacks (`truthy?`, `or-else`, `join`...). `set`, `find`, `repeat`, `empty`, `time` and `type` are the host's
  here, not Clojure's: inside a call of one, its arguments aren't checked against Clojure's.
- **Paths.** A dotted word nothing defines is a path of the app (`state.count`, `computed.left`, `event.key`, `item.name`),
  never an unresolved symbol. The fixed aliases `str/` and `set/` need no `require`.
- **Both kinds of file.** A file with no `ns` and a file with `(ns app.name)` read the same, ready for when the
  language takes an `ns` (it doesn't yet: `affrescoui check` stops at one).

## What a linter can't know here

- **Whether a path exists.** `state.cuont` is a dotted word like any other to the linter. `affrescoui check` knows the
  app's state, computed values and data, and says so with the line and the column.
- **The app's other files.** The linter reads one file at a time, so a function defined in another file of the app is
  an unresolved symbol where it is called. `affrescoui check` reads the whole app from its entry.
- **An id written on its own.** `#rooms`, an id in the Lisp notation, is a tagged literal to Clojure's readers. Write
  the id after the type, `column#rooms`, or as `:id "rooms"` in files you lint.
- **A local read only through a path in a function.** In a `defn`, `(fn [task] task.done)` reads `task`, but the linter
  sees the dotted word only, and reports `task` as an unused binding (views are read correctly). Write `(:done task)`
  where you want the linter quiet.
- **Arity across a step's callbacks.** A function a handler or a step names (`:onClick save`, `:then done`) gets its
  parameters by name when it runs; the linter doesn't check those.
- **Code inside a definition that isn't a view or a command.** A `defcomputed`'s form, a `defcustom`'s keys and a
  `defscreen`'s template aren't linted; `affrescoui check` builds and checks them.

## Checking the configuration

```sh
editors/clj-kondo/check.sh
```

It lints `samples/` and `examples/streak/lisp` and `examples/flow/lisp`, prints what it ran, and exits with 1 on a
difference: the samples must give exactly the four mistakes planted in them (marked `planted`), and the apps none of
the findings the configuration is meant to silence. It exits with 2 when clj-kondo isn't installed. Nothing in CI
runs it: clj-kondo isn't a dependency of the project.
