# The everyday forms

The forms people write first in Clojure, each a macro of the language's library (`../src/PreludeForms.cpp`) that expands
to the special forms and functions the language already has. They mean what they mean in Clojure, but where the table
says otherwise. A program that defines one of these names (a macro `doseq`, a view `comment`) has its own, as with any
name of the library.

| Form | What it means | A binding? |
|---|---|---|
| `(-> x f (g a))` | `(g (f x) a)`: each step takes the value as its first argument; a bare word is called | Yes |
| `(->> x f (g a))` | `(g a (f x))`: as its last argument | Yes |
| `(as-> x name a b)` | `(let [name x name a name b] name)` | Not yet: it is a `let` |
| `(some-> x f (g a))`, `(some->> ...)` | As `->` and `->>`, giving nil at the first step that gives nil | Not yet: a `let` |
| `(cond-> x test (f a) ...)`, `(cond->> ...)` | As `->` and `->>`, taking only the steps whose test holds | Not yet: a `let` |
| `(if-not test a b?)` | `(if (not test) a b)` | Yes |
| `(when-not test body...)` | `(when (not test) body...)` | Yes |
| `(if-let [pattern e] a b?)`, `(when-let [pattern e] body...)` | `e` bound to the pattern when it is true (not nil or false) | Not yet: a `let` |
| `(if-some [pattern e] a b?)`, `(when-some [pattern e] body...)` | The same, when `e` isn't nil | Not yet: a `let` |
| `(when-first [x xs] body...)` | The body with `x` the first item, when `xs` isn't empty | Not yet: a `let` |
| `(case e c r ... default?)` | The `r` of the first constant `=` to `e`. A constant is a number, a text, a keyword, `true`, `false`, `nil`, a word or a vector (taken as written, not run); a list `(c1 c2)` is alternatives. With no default and no match, an error | Yes, when `e` is a word or a constant |
| `(condp pred e c r ... default?)` | The `r` of the first `c` for which `(pred c e)` is true. `:>>` isn't supported: bind the value with `let` | Yes, when `pred` and `e` are words or constants |
| `(for [x xs :let [...] :when p :while q ...] body)` | A vector of the body for each item, nested over several bindings. `:while` takes the binding's collection while `q` holds | Yes, for bindings, `:when` and words the expressions have |
| `(doseq [x xs ...] body...)` | The body run for each item, as in `for`, for what it does; gives nil | No |
| `(dotimes [i n] body...)` | The body run with `i` from 0 to `n - 1`; gives nil | No |
| `(letfn [(f [a] ...) ...] body...)` | Local functions that may call themselves and each other | No |
| `(doto x (f a) g)` | Calls each step with `x` as its first argument, then gives `x` | No |
| `(comment ...)` | Nothing is run; gives nil | Yes |
| `(assert x message?)` | An error, `Assert failed` (with a literal text: `Assert failed: text`), when `x` is false or nil | No |
| `(defn- name ...)`, `(defonce name value)` | `defn` and `def`: a program has no namespaces to keep a name private in, and nothing is loaded twice | Top-level only |

A form that can't be a binding yet expands to `let`, `loop` or `throw`; a later package lets `let` be one. The forms
check their shape where they are written, and the error names that place: `if-let takes a vector of one name and one
value`, `for takes a vector of names and collections`, `dotimes takes [name count]`, `case takes pairs of a constant and
a form, and maybe a default`.

## The guard

A form's expansion calls `not`, `=`, `nil?`, `some?`, `map`, `filter` and `mapcat` by their names, so that it lowers to
a binding, and every other function as the language's own value, which no program changes. Where a local or the
program's own definition hides one of those names, the call is an error that names both: `(let [not 1] (when-not x y))`
gives `"when-not" uses the language's "not", which a local hides here: rename the local`.

## Left out

- `while`: the language has no mutable state for its test to watch; write `loop` and `recur`.
- `binding`: there are no dynamic variables; pass the value as an argument.
- `with-open`: the language opens nothing; a host's services do.
- `lazy-seq`: collections aren't lazy; `for` and `map` give vectors.
- `declare`: a program's definitions are all read before any is run, so a name needs no declaring (refused, with a
  hint).
