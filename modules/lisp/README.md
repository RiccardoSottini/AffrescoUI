# lisp: the Lisp language

`lisp` is a programming language in Lisp syntax with its own runtime (RFC 0005, `rfcs/0005-lisp-language.md`): Clojure's
syntax, Emacs's vocabulary. It has functions, closures, macros and conditions, and builds the JSON tree the renderer
already understands. It has no way to the outside world but the primitives a host registers. It is neither Clojure nor
Emacs Lisp, and the library stays agnostic: the language holds no app feature. Target: `affrescoui::lisp` (uses `core` and
`notation`).

**What runs it today:** a document holding a program: loading and every revision on it build it, purely (`Document.hpp`, and
VERSION_CONTROL.md, "Programs"). The command line (`affrescoui run`, `affrescoui repl`) and this module's own API run the rest.
Handlers don't call functions yet: that is RFC 0005's stage 3, and this README says what it builds on.

```lisp
(defn card "One room as a node." [room]
  `(panel :id ~(str "room-" (:id room))
     (label :text ~(:name room))))

(defcommand toggle-side "Shows or hides the side bar." :title "Toggle Side" [] nil)
(defkey "mod+b" toggle-side)
(defcustom max-rooms 60 "How many rooms are listed." :type integer)

(defscreen main :window {:title "Rooms"}
  (column#rooms ~@(map card [{:id "a" :name "Kitchen"} {:id "b" :name "Hall"}])))
```

## Public headers (`include/affresco/lisp/`)

| Header | Offers |
|---|---|
| `Value.hpp` | `Value`, `Kind`, `Place`: JSON's values plus symbols, keywords, lists and functions |
| `Interpreter.hpp` | `Interpreter` (`eval`, `call`, `global`, `describe`, `definePrimitive`, `setGate`, `setPure`, `authorsOnStack`, `leastAuthority`, `authorOf`, `viewJson`, `toTree`, `print`, `budget`, `setBudget`), `Limits`, `Budget`, `Primitive`, `Gate`, `CallContext`, `Refusal`, `LispError`, `DefinitionInfo` |
| `Program.hpp` | `Program` (`addFile`, `addEntry`, `entries`, `definePure`, `load`, `build`, `extensions`, `undefinedNames`), `Entry`, `splitEntries`, `isProgram`, `needsRunning`, `buildText`, `Extension`, `UndefinedName` |
| `Document.hpp` | A document holding a program: `readDocument` and `jsonText` (what a loader goes through), `sourceDocument`, `programOf`, `builtProgram`, `buildDocument`, `checkProgramShape`, `replaceKeyword`, `keywordValueText`, and their limits |

## Values

`nil`, booleans, numbers, strings, vectors and maps **are JSON's**: an integer stays an integer, a key's order stays, and a
number read keeps the exact kind `parseUntrusted` gives. A map's keys are strings, and a keyword or a string name the same
key (`(get m :a)` is `(get m "a")`). JSON read through `Interpreter::viewJson` is viewed, not converted: a vector or a map
from it wraps its items as they are read. Besides those: **symbols** (a word in code; in data the string of the same text),
**keywords** (as a value in data, their text), **lists** (code; in data a node), **functions**. `nil` and `false` are false,
everything else is true. Nothing changes a value: `assoc` and `conj` give new ones (appending to a vector's end shares its
storage, so a loop of `conj` is linear).

## The language

Special forms: `quote`, backquote with `~` and `~@`, `if`, `do`, `when`, `cond` (`:else`), `and` and `or` (giving an operand),
`let`, `fn` (lexical closures; flat, as values never change), `loop` and `recur`, `try` and `catch` and `throw`. A call in tail
position replaces the function being run. Destructuring: `[a b & more :as all]`, `{:keys [a b] :or {a 1} :as m x :x}`. A
function has one list of parameters: there are no arities (`& rest` gives the rest as a vector).

Functions: `+ - * / mod rem inc dec min max abs < > <= >= = not= not`, `nil? some? boolean? number? string? symbol? keyword?
list? vector? map? fn? empty? contains?`, `first second last rest nth cons conj list vec concat reverse range sort sort-by
take drop count get get-in assoc assoc-in dissoc update update-in merge keys vals zipmap`, `map map-indexed mapcat filter
remove find some every? reduce apply identity`, `str join subs upper-case lower-case name keyword symbol gensym`. `map` and
the others give vectors, not lazy sequences. `map` takes one collection. `(:a m)` and `(m :a)` look a key up. Integers stay
integers until they overflow, then go on as doubles; dividing by zero is an error.

Macros are unhygienic, as in Emacs Lisp and without Clojure's namespaced symbols: use `gensym` for a macro's own variables.
A `gensym` is numbered per definition, so the same source gives the same names.

Definitions, top-level only, each with a docstring after the name (after the value in `defcustom`):

| Form | Defines | Builds (`Program::build`) |
|---|---|---|
| `(def name "doc"? value)` | A global, worked out when first read | nothing |
| `(defn name "doc"? [params] body...)` | A function | nothing |
| `(defmacro name "doc"? [params] body...)` | A macro, load time only | nothing |
| `(defscreen name "doc"? :key value ... node)` | Nothing callable | `main`: `root`, and its keys at the top level; another name: `screens.name`. The whole is a template |
| `(defcommand name "doc"? :key value ... [params] body...)` | A function | `actions.name`: the keys (a template), `description` from the docstring, `do` empty for now (below) |
| `(defkey "chord" "doc"? what)` | Nothing callable | `keys.chord`: a template, a word being an action's name |
| `(defcustom name value "doc"? :key value ...)` | A global: the value | `settings.name`: `type`, `default`, the other keys, `description` |
| `(add-hook hook "doc"? function)`, `(advice target "doc"? how... function)` | Nothing yet | Nothing: recorded in `Program::extensions()` |

## Programs

A file is a **program** when it has several top-level forms, or one that is a definition (`isProgram`); otherwise it holds
one data value, a **template**.

- **Definitions, not statements.** Every top-level form is a definition with a stable address (`Entry::name`): its name, or
  `defkey ctrl+k` and `add-hook after-save f` for those with none. A top-level call of a macro counts, named by its second
  item, and may expand to a definition or a `do` of them. Two definitions of one name are an error naming both places.
- **Order doesn't matter.** Macros are defined first, then the rest; a `def` is worked out when first read, wherever it is, so
  a definition can use one below it; the sections built are sorted by name. Moving a definition changes nothing in the tree.
- **The split keeps the text.** `splitEntries` gives each form's exact text, from the first character after the previous
  form's line, comments above it included, to the end of its own line; comments after the last form are the last entry's. So
  every word and comment of a file is in exactly one entry. Stage 2 versions these. `Program::addEntry` takes one back, and
  refuses a text that isn't one form with that name.
- **The build is pure.** `Program::build` runs in a fresh interpreter, with `setPure(true)` and no primitives but those given
  with `definePure`. There is no state, clock, randomness or outside world in the language, and no hash order in what it
  builds: the same source gives the same tree. Function bodies are compiled at load, and run only if the build calls them.
  A name they use that nothing defines isn't an error until they run: `Program::undefinedNames()` lists them.
- **Templates.** `buildText` reads a data file as if backquoted: words stay data, `~` drops into code. Every existing `.lisp`
  file builds to the tree the notation's reader gives, key for key (`tests/unit/lisp.cpp`, and fuzzed).
  A word the language runs (`notation::languageWords`) isn't a node type in a data position: `(not x)` is the error
  `"not" is not a node type; to run it, write ~(not ...); for a node of that type, write (node :type not ...)`. So that no file
  breaks, the notation's writer never writes such a type as a head (`(node :type range ...)`), and that list leaves out `list`,
  which is a node type: `(list ...)` in data stays a node. The notation's own reader applies this rule too. **A data file an app
  loads goes through that reader, and `~` isn't read there**: `buildText` serves `affrescoui run --print`.
- **In a document.** `sourceDocument` makes a file's `{file: {name: text}}`, `programOf` takes it back. `buildDocument` gives what the
  rest of the library reads: what it builds, its other sections merged in member by member (a member both give must be the same, so
  building a built document gives itself), and `program` kept. `builtProgram` keeps a thread's last four builds per section, since a
  revision judges before and after, and a build under a document has tighter limits than running does (`maxBuildOperations`,
  `maxBuildBytes`), as every revision pays for one.
- **The keyword edit.** `replaceKeyword` finds an option's value by tokens, so every other byte stays: `(defcommand c "Doc."
  :title "x" [] nil)`'s `:title`, after the name and the docstring of `defcommand` and `defscreen`, after the value of `defcustom`, and
  inside a map through further keywords. It reads no body: options stop at the first thing that isn't a key. The new value must be
  exactly one form, and the result one definition of the same address.

## How it runs

The compiler turns forms into a small instruction set (`src/Objects.hpp`, `Op`), and the machine runs it (`src/Machine.cpp`, one
function per instruction). Calls and returns use the machine's own stack, so no recursion of the program touches the native one:

- Functions that call a function back (`map`, `reduce`...) are written in Lisp (`src/Prelude.cpp`). They see the language's own
  functions as constants, so a program redefining `count` doesn't change `reduce`. An error inside one names its caller's place.
- All objects live in one `Heap`, freed in one flat loop: a long list or a chain of closures takes no recursion to free. A
  `Value` is a plain 16-byte copy. **Nothing is freed before the interpreter goes**; stage 3 needs a mark and a release around
  an event, or a fresh heap for what an event allocates. What a global holds must survive that.
- The walks over a whole value (making a tree, printing, `=`) spend from the budget and stop at a depth (`maxValueDepth`,
  `maxNesting`). The native recursion that remains is the reader's (`maxNesting`), the compiler's (`maxCompileDepth`, macro output
  included) and runs inside runs (`maxNestedRuns`: macro expansion and a primitive calling back).

## Limits

| Limit | Default | Exceeded |
|---|---|---|
| `Budget::operations`, each instruction and each item a function walks | 10,000,000 | `LispError` with `exhausted()` |
| `Budget::bytes`, each object, string and item made | 64 MiB | the same |
| `Limits::callDepth`, calls waiting to return | 20,000 | the same |
| `Limits::macroOperations`, macro expansion's own operations | 1,000,000 | the same |
| Stack values | 4,194,304 | the same |
| Reader: text, nesting, numbers | 64 MiB, 256 levels, 40 characters | `LispError` at the place |

A limit running out is never caught by a `try`, and every limit names the place. `format`'s `WorkBudget` isn't a dependency here:
`Budget` has its `operations` and `bytes` meaning; stage 3 can copy them in with `setBudget` and out with `budget()` around a call.

## Errors

`LispError` is a `DocumentError` with `file()`, `line()`, `column()`, `definition()`, `message()` and `exhausted()`:
`AffrescoUI lisp: killring.lisp: line 3, column 8: in kill-line: "first" takes something with items, and was given a number`.
A message says names and kinds, never a value: a value may come from a masked path. A `throw` that nothing catches shows its
message only if the form wrote it as a literal string; `catch` still gets the value. A condition the machine raises is
`{:message "..."}` in `catch`.

## For the next stages: the way out and who is running

**The host.** The language has no file, process, network, foreign-function or host-eval primitive. A host adds them:

```cpp
interpreter.definePrimitive("kill", {.capability = "control", .pure = false,
    .call = [](lisp::Interpreter& in, std::span<const lisp::Value> args) { ... return lisp::Value(); }});
interpreter.setGate([&](const lisp::CallContext& call) {
    if(call.capability == "control" && !allowed(call.authors)) throw lisp::Refusal("...");
});
```

- **The one choke point** is `Machine::callHost`: every call of a primitive passes through it, however it got there: a direct
  call, `apply`, a value in a variable, a hook. It refuses what isn't `pure` while `setPure(true)`, then calls the gate, then the
  primitive. A primitive or the gate throws `Refusal`, a condition the program may catch; any other exception leaves as it is.
- **`Primitive::capability`** is free text for the gate: this module doesn't interpret it.
- A primitive's span lives only for the call. A primitive that wants to call a function back uses `Interpreter::call`, which
  nests a native run, up to 64 deep.

**Authors.** `Program::addFile(text, file, author)` and `Entry::author` say who wrote a definition; `Interpreter::eval(text, file,
author)` the same for what is evaluated.

- Every compiled function (`Code`) carries its definition's `author`, kept by its closures wherever they go
  (`Interpreter::authorOf`), and the set of `authors`: that author, plus the author of every macro whose output it holds, since
  an agent's macro writes the code of a person's function. A nested function's macros count for the function around it.
- Every stack frame carries the set of authors it runs under: its caller's plus its code's. A tail call adds to the frame's
  set rather than replacing it, so **a tail call can't launder authority**: an agent's function tail-calling a person's stays on
  the record until that frame returns. The language's own functions have no author, so `map` lends and takes nothing.
- `Interpreter::authorsOnStack()` gives the set of the top frame, and so of the whole stack, as the gate's `CallContext::authors`
  does; `leastAuthority(rank)` picks the lowest by the host's ranking. Nothing checks this yet: stage 3 wires its rules and
  attack tests into the gate.

**Other seams.** `defcommand` builds `do: []`, and the body is the function of that name: stage 3 connects the action to the
function. `Program::load(interpreter)` puts the definitions into a live interpreter, with its primitives and gate; a global is then
replaced by `eval`, and the next call uses it (a call looks its global up each time). `Interpreter::describe` reads a
definition's docstring, author and place (stage 4). `Program::extensions()` lists hooks and advice for stage 5.

**A live program, with its authors (stage 3).** `VersionControl::sourceDocument()["program"]` is the section and
`VersionControl::programAuthors()` who last wrote each entry, `"file"` for the app's own: `lisp::programOf(section, authors)` gives a
`Program` whose entries carry them, ready to `load` into the interpreter. A change reaches a live program through
`VersionObserver::documentChanged()`, after every commit, preview and undo: load the section again (a definition replaces what the
interpreter had of its name; one no longer there has to be taken out by the host).

## Files

```
include/affresco/lisp/  the public headers above
src/
  Objects.hpp           what values point at, the heap, the instructions, compiled code
  Value.cpp             values and the heap
  Reader.hpp, .cpp      text into forms, on notation's Lexer
  Compiler.hpp, .cpp    forms into instructions: calls, variables, functions, destructuring
  CompilerForms.cpp     the special forms and templates
  Definitions.hpp, .cpp globals, the definition forms, authors
  Machine.hpp, .cpp     the stack, the instructions, the choke point
  CoreNumbers.cpp       the language's own functions on numbers and truth
  CoreCollections.cpp   on lists, vectors and maps
  CoreText.cpp          on text
  Prelude.cpp           those written in Lisp, which call a function back
  Tree.cpp              the walks: a value as a tree, as text, `=`
  TopLevel.hpp          what the interpreter and programs share
  Interpreter.cpp       the machine, seen from outside
  Program.cpp           the split, loading, the build
  Document.cpp          documents holding programs: reading them, what they build, the keyword edit
CMakeLists.txt          the affrescoui_lisp target
```
