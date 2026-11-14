# The reader's forms

What the reader takes from Clojure, how it turns each into the language's ordinary forms, and what it refuses and what
to write instead. Nothing after the reader knows these forms: the compiler, the macros and `affrescoui fmt` see only
what they turn into. They are language-only. The notation's reader (`modules/notation`), which reads documents as data,
refuses each with an error naming its place.

Where the code is: the tokens in `modules/notation/src/Lexer.cpp`, which both readers share; the forms in
`modules/lisp/src/ReaderForms.cpp`; the tests in `tests/unit/lisp_clojure_reader.cpp`.

## Function literals: `#(...)`

`#(+ % 1)` reads as `(fn [%] (+ % 1))`, so it runs, prints and is quoted as that `fn` form.

| Written | Means |
|---|---|
| `%` or `%1` | the first parameter (the two are the same parameter) |
| `%2` to `%20` | the second to the twentieth |
| `%&` | the rest, as `& %&` |
| `%.title`, `%2.id` | a path into a parameter, as `x.title` is into `x` |

The function takes as many parameters as the highest one it names: `#(str %3)` takes three, and the first two go
unused, as in Clojure. A word that starts with `%` and isn't a number, such as `%map`, stays a word.

Refused, each with what to write instead:

- A `#(...)` inside another: "name the inner function's parameter with (fn [x] ...)".
- `%0`, or a parameter past `%20`.

## Reader conditionals: `#?(...)` and `#?@(...)`

A reader conditional holds pairs of a platform's keyword and a form. The language is the platform `:affresco`, and
reads the first branch whose keyword is `:affresco` or `:default`, in the order written, as Clojure does:

```clojure
#?(:clj (slurp f) :cljs (fetch f) :affresco (read-state f) :default nil)   ; reads (read-state f)
```

Every branch is read, so a mistake in one that isn't chosen is still an error. A conditional that chooses no branch
reads as nothing: in a list or a vector it leaves no item, and at the top level of a file no form. `#?@(...)` splices
the items of the chosen vector into the list or the vector around it: `[1 #?@(:affresco [2 3]) 4]` reads as
`[1 2 3 4]`. Used where there is nothing to splice into, or choosing something other than a vector, it is an error.

## Discarded forms: `#_`

`#_` drops the form after it, as in Clojure and in the notation: `[1 #_ 2 3]` reads as `[1 3]`, and `#_ #_ a b` drops
both. `affrescoui fmt` keeps a discarded form where it was.

## Metadata: `^`

`^:private`, `^String` and `^{:doc "..."}` are read and dropped, with the form after them kept: the language has no
metadata. A `^` with no form after it is an error.

## Characters: `\a`

A character reads as a string of that one character, the nearest JSON value.

| Written | Reads as |
|---|---|
| `\a`, `\é` | `"a"`, `"é"` (any one UTF-8 character) |
| `\newline`, `\space`, `\tab`, `\return`, `\formfeed`, `\backspace` | that character |
| `A` | `"A"`; half of a surrogate pair is refused, as JSON refuses it |

## Numbers

Clojure's ways to write a number read as the nearest JSON number. The language's numbers are JSON's, so a big integer
or a big decimal is only as wide as JSON's.

| Written | Reads as |
|---|---|
| `12N` | `12`: a whole number up to 64 bits (unsigned up to 18446744073709551615); a wider one is an error |
| `1.5M` | `1.5` |
| `1/2`, `4/2` | `0.5`, `2`: a ratio is divided as it is read; `1/0` is an error |
| `0x1F`, `-0xff` | `31`, `-255` |

## Refused, with what to write instead

| Written | Error says |
|---|---|
| `#{1 2}` | a set literal isn't in the language: write a vector (`set/` works on vectors of distinct items) |
| `#'f` | there are no vars: name the function itself |
| `##Inf`, `##NaN` | JSON has no infinity or not-a-number |
| `::name`, `:a/b` | a keyword has no namespace here: write `:name`, or `:"a/b"` |

## Portable documents

A screen written in the Lisp notation reads `column#rooms` as a node of type `column` with the id `rooms`. Clojure's
own tools read a `#` inside a word as part of the word, but a lone `#rooms` as a tagged literal: write the id after
the type, or as `:id "rooms"`, in a file other tools must read too.

## Not in the reader

What Clojure's reader also has and the language leaves out: syntax-quote's namespace qualification (`` `x `` stays
`x`), regular expressions `#"..."`, tagged literals such as `#inst` and `#uuid`, namespaced maps `#:ns{...}`, and `@x`
(there are no atoms). A `#` before a word or a string reads as a node's id, so these are the error "an id outside a
node"; `@x` is a name that isn't defined.
