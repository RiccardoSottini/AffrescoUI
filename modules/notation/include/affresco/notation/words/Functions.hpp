#pragma once

/// @file
/// @brief The language's further functions written as plain words (Words.hpp): those beyond the core, on numbers,
/// collections and sequences.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's further functions. Empty until the package that adds them fills it, as
/// std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too.
inline constexpr std::array<std::string_view, 0> functionsWords{};

} // namespace affresco::notation
