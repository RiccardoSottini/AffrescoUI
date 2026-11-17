# AffrescoUI Lisp

AffrescoUI apps are written in **AffrescoUI Lisp**, a Lisp with its own runtime.
A program **builds** the screen document the engine draws ([SCREEN_FORMAT.md](SCREEN_FORMAT.md)), and its functions **run**
when a button is pressed, a key typed or a value bound. JSON is the same document written out directly, for data and for
tools. This page is the standard in one place; the full reference is
[SCREEN_FORMAT.md, "The Lisp language"](SCREEN_FORMAT.md#the-lisp-language), and the runtime is
[`modules/lisp`](modules/lisp/README.md).

## An app in one file

```lisp
;;; tasks.lisp --- a to-do list

(defstate tasks [{:id 1 :title "Write the docs" :done false}] "The tasks, in the order they were added.")
(defstate draft "" "What is typed in the field.")

(defcomputed left "How many tasks are still to do."
  (count (remove (fn [task] task.done) state.tasks)))

(defn add-task "Adds the typed task, unless the field is empty." []
  (when (not= (trim state.draft) "")
    (push "state.tasks" {:id (inc (count state.tasks)) :title (trim state.draft) :done false})
    (set "state.draft" "")))

(defn toggle "Ticks a task, or unticks it." [i]
  (set (str "state.tasks." i ".done") (not (:done (get state.tasks i)))))

(defcommand clear-done "Removes the finished tasks." :title ~(t "command.clear") []
  (set "state.tasks" (remove (fn [task] task.done) state.tasks)))

(defkey "mod+shift+k" clear-done)
(defcustom show-count true "Shows how many tasks are left." :type boolean)

(defview task-row "One task: a box to tick, labelled with its title." [task i]
  `(checkbox#task :checked ~task.done :label ~task.title :onClick ~(handler toggle i)))

(defscreen main :window {:title "Tasks"}
  (column#list :gap 4
    (textarea#draft :bind state.draft :placeholder ~(t "draft.hint") :onSubmit add-task)
    ~(repeat [task state.tasks :index i] (task-row task i))
    (label#left :visible ~settings.show-count :text ~(t "left" {:n computed.left}))))
```

`affrescoui run tasks.lisp` prints what it builds: `~task.done` becomes the binding `"{{task.done}}"`, `left` becomes
`"{{length(filter(state.tasks, !(it.done)))}}"`, and the repeat becomes `"repeat": {"for": "task", "in": "state.tasks", "index": "i"}`.

## Definitions

Every top-level form is a definition with a name and a docstring. Their order doesn't matter, and a name used twice is
an error. Each definition is versioned on its own.

| Form | What it defines | What it builds in the document |
|---|---|---|
| `(defscreen main "doc"? :key value ... node)` | The window | `root`, and its options at the top: `:window`, `:fonts`, `:strings`, `:theme`, `:permissions`, `:persist`. Another name builds `screens.name` |
| `(defview name "doc" [params] body)` | A function the build calls | Nothing of its own: the nodes it returns, wherever it is called |
| `(defcomponent name "doc" {:param default} node)` | A component | `components.name`, used with `:use name :with {...}` |
| `(defstate name value "doc" :type schema?)` | A state key | `state.name` (and `stateTypes.name`). A value already in the document wins |
| `(defcomputed name "doc" form)` | A derived value | `computed.name`, as a binding |
| `(defn name "doc" [params] body)` | A function | Nothing: handlers, bindings and other functions call it |
| `(defcommand name "doc" :title ~(t "k") ... [params] body)` | A command: a function plus its palette entry. `:arguments {:card {:type string :description "..."} :to {:enum [todo doing done]}}` types its parameters for the agents that run it, each key one of them | `actions.name`, whose `do` runs the function; `arguments` is the map, else the parameters' names |
| `(defkey "chord" "doc"? what)` | A shortcut | `keys.chord` |
| `(defcustom name value "doc" :type t :title ~(t "k"))` | A setting, also a global | `settings.name` |
| `(defstyle name "doc" node)` | A style | `styles.name` |
| `(defmenu name "doc" items)` | A menu | `menus.name` |
| `(deftokens name "doc" {:token value})` | Design tokens | members of `tokens` |
| `(defpart name "doc" :section value ...)` | Any other section (`watch`, `onLoad`, `channels`...) | each section's members |
| `(defdefaults name "doc" :section {...})` | Defaults an app may replace (shared add-ons) | members that yield to any other definition |
| `(def name "doc" value)` | A constant, worked out at the build | Nothing |
| `(defmacro name "doc" [params] body)` | A macro, expanded at load | Nothing |
| `(defhook name "doc")`, `(add-hook h f)`, `(remove-hook h f)`, `(advice f :before/:after/:around g)` | Extension points | Nothing: they run in the app |
| `(include "file.lisp" ...)` | The app's other files, in the entry file | `include`. Read from disk with the entry, never by a revision |

## Views and bindings

A view's backquoted template is the tree it returns. Inside it, `~form` either works a value out at the build, or, when the
form reads the running app, becomes a **binding** that the engine keeps up to date.

| You write | It builds | Notes |
|---|---|---|
| `(type#id :key value child ...)` | a node `{"type", "id", ..., "children"}` | `(node#id ...)` when there is no type |
| `{:key value}`, `[a b]`, `word`, `"Text"`, `@color.accent` | a map, a list, the string `"word"`, a string, a token | data, as in [the notation](SCREEN_FORMAT.md#the-lisp-notation) |
| `~state.view`, `~computed.card`, `~settings.x` | `"{{state.view}}"` | a dotted word nothing defines is a path of the app |
| `~(= state.view view)` with `view` = `"board"` | `"{{(state.view == 'board')}}"` | build-time values are written in |
| `~(repeat [card list :index i :key form] (card-view card i))` | the node with `repeat {for, in, index, key}` | `card` and `i` are names of the running app |
| `~card.title` inside that view | `"{{card.title}}"` | |
| `~(handler move c i)` | `{"lisp": "move", "with": [...]}` | arguments that read the app become bindings |
| `:onClick save` | runs the function or command `save` | parameters come by name from the variables around it |
| `'menus.card` | `"{{menus.card}}"` | a quoted symbol is a name of the running app |
| `~(t "key" {:n x})` | `"{{t('key', {'n': x})}}"` | text in the current language |
| `~~form` (in a macro's template) | `~form` in what the macro writes | |
| `~(my-fn state.x)` | `"{{lisp('my-fn', state.x)}}"` | slower: prefer a macro or forms the expressions have |

**Forms that run at an expression's speed:**

| Lisp | Expression | Lisp | Expression |
|---|---|---|---|
| `(not x)` `(and a b)` `(or a b)` | `!x` `a && b` `a \|\| b` | `(if c a b)` `(cond ...)` `(when c a)` | `c ? a : b` |
| `(= a b)` `(not= a b)` `(< a b)` | `==` `!=` `<` | `(+ a b)` `(- a)` `(* a b)` `(mod a b)` | `a + b` `-a` `a * b` `a % b` |
| `(count x)` | `length(x)` | `(:k x)` `(get x :k)` `(get-in x [a :b])` | `x.k` `x.k` `x[a].b` |
| `(str "a" x)` | the text `a{{x}}` | `(or-else a b)` | `a ?? b` |
| `(nil? x)` `(some? x)` | `x == null` `x != null` | `(first x)` `(inc x)` `(dec x)` | `x[0]` `x + 1` `x - 1` |
| `(filter (fn [c] ...) xs)` | `filter(xs, ...it...)` | `(remove ...)` `(map ...)` `(find ...)` `(some ...)` `(every? ...)` `(sort-by ...)` `(reduce ...)` | their expression forms, with `it`, `index`, `acc` |
| `(truthy? x)` | `!(!(x))` | `(join ...)` `(vals ...)` `(upper-case ...)` `(range ...)` `(sort ...)` | their expression forms |
| `(-> x f)` `(->> x f)` | the steps nested | `(if-not c a b)` `(when-not c a)` | `!c ? a : b` |
| `(case x 1 a 2 b c)` `(condp = x ...)` | `x == 1 ? a : ...` | `(for [x xs :when p] body)` | `filter`, then `map`, with `it` |

## Running

| Topic | The rule |
|---|---|
| Reading | `state.count`, `computed.h`, `data.songs`, `settings.x`, `event.x` read the running app. `item.name` reads a local's member, and `state.rows.0` is an index |
| Changing | Only through steps, each a function: `(set "state.x" 1)`, `(toggle "state.open")`, `(push "state.list" v)`, `(insert "songs" :value v)`, `(remove-state ...)`, `(update-record ...)`, `(run "action")`, `(invoke "files.read" ...)`, `(undo)` |
| Callbacks | `:then f`, `:onDone [f arg ...]`; the parameters past the given ones come by name (`result`, `error`) |
| Later | `(after 500 f arg)`, `(debounce 300 f arg)`: a function named by a definition, never a closure |
| Truth | In functions only `nil` and `false` are false; in bindings `""`, `0` and `[]` are too. Write `(truthy? x)`, `(some? x)` or `(not (empty? x))`. `affrescoui check --truth` lists the places to look at |
| Text | `(str a b)` joins text; `+` adds numbers only |
| Reserved names | The expressions' functions (`now`, `trim`, `log`, `t`...): no definition may take their names |
| Reading only | A value read for the screen can't run steps, whoever wrote it |
| A new version | A change of the program takes effect at once, whole. The state stays, and scheduled calls find the new definition |

## The language

| Part | What it has |
|---|---|
| Values | JSON's (`nil`, booleans, numbers, strings, vectors `[...]`, maps `{...}`), plus symbols, keywords, lists, functions. Nothing changes a value: `assoc`, `conj` give new ones. Nothing is lazy: a sequence function gives a whole vector |
| Special forms | `quote`, backquote with `~` and `~@`, `if`, `do`, `when`, `cond`, `and`, `or`, `let`, `fn`, `loop`/`recur`, `try`/`catch`/`finally`/`throw` |
| Functions | Lexical closures, proper tail calls, destructuring `[a b & more :as all]` and `{:keys [a b] :or {a 1}}`, several parameter lists, `(defn f ([a] ...) ([a b] ...))` |
| Everyday forms | `->` `->>` `as->` `some->` `cond->` `if-let` `when-let` `if-some` `when-some` `if-not` `when-not` `case` `condp` `for` `doseq` `dotimes` `letfn` `doto` `comment` ([reference/forms.md](modules/lisp/reference/forms.md)) |
| Built-ins | Clojure's names: `map filter reduce find first rest cons conj assoc get get-in merge keys vals count str range apply sort-by partition group-by frequencies update-vals`... ([modules/lisp](modules/lisp/README.md#the-language), [reference/functions.md](modules/lisp/reference/functions.md)) |
| Text | `str/` (or `string/`, `clojure.string/`): `str/join str/split str/trim str/includes? str/starts-with? str/index-of str/replace str/upper-case str/blank?`..., and `format`, `ex-info`, `ex-message`, `ex-data`, `parse-long`, `parse-double` ([reference/strings.md](modules/lisp/reference/strings.md)) |
| Reader forms | `#(+ % 1)`, `#?(:affresco a :default b)`, `#_` to drop a form, `^metadata` read and dropped, `\a` as `"a"`, `12N` `1.5M` `1/2` `0x1F` as the nearest JSON number ([reference/reader.md](modules/lisp/reference/reader.md)) |
| Macros | `defmacro`, unhygienic; `gensym` for a macro's own names |
| Outside world | None of its own: no files, network, clock or randomness. A host grants steps, each judged by who wrote the code that runs |
| Differences from Clojure | On purpose, each with what to write instead: nothing changes a value, nothing is lazy, no host interop, a map's keys are text (`(= :a "a")`), numbers are JSON's, no characters, regular expressions, metadata or vars ([reference/differences.md](modules/lisp/reference/differences.md)) |
| Conformance | Measured against Clojure's cross-dialect test suite: at commit `a9f32fd7`, 2,377 assertions pass, 62 fail, 395 raise an error and 1,161 are out of scope; 217 of its 269 files read whole, and 177 of the 267 functions it tests are defined ([CONFORMANCE.md](modules/lisp/CONFORMANCE.md)) |
| The build | Pure: the same source always builds the same tree |

## Text and languages

| Part | How |
|---|---|
| In views, commands, settings | `~(t "key")`, `~(t "key" {:n 3})` (ICU plurals and selects) |
| Language files | `locales/en.lisp`, `es.lisp`, `fr.lisp`, data: `{:strings {:en {:key "Text"}}}`, included first |
| The choice | `:strings {:default en :locale ~settings.appearance.language :keep ["Product name"]}` in `defscreen main` |
| Checked by | `affrescoui strings report <entry>`: text written in place, and keys a language is missing |

## How an app is laid out

| File | Holds |
|---|---|
| `app.lisp` | The manifest (data): id, name, `:entry index.screen.lisp`, capabilities, models, seed |
| `index.screen.lisp` | `(include ...)` and `defscreen main`: an outline of the window's parts, in drawing order |
| `state.lisp`, `computed.lisp`, `look.lisp`, `keys.lisp` | `defstate`, `defcomputed`, `deftokens`/`defstyle`, `defkey` |
| `<area>.lisp` (`toolbar.lisp`, `sidebar.lisp`...) | The views of one area of the window |
| `<app>.lisp` | The logic: `defn` and `defcommand` |
| `locales/`, `models.lisp`, `seed/`, `*.policy.lisp` | Data |
| `features/<name>/` | A feature's folder: `feature.lisp` (the manifest) and its program files |

**The rules** ([full list](SCREEN_FORMAT.md#writing-an-app-as-a-program)):

1. Each file starts with a `;;;` header and groups its definitions under `;;;` sections.
2. Every definition has a docstring.
3. A view is at most about 40 lines; a long one calls others.
4. No `"{{...}}"` text in a program: bindings are Lisp forms.
5. Handlers name functions; a step map holding an expression becomes a function.
6. All visible text goes through `(t ...)`.
7. Ids stay stable: scenarios, permissions and agents address nodes by them.

## Version control

| Topic | The rule |
|---|---|
| What is versioned | The source, one entry per definition, comments included: `/program/<file>/<name>` |
| A change | `{"set": {"/program/board.lisp/card-view": "(defview card-view ...)"}}` |
| A keyword edit | `{"set": {"/program/index.screen.lisp/main/:window/:title": "Tasks"}}`: one value inside a form, far cheaper |
| A new definition | `{"add": {"/program/board.lisp/new-view": "(defview new-view ...)"}}` |
| Judging | The new source is built, and the rules judge what it builds, as for JSON. A build that fails is refused |
| Authority | Code runs with the least authority of the authors that led to it; an agent's code stays the agent's |
| Running commands | Which commands an agent may run: `:run {:allow ["agent(move-card)"] :deny ["*(clear-done)"]}` in `:permissions` |

## Connecting an editor

An nREPL client (CIDER, Calva, Conjure) can evaluate in a running app, as you:

```sh
affrescoui run examples/flow/lisp --nrepl        # or: affrescoui_launcher examples/flow/lisp/index.screen.lisp --nrepl 7888
```

The host prints `nREPL server started on port N on host 127.0.0.1` and writes `.nrepl-port` in the app's folder, where
the clients look; connect as to any nREPL server. Your client's first request that touches the app makes the window ask
**Let this editor evaluate as you?**, showing the text it wants to run: only your own press of Let It Evaluate pairs
that connection, until it disconnects. Refuse a question you didn't expect.

| Topic | The rule |
|---|---|
| Evaluating | A definition lands as your change: a revision by `user`, `Evaluate <name> (editor)`, in the file your buffer is (VERSION_CONTROL.md, "The person's own input") |
| A whole file | `load-file` lands the definitions that changed, in one revision |
| Names | Completion, lookup and argument hints come from the running program; lookup jumps to the definition's file and line |
| Errors | Name the line and column in your buffer |
| Before it runs | `(describe-form text)` tells what evaluating a text would do, without running it (SCREEN_FORMAT.md) |
| Not Clojure | `describe` reports `affrescoui`, not `clojure`; namespaces are file names, and features of a JVM aren't there |
| Safety | 127.0.0.1 only, never the Launcher, three refusals end the asking: SECURITY.md, "Editors (nREPL)" |

## Limits

| Limit | Value |
|---|---|
| Running, per event | 10,000,000 operations, 64 MiB, 20,000 nested calls; calls through steps nest 64 deep |
| A build (every revision) | 2,000,000 operations, 16 MiB |
| A program | 16,384 definitions, each at most 1 MiB |
| Reader | 256 levels, numbers of 40 characters, files of 64 MiB |

A limit ends the run with an error naming the file, line, column and definition, never a value.

## Tools

| Command | Does |
|---|---|
| `affrescoui run <file.lisp>` | Prints what a program builds |
| `affrescoui repl` | Tries the language |
| `affrescoui run <app> --nrepl [PORT]` | Runs an app an nREPL client evaluates in, once you pair it |
| `affrescoui check <entry>` | Checks an app; `--truth` lists conditions to look at |
| `affrescoui lisp-binding '{{a && !b}}'` | Writes an expression as its Lisp form: `~(and a (not b))`; `--prove` checks the meaning |
| `affrescoui strings report <entry>` | Text outside the locales |
| `tools/notation/lisp_program.py` | Drafts a data file as a program |
| `tools/notation/lisp_metrics.py <app>` | Measures an app's Lisp: longest definition, deepest line, timing |
| `ctest -R lisp_parity` | Each example's Lisp version draws its first frame as its JSON version does |
