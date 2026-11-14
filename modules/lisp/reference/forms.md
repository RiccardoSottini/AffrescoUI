# The everyday forms

The forms people write first in Clojure, each a macro of the language's library (`../src/PreludeForms.cpp`) that expands
to the special forms and functions the language already has. They mean what they mean in Clojure, but where the table
says otherwise. A program that defines one of these names (a macro `doseq`, a view `comment`) has its own, as with any
name of the library.

| Form | What it means | A binding? |
|---|---|---|
| `(-> x f (g a))` | `(g (f x) a)`: each step takes the value as its first argument; a bare word is called | Yes |
| `(->> x f (g a))` | `(g a (f x))`: as its last argument | Yes |
| `(as-> x name a b)` | `(let [name x name a name b] name)` | Yes |
| `(some-> x f (g a))`, `(some->> ...)` | As `->` and `->>`, giving nil at the first step that gives nil | Yes |
| `(cond-> x test (f a) ...)`, `(cond->> ...)` | As `->` and `->>`, taking only the steps whose test holds | Yes |
| `(if-not test a b?)` | `(if (not test) a b)` | Yes |
| `(when-not test body...)` | `(when (not test) body...)` | Yes |
| `(if-let [pattern e] a b?)`, `(when-let [pattern e] body...)` | `e` bound to the pattern when it is true (not nil or false) | Yes, for a name, not a pattern |
| `(if-some [pattern e] a b?)`, `(when-some [pattern e] body...)` | The same, when `e` isn't nil | Yes, for a name, not a pattern |
| `(when-first [x xs] body...)` | The body with `x` the first item, when `xs` isn't empty | Yes, for a name, not a pattern |
| `(case e c r ... default?)` | The `r` of the first constant `=` to `e`. A constant is a number, a text, a keyword, `true`, `false`, `nil`, a word or a vector (taken as written, not run); a list `(c1 c2)` is alternatives. With no default and no match, an error | Yes, with a default |
| `(condp pred e c r ... default?)` | The `r` of the first `c` for which `(pred c e)` is true. `:>>` isn't supported: bind the value with `let` | Yes, when `pred` is a word or a constant, with a default |
| `(for [x xs :let [...] :when p :while q ...] body)` | A vector of the body for each item, nested over several bindings. `:while` takes the binding's collection while `q` holds | Yes, for one collection, `:when` and words the expressions have |
| `(doseq [x xs ...] body...)` | The body run for each item, as in `for`, for what it does; gives nil | No |
| `(dotimes [i n] body...)` | The body run with `i` from 0 to `n - 1`; gives nil | No |
| `(letfn [(f [a] ...) ...] body...)` | Local functions that may call themselves and each other | No |
| `(doto x (f a) g)` | Calls each step with `x` as its first argument, then gives `x` | No |
| `(comment ...)` | Nothing is run; gives nil | Yes |
| `(assert x message?)` | An error, `Assert failed` (with a literal text: `Assert failed: text`), when `x` is false or nil | No |
| `(defn- name ...)`, `(defonce name value)` | `defn` and `def`: a program has no namespaces to keep a name private in, and nothing is loaded twice | Top-level only |

A binding writes `let` by substitution, so the forms made of it are expressions; those that do things or name a
pattern are refused with what to write instead (`bindings.md`). The forms
check their shape where they are written, and the error names that place: `if-let takes a vector of one name and one
value`, `for takes a vector of names and collections`, `dotimes takes [name count]`, `case takes pairs of a constant and
a form, and maybe a default`.

## The guard

A form's expansion calls `not`, `=`, `nil?`, `some?`, `empty?`, `first`, `map`, `filter` and `mapcat` by their names,
so that it lowers to a binding, and every other function as the language's own value, which no program changes. Where a
local (a binding's `let` name too) or the program's own definition hides one of those names, the call is an error that
names both: `(let [not 1] (when-not x y))` gives `"when-not" uses the language's "not", which a local hides here: rename the local`.

## Left out

- `while`: the language has no mutable state for its test to watch; write `loop` and `recur`.
- `binding`: there are no dynamic variables; pass the value as an argument.
- `with-open`: the language opens nothing; a host's services do.
- `lazy-seq`: collections aren't lazy; `for` and `map` give vectors.
- `declare`: a program's definitions are all read before any is run, so a name needs no declaring (refused, with a
  hint).
