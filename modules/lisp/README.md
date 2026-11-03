# lisp: the Lisp language

`lisp` is a programming language in Lisp syntax with its own runtime (RFC 0005, `rfcs/0005-lisp-language.md`): Clojure's
syntax, Emacs's vocabulary. It has functions, closures, macros and conditions, and builds the JSON tree the renderer
already understands. It has no way to the outside world but the primitives a host registers. It is neither Clojure nor
Emacs Lisp, and the library stays agnostic: the language holds no app feature. Target: `affrescoui::lisp` (uses `core` and
`notation`).

**What runs it:** a document holding a program: loading and every revision on it build it, purely (`Document.hpp`, and
VERSION_CONTROL.md, "Programs"), and `format::Programs` runs its functions in the app, from handlers and bindings, with every way out
checked by author (`modules/format/src/programs/`; SECURITY.md, "The Lisp language"; SCREEN_FORMAT.md, "Running"). The command line
(`affrescoui run`, `affrescoui repl`) and this module's own API run the rest.

```lisp
(defn card "One room as a node." [room]
  `(panel :id ~(str "room-" (:id room))
     (label :text ~(:name room))))

(defcommand toggle-side "Shows or hides the side bar." :title "Toggle Side" [] (toggle "state.side"))
(defkey "mod+b" toggle-side)
(defcustom max-rooms 60 "How many rooms are listed." :type integer)

