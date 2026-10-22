#pragma once

/// @file
/// @brief How large a revision, a document and a state may get (SECURITY.md, "Limits"), and the measure of a value's
/// size they use.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <limits>

namespace affresco::format {

inline constexpr std::size_t maxRevisionOperations = 100000; ///< Operations of one revision, after compact edits.
inline constexpr std::size_t maxRevisionBytes = 16u << 20;   ///< One revision's operations, as JSON: 16 MiB.
inline constexpr std::size_t maxDocumentBytes = 32u << 20;   ///< A document without its state, as JSON: 32 MiB.
inline constexpr std::size_t maxStateBytes = 32u << 20;      ///< The live state, as JSON: 32 MiB.

/// @brief About how many bytes a value takes written as JSON: its texts, keys and numbers, and one byte per
/// separator. Escapes aren't counted.
/// @param value The value.
/// @param enough Stops counting once the size is past this (the result is then larger, not exact).
/// @return The size.
[[nodiscard]] std::size_t approximateBytes(const json& value,
                                           std::size_t enough = std::numeric_limits<std::size_t>::max());

} // namespace affresco::format
