# Strings: `clojure.string`, `format`, errors as values and parsing

The language's text functions under the names Clojure gives them. The `clojure.string` functions live in
`src/CoreStrings.cpp`; `format`, `ex-info`, `ex-message`, `ex-data` and the `parse-` functions are plain names, in
`src/CoreText.cpp`, and listed in `modules/notation/include/affresco/notation/words/Text.hpp`.

Positions and lengths are in characters, as `count` and `subs` have them: a character is a whole UTF-8 sequence, so
`(str/index-of "é→x" "x")` is `2`, and no function cuts a character in two.

## Three spellings

`clojure.string` is a namespace of the language with two aliases, `str` and `string`. Each of its functions answers to
all three spellings, and they are the same function:

```clojure
(str/trim "  a ")             ; "a"
(string/trim "  a ")          ; "a"
(clojure.string/trim "  a ")  ; "a"
```

`str/join`, `str/upper-case` and `str/lower-case` are the language's bare `join`, `upper-case` and `lower-case`.

## `clojure.string`

| Name | Gives |
|---|---|
| `blank?` | true for nil, `""` or only spaces, tabs and line breaks (`\n`, `\r`) |
| `capitalize` | the first character upper case, the rest lower |
| `starts-with?`, `ends-with?`, `includes?` | `(f text part)` |
| `index-of`, `last-index-of` | `(f text part from?)`: a position in characters, or nil. `index-of` looks from `from` on (a negative one is 0); `last-index-of` finds the last place that starts at `from` or before |
| `join` | `(join coll)` and `(join separator coll)`; each item as `str` shows it, nil as nothing |
| `lower-case`, `upper-case` | the language's own (below) |
| `replace`, `replace-first` | `(f text match replacement)`, `match` a string: every one (left to right, never overlapping), or the first. An empty match is before every character and at the end, as in Clojure |
| `reverse` | the text reversed, characters kept whole |
| `split` | `(split text separator)` and `(split text separator limit)`, `separator` a string. Trailing empty strings are dropped, as in Clojure; a limit above 0 gives at most that many pieces and keeps them all, below 0 keeps every piece. An empty separator gives the characters; an empty text is `[""]` |
| `split-lines` | at `\n` or `\r\n`; empty lines at the end are dropped, as `split` drops them |
| `trim`, `triml`, `trimr` | without spaces, tabs and line breaks around, before or after |
| `trim-newline` | without the `\n` and `\r` at its end |

Every function takes a string where Clojure takes one: nil is an error, but for `blank?` (true) and `join`'s
collection (`""`). The error names the function and what it takes: `"str/trim" takes a string`.

**Case.** `upper-case`, `lower-case` and `capitalize` change ASCII letters only: `(str/upper-case "été")` is `"éTé"`.
Every other character stays as it is, so the text stays valid UTF-8 and the same length.

**Cost.** `includes?`, `index-of`, `last-index-of`, `replace` and `split` find their part in time linear in the text
and the part together, whatever they are; a result is charged to the budget as it grows, so a long replacement at many
matches ends with the budget's error, not a long wait.

## No regular expressions, and what to write instead

The language has no regular expressions (`re-find`, `re-matches` and the rest are not defined, with a hint). In this
language `#"..."` reads as a node's id (`:id "..."`), so `split`, `replace` and `replace-first` refuse the keyword
`:id` where the match should be:

```clojure
(str/split "a b" #" ")
; a regular expression (#"...") isn't in this language: give the text to match, as a string
```

Write the text to match as a string. What a pattern would find takes a few plain steps:

| With a regular expression | Instead |
|---|---|
| `(str/split s #",")` | `(str/split s ",")` |
| `(str/split s #"\s+")` | `(remove str/blank? (str/split (str/replace s "\t" " ") " "))` |
| `(str/replace s #"-" "+")` | `(str/replace s "-" "+")` |
| `(re-find #"^ab" s)` | `(str/starts-with? s "ab")` |
| `(str/split-lines s)` | the same: it splits at `\n` and `\r\n` |

## `format`

`(format pattern value...)`: `(format "%s has %d items (%.1f%%)" name n share)`.

| Directive | Shows |
|---|---|
| `%s` | the value as `str` shows it (nil as nothing); `%.Ns` its first N characters |
| `%d` | a whole number (`4.0` too, as an index takes one) |
| `%f`, `%.Nf` | a number with N digits after the point, 6 if none is given |
| `%x` | a whole number in base 16; a negative one as its 64-bit two's complement, as in Clojure |
| `%%` | `%` |
| `%n` | a line break |

Flags and a width go between the `%` and the letter: `%5d` pads on the left to 5 characters, `%-10s` on the right,
`%05d` with zeros after the sign (numbers only). A width or a precision above 1,000 is refused. Widths count
characters. Another directive is the error `format doesn't have %q: it has %s %d %f %x %%`; too few values, or a value
of the wrong kind, is an error that names the directive (`format's %d takes a whole number, given a string`). Values
past the last directive are ignored, as in Clojure. The result is charged to the budget.

## Errors as values

| Name | Gives |
|---|---|
| `ex-info` | `(ex-info message data)`: the map `{:message message :data data}`, `data` a map |
| `ex-message` | `:message` of a map, the text of a string that was thrown, else nil |
| `ex-data` | `:data` of a map, else nil |

`throw` takes any value and `catch` binds it; a condition the machine raises is `{:message "..."}`. So:

```clojure
(try (throw (ex-info "no" {:code 3})) (catch e (:code (ex-data e))))  ; 3
(try (/ 1 0) (catch e (ex-message e)))                                ; "dividing by zero"
```

## Parsing

| Name | Gives |
|---|---|
| `parse-long` | the whole number a text writes (`"-12"`, `"+3"`), or nil; nil past a 64-bit integer |
| `parse-double` | the number a text writes (`"1.5"`, `"-2e3"`, `".5"`, `"5."`, a suffix `d` or `f`, spaces around), or nil. `NaN`, `Infinity` and a number too large for a double are nil, as JSON has none of them; one too small is `0.0` |
| `parse-boolean` | true for `"true"`, false for `"false"`, else nil |

Anything but a string is an error, as in Clojure. A text longer than 40 characters is nil.

## Plain names that are the host's

An app's expressions have functions of their own named `split`, `trim`, `slice`, `indexOf`, `lower`, `upper` and
`reverse`, with the expression machine's meaning (`split(text, separator)`). In a program, a plain name the language
defines wins over an expression function of the same name, so the language never defines plain `split`, `trim`,
`replace`, `includes` or `blank`: an app that calls one keeps calling the host's. Clojure's are written with the
namespace: `str/split` is Clojure's `split`, `split` is the host's. The bare `reverse` is the language's sequence
function (`(reverse [1 2])`); the text one is `str/reverse`.