(defscreen main :window {:title "Rooms"}
  (column#rooms ~@(map card [{:id "a" :name "Kitchen"} {:id "b" :name "Hall"}])))

(defstate current "a" "The room shown." :type (string))
(defview room-row "A room of the running app's list: `room` is the repeat's." [room]
  `(label :text ~room.name :bold ~(= room.id state.current)))   ; bindings: "{{room.name}}", "{{(room.id == state.current)}}"
(defscreen rooms (column ~(repeat [room state.rooms] (room-row room))))
```

## Public headers (`include/affresco/lisp/`)

| Header | Offers |
|---|---|
| `Value.hpp` | `Value`, `Kind`, `Place`: JSON's values plus symbols, keywords, lists and functions |
| `Interpreter.hpp` | `Interpreter` (`addAppVariable`, `eval`, `evaluate`, `expandsToDefinition`, `extensions`, `definitionNames`, `wrapGlobal`, `makeFunction`, `Sealed`, `addAuthorsThatRan`, `authorsOf`, `call`, `global`, `describe`, `definePrimitive`, `setGate`, `setPure`, `authorsOnStack`, `authorsThatRan`, `leastAuthority`, `authorOf`, `definitionNameOf`, `parametersOf`, `isProgramFunction`, `workOutDefinitions`, `viewJson`, `toTree`, `print`, `budget`, `setBudget`, `Transient`), `Limits`, `Budget`, `Primitive`, `Gate`, `CallContext`, `Refusal`, `LispError`, `DefinitionInfo` |
| `Program.hpp` | `Program` (`addFile`, `addEntry`, `addEntryUnread`, `entries`, `definePure`, `load`, `build`, `extensions`, `undefinedNames`, `truthTests`, `namesUsed`, `namedCalls`), `NamedCalls`, `CallsByName`, `Entry`, `splitEntries`, `isProgram`, `needsRunning`, `buildText`, `UndefinedName`; forms in a text a person has in hand: `formBefore`, `formAround`, `formsOf`, `FormAt` |
| `Document.hpp` | A document holding a program: `readDocument` and `jsonText` (what a loader goes through), `sourceDocument`, `programOf`, `builtProgram`, `buildDocument`, `checkProgramShape`, `replaceKeyword`, `keywordValueText`, and their limits; `findKeyword` and `withKeyword`, a keyword's value in any text of Lisp, read and replaced by tokens |

## Values

`nil`, booleans, numbers, strings, vectors and maps **are JSON's**: an integer stays an integer, a key's order stays, and a
number read keeps the exact kind `parseUntrusted` gives. A map's keys are strings, and a keyword or a string name the same
key (`(get m :a)` is `(get m "a")`). JSON read through `Interpreter::viewJson` is viewed, not converted: a vector or a map
from it wraps its items as they are read. Besides those: **symbols** (a word in code; in data the string of the same text),
**keywords** (as a value in data, their text), **lists** (code; in data a node), **functions**. `nil` and `false` are false,
everything else is true. **A whole number that is a double** (`13.0`, from `(* 26 0.5)` or a JSON file that writes it so) keeps
its kind, as JSON's model says, and is printed `13.0` to be read back (`pr-str`, `printForm`); but it counts as the integer wherever an
index, a count or a position is taken (`get`, `nth`, `contains?`, `assoc`, `subs`, `range`, a vector pattern; up to 2^53), as
JavaScript does, where it was nil or an error; and `str` and `join` show it as an expression's text does, `13` (below 10^15; one inside a
vector or a map is written as JSON writes it). So text a function makes reads as the expression it replaces made it. Expressions themselves
don't change (`list[1.0]` there still leads nowhere). Nothing changes a value: `assoc` and `conj` give new ones (appending to a vector's end shares its
storage, so a loop of `conj` is linear).

## The language

Special forms: `quote`, backquote with `~` and `~@`, `if`, `do`, `when`, `cond` (`:else`), `and` and `or` (giving an operand),
`let`, `fn` (lexical closures; flat, as values never change), `loop` and `recur`, `try` and `catch` and `throw`. A call in tail
position replaces the function being run. Destructuring: `[a b & more :as all]`, `{:keys [a b] :or {a 1} :as m x :x}`. A
function has one list of parameters: there are no arities (`& rest` gives the rest as a vector). A dotted word reads a path: `room.name`
is the member of a local `room`, and with no such local and no definition of that name (`state.count`) it calls `%read`, which the language refuses
while building and a host gives while running.

Functions: `+ - * / mod rem inc dec min max abs < > <= >= = not= not truthy?`, `nil? some? boolean? number? string? symbol? keyword?
list? vector? map? fn? empty? contains?`, `first second last rest nth cons conj list vec concat reverse range sort sort-by
take drop count get get-in assoc assoc-in dissoc update update-in merge keys vals zipmap`, `map map-indexed mapcat filter
remove find some every? reduce apply identity`, `str pr-str join subs upper-case lower-case name keyword symbol gensym`. `map` and
the others give vectors, not lazy sequences. `map` takes one collection. `(:a m)` and `(m :a)` look a key up. Integers stay
integers until they overflow, then go on as doubles; dividing by zero is an error.

`truthy?` is an expression's truth (nil, false, 0, `""`, `[]` and `{}` false), for code that tests a value of the running app as a
binding would; everywhere else only nil and false are false. Strings may span lines: a line break or a tab typed inside the quotes is
that character (any other control character is refused, as is a backslash before a line break).

Macros are unhygienic, as in Emacs Lisp and without Clojure's namespaced symbols: use `gensym` for a macro's own variables.
In a template, `~~form` writes `~` and what the form gives (`~~@form`, `~@`): a macro's template writes a binding of the template it
gives, `` `(defcommand ~name "" :title ~~(list 't key) [] nil) ``.
A `gensym` is numbered per definition, so the same source gives the same names.

Definitions, top-level only, each with a docstring after the name (after the value in `defcustom` and `defstate`). A section's
member is addressed as `state.view`, `computed.card`, `styles.chip`, `components.chip`, `menus.card`, `tokens.colours`, and a
`defpart` or `defdefaults` as `part.name`, `defaults.name`, so they never meet a function's name:

| Form | Defines | Builds (`Program::build`) |
|---|---|---|
| `(def name "doc"? value)` | A global, worked out at the build or the load, purely, used or not | nothing |
| `(defn name "doc"? [params] body...)` | A function | nothing |
| `(defmacro name "doc"? [params] body...)` | A macro, load time only | nothing |
| `(defscreen name "doc"? :key value ... node)` | Nothing callable | `main`: `root`, and its keys at the top level; another name: `screens.name`. The whole is a template |
| `(defcommand name "doc"? :key value ... [params] body...)` | A function | `actions.name`: the keys (a template), `description` from the docstring, and `do: [{"lisp": name}]`, which runs the function |
| `(defkey "chord" "doc"? what)` | Nothing callable | `keys.chord`: a template, a word being an action's name |
| `(defcustom name value "doc"? :key value ...)` | A global: the value | `settings.name`: `type`, `default`, the other keys, `description` |
| `(defhook name "doc"?)` | A global: its own name, as text | Nothing: a hook point, for `run-hooks` and `describe-*` |
| `(defview name "doc"? [params] body...)` | A function the build calls: its templates hold bindings (below), and a template's call of it is never a binding | nothing |
| `(defcomponent name "doc"? {:param default ...} body...)` | Nothing callable | `components.name`: `note`, `params`, and the node the body gives, each parameter bound to its own name as a symbol |
| `(defstate name value "doc"? :type schema)` | Nothing callable | `state.name` (a template), `stateTypes.name`; a document's own value of the key wins, as with `defvar` |
| `(defcomputed name "doc"? form)` | Nothing callable | `computed.name`: the form as a `~form` in a template, a binding when it needs the app |
| `(defstyle name "doc"? value)`, `(defmenu ...)`, `(deftokens ...)` | Nothing callable | `styles.name`, `menus.name` (templates); `deftokens` its map's members, in `tokens` |
| `(defpart name "doc"? :section value ...)` | Nothing callable | Each section's members; a list joins, a value is given once |
| `(defdefaults name "doc"? :section {...} ...)` | Nothing callable | Members no other definition gives, which a document's own value replaces |
| `(add-hook hook "doc"? function)`, `(remove-hook hook "doc"? function)`, `(advice target "doc"? :before/:after/:around function)` | Nothing the language runs: a host does | Nothing: recorded in `Program::extensions()`. The build refuses one whose function the program doesn't define |

## Programs

A file is a **program** when it has several top-level forms, or one that is a definition (`isProgram`); otherwise it holds
one data value, a **template**.

- **Definitions, not statements.** Every top-level form is a definition with a stable address (`Entry::name`): its name, or
  `defkey ctrl+k` and `add-hook after-save f` for those with none. A top-level call of a macro counts, named by its second
  item, and may expand to a definition or a `do` of them. Two definitions of one name are an error naming both places.
- **Order doesn't matter.** Macros are defined first, then the rest; a `def` is worked out when first read, then all that are left
  (a build reads them all, so a value that can't be worked out refuses the build), wherever it is, so
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
  which is a node type: `(list ...)` in data stays a node. The notation's own reader applies this rule too. A data file an app loads
  goes through that reader, or `buildText` when it holds a `~` (`readDocument`).
- **A `~form` in a top-level template that needs the running app** stays in the tree as a binding (`CompilerBindings.cpp`): one that names a
  path (a dotted word no definition has), what nothing defines, or a function reaching either, or a host's primitive that isn't pure. Those
  the expression machine has (`not = not= < > <= >= + - * / mod if and or`) become its text, an undefined head becomes an expression's function, and a
  function of the program or the language becomes `lisp('name', ...)`. What it holds that needs no app is worked out at the build. The rest of
  a template, and a backquote in a function's body, run at once, as ever.
- **Views.** A `defview`'s body, a `defcomponent`'s, and a top-level template's code build a view: each backquote in them is a template
  whose `~form` may become a binding (`CompilerBindings.cpp`, `CompilerLowering.cpp`). A local holding a **symbol** is a name of the
  running app (`repeat` binds its item and index so, a `defcomponent` its parameters, and `'menus.card` is one): a `~form` that uses
  locals is compiled twice, as the binding that writes each local (a symbol as its path, a value as a literal) and as code, and
  `%symbolic?` chooses when it runs. `(repeat [x list :index i :key form] body)` adds `repeat` to the node its body gives, after its
  type and id (`%repeat`); `(handler f args...)` gives the step `{"lisp": "f", "with": [...]}`, an argument that reads the app its
  binding. A word that names a function (of the program, the language or a host), uncalled, is a name of the running app in a binding,
  unless a local has it, or a function of the language that takes functions is given it (`apply`, `map`, `filter`, `sort-by`...).
  `(str ...)` joined as a text writes what the build knows as text (`%text`; a text with a brace as a quoted literal), and keeps
  the spaces around a lone value as placeholders of their own (`keptSpaces`): `"  {{x}}"` would be x's value alone.
- **Lowering.** A binding's form is written as the expression machine's text when it can be: the operators, `if`, `when`, `cond`,
  `not`, unary `-`, `or-else` (`??`), the language's functions the expressions have (`count` as `length`, `vals` as `values`, `join`
  with its arguments swapped, `get`, `get-in`, a keyword's call, `first`, `inc`, `nil?`, `str` as `+` or, whole, as the text
  `a{{x}}`), and the functions on each item given `(fn [x i] body)` (`filter`, `remove`, `map`, `map-indexed`, `find`, `some`,
  `every?`, `sort-by` with `>` for descending, `reduce` with `[acc x]`), their parameters written `it`, `index` and `acc`; an inner
  function's item hides an outer one's, an error if it is used. An expression's own function may take `(fn [it] ...)` too. A macro's
  call is expanded first, once a call (`Compiler::expanded`), for the liveness check, the binding and the code alike.
