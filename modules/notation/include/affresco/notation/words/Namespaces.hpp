#pragma once

/// @file
/// @brief The words of the language's namespaces written as plain words (Words.hpp): `ns` and what goes with it.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's namespace forms (modules/lisp/reference/namespaces.md), as
/// std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too.
inline constexpr auto namespacesWords = std::to_array<std::string_view>({"ns"});

} // namespace affresco::notation
