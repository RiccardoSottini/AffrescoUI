#pragma once

/// @file
/// @brief The language's everyday forms written as plain words (Words.hpp): the macros and special forms beyond the
/// core.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The words of the language's everyday forms (`when-let`, `->`...), the macros of modules/lisp's PreludeForms.cpp,
/// as std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's
/// isLanguageWord() reads it too. The threading forms aren't plain words, so a reader never meets them as a node's
/// type.
// clang-format off
inline constexpr auto formsWords = std::to_array<std::string_view>({
    "->", "->>", "as->", "assert", "case", "comment", "cond->", "cond->>", "condp", "defn-", "defonce", "doseq",
    "dotimes", "doto", "for", "if-let", "if-not", "if-some", "letfn", "some->", "some->>", "when-first", "when-let",
    "when-not", "when-some"});
// clang-format on

} // namespace affresco::notation
