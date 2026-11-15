# RFC 0006: The language as a subset of Clojure

- **Issue:** none yet
- **Author:** the maintainer
- **Changes:** the modules (`lisp`, and `notation`'s lexer and word lists); no change to the screen format, the live
  protocol or the security model
- **Builds on:** RFC 0005 (Lisp as a programming language)

## The problem

The language of RFC 0005 has Clojure's syntax and a small vocabulary of its own. People and agents who write it reach for
what they know: `when-let`, `->`, `str/join`, `#(...)`, `into`, `frequencies`. Today each of those is an error that says
only that the name is not defined, and the writer has to guess what the language has instead. An agent that knows Clojure
well spends its turns rediscovering a smaller language.

The aim is a documented, sandboxed subset of Clojure: code that stays within the subset means what it means in Clojure,
code that leaves it gets an error that says what to write instead, and how much of a set of everyday programs runs
unchanged is measured rather than claimed.

Out of scope: mutable state, laziness, host interop, regular expressions, Clojure's numeric tower and its namespaces as
files. Section "Intended differences" says why for each.

This RFC describes the seams the additions go through. The additions themselves (the everyday forms, the further
functions, the text and set functions, the reader forms, `ns`) come one package at a time, each with its own tests and
page of the reference (`modules/lisp/reference/`).

## The design

### Three kinds of addition

Every addition to the language is one of three kinds, and each has one place to go:

1. **A library function or macro**, written in the language: a row of the library's table (`LibraryEntry` in
   `modules/lisp/src/Library.hpp`), in the file of its area (`PreludeForms.cpp` for forms, `PreludeFunctions.cpp`,
   `CoreSequences.cpp`, `CoreStrings.cpp`, `CoreSets.cpp`). Most additions are this kind: they run on the machine's own
   stack, within the budget, and need no review of native code.
2. **A native function**, where speed or a primitive operation needs one: registered in the same file with the machine's
   `defineNative`, as the language's own functions are today.
3. **A reader rewrite**, for syntax: the lexer gives the form a token of its own (`#(`, `#{`, `#?(`, `#?@(`), and the
   reader turns it into ordinary forms (`modules/lisp/src/ReaderForms.cpp`). Nothing after the reader knows the syntax
   existed.

A word an addition makes plain goes on its area's list (`modules/notation/include/affresco/notation/words/`), so that in a
data position of a language file it isn't read as a node type. The lists are split by area so that packages working in
parallel never edit the same line.

### The library

An entry is registered when an interpreter is made and compiled the first time something uses it, so making an
interpreter costs the same however large the library grows. Its code sees the language's own functions, never a
program's of the same name: a program defining `count` doesn't change `last`. A program defining a name of the library
has its own where it calls it.

A library macro is unhygienic, as a program's macro is: its expansion calls functions by name. Its entry lists those
names (`uses`), and a call where a local hides one of them, or where the program defines one, is an error that names both:

```
"when-not" uses the language's "not", which a local hides here: rename the local
```

### Qualified names

A word with a `/` inside it names a namespace's function. The language has three namespaces: `clojure.core`, whose
functions are the bare names (`clojure.core/inc` is `inc`), `clojure.string` (alias `str/`, also `string/`) and
`clojure.set` (alias `set/`). A qualified word is never a local. An unknown prefix is an error that lists the namespaces.
A host adds its own namespaces and aliases (`Interpreter::addNamespace`, `addAlias`). A file's `ns` form gives it aliases
of its own (`modules/lisp/reference/namespaces.md`).

### Hints

A name the language doesn't have, but another dialect does, says what to write here: `(atom 1)` gives
`"atom" is not defined: the language has no mutable state: a program keeps it in the app's state (defstate), and a step
changes it`. A hint names names, never a value.

### Intended differences

These stay, on purpose, and the reference's `differences.md` lists them for writers:

