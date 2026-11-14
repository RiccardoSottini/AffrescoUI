# Functions

The everyday functions on numbers, collections and maps, beyond the core the module's page (`../README.md`) lists.
Each means what it means in Clojure, except for the differences listed at the end. The natives live in
`src/CoreNumbers.cpp`, `src/CoreSequences.cpp`, `src/CoreCollections.cpp` and `src/PreludeFunctions.cpp`. The
functions that take functions are written in the language itself (`src/Prelude.cpp`, `src/PreludeFunctions.cpp`), so a
host's primitive passed to them still goes through the host's gate, with the caller's authors. The editor's word list
is in `words/Functions.hpp`. The tests are in `tests/unit/lisp_clojure_functions.cpp`.

## Numbers

| Name | What it does |
|---|---|
| `even?` `odd?` | Whether a whole number is even or odd. `1.5` is an error: `"even?" takes a whole number` |
| `zero?` `pos?` `neg?` | Compare a number with zero. NaN is none of them |
| `integer?` `int?` `float?` `double?` | Whether a value is a whole number or a number with a fraction (`1.0` is not an integer) |
| `nat-int?` `pos-int?` `neg-int?` | A whole number that is at least 0, above 0 or below 0 |
| `quot` | Division rounded toward zero: `(quot -7 2)` is `-3`. Dividing by zero is an error |
| `int` `double` | A number as a whole number (rounded toward zero) or with a fraction. `int` of a number beyond an integer's range is an error |
| `==` | Numbers equal by value, whatever their kind: `(== 1 1.0)` |
| `compare` | -1, 0 or 1. Numbers with numbers, text with text (a keyword is the string of its text), nil before everything. Booleans aren't compared |
| `bit-and` `bit-or` `bit-xor` `bit-not` | Bitwise operations on whole numbers |
| `bit-shift-left` `bit-shift-right` | Shift a whole number by 0 to 63 places |

## Values

| Name | What it does |
|---|---|
| `true?` `false?` | Whether a value is exactly `true` or exactly `false` |
| `boolean` | `false` for nil and false, `true` for everything else |
| `coll?` `seq?` `sequential?` `associative?` | Whether a value is a collection, a list, a list or a vector, a map or a vector |
| `ident?` `ifn?` | Whether a value is a keyword or a symbol, or whether it can be called |

## Sequences

| Name | What it does |
|---|---|
| `seq` | nil for an empty collection. A list or a vector is returned as it is; a map gives its `[key value]` pairs, a string its characters |
| `next` `nnext` `nthnext` | The rest after one, two or n items, or nil when nothing is left |
| `nthrest` | The rest after n items, empty when nothing is left |
| `ffirst` `fnext` `nfirst` | `(first (first x))`, `(first (next x))`, `(next (first x))` |
| `peek` `pop` | The last item of a vector or the first of a list, and the collection without it. `pop` of an empty vector is an error |
| `subvec` | The items of a vector from one index up to another |
| `rseq` | A vector's items in reverse, or nil when it is empty |
| `butlast` `drop-last` `take-last` | All but the last item, all but the last n, or the last n |
| `take-nth` `split-at` | Every nth item, and `[(take n xs) (drop n xs)]` |
| `interpose` `interleave` | A separator between items, and the items of several collections in turn |
| `partition` `partition-all` | Groups of n items, with an optional step and padding. `partition` drops a short last group; `partition-all` keeps it |
| `distinct` `dedupe` | The items without repeats, or without repeats next to each other. `1` and `1.0` count as the same item, as do `:a` and `"a"` |
| `flatten` | The items of nested lists and vectors, in one vector |
| `into` | Adds the items of one collection to another: a vector, a list (which adds at the front), nil or a map |
| `vector` `list*` | A vector of the arguments, and a list of the arguments followed by the items of the last one |
| `not-empty` | The collection, or nil when it is empty |
| `key` `val` | The key and the value of a `[key value]` pair, as in `(map key m)` |
| `doall` `dorun` | Nothing is lazy, so these return the collection and nil |

## Maps

| Name | What it does |
|---|---|
| `hash-map` | A map from keys and values, in pairs |
| `zipmap` | A map from a collection of keys and one of values, up to the shorter |
| `frequencies` | A map of each item to how many times it occurs |
| `group-by` | A map of each `(f item)` to the items that gave it, in order |
| `select-keys` | The map with only the keys given |
| `merge-with` | `merge`, combining the values of a key in both maps with a function |
| `update-vals` `update-keys` | The map with a function applied to each value or each key |
| `reduce-kv` | `reduce` over a map's keys and values, or a vector's indexes and items |

## Functions that take functions

| Name | What it does |
|---|---|
| `mapv` `filterv` | `map` and `filter`, whose results are already vectors |
| `keep` `keep-indexed` | The results of `(f x)` (or `(f i x)`) that aren't nil |
| `take-while` `drop-while` `split-with` | The items before the first one a test fails, the items from that one on, and both |
| `partition-by` | Splits the items each time `(f item)` changes |
| `reductions` | The steps of `reduce`, as a vector |
| `not-any?` `not-every?` | `(not (some f xs))` and `(not (every? f xs))` |
| `partial` `comp` `juxt` `complement` `constantly` | Make functions out of functions |
| `fnil` | A function that puts defaults in place of nil arguments |
| `every-pred` `some-fn` | Combine tests: all of them hold, or the first true result |
| `max-key` `min-key` | The argument for which `(f x)` is largest or smallest |
| `run!` | Calls a function on each item for its effect, and returns nil |

The core functions are wider too:

- `map` and `mapcat` take several collections and stop at the shortest: `(map + [1 2 3] [10 20])` is `[11 22]`.
- `reduce` of an empty collection with no start calls `(f)`: `(reduce + [])` is `0`.
- `conj` onto a map takes `[key value]` pairs, maps and nil.
- `sort` and `sort-by` take a comparator: a function that returns a boolean (whether the first comes before the second)
  or a number (as `compare` does). The sort is stable. `<`, `>` and `compare` sort natively; any other comparator is
  run by a merge sort written in the language.

## Different from Clojure

- **Nothing is lazy.** Every function returns a whole vector, never a lazy sequence, and there are no transducers:
  `(into [] xf coll)` is an error that says so. `doall` and `dorun` are there only so that Clojure code still runs.
- **`seq` doesn't copy.** A list or a vector, a view of JSON included, comes back as it is, so `(seq [1 2])` prints
  as `[1 2]`, not `(1 2)`.
- **A keyword is the string of its text (decision D2).** `(= :a "a")` is true in functions, in `distinct`, in
  `frequencies` and in the bindings a template's `~form` lowers to. Map keys are text in JSON, and a keyword is how
  the language writes one.
- **A map's key is text (decision D3).** A function that builds a map refuses a key that isn't a keyword or a string,
  and its message names the fix: `(frequencies [1 1 2])` says `a map's key is a keyword or a string, not a number:
  give the key as text, (frequencies (map str xs))`. `(group-by even? xs)` is refused the same way; write
  `(group-by (comp str even?) xs)`.
- **No sets.** `(set xs)` gives a hint: a vector of distinct items stands for a set, as in JSON. `set`, `empty`,
  `time`, `type`, `find` and `repeat` keep the meanings they already had in the language (decision D5).
- **`compare` and `sort`** order numbers with numbers and text with text, with nil first; booleans and mixed kinds
  are an error, not an order. `<` and `>` used as comparators take numbers only.
- **`int`** is an error beyond an integer's range, instead of wrapping around.
- **`fnil`** takes any number of defaults, not only up to three.
