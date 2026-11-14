#pragma once

/// @file
/// @brief The language's everyday forms written as plain words (Words.hpp): the macros and special forms beyond the
/// core.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's everyday forms (`when-let`, `->`...). Empty until the package that adds them fills
/// it, as std::to_array<std::string_view>({...}), the form tools/notation/lisp_program.py reads; Words.hpp's
/// isLanguageWord() reads it too.
inline constexpr std::array<std::string_view, 0> formsWords{};

} // namespace affresco::notation
