#pragma once

/// @file
/// @brief Vector content (the types; Shapes.hpp draws them): paths of anchor points with curve handles (the pen's), and
/// the content a type or shape layer is drawn from, rendered into the layer's pixels whenever it changes.

#include "affresco/imaging/Pixels.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::imaging {

/// @brief A point of vector content, in image pixels (the top left of the image 0, 0).
struct VectorPoint {
	float x = 0.0f; ///< Across.
	float y = 0.0f; ///< Down.

	/// @brief Whether two points are the same.
	[[nodiscard]] friend bool operator==(const VectorPoint&, const VectorPoint&) = default;
};

/// @brief One anchor of a path, with the handles of the curves into and out of it (both at the anchor for a corner).
struct PathPoint {
	VectorPoint anchor; ///< Where the path passes.
	VectorPoint in;     ///< The handle of the curve that arrives here.
	VectorPoint out;    ///< The handle of the curve that leaves here.

	/// @brief Whether two points are the same.
	[[nodiscard]] friend bool operator==(const PathPoint&, const PathPoint&) = default;
};

/// @brief A path of the document (the Paths panel's rows): cubic curves through anchors, open or closed.
struct VectorPath {
	std::string name;              ///< Its name, at most 255 bytes.
	std::vector<PathPoint> points; ///< Its anchors, in drawing order.
	bool closed = false;           ///< Whether the last anchor joins the first.

	/// @brief Whether two paths are the same.
	[[nodiscard]] friend bool operator==(const VectorPath&, const VectorPath&) = default;
};

/// @brief What a layer's pixels are drawn from: nothing (painted pixels), text, or a shape.
enum class ContentKind : std::uint8_t {
	none,  ///< Painted pixels.
	text,  ///< A type layer.
	shape, ///< A shape layer.
};

/// @brief The outline of a shape layer.
enum class ShapeKind : std::uint8_t {
	rectangle, ///< A rectangle between two corners, its corners rounded by a radius.
	ellipse,   ///< The ellipse a rectangle encloses.
	polygon,   ///< A regular polygon inscribed in the ellipse a rectangle encloses.
	line,      ///< A line from one corner to the other (drawn by its stroke).
	path,      ///< A path's anchors.
};

/// @brief How lines of text line up.
enum class TextAlign : std::uint8_t {
	left,   ///< On the origin's left edge.
	center, ///< Centred on the origin.
	right,  ///< Ending at the origin.
};

/// @brief The content a type or shape layer is drawn from. Kept on the layer, so each edit draws it again, sharp.
struct LayerContent {
	ContentKind kind = ContentKind::none; ///< What it is.
	// text
	std::string text;                  ///< text: its characters, UTF-8; '\n' starts a line.
	std::string font;                  ///< text: the font's name (the document's), "" for the default.
	float size = 48.0f;                ///< text: the size in pixels.
	float leading = 1.2f;              ///< text: the distance between baselines, in sizes.
	bool bold = false;                 ///< text: thickened.
	bool italic = false;               ///< text: slanted.
	TextAlign align = TextAlign::left; ///< text: how its lines line up.
	float tracking = 0.0f;             ///< text: space added after each character, in thousandths of the size.
	std::map<std::string, float> kerning; ///< text: space added between two characters (the key, UTF-8), as tracking.
	// both: text's origin is its first line's top at the alignment edge; a shape's box is from origin to corner
	VectorPoint origin; ///< Where it is.
	VectorPoint corner; ///< shape: the box's opposite corner.
	// shape
	ShapeKind shape = ShapeKind::rectangle; ///< shape: its outline.
	float radius = 0.0f;                    ///< rectangle: its corners' radius.
	int sides = 6;                          ///< polygon: its sides, 3 to 64.
	std::vector<PathPoint> points;          ///< path: its anchors.
	bool closed = true;                     ///< path: whether it is closed.
	Pixel fill{0.0f, 0.0f, 0.0f, 1.0f};     ///< The text's colour, or the shape's fill (transparent for none).
	Pixel stroke{0.0f, 0.0f, 0.0f, 0.0f};   ///< shape: its stroke's colour (transparent for none).
	float strokeWidth = 0.0f;               ///< shape: its stroke's width in pixels.

	/// @brief Whether two contents are the same.
	[[nodiscard]] friend bool operator==(const LayerContent&, const LayerContent&) = default;
};

/// @brief The most anchors a path or a shape keeps.
inline constexpr std::size_t maxPathPoints = 4096;
/// @brief The most paths a document keeps.
inline constexpr std::size_t maxPaths = 256;
/// @brief The most pairs a type layer's kerning holds.
inline constexpr std::size_t maxKerningPairs = 256;
/// @brief The most bytes of a type layer's text.
inline constexpr std::size_t maxTextBytes = 16384;

/// @brief The names of the content kinds ("none", "text", "shape").
[[nodiscard]] std::span<const std::string_view> contentKindNames() noexcept;
/// @brief The names of the shape kinds ("rectangle", "ellipse", "polygon", "line", "path").
[[nodiscard]] std::span<const std::string_view> shapeKindNames() noexcept;
/// @brief The names of the alignments ("left", "center", "right").
[[nodiscard]] std::span<const std::string_view> textAlignNames() noexcept;

} // namespace affresco::imaging
