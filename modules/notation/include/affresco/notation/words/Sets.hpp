#pragma once

/// @file
/// @brief The language's set functions written as plain words (Words.hpp): the `clojure.set` namespace's, which a
/// program may also write bare.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's set functions. Empty until the package that adds them fills it, as
/// std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too.
inline constexpr std::array<std::string_view, 0> setsWords{};

} // namespace affresco::notation
