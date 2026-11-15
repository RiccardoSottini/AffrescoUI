# What differs from Clojure

The language is a sandboxed subset of Clojure: what it has means what it means in Clojure, but where this page says
otherwise. Each row below is a choice, not a gap: why it is so, what to write instead, and how many of the Clojure test
suite's assertions it explains. The counts are those the conformance run marks out of scope with that reason
(`../CONFORMANCE.md`, at the suite's commit written there); a row the run doesn't mark says so. The gaps, the forms
the language means to have and doesn't yet, are in `../CONFORMANCE.md`, "Failing today", not here.

| Difference | Out of scope in the suite |
|---|---|
| [Nothing changes a value](#nothing-changes-a-value) | 294 (`mutation`) |
| [Nothing is lazy](#nothing-is-lazy) | 105 (`laziness`) |
| [No host interop, output, clock or randomness](#no-host-interop-output-clock-or-randomness) | 77 (`host`) |
| [A map's keys are text](#a-maps-keys-are-text) | 117 (`keys`) |
| [Numbers are JSON's](#numbers-are-jsons) | 494 (`numbers`) |
| [No characters](#no-characters) | 0 (`characters`) |
| [No regular expressions](#no-regular-expressions) | 51 (`regex`) |
| [No metadata, vars, protocols, records or multimethods](#no-metadata-vars-protocols-records-or-multimethods) | 11 (`metadata` 10, `vars` 1) |
| [One flat space of names per program](#one-flat-space-of-names-per-program) | 12 (`namespaces`) |
| [The host's words](#the-hosts-words) | not marked |
| [A dotted word is a path](#a-dotted-word-is-a-path) | not marked |
| [Library macros name nine functions by word](#library-macros-name-nine-functions-by-word) | not marked |

## Nothing changes a value

**Why.** A program's values are JSON's, and an app's state lives in the app, where every change is a step that version
control records and the permission rules judge. A value that changed in place would be a change no one reviewed.

**Instead.** `assoc`, `conj`, `update` and the rest give a new value. State that lasts is the app's: `defstate`, read as
`state.name`, changed by a step (`(set "state.name" v)`). Inside a function, `loop` and `recur` carry what changes.
There is no `atom`, `swap!`, `reset!`, `volatile!`, `transient` or `@x`, and no `memoize`: each name says what to write.

## Nothing is lazy

**Why.** Every value can be printed, compared and sent as JSON, and the work a function does is charged to its budget
when it is called, not later, when no one knows who asked for it.

**Instead.** Every sequence function gives a whole vector. A bounded `range`, `repeat` or `take` stands for an
infinite one; `loop` for `iterate`. There is no `lazy-seq`, `cycle`, `repeatedly`, `trampoline`, `realized?` or
transducer; `doall` and `dorun` are there so that Clojure code still runs.

## No host interop, output, clock or randomness

**Why.** The language reaches nothing but what its host registers, and in an app each such call is a step judged by
who wrote the code that runs it (SECURITY.md, "The Lisp language"). A build is pure: the same source gives the same
tree.

**Instead.** `(.method x)`, `Class/member`, `js/`, arrays, `instance?` and threads have no meaning here. Output is the
host's `log` step, time and randomness come from the app's state or a step a host grants. A `catch` catches every
error whatever class it names, `(catch Exception e ...)` as `(catch e ...)`, and the caught value is the condition
(`ex-message`, `ex-data`) or the value thrown.

## A map's keys are text

A keyword and a string of the same text are one value: `(= :a "a")` is true, `(get {:a 1} "a")` is `1`, and
`(keys {:a 1})` is `["a"]`.

**Why.** A JSON object's keys are text, and a keyword is how the language writes one, so a map is always a JSON object.

**Instead.** A function that builds a map refuses a key that isn't a keyword or a string, and its message names the
fix: `(frequencies [1 1 2])` says to give the key as text, `(frequencies (map str xs))`; `(group-by even? xs)` becomes
`(group-by (comp str even?) xs)`.

## Numbers are JSON's

**Why.** Numbers leave the language as JSON's: a whole number of 64 bits or a double.

**Instead.** `12N` reads as `12`, `1.5M` as `1.5`, `1/2` as `0.5`. A whole number that overflows goes on as a double,
`(= 1 1.0)` is true and `(str 1.0)` is `"1"`. `##Inf` and `##NaN` are refused, as JSON has neither. There is no
`bigint`, `bigdec`, `rationalize`, `numerator` or `denominator`.

## No characters

**Why.** JSON has no character: a character would leave as a string, and so would never be one again.

**Instead.** `\a` reads as the string `"a"`, and a string's items are strings of one character each: `(first "abc")`
is `"a"`. There is no `char` or `char?`. The run marks nothing with this reason: where a test needs a character's
kind, the function it calls isn't defined, and the test is not run.

## No regular expressions

**Why.** A pattern is a second language, to be sandboxed and budgeted as the first is; the text functions cover what a
screen needs.

**Instead.** The `str/` functions: `str/includes?`, `str/starts-with?`, `str/index-of`, `str/split` on a string,
`str/replace` with a string.

## No metadata, vars, protocols, records or multimethods

**Why.** A definition is its name, its docstring and its value, versioned as text. Anything attached to a value out of
sight, or a name that dispatches elsewhere, would be code no reviewer sees where it runs.

**Instead.** `^:private` and `^{:doc ...}` are read and dropped; write the docstring after the name. `#'f` is refused:
name the function. A map with a `:kind` key and a `case` or `cond` on it stands for a record and a multimethod.

## One flat space of names per program

**Why.** A program is one set of definitions, versioned and reviewed as one: a definition may use one written after it,
and two of one name are an error naming both places.

**Instead.** A qualified word names the same global (`clojure.core/inc` is `inc`; `str/join` is
`clojure.string/join`), and `declare` isn't needed. There are no namespaced keywords (`::k`, `:a/b`: write `:k` or
`:"a/b"`), no `*ns*` and no `in-ns`.

## The host's words

**Why.** `set`, `empty`, `time` and `type` had meanings in the language and in apps before Clojure's names came: `set`
is the step that changes the state, `(set "state.x" 1)`. `find` takes a predicate first, `(find pred xs)`, as the
language's own did, and `repeat` with a vector first, `(repeat [room state.rooms] node)`, is a view's repeat.

**Instead.** `(into #{} xs)` or `(hash-set x y)` for Clojure's `set`; `[]` or `{}` for `empty`; `(get m k)` and
`(contains? m k)` for Clojure's `find` on a map. `(repeat 2 x)` is Clojure's.

## A dotted word is a path

**Why.** The bindings an app shows read paths: `room.name` is the member `name` of a local `room`, and `state.count`
reads the running app.

**Instead.** A name of Clojure's with a dot in it (`clojure.string`) is written qualified, `str/join`. A local's member
is read as `room.name` or `(:name room)`, the same value.

## Library macros name nine functions by word

**Why.** A macro of the language's library is unhygienic, as a program's is: its expansion calls `not`, `=`, `nil?`,
`some?`, `empty?`, `first`, `map`, `filter` and `mapcat` by those words, so that a template's form can become a binding
the app's expressions run.

**Instead.** Don't name a local one of those nine where such a macro is used, and don't define one in a program that
uses such a macro: the error names the macro and the word, `"when-not" uses the language's "not", which this program
defines: name yours otherwise`.
