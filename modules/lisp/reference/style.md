# Style: writing an app's program in the dialect

The examples' programs (`examples/<app>/lisp/`) were written before the language took Clojure's forms (RFC 0006). This
page says how to write them in the dialect, so that they read as Clojure and stay what they are: the same pixels, the
same tree, the same behaviour. The examples' rows are from `examples/flow/lisp/` as it is written today. The other
pages of this folder say what each form does; this one says which to choose.

## What a rewrite may not change

- **The pixels.** Every frame of the rendering check and of `lisp_parity` stays identical.
- **The tree.** The document the app builds (`affrescoui run --print examples/<app>/lisp`) stays the same, but for
  a binding's text where a form now lowers to another, equal expression (`lisp_tree.py --compare`, below). No binding
  may become a call of a function (`{{lisp(...)}}`) where it was an expression.
- **The names:** of definitions, files, ids, state keys and parameters. A handler that names a function passes its
  arguments by parameter name, and agents, scenarios and the agent benchmark name definitions.
- **The order of definitions.** It is the order of the sections they fill (commands, menus, keys, the palette).
- **The JSON version.** Each example keeps its `json/` folder beside its program, untouched.

## Today, and written as

"In a function" is a `defn`, `defcommand` or a view's code outside its template; "in a binding" is a `~form` in a
template and a `defcomputed`, which run as the app's expressions (`bindings.md`).

| Today | Written as | In a function | In a binding |
|---|---|---|---|
| `(fn [it] (= it.tag tag))` | `#(= (:tag %) tag)` | One line, no `#(` inside | Yes |
| `(map (fn [it] it.ref) xs)` | `(map :ref xs)` | Yes | Yes |
| Three or more nested calls | `->`, `->>` | Yes | When each step is an expression |
| `(if (nil? x) nil (f x))` | `(some-> x f)` | Yes | Yes |
| `(if (nil? x) a (f x))`, `(when (some? x) ...)` | `if-some`, `when-some` | Yes | When `x` is a path or used once |
| `(let [v e] (when v ...))` | `when-let` | Only where the test was truth | No |
| A `cond` comparing one value | `case`, always with a default | Yes | Yes |
| An `or` of `=`, `(contains [...] x)` | `(#{:a :b} x)` | Yes | Only where a set is an expression (`sets.md`) |
| `(not (= a b))`, `(when (not c))`, `(if (not c) a b)` | `not=`, `when-not`, `if-not` | Yes | Yes |
| `(- n 1)`, `(+ n 1)`, `(= (count xs) 0)` | `dec`, `inc`, `empty?` | Yes | Yes |
| `(= n 0)`, `(> n 0)` | `zero?`, `pos?` | Only where `n` is surely a number | Yes |
| `(trim s)` and the other string functions | `str/trim`... | Yes | Only the rows of `bindings.md` |
| `(join ", " xs)` | `str/join` | Yes | No: `join` stays |
| A tally by hand | `frequencies`, `group-by`, `merge-with`, `update-vals` | Yes | No |
| `(filter some? (map f xs))` | `(keep f xs)` | Yes | No |
| `(let [a (:a m) b (:b m)] ...)` | `{:keys [a b]}` | Yes | No |
| `map` run for its steps | `doseq` | Yes | |
| A parameter that is `nil` when left out | Several parameter lists | `defn` only | |
| Definitions that differ in one word | A vector and `for` or `doseq` | Where the tree stays identical | |

Each row, on code of Flow's (where Flow has no such place today, a short one in its manner):

