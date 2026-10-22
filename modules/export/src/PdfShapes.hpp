#pragma once

/// @file
/// @brief detail functions that write numbers, strings and paths in PDF syntax.

#include "affresco/render/Geometry.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Transform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace affresco::detail {

/// @brief Appends a number: at most three decimals, no trailing zeros, no exponent, never "-0".
/// @param out Where it goes, followed by a space.
/// @param value The number; not finite is written as 0.
void appendNumber(std::string& out, double value);
/// @brief Appends a transform as the six numbers of a matrix ("a b c d e f ").
/// @param out Where it goes.
/// @param transform The transform.
void appendMatrix(std::string& out, const Affine2& transform);
/// @brief Appends a rectangle as a closed subpath ("x y w h re").
/// @param out Where it goes.
/// @param rect The rectangle.
void appendRect(std::string& out, const Rect& rect);
/// @brief Appends a rounded rectangle as a closed subpath, its corners as Bézier curves.
/// @param out Where it goes.
/// @param rect The rectangle.
/// @param radii Its corners, each clamped to half the shorter side.
void appendRoundedRect(std::string& out, const Rect& rect, const CornerRadii& radii);
/// @brief Appends the outline of a mesh of anti-aliased quads: each quad cut where its coverage is one half, all in one
/// direction, so a nonzero fill of them is the shape (overlaps don't cancel, and shared edges leave no seam).
/// @param out Where it goes.
/// @param quads The quads, 4 vertices each.
/// @param offset Added to every position.
/// @return Whether any subpath was appended.
bool appendMeshOutline(std::string& out, std::span<const MeshVertex> quads, Vec2 offset);
/// @brief A text string as a PDF hex string of UTF-16BE with a byte order mark ("<FEFF...>").
/// @param utf8 The text, in UTF-8 (invalid bytes become U+FFFD).
/// @return It.
[[nodiscard]] std::string textString(std::string_view utf8);
/// @brief A character as a PDF hex string of UTF-16BE, without a byte order mark ("<D83DDE00>").
/// @param codepoint The character.
/// @return It.
[[nodiscard]] std::string utf16Hex(std::uint32_t codepoint);
/// @brief Insets each corner radius, as a border's inner corners are.
/// @param radii The outer radii.
/// @param by How much smaller they become, at least zero.
/// @return The inner radii.
[[nodiscard]] CornerRadii insetRadii(const CornerRadii& radii, float by) noexcept;

} // namespace affresco::detail
