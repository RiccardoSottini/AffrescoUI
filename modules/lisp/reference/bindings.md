# Bindings: the forms a `~form` writes as an expression

A template's `~form` that reads the running app stays in the tree as a binding (`../README.md`, "A `~form` in a top-level
template"). The compiler writes it as the expression machine's text when it can (`src/CompilerLowering.cpp`), so that it
runs at an expression's speed and reads as one in the JSON; what it can't write is a call of the program's function,
`{{lisp('name', ...)}}`, which gives the same value more slowly. This page lists what the binding writes as an
expression since `let` became one, what it refuses, and the functions it leaves as calls on purpose. The rows before
(operators, `if`, `when`, `cond`, `get`, `first`, `str`, the functions on each item) are in `../README.md`, "Lowering".

## `let` and the forms made of it

The expression machine has no local names. A `let` in a binding is written by substitution: `(let [x e] body)` is
`body` with `e`'s text wherever `x` stands, in parentheses unless it is one term (a path, a call, a group or a value).
Each name sees the names before it, and an inner `let`'s name hides an outer one's, as in Clojure.

| Form | Its binding |
|---|---|
| `(let [n (count state.xs) k 2] (+ n k n))` | `{{(length(state.xs) + 2 + length(state.xs))}}` |
| `(let [a state.n b (inc a)] (* a b))` | `{{(state.n * (state.n + 1))}}` |
| `(if-let [m state.mode] (str/upper-case m) "none")` | `{{(state.mode ? upper(state.mode) : 'none')}}` |
| `(when-let [m state.mode] (str m "!"))` | `{{(state.mode ? (state.mode + '!') : null)}}` |
| `(if-some [m state.mode] m "none")`, `when-some` | `{{((state.mode != null) ? state.mode : 'none')}}` |
| `(when-first [x state.xs] (inc x))` | `{{(!(empty(state.xs)) ? (state.xs[0] + 1) : null)}}` |
| `(as-> state.n x (inc x) (* x 2))` | `{{((state.n + 1) * 2)}}` |
| `(some-> state.user :name str/upper-case)`, `some->>` | `{{((state.user == null) ? null : ((state.user.name == null) ? null : upper(state.user.name)))}}` |
| `(cond->> state.xs state.on (filter :done))`, `cond->` | `{{(state.on ? filter(state.xs, it.done) : state.xs)}}` |
| `(case (count state.xs) 0 "none" 1 "one" "many")` | `{{((length(state.xs) == 0) ? 'none' : ((length(state.xs) == 1) ? 'one' : 'many'))}}` |
| `(condp = state.mode "edit" 1 "view" 2 0)` | `{{(('edit' == state.mode) ? 1 : (('view' == state.mode) ? 2 : 0))}}` |
| `(for [x state.xs :when (> x 1)] (* x 2))` | `{{map(filter(state.xs, (it > 1)), (it * 2))}}` |

`case` and `condp` take any value now, not only a word or a constant: they bind it with `let`. A `let` inside a
function on each item may name the item: `(filter (fn [c] (let [d c.done] d)) state.xs)` is
`{{filter(state.xs, it.done)}}`. A `let`'s name hides a function or a macro of that name, as a local does, and a form
whose expansion calls a word of the language (`when-first` calls `first`) is refused where a `let`'s name or an item's
hides it, as everywhere (`forms.md`, "The guard").

The tests are an expression's: in `if-let`, `when-let`, `if`, `when`, `and`, `or` and `not`, `0`, `""`, `[]` and `{}` are
false, where a program's function takes them as true (`../README.md`; `affrescoui check --truth` lists the functions it
matters in).

## The cap

Each use of a `let`'s name writes its value again, and nested `let`s multiply that: `some->` of 40 steps would write the
first value hundreds of times. A binding that substitutes a name and whose text grows past 4000 characters is refused:

`this binding is too large as an expression: define a function for it`

A value the build works out counts as 16 characters, since its text is known only then. A long binding with no `let` is
not capped: its text is as long as its form.

## Functions under an expression's names

| Form | Its binding |
|---|---|
| `(str/upper-case x)`, `(str/lower-case x)` | `upper(x)`, `lower(x)` (the bare `upper-case` and `lower-case` already were) |
| `(str/trim x)` | `trim(x)` |
| `(str/split x ",")` | `split(x, ',')`, with a text to split at |
| `(str/includes? x "a")` | `contains(x, 'a')` |
| `(zero? x)`, `(pos? x)`, `(neg? x)` | `(x == 0)`, `(x > 0)`, `(x < 0)` |
| `(even? x)`, `(odd? x)` | `((x % 2) == 0)`, `((x % 2) != 0)` |
| `(true? x)`, `(false? x)` | `(x == true)`, `(x == false)` |
| `(not-empty x)` | `(empty(x) ? null : x)` |

Each of these gives the same value as the function wherever the function gives one, over nil, booleans, numbers, texts,
vectors and maps (`tests/unit/lisp_clojure_bindings.cpp` runs every row both ways). Where the function gives an error,
the expression may give a value, as the rows before do (`(inc "a")` is `"a1"`).

## What is refused

A binding is one value. A form that does things, or a function the expression machine can't hold, is refused with what
to do instead:

| Form | Message |
|---|---|
| `doseq`, `dotimes`, `letfn`, `doto`, `loop`, `for` with `:while`, a `fn` of several arities called, a `let` of several body forms | `a value read while the app runs is an expression or a call of a function: define a function for this` |
| `for` over several collections, or with `:let` | `a binding can't give a function to "mapcat": define a function for this` |
| A `let` (or `if-let`...) whose name is a pattern: `[a b]`, `{:keys [a]}` | `a let in a binding names plain values: define a function for a pattern` |
| `case` with no default, a `throw` | `a binding can't throw: give a value for when nothing matches, or define a function for this` |
| An inner function using a `let`'s name whose value reads the outer function's item | `a function in a binding can't use "d", whose value reads the item of a function around it: define a function for this` |

The last is `(map (fn [c] (let [d c.n] (filter (fn [e] (> e d)) c.xs))) state.xs)`: the inner `filter`'s `it` is `e`,
and `d`'s text, `it.n`, would read `e`. The same form reading nothing of the app is worked out at the build, as before.

## Left out, staying calls of the function

| Function | Why |
|---|---|
| `str/join` | On nil the function gives `""`, the expression's `join` an error |
| `mapv`, `filterv` | On a text or a map the function goes through its items (characters, entries), where `map` and `filter` give an error |
| `not-any?`, `not-every?` | On nil and on a text or a map, as above: `some` and `every` give another value there |

They stay `{{lisp('mapv', 'done', state.xs)}}` and run as the program's function.

## Where the rows before differ

Running the rows written before this page both ways finds where the expression gives another value than the function,
or an error, though the function gives a value. They are kept as they were, and the test pins them so that a change shows:

- `sort`, `filter`, `remove` and `map` on a text or a map: an error.
- `concat` and `reverse` on nil, a text or a map: the expression treats a text as text, not as characters.
- `mod` of a negative number: `-1` where Clojure gives `1`.
- `first` of a text: null, not the first character.
- `join` of nil: an error, not `""`.
- `str` of a vector: `[1,2]`, not `[1 2]`.
- `some` gives true or false, not the function's value, and `[]` on nil; `every?` gives `[]` on nil.
