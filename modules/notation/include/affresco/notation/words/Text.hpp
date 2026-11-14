#pragma once

/// @file
/// @brief The language's text functions written as plain words (Words.hpp): `format`, errors as values and the `parse-`
/// functions.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's text functions, as std::to_array<std::string_view>({...}), the form
/// tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord() reads it too. The `clojure.string` functions are
/// written with their namespace (`str/split`), and a name with a '/' is not a plain word, so none is here.
inline constexpr auto textWords = std::to_array<std::string_view>(
    {"ex-data", "ex-info", "ex-message", "format", "parse-boolean", "parse-double", "parse-long"});

} // namespace affresco::notation
