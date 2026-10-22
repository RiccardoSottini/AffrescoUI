#pragma once

/// @file
/// @brief The evidence of a proposal (VERSION_CONTROL.md, "Evidence"): the region where two pictures differ.

#include "Png.hpp"

#include <affresco/core/Json.hpp>

#include <array>

namespace affresco::detail {

/// @brief Finds the smallest rectangle that holds every pixel where two pictures differ.
/// @param before One picture.
/// @param after The other.
/// @return `{x, y, width, height}`: all 0 when they are the same, the whole of @p after when their sizes differ.
[[nodiscard]] std::array<int, 4> changedRegion(const RgbImage& before, const RgbImage& after);

/// @brief Finds the texts a proposal changes: every node with an id whose "text" differs, or that only one side has.
/// @param main Main's document.
/// @param proposal The proposal's document.
/// @return `[{"node", "original", "modified", "language"}]`, in the proposal's order; a side without it is "".
[[nodiscard]] json changedTexts(const json& main, const json& proposal);

} // namespace affresco::detail
