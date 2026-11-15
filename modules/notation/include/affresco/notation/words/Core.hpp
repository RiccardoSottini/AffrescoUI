#pragma once

/// @file
/// @brief The language's core written as plain words (Words.hpp): its special forms, its definitions and the
/// functions it has had from the start.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's core. `list` is also a node type and isn't here: in a data position it
/// stays a node, and running `list` there takes `~`. A test in tests/unit/lisp.cpp checks this
/// list and the others against the language.
// clang-format off
inline constexpr auto coreWords = std::to_array<std::string_view>({
    "abs", "add-hook", "advice", "and", "apply", "assoc", "assoc-in", "backquote", "catch", "concat", "cond", "conj",
    "cons", "count", "dec", "def", "defcommand", "defcomponent", "defcomputed", "defcustom", "defdefaults",
    "defhook", "defkey", "defmacro", "defmenu", "defn", "defpart", "defscreen", "defstate", "defstyle", "deftokens",
    "defview", "dissoc", "do", "drop", "filter", "find", "first", "fn", "gensym", "get", "get-in", "handler",
    "identity", "if", "inc", "include", "join", "keys", "keyword", "last", "let", "loop", "lower-case", "map",
    "map-indexed", "mapcat", "max", "merge", "min", "mod", "name", "not", "nth", "or", "or-else", "pr-str", "quote",
    "range", "recur", "reduce", "rem", "remove", "remove-hook", "repeat", "rest", "reverse", "second", "some",
    "sort", "sort-by", "str", "subs", "symbol", "take", "throw", "try", "unquote", "unquote-splicing", "update",
    "update-in", "upper-case", "vals", "vec", "when", "zipmap"});
// clang-format on

} // namespace affresco::notation
