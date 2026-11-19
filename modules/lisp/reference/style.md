# Style: writing an app's program in the dialect

Every example's program (`examples/<app>/lisp/`, and `examples/shared/`) is written in the dialect (RFC 0006): it
reads as Clojure and draws, builds and behaves as it did before the language took Clojure's forms. This page says how
to write a program that way, and how to change one without changing what it does. The examples' rows are from
`examples/flow/lisp/`. The other pages of this folder say what each form does; this one says which to choose.

## What a rewrite may not change

- **The pixels.** Every frame of the rendering check and of `lisp_parity` stays identical.
- **The tree.** The document the app builds (`affrescoui run --print examples/<app>/lisp`) stays the same, but for
  a binding's text where a form now lowers to another, equal expression (`lisp_tree.py --compare`, below). No binding
  may become a call of a function (`{{lisp(...)}}`) where it was an expression.
- **The names:** of definitions, files, ids, state keys and parameters. A handler that names a function passes its
  arguments by parameter name, and agents, scenarios and the agent benchmark name definitions.
- **The order of definitions.** It is the order of the sections they fill (commands, menus, keys, the palette).
- **The JSON version.** Each example keeps its `json/` folder beside its program, untouched.

## The old spelling, and the dialect's

"In a function" is a `defn`, `defcommand` or a view's code outside its template; "in a binding" is a `~form` in a
template and a `defcomputed`, which run as the app's expressions (`bindings.md`).

| Old spelling | Written as | In a function | In a binding |
|---|---|---|---|
| `(fn [it] (= it.tag tag))` | `#(= (:tag %) tag)` | One line, no `#(` inside | Yes, but not given to `sum` |
| `(map (fn [it] it.ref) xs)` | `(map :ref xs)` | Yes | Yes, but not given to `sum` |
| Three or more nested calls | `->`, `->>` | Yes | When each step is an expression |
| `(if (nil? x) nil (:k x))` | `(:k x)`: a keyword gives `nil` on `nil` | Yes | Yes |
| `(if (nil? x) nil (f x))` | `(some-> x f)` | Yes | Yes |
| `(if (nil? x) a (f x))`, `(when (some? x) ...)` | `if-some`, `when-some` | Yes | A plain name, read as `name.key` |
| `(let [v e] (when v ...))` | `when-let` | Only where the test was truth | No |
| A `cond` comparing one value | `case`, always with a default | Yes | Yes |
| An `or` of `=`, `(contains [...] x)` | `(#{:a :b} x)` | Yes | Only where a set is an expression (`sets.md`) |
| `(not (= a b))`, `(when (not c))`, `(if (not c) a b)` | `not=`, `when-not`, `if-not` | Yes | Yes |
| `(- n 1)`, `(+ n 1)`, `(= (count xs) 0)` | `dec`, `inc`, `empty?` | Yes | Yes |
| `(> (count xs) 0)` | `(seq xs)` | Yes | No: `(pos? (count xs))`, the same expression |
| `(= n 0)`, `(> n 0)` | `zero?`, `pos?` | Only where `n` is surely a number | Yes |
| `(trim s)` and the other string functions | `str/trim`... | Yes | Only the rows of `bindings.md` |
| `(join ", " xs)` | `str/join` | Yes | No: `join` stays |
| A tally by hand | `frequencies`, `group-by`, `merge-with`, `update-vals` | Yes | No |
| `(filter some? (map f xs))` | `(keep f xs)` | Yes | No |
| `(let [a (:a m) b (:b m)] ...)` | `{:keys [a b]}`, `[a b]` | Yes | No |
| `(merge m {:k v})` | `(assoc m :k v)` | Yes | No: `assoc` is a call, `merge` an expression |
| `map` run for its steps | `doseq` | Yes | |
| A parameter that is `nil` when left out | Several parameter lists | `defn` only | |
| Definitions that differ in one word | A vector and `for` or `doseq` | Where the tree stays identical | |