```clojure
;; A one-parameter fn (computed.lisp, visible)
(filter (fn [it] (or (= state.owner "") (= it.who state.owner))) computed.matches)
(filter #(or (= state.owner "") (= (:who %) state.owner)) computed.matches)

;; A member of each item (computed.lisp, matchRefs)
(defcomputed matchRefs (map (fn [it] it.ref) computed.matches))
(defcomputed matchRefs (map :ref computed.matches))

;; Nested calls (flow.lisp, nextOrder)
(inc (reduce max -1 (map :order (cardsOf c))))
(->> (cardsOf c) (map :order) (reduce max -1) inc)

;; nil, or a function of the value (flow.lisp, announcePresence)
(if (nil? computed.card) nil computed.card.title)
(some-> computed.card :title)

;; nil, or another value (computed.lisp, next)
(if (nil? state.selected) computed.visible.0 (find ... computed.visible))
(if-some [selected state.selected] (find ... computed.visible) computed.visible.0)

;; A test that was a value's truth (not in Flow today)
(let [col (column c)] (when col (notify (:name col))))
(when-let [col (column c)] (notify (:name col)))

;; A cond on one value (flow.lisp, setFilter)
(cond (= prop "status") (set "state.fStatus" (if on -1 v)) (= prop "assignee") ...)
(case prop "status" (set "state.fStatus" (if on -1 v)) "assignee" ... nil)

;; Membership (flow.lisp, the owner of a reference; computed.lisp, the card's commands)
(or (= (lower (:who it)) wanted) (= (lower (:name it)) wanted))   ; two values: stays an or
(contains ["changeStatus" "changePriority" "editCard"] it.id)  ->  (#{"changeStatus" "changePriority" "editCard"} it.id)

;; not (flow.lisp, selectCard and toggleSwimlanes)
(when (not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i))) ...)
(when-not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)) ...)

;; By one, and empty (flow.lisp, the inbox's enablement and undo)
(< (+ state.inboxAt 1) (count computed.inboxShown))  ->  (< (inc state.inboxAt) (count computed.inboxShown))
(if (= (count state.undoStack) 0) ...)  ->  (if (empty? state.undoStack) ...)

;; Zero and positive (flow.lisp, the marked cards)
(if (> (count state.marked) 0) ...)
(if (pos? (count state.marked)) ...)

;; The string functions (flow.lisp, addCard)
(let [title (trim state.newCard)] ...)
(let [title (str/trim state.newCard)] ...)

;; Joining, in a binding (columns.lisp, a card's labels): join stays
:tooltip ~(join ", " (map (fn [it] (t (str "label." it))) (or-else card.labels [])))
:tooltip ~(join ", " (map #(t (str "label." %)) (or-else card.labels [])))

;; A tally (not in Flow today)
(reduce (fn [counts it] (assoc counts it.tag (inc (or-else (get counts it.tag) 0)))) {} cards)
(frequencies (map :tag cards))

;; The values that are there (flow.lisp, columnOf)
(first (filter some? (map-indexed (fn [c col] (when ... c)) computed.columns)))
(first (keep-indexed (fn [c col] (when ... c)) computed.columns))

;; Members read one by one (flow.lisp, checkLimit)
(let [col (column c)] ... (:limit col) ... (:cards col) ... (:name col))
(let [{:keys [limit cards name]} (column c)] ...)

;; Steps for each item (not in Flow today)
(map (fn [it] (remove-state "state.marked" :at 0)) state.marked)
(doseq [_ state.marked] (remove-state "state.marked" :at 0))

;; A parameter left out (not in Flow today)
(defn notifyCard [ref message] (notify (or-else message (t "toast.noCard" {:ref ref}))))
(defn notifyCard ([ref] (notifyCard ref nil)) ([ref message] ...))

;; Definitions that differ in one word (not in Flow today)
(defn showBoard [] (set "state.page" "board"))  (defn showList [] (set "state.page" "list"))
[["board" ...] ["list" ...]] and a for over it, in one file              ; only if the tree is identical
```

## The traps

- **Truth differs between a function and a binding.** In a binding `0`, `""`, `[]` and `{}` are false; in a function
  only `nil` and `false` are. So a `nil` test never becomes `if-let` or `when-let`: write `if-some` and `when-some`,
  which test for `nil` in both. `affrescoui check --truth` lists the functions where it matters.
- **`case` and `condp` throw when nothing matches.** Always give the default, even where a `cond` had none (`nil`).
- **`zero?`, `pos?`, `inc` and `dec` raise on `nil` in a function**, where `(= n 0)` and `(+ n 1)` gave a value. Use
  them only where the value is surely a number.
- **A binding's `let` writes its value at each use** (`bindings.md`, "The cap"): a name used five times is its text
  five times. A `let` in a binding is for reading, not for saving work.