- **What is noted.** `Program::truthTests()` lists where a function tests a path of the running app with the language's truth
  (`(if state.query ...)`), and gives `+` a text written out: `affrescoui check --truth` prints them.
- **Read once.** Reading is pure, so the process keeps each Lisp file's document by its text, file and name (`readDocument`, 64 MiB
  at most) and each entry's form by its text, file and place (`FormCache.cpp`, 32 MiB at most; a form is copied into the heap that
  asks, never shared); `isProgram` decides by tokens, so a data file is read once, by the notation's reader. `Program::load(interpreter,
  false)` compiles functions and macros only, the rest when called: what a host that runs a program, and never builds it, needs.
- **`include`.** `readDocument` takes a program file's top-level `(include "a.lisp" ...)` forms out of its text (each character a
  space but line ends, so places stay) into the document's `include`: the loader reads them as a data file's, with its checks. They
  are never entries, so a revision can't add one: `definitionAddress` refuses the form.
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
  `Value` is a plain 16-byte copy.
- **A region gives back what an event made** (`Interpreter::Transient`, `Heap::mark`, `release`): objects are freed down to where it
  started. Nothing lasting points into it: a `def`'s value is worked out before any event, compiled code too (either one pins the region, for safety,
  rather than leave a dangling value), `conj` copies a vector older than the region rather than add to its storage, and a host keeps only
  JSON and names across events. A value made inside isn't read after it.
- The walks over a whole value (making a tree, printing, `=`) spend from the budget and stop at a depth (`maxValueDepth`,
  `maxNesting`). The native recursion that remains is the reader's (`maxNesting`), the compiler's (`maxCompileDepth`, macro output
  included) and runs inside runs (`maxNestedRuns`: macro expansion and a primitive calling back).
- Those depths are set against an 8 MiB main-thread stack, as on macOS and Linux. On Windows, a program that links `lisp` gets the
  same (`/STACK` with MSVC, `--stack` with MinGW), passed on through the target and the installed package.

## Limits

| Limit | Default | Exceeded |
|---|---|---|
| `Budget::operations`, each instruction, each item a function walks, each 64 bytes of a text compared or counted, each 16 members of a map viewed from JSON searched, and each 8 locals a name is looked for among | 10,000,000 | `LispError` with `exhausted()` |
| `Budget::bytes`, each object, string and item made | 64 MiB | the same |
| `Limits::callDepth`, calls waiting to return | 20,000 | the same |
| `Limits::macroOperations`, macro expansion's own operations | 1,000,000 | the same |
| Stack values | 4,194,304 | the same |
| Reader: text, nesting, numbers | 64 MiB, 256 levels, 40 characters | `LispError` at the place |

A limit running out is never caught by a `try`, and every limit names the place. `format`'s `WorkBudget` isn't a dependency here:
`Budget` has its `operations` and `bytes` meaning, and `format::Programs` copies them in with `setBudget` and out with `budget()` around a call
and around each primitive.

## Errors

`LispError` is a `DocumentError` with `file()`, `line()`, `column()`, `definition()`, `message()` and `exhausted()`:
`AffrescoUI lisp: killring.lisp: line 3, column 8: in kill-line: "first" takes something with items, and was given a number`.
A message says names and kinds, never a value: a value may come from a masked path. A `throw` that nothing catches shows its
message only if the form wrote it as a literal string; `catch` still gets the value. A condition the machine raises is
`{:message "..."}` in `catch`.

## The way out, and who is running

**The host.** The language has no file, process, network, foreign-function or host-eval primitive. A host adds them:

```cpp
interpreter.definePrimitive("kill", {.capability = "control", .pure = false,
    .call = [](lisp::Interpreter& in, std::span<const lisp::Value> args) { ... return lisp::Value(); }});
