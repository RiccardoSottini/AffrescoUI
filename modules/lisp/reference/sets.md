# Sets

A set holds distinct items: `#{1 2 3}`. It is the one value of the language that JSON hasn't, so it leaves the
language as a list. What it has means what it means in Clojure, with the three differences at the end.

Where the code is: the value in `modules/lisp/src/Objects.hpp` and `Value.cpp`, its hash and equality in `Tree.cpp`,
the literal in `ReaderForms.cpp`, the functions in `CoreSets.cpp`, `CoreCollections.cpp` and `CoreSequences.cpp`; the
tests in `tests/unit/lisp_clojure_sets.cpp`.

## What a set is

- **Membership is by `=`.** A keyword is the string of its text (decision D2), so `(contains? #{:a :b} "a")` is true,
  and `1` and `1.0` are one item. A set keeps each member as it was first added: `(get #{:a} "a")` is `:a`.
- **Finding a member doesn't walk the set.** A set keeps a hash index beside its items, so membership in a set of
  100,000 items costs about what it costs in a set of ten.
- **`=` of two sets** is the same members in any order: `(= #{1 2} #{2 1})` is true. A set is never equal to a vector
  or a list, even with the same items.
- **Nothing changes a set.** `conj` and `disj` give new ones; adding at the end of a set nothing else has added to
  shares its storage, so a loop of `conj` is linear.
- **What it costs.** Each item spends from the budget for allocation, and each walk (`=`, printing, building a tree)
  spends per item and stops at the depth a value may have.

## Making one

| Written | Gives |
|---|---|
| `#{1 2 3}` | the set; the same item twice is an error at the second (`reader.md`) |
| `(hash-set 1 2 1)` | `#{1 2}`: each item once, where it first comes |
| `(into #{} xs)` | the set of a collection's items |
| `(conj s x y)` | `s` with those that aren't members, at its end |
| `(disj s x y)` | `s` without them, its other members in their order; `(disj nil x)` is nil |

## Using one

`count`, `empty?`, `not-empty`, `seq`, `first`, `rest`, `map`, `filter`, `reduce` and every function that takes a
collection take a set, its members in their order. Those that give a sequence give a vector: `(vec s)`, `(sort s)`,
`(distinct s)`, `(map inc s)`.

| Written | Gives |
|---|---|
| `(contains? s x)` | whether `x` is a member |
| `(get s x)`, `(get s x default)` | the member equal to `x`, or nil (or the default) |
| `(s x)` | the same as `(get s x)`: `(some #{2} [1 2 3])` is `2`, `(filter #{:a} xs)` keeps the `:a`s |
| `(set? x)` | whether `x` is a set |

A vector is a function too, of an index: `([10 20] 1)` is `20`, and an index outside it is an error.

## In a binding

A `~form` that reads the running app writes a set's test of membership as an expression when every member is a
constant (nil, a number, a text, a keyword or a boolean): `(contains? #{:a :b} state.kind)` is
`{{contains(['a', 'b'], state.kind)}}`, and so is `(#{:a :b} state.kind)` where only its truth is read, as in
`(label :when ~(#{:text :shape} state.kind))`. Called for its value it gives `state.kind` or null; `filter`, `remove`,
`every?`, and `some` tested, take such a set as their function. A set with any other member stays a call of the
function, as before. `bindings.md` has every row ("A set's membership test") and what stays a call ("Left out").

## `clojure.set`, written `set/`

| Function | Gives |
|---|---|
| `(set/union s1 s2 ...)` | the first's members, then each other's that aren't members yet |
| `(set/intersection s1 s2 ...)` | the first's members that every other has |
| `(set/difference s1 s2 ...)` | the first's members that no other has |
| `(set/subset? a b)`, `(set/superset? a b)` | whether every member of `a` is one of `b`, or the other way |
| `(set/select pred s)` | the members `pred` is true for |
| `(set/rename-keys m {:old :new})` | the map with those keys renamed; a new key the map has takes the old one's value |
| `(set/map-invert m)` | a map from each value to its key; the values must be text, as every map key is |

These take vectors too, since a set that comes back from an app's state is a vector, and give sets in the order of
their first argument.

## Three differences from Clojure

1. **A set leaves the language as a list.** In a template, a binding or a state, `#{3 1 2}` is the JSON list
   `[3, 1, 2]`; nothing that comes back from JSON is a set, so a state that held one is read back as a vector. Write
   `(into #{} state.tags)` to have a set again.
2. **A set keeps the order its members were added in.** `(vec (conj #{3 1} 2))` is `[3 1 2]` and prints `#{3 1 2}`,
   so the same program builds the same tree every time. Clojure's order is the hash's.
3. **`set` is the host's word**, for a state's value (decision D5), not a function that makes a set. Write
   `(into #{} xs)` or `(hash-set x y)`; `(set xs)` gives a hint saying so.
