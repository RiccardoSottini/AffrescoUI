#pragma once

/// @file
/// @brief The plain words the language's reader forms stand for (Words.hpp): what a function literal or a reader
/// conditional is read as.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the reader's forms. Empty until the package that adds them fills it, as
/// std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord()
/// reads it too.
inline constexpr std::array<std::string_view, 0> readerWords{};

} // namespace affresco::notation