interpreter.setGate([&](const lisp::CallContext& call) {
    if(call.capability == "control" && !allowed(call.ran)) throw lisp::Refusal("...");
});
```

- **`Primitive::reserved`**: no definition of a program may take its name (the expressions' functions, `now`): every file's code,
  a shared add-on's, means the host's by it. **`addAppVariable(name)`**: a bare word that names a variable of the running app
  (`preview`, which a step's name would hide) reads it through `%read`, where no local or definition has it and it isn't called.
- **The one choke point** is `Machine::callHost`: every call of a primitive passes through it, however it got there: a direct
  call, `apply`, a value in a variable, a hook. It refuses what isn't `pure` while `setPure(true)`, then calls the gate, then the
  primitive. A primitive or the gate throws `Refusal`, a condition the program may catch; any other exception leaves as it is.
- **`Primitive::capability`** is free text for the gate: this module doesn't interpret it.
- **`CallContext::named`** is the primitive's first argument when the code calling it names it with that text written out
  (`(invoke "files.read")`), and empty otherwise: through `apply`, a value in a variable, a computed first argument, or the
  language's own functions. `Program::namedCalls()` lists, per definition, every such call and which of those names it
  also uses another way, so a host can judge such code by what its text says (a `run` by what the name it writes runs).
- **`CallContext::ran` starts with the outermost run, or the outermost `Interpreter::Transient` when a host has one:** what a host adds with `addAuthorsThatRan` stays on the record, whatever the stack holds then.
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
- `CallContext::authors` (`authorsOnStack()`) is the top frame's set. **`CallContext::ran` (`authorsThatRan()`) is what a host should weigh:**
  every author whose code has run since the outermost call began, those that returned too, since what such code returned (a function, a
  name) is in what runs now. `leastAuthority(rank)` picks the lowest of the stack by the host's ranking.

**`format::Programs` is the host in an app** (`modules/format/src/programs/`): the rules are in SECURITY.md, "The Lisp language". For the next
stages:

- **Hooks and advice** (`ProgramExtensions.cpp`). `Interpreter::extensions()` lists them. Advice wraps the function's global (`wrapGlobal`), so it applies however the function is called; the wrapper stands for it (`parametersOf`, `definitionNameOf`). A hook, `:before` and
  `:after` run sealed (`Interpreter::Sealed`: `authorsThatRan()` goes back to what it was when one ends), with the authors of whoever added it added (`addAuthorsThatRan`). A hook belongs to its entry, so it goes with a new interpreter when the entry goes.
- **`evaluate`** (`ProgramEvaluate.cpp`): `formsOf` splits the text. If every form is one run of what the person typed (`TypedText.hpp`), a definition goes in a revision by the author `user`, through `VersionControl::commitOwnInput()` (the step checked the person's own press), and anything else through `Interpreter::evaluate`, which compiles with purity on and runs as that author, the authors of any macro it holds with it. If not, its definitions are a proposal in the name `plugin:evaluated`, and anything else doesn't run; or, with `confirm`, the window asks the person (`Window::confirm()`, a question it draws itself), and their yes runs exactly the text shown as if they had typed it, once.
- **`describe-*`** read `Interpreter::describe` (docstring, author, place), and give no value.

## Files

```
include/affresco/lisp/  the public headers above
src/
  Objects.hpp           what values point at, the heap, the instructions, compiled code
  Value.cpp             values and the heap
  Reader.hpp, .cpp      text into forms, on notation's Lexer
  Compiler.hpp, .cpp    forms into instructions: calls, variables, paths, functions, destructuring
  CompilerForms.cpp     the special forms and templates
  CompilerBindings.cpp  a template's ~form that needs the running app, written as a binding; views and symbols
  CompilerLowering.cpp  a binding's form as the expression machine's text
  Definitions.hpp, .cpp globals, the definition forms, authors
  Machine.hpp, .cpp     the stack, the instructions, the choke point
  CoreNumbers.cpp       the language's own functions on numbers and truth
  CoreCollections.cpp   on lists, vectors and maps
  CoreText.cpp          on text
  CoreViews.cpp         for views: handler, or-else, and repeat's, defcomponent's and a binding's own
  Prelude.cpp           those written in Lisp, which call a function back
  Tree.cpp              the walks: a value as a tree, as text, `=`
  TopLevel.hpp          what the interpreter and programs share
  Interpreter.cpp       the machine, seen from outside
  FormCache.cpp         an entry's form, read once for the process and copied
  Program.cpp           the split, loading, the build
  Forms.cpp             forms in a text being edited: before a position, around it, all of them
  Document.cpp          documents holding programs: reading them, what they build, the keyword edit
CMakeLists.txt          the affrescoui_lisp target
```
