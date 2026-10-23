#pragma once

/// @file
/// @brief Readers for the values of a node's properties that need nothing but the value: lengths, alignments,
/// cursors, placements, anchors, numbers of pixels and corner radii; and the layout properties every panel has.

#include "affresco/core/Json.hpp"
#include "affresco/render/Input.hpp"
#include "affresco/render/Length.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Stack.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace affresco::format {

/// @brief Reads a length: 120 or "120px" (pixels), "50%" (of the parent).
/// @param value The value.
/// @param path Its place, for errors.
/// @return The length.
/// @throws DocumentError If it is not a length.
[[nodiscard]] Length length(const json& value, const std::string& path);

/// @brief Reads "start", "center" or "end".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The alignment.
/// @throws DocumentError If it is something else.
[[nodiscard]] Align align(const json& value, const std::string& path);

/// @brief Reads a label's alignment: one value for both axes, two words for horizontal then vertical
/// ("start center"), or `{ "horizontal", "vertical" }` (each start when left out).
/// @param value The value.
/// @param path Its place, for errors.
/// @return The horizontal and vertical alignments.
/// @throws DocumentError If it is something else.
[[nodiscard]] std::pair<Align, Align> alignments(const json& value, const std::string& path);

/// @brief Reads a cursor name, e.g. "hand".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The cursor.
/// @throws DocumentError If the name is unknown.
[[nodiscard]] Cursor cursor(const json& value, const std::string& path);

/// @brief Reads an overflow, "clip" or "visible".
/// @param value The value.
/// @param path Its place, for errors.
/// @return true for "visible".
/// @throws DocumentError If it is something else.
[[nodiscard]] bool overflowVisible(const json& value, const std::string& path);

/// @brief Reads an overlay placement, e.g. "below".
/// @param value The value.
/// @param path Its place, for errors.
/// @return The placement.
/// @throws DocumentError If the name is unknown.
[[nodiscard]] Placement placement(const json& value, const std::string& path);

/// @brief Reads a finite number: JSON allows numbers too large for a float, which would become infinite.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The number.
/// @throws DocumentError If it isn't a number, or is too large.
[[nodiscard]] float finiteNumber(const json& value, const std::string& path);

/// @brief Reads a number of pixels that can't be negative: a radius, a border width or a blur.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The number.
/// @throws DocumentError If it isn't a finite number of 0 or more.
[[nodiscard]] float pixels(const json& value, const std::string& path);

/// @brief Reads "radius": one number for every corner, or { "topLeft", "topRight", "bottomRight", "bottomLeft" }
/// (corners left out are square).
/// @param value The value.
/// @param path Its place, for errors.
/// @return The radii.
/// @throws DocumentError If it is malformed.
[[nodiscard]] CornerRadii radii(const json& value, const std::string& path);

/// @brief Applies a layout property: x, y, width, height, minWidth, minHeight, grow, fill or anchors.
/// @param panel The panel.
/// @param key The property.
/// @param value Its value.
/// @param at Its place, for errors.
/// @return false if it isn't one of these.
/// @throws DocumentError If the value is malformed.
bool layoutProperty(Panel& panel, std::string_view key, const json& value, const std::string& at);

} // namespace affresco::format
