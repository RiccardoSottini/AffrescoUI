#pragma once

/// @file
/// @brief The words of the language's namespaces written as plain words (Words.hpp): `ns` and what goes with it.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's namespace forms. Empty until the package that adds them fills it, as
/// std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too.
inline constexpr std::array<std::string_view, 0> namespacesWords{};

} // namespace affresco::notation