- **No mutation.** No atoms, refs, vars or volatiles: a program's state is the app's state, changed by steps, so that every
  change is a reviewable revision.
- **No laziness.** Sequence functions give vectors; there is no `iterate`, `cycle` or `lazy-seq`. Everything a program
  makes ends, within the budget.
- **No host interop.** No `.method`, no classes, no `import`: the only way out is the primitives a host registers.
- **Text keys.** A map's keys are text, as JSON's are: a keyword key and a string key of the same name are the same key,
  and a key that isn't text is refused.
- **JSON's numbers.** Integers and doubles, as JSON has; an integer that overflows goes on as a double. No ratios, no big
  numbers.
- **Flat names.** Namespaces name functions, and a program is one flat space of names. As built, a file's `ns` form
  labels the file, gives it aliases and names the files it needs, which the loader reads as `include`
  (`modules/lisp/reference/namespaces.md`).
- **Sets.** As built, a set is a value of the language (`#{...}`, `hash-set`, `set/`): it leaves as a JSON list, its
  items in the order added, and comes back as a vector (`modules/lisp/reference/sets.md`).
- **The host's words.** `set`, `empty`, `time`, `type`, `find` and `repeat` keep the host's meaning (a step, an
  expression's function, a view's repeat) and are listed in `modules/lisp/reference/differences.md`.

## The alternatives

- **Embed a full Clojure.** It would bring mutation, laziness and interop, which the sandbox would then have to take away
  one by one, and a runtime far larger than the module.
- **A language of our own vocabulary.** That is today's state: every writer who knows Clojure pays for it on every
  program.
- **Additions as native code only.** Faster to write one at a time, but each would need native review, and a function
  that calls a function back would recurse on the native stack, outside the machine's limits.
- **One word list.** Simpler, but every package would edit the same lines of one file.

## The cost in tokens

None measured here: this RFC adds seams and no vocabulary. Each package that adds to the language measures what it
changes, and the overall score is measured once the packages are in, on a fixed set of programs.

## Performance

Making an interpreter registers the library's entries without reading or compiling them; an entry is compiled once,
when code that names it is first compiled. Measured with `affrescoui_bench`, Release, on one machine, the old and the
new build run one after the other, the lowest of three runs:

| Workload | Before | After |
|---|---|---|
| `lisp/start` (1,000 interpreters) | 96.3 µs | 18.0 µs |
| `lisp/event` | 1.17 µs | 1.18 µs |
| `lisp/binding` (lowest of five runs of 40) | 1.73 µs | 1.63 µs |
| `lisp/open-arcadia` | 449 ms | 445 ms |
| `lisp/reload-200` | 3,407 µs | 3,288 µs |

A qualified word is resolved once, when it is compiled; a bare word costs what it did.

## Security review

- **New input:** the lexer's four new tokens, only in language text, each followed by a bracket the existing reader reads
  with its existing limits. The reader refuses all three forms for now. `affrescoui_fuzz_lisp` and
  `affrescoui_fuzz_notation` cover both readers.
- **Capabilities:** none added. Library entries are compiled with the language's own author and see only the language's
  own functions; a host's namespace holds only primitives the host registers, under the same gate.
- **Revisions:** a program can't change the library: its definitions never reach an entry's code, and a library macro
  refuses a call where a program's definition would take the place of a function it uses.
- **Secrets:** errors name names and kinds, never values, as before.
- **Callbacks or timers:** none.

## Compatibility

Additive. No existing program changes meaning: every `.lisp` file under `examples/` and `tests/` reads and builds as it
did, which a unit test checks. A word with a `/` inside it, which no file uses outside strings and comments, is now a
qualified name. The public C++ API gains two functions on `Interpreter`; `notation::languageWords` still names the core
list.

## Open questions

- The set of everyday programs the score is measured on, and where it lives.
- Whether a file's own aliases (`ns`'s `:require ... :as`) may shadow the language's fixed ones.
