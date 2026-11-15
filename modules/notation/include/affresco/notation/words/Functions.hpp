#pragma once

/// @file
/// @brief The language's further functions written as plain words (Words.hpp): those beyond the core, on numbers,
/// collections and sequences.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's further functions (modules/lisp/reference/functions.md). The form is
/// std::to_array<std::string_view>({...}), which tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too. Those with a '?', a '!' or a '*' aren't plain words, and are on none of the lists.
// clang-format off
inline constexpr auto functionsWords = std::to_array<std::string_view>({
    "bit-and", "bit-not", "bit-or", "bit-shift-left", "bit-shift-right", "bit-xor", "butlast", "comp", "compare",
    "complement", "constantly", "dedupe", "distinct", "doall", "dorun", "double", "drop-last", "drop-while",
    "every-pred", "ffirst", "filterv", "flatten", "fnext", "fnil", "frequencies", "group-by", "hash-map", "int",
    "interleave", "interpose", "into", "juxt", "keep", "keep-indexed", "key", "mapv", "max-key", "merge-with",
    "min-key", "next", "nfirst", "nnext", "not-empty", "nthnext", "nthrest", "partial", "partition", "partition-all",
    "partition-by", "peek", "pop", "quot", "reduce-kv", "reductions", "rseq", "select-keys", "seq", "some-fn",
    "split-at", "split-with", "subvec", "take-last", "take-nth", "take-while", "update-keys", "update-vals", "val"});
// clang-format on

} // namespace affresco::notation