- **Some forms in a binding become slow calls or are refused:** `str/join`, `mapv`, `filterv`, `not-any?` and `format`
  stay calls of the function (`{{lisp(...)}}`), and patterns (`{:keys [...]}`, `[a b]`) are refused. Keep `join`,
  `map`, `filter` and plain names there.
- **A step's method, service or path stays written text:** `(set "state.x" 1)`, `(call "service" ...)`. Only the
  values a step writes may become keywords.
- **A handler that names a function passes its arguments by parameter name:** renaming a parameter changes what the
  handler sends.
- **`set`, `find`, `empty`, `time`, `type` and `repeat` are the host's** (`differences.md`, "The host's words"): `set`
  is the step, `find` takes the predicate first, `repeat` with a vector first is a view's repeat.
- **A local named `first`, `map`, `filter`, `not`, `=`, `mapcat`, `nil?` or `some?` stops the library macros** that
  call that word (`differences.md`, "Library macros name nine functions by word"): name the local otherwise.
- **A set read back from state is a vector:** state holds JSON. Never store a set in state; make it where it is
  tested.
- **The order of definitions fills the sections:** folding or moving definitions moves the commands, menus and keys.
- **`or-else` is not `or`:** `or-else` keeps `0`, `""` and `false` and replaces only `nil`; `or` replaces every false
  value. Keep `or-else` where it is.

## `ns`

Every code file gets a label, `(ns <app>.<file>)`, as its first form, after its `;;;` header: `(ns flow.computed)` for
`computed.lisp`, `(ns forge.features.editor.editor)` for `features/editor/editor.lisp` (`namespaces.md`, "Which file a
name requires"). The entry's `(:require ...)` replaces its `(include ...)` forms and lists the app's files in the order
the include had: code files by their dotted name, data files and shared files by their path as a string.

```clojure
(ns flow.index
  (:require "locales/en.lisp" "locales/es.lisp" "locales/fr.lisp"     ; data files: by path
            flow.state flow.computed flow.look flow.keys flow.toolbar    ; code files: by dotted name
            ...
            "../../shared/components/charts.lisp"                       ; shared files: by path
            ...))
```

No `:refer` lists: every program is one flat space of names (`differences.md`), so a definition is reached by its own
name. Write an alias (`:as`) only where the file uses it. `examples/shared/` files get labels too, and are still
required by path.

## Keywords and text

- **Keywords** for fixed choices in code: what a value is compared with, `case` clauses, a set's members, and the
  values a step writes (`(set "state.page" :board)`, compared as `(= state.page :board)`).
- **Text** for what a person reads, for paths, ids and keys given to steps (`"state.page"`, `"issues"`, `"#card"`), and
  in data files (models, seed, locales).

A keyword and the string of its text are one value, `(= :board "board")` is true, and the tree holds its text
(`differences.md`, "A map's keys are text"), so a choice can move to keywords one file at a time.

## Marking a place that must stay

A place the counter below would count, but that must stay as it is (a binding where the other spelling is slower, a
value that may be `nil`), is marked by a comment on the line before:

```clojure
; style: keep, the count may be nil before the first load, where dec raises
(- (or-else state.count 0) 1)
```

## How to check an app

For an app, `flow` here, with the build in `build/`:

```sh
export AFFRESCOUI_HIDDEN=1
python3 tools/notation/lisp_tree.py --save build/tree-before flow       # before the rewrite
python3 tools/notation/lisp_tree.py --compare build/tree-before flow    # after: identical, or only bindings' text
python3 tools/notation/lisp_idioms.py --check                           # no rule counts more than idioms/flow.json allows
python3 tools/notation/lisp_idioms.py --count flow                      # what is left
python3 tools/notation/lisp_idioms.py --measure                         # files, lines, bytes, tokens per app
./build/affrescoui check examples/flow/lisp/index.screen.lisp           # and every file changed
ctest --test-dir build -R "scenarios/flow/|lisp_parity|cli_lisp_idioms|cli_lisp_examples"
python3 tests/render/check.py                                           # every frame identical
```

Once a rule's count drops, write the new count into the app's `tools/notation/idioms/<app>.json` (`--write` writes
every file's), so that it never grows back.
