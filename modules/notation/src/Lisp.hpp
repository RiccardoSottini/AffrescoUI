#pragma once

/// @file
/// @brief The Lisp notation's reader and writer (README.md), which Notation.cpp offers through read() and write().

#include <affresco/core/Json.hpp>

#include <string>
#include <string_view>

namespace affresco::notation::detail {

/// @brief Reads a value written in the Lisp notation.
/// @param text The text.
/// @return The value, its keys in the order written.
/// @throws NotationError If the text isn't one value, nests deeper than maxNesting or is longer than maxLispBytes.
[[nodiscard]] json readLisp(std::string_view text);

/// @brief Writes a value in the Lisp notation, canonically: lines of at most 120 columns where the values allow it,
/// a node's children one per line, a line break at the end.
/// @param value The value, at most maxNesting deep.
/// @return The text.
[[nodiscard]] std::string writeLisp(const json& value);

} // namespace affresco::notation::detail