Each row, on code of Flow's (where Flow has no such place, a short one in its manner), the old spelling first:

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

;; nil, or a member of the value (flow.lisp, announcePresence): a keyword gives nil on nil
(if (nil? computed.card) nil computed.card.title)
(:title computed.card)

;; nil, or a function of the value (not in Flow today)
(if (nil? state.user) nil (str/upper-case state.user.name))
(some-> state.user :name str/upper-case)

;; nil, or another value, in a binding (computed.lisp, next): a plain name, read as at.c
(if (nil? state.selected) computed.visible.0 (find #(... state.selected.c ...) computed.visible))
(if-some [at state.selected] (find #(... at.c ...) computed.visible) computed.visible.0)

;; A test of nil where the value isn't used (flow.lisp, setTag): check --truth lists (when computed.card ...)
(when computed.card (updateCard {:tag tag}))
(when-some [_ computed.card] (updateCard {:tag tag}))

;; A test that was a value's truth (not in Flow today)
(let [col (column c)] (when col (notify (:name col))))
(when-let [col (column c)] (notify (:name col)))

;; A cond on one value (flow.lisp, setFilter; computed.lisp, filterOptions, in a binding)
(cond (= prop "status") (set "state.fStatus" (if on -1 v)) (= prop "assignee") ...)
(case prop :status (set "state.fStatus" (if on -1 v)) :assignee ... nil)

;; Membership (flow.lisp, memberNamed; computed.lisp, paletteFirst, in a binding)
(or (= (lower (:who it)) wanted) (= (lower (:name it)) wanted) (= (first (split (lower (:name it)) " ")) wanted))
(some #{wanted} [(str/lower-case (:who member)) full (first (str/split full " "))])
(contains ["changeStatus" "changePriority" ...] (:name %))  ->  (#{:changeStatus :changePriority ...} (:name %))

;; not (flow.lisp, selectCard and toggleSwimlanes)
(when (not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i))) ...)
(when-not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)) ...)

;; By one, and empty (flow.lisp, the inbox's enablement and undo)
(< (+ state.inboxAt 1) (count computed.inboxShown))  ->  (< (inc state.inboxAt) (count computed.inboxShown))
(if (= (count state.undoStack) 0) ...)  ->  (if (empty? state.undoStack) ...)

;; Some, and positive (flow.lisp, the marked cards: seq in a function, pos? in a binding)
(if (> (count state.marked) 0) ...)                ->  (if (seq state.marked) ...)
:enablement ~(> (count state.marked) 0)            ->  :enablement ~(pos? (count state.marked))

;; The string functions, and blank text (flow.lisp, addCard)
(let [title (trim state.newCard)] (when (not= title "") ...))
(let [title (str/trim state.newCard)] (when-not (str/blank? title) ...))

;; Joining, in a binding (columns.lisp, a card's labels): join stays
:tooltip ~(join ", " (map (fn [it] (t (str "label." it))) (or-else card.labels [])))
:tooltip ~(join ", " (map #(t (str "label." %)) (or-else card.labels [])))

;; A tally, and a grouping (flow.lisp, buildTimeline)
(map (fn [m] {... :clips (map ... (filter #(= (:who %) (:who m)) cards))}) state.members)
(let [owned (group-by :who (filter :due computed.allCards))]
  (map (fn [{:keys [who name]}] {... :clips (map ... (get owned who []))}) state.members))
(reduce (fn [counts it] (assoc counts it.tag (inc (or-else (get counts it.tag) 0)))) {} cards)  ; not in Flow
(frequencies (map :tag cards))

;; The values that are there (flow.lisp, columnOf)
(first (filter some? (map-indexed (fn [c col] (when ... c)) computed.columns)))
(first (keep-indexed (fn [c col] (when ... c)) computed.columns))

;; Members read one by one (flow.lisp, checkLimit; openRow's "c-i")
(let [col (column c)] ... (:limit col) ... (:cards col) ... (:name col))
(let [{:keys [limit cards name]} (column c)] ...)
(let [parts (split event.id "-")] (selectCard (number (first parts)) (number (second parts))))
(let [[c i] (map number (str/split event.id "-"))] (selectCard c i))

;; Steps for each item (flow.lisp, moveSelected)
(map #(moveMarked % (columnOf %) to) state.marked)
(doseq [ref state.marked] (moveMarked ref (columnOf ref) to))

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
- **Some forms in a binding become slow calls or are refused:** `str/join`, `mapv`, `filterv`, `not-any?`, `assoc`
  and `format` stay calls of the function (`{{lisp(...)}}`), and patterns (`{:keys [...]}`, `[a b]`) are refused, in
  a `let`, an `if-some` and a `when-some` alike. Keep `join`, `merge`, `map`, `filter` and plain names there.
- **`sum` is the expression's own function, not the library's:** the function it takes names its item `it`, so
  `(sum xs (fn [it] (or-else it.points 0)))` stays, marked keep; `#(...)` is refused, and `:points` becomes the text
  `'points'`.
- **`#(...)` can't give a map:** `(fn [it] {:v it.who ...})` stays, marked keep.
- **A step's method, service or path stays written text:** `(set "state.x" 1)`, `(call "service" ...)`. Only the
  values a step writes may become keywords.
- **`str` writes a keyword with its colon:** `(str :es)` is `":es"`. A keyword stays out of `str` and of a
  translation's key; compared, written by `set`, matched by `case` or passed to a handler, it is its text.
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
- **`str/split` drops the empty parts at the end**, as Clojure's does, where the host's `split` keeps them. Give it `-1`
  where `last`, `butlast` or `count` reads the parts (`(str/split path "/" -1)`), in a function; in a binding, `-1`
  makes it a call, so the host's `split` stays there.
- **`cons` gives a list, and a list set into state reads back as a node form:** `(set "state.recent" (cons x xs))`
  fails on the next read. Write `(concat [x] xs)`, or give the list to `take` or `vec` first.

## `ns`

Every code file gets a label, `(ns <app>.<file>)`, as its first form, after its `;;;` header and a blank line:
`(ns flow.computed)` for `computed.lisp`, `(ns forge.features.editor.editor)` for `features/editor/editor.lisp`,
`(ns shared.components.charts)` for `examples/shared/components/charts.lisp`
(`namespaces.md`, "Which file a name requires"). The entry's `(ns <app>.index (:require ...))` replaces its
`(include ...)` and lists the same files in the same order: code files by their dotted name, data files and shared
files by their path as a string. Flow's (`index.screen.lisp`):

```clojure
;;; index.screen.lisp --- Flow: an issue tracker with a kanban board
;;; ...

(ns flow.index
  (:require "locales/en.lisp"                                  ; data files: by path
            "locales/es.lisp"
            "locales/fr.lisp"
            flow.state                                         ; code files: by dotted name
            flow.computed
            ...
            flow.overlays
            "../../shared/components/charts.lisp"              ; shared files: by path
            flow.board
            ...
            flow.select
            "../../shared/addons/assistant/assistant.lisp"
            flow.flow))
```

and each code file's (`computed.lisp`):

```clojure
;;; computed.lisp --- what Flow works out from its state and its records
;;; ...

(ns flow.computed)

;;; The board's columns and cards
```

The program keeps an `ns` form as an entry of its own, `ns`, and the file's `;;;` header moves to it from the file's
first definition. `lisp_tree.py --compare` counts both as a definition's text, so the check stays at exit 0; the
built screen is the same.

No `:refer` lists: every program is one flat space of names (`differences.md`), so a definition is reached by its own
name. Write an alias (`:as`) only where the file uses it. `examples/shared/` files get labels too, and are still
required by path. A file that others require names its own data files in its `ns` (`(ns shared.addons.assistant.assistant
(:require "locales/en.lisp" ...))`), as an entry does. A program whose `ns` forms are the same before and after a
change still builds in part and redefines in place (`namespaces.md`); changing an `ns` form builds it whole.

## Keywords and text

- **Keywords** for fixed choices in code: what a value is compared with, `case` clauses, a set's members, and the
  values a step writes (`(set "state.page" :board)`, compared as `(= state.page :board)`).
- **Text** for what a person reads, for paths, ids and keys given to steps (`"state.page"`, `"issues"`, `"#card"`), and
  in data files (models, seed, locales).

A keyword and the string of its text are one value, `(= :board "board")` is true, and the tree holds its text
(`differences.md`, "A map's keys are text"), so a choice can move to keywords one file at a time.

## Marking a place that must stay

A place the counter below would count, but that must stay as it is (a binding where the other spelling is slower, a
value that may be `nil`), is marked by a comment on the line before, with its reason:

```clojure
; style: keep, the count may be nil before the first load, where dec raises
(- (or-else state.count 0) 1)
```

A mark with no reason fails the check, and so does a new mark that the app's idioms file doesn't count (below). A mark
covers every rule on its line.

## When not to

The places the examples keep, by reason (`lisp_idioms.py --kept` lists each with its file and line):

- **`#(...)` can't give a map**, since `#({...})` is a call: `(map (fn [it] {:id it.id ...}) xs)` stays `fn`. It is the
  most common reason by far.
- **`#(...)` can't hold another `#(...)`:** the inner one stays `#(...)` and the outer one `fn`.
- **`sum` is the expression's own function:** the function it takes names its item `it`.
- **A binding takes no `constantly`, and a function that leaves out its item has no `#(...)` form:** `(fn [it] 0)`
  stays.
- **A macro or a template hands its form a fixed name** (`it`, or an unquoted `~kind`): its `fn` stays.
- **The host's `split` keeps what `str/split` drops:** the empty part after a last `/`, `.`, space or line break, where
  the code counts or joins the parts.
- **The host's `trim` gives `""` for `nil`,** where `str/trim` raises: an optional field keeps `trim`.
- **A word that lowers to another binding text:** `inc` puts the 1 after (`(+ 1 x)` stays), and threading can move an
  operand (`(-> x (max 0.5))` is `(max x 0.5)`).
- **A `cond` that isn't one value:** its tests read two fields, or a part of a value and the whole value, or its values
  are different texts.
- **Three states:** `(if (nil? checked) nil (= checked true))`, where `nil` (no check mark) stays apart from `false`.
- **A name of the program's own:** a definition named like a string function (`starts-with?`) is not the host's.

A nest of four calls on one line is a shape, not a spelling: write it with `->` or `->>` where each step
reads better so, and leave the rest (arithmetic on a frame, a long view formula). The check holds each app to its
count of them, so none is added unseen.

## How to check an app

For an app, `flow` here, with the build in `build/`:

```sh
export AFFRESCOUI_HIDDEN=1
python3 tools/notation/lisp_tree.py --save build/tree-before flow       # before the rewrite
python3 tools/notation/lisp_tree.py --compare build/tree-before flow    # after: identical, or only bindings' text
python3 tools/notation/lisp_idioms.py --check                           # every app at its lock (idioms/flow.json)
python3 tools/notation/lisp_idioms.py --count flow                      # what is left
python3 tools/notation/lisp_idioms.py --measure                         # files, lines, bytes, tokens per app
./build/affrescoui check examples/flow/lisp/index.screen.lisp           # and every file changed
ctest --test-dir build -R "scenarios/flow/|lisp_parity|cli_lisp_idioms|cli_lisp_examples"
python3 tests/render/check.py                                           # every frame identical
```

The check is a lock. Every old spelling counts 0 in every app; each rule's kept places are exactly the number the app's
`tools/notation/idioms/<app>.json` gives under `kept`, and its nests of four calls exactly the number under `allowed`.
So a change that adds or removes a kept place or a nest changes that file too, by hand, in the same commit, where the
review sees it (`--write` rewrites every app's file, other apps' too). A new example's code fails until it has an
idioms file of its own.
