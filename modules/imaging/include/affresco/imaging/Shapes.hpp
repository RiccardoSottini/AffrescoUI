#pragma once

/// @file
/// @brief Vector content drawn: paths flattened, strokes, and type and shape layers rendered into pixels.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Image.hpp"
#include "affresco/imaging/Selection.hpp"
#include "affresco/imaging/Vector.hpp"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace affresco {
class Font;
}

namespace affresco::imaging {

/// @brief A path's outline as straight segments: each curve split finely enough to look smooth.
/// @param points The anchors.
/// @param closed Whether the last joins the first.
/// @return The points, the first not repeated at the end.
[[nodiscard]] std::vector<Point> flattenPath(std::span<const PathPoint> points, bool closed);

/// @brief A shape's outline as straight segments (empty for a line, which only strokes).
/// @param content The content.
/// @return The points.
[[nodiscard]] std::vector<Point> shapeOutline(const LayerContent& content);

/// @brief The pixels within a distance of a line through points: a stroke, with round joins and caps.
/// @param width The image's width.
/// @param height The image's height.
/// @param points The line's points.
/// @param closed Whether the last joins the first.
/// @param strokeWidth The stroke's width.
/// @param limits The limits (polygonPoints).
/// @return The coverage.
[[nodiscard]] Coverage strokeCoverage(int width, int height, std::span<const Point> points, bool closed,
                                      float strokeWidth, const Limits& limits);

/// @brief Lays a colour over an image where a coverage says, as much as it says (straight alpha, over).
/// @param image The image (rgba8 or rgba16f).
/// @param coverage Where.
/// @param colour The colour.
void paintCoverage(Image& image, const Coverage& coverage, const Pixel& colour);

/// @brief Draws a shape layer's content into a new transparent image.
/// @param content The content (kind shape).
/// @param width The image's width.
/// @param height The image's height.
/// @param format Its format.
/// @param limits The limits.
/// @return The image.
[[nodiscard]] std::shared_ptr<Image> renderShape(const LayerContent& content, int width, int height, PixelFormat format,
                                                 const Limits& limits);

/// @brief Draws a type layer's content into a new transparent image, with a font in the content's style; the glyphs
/// are scaled from the font's size to the content's (a font a little larger than the text draws it best).
/// @param content The content (kind text).
/// @param font The font, in the content's style.
/// @param width The image's width.
/// @param height The image's height.
/// @param format Its format.
/// @param limits The limits.
/// @return The image.
[[nodiscard]] std::shared_ptr<Image> renderText(const LayerContent& content, const Font& font, int width, int height,
                                                PixelFormat format, const Limits& limits);

/// @brief The box a type layer's text takes, laid out with a font: {left, top, right, bottom}.
/// @param content The content (kind text).
/// @param font The font, in the content's style (scaled as renderText() does).
[[nodiscard]] std::array<float, 4> textBounds(const LayerContent& content, const Font& font);

/// @brief A content moved by an offset (its origin, corner and anchors).
/// @param content The content.
/// @param dx Across.
/// @param dy Down.
/// @return It, moved.
[[nodiscard]] LayerContent movedContent(LayerContent content, float dx, float dy);

/// @brief A content as JSON: {"kind", "text", "font", "size", ... "fill": "#rrggbbaa", "points": [...]}; a text's keys
/// and a shape's, both for no kind (a tool's settings).
/// @param content The content.
[[nodiscard]] json contentToJson(const LayerContent& content);

/// @brief Changes a content by the keys of a JSON object it has (contentToJson()'s), checked and bounded.
/// @param value The object.
/// @param content The content changed.
/// @throws DocumentError If a key is malformed or beyond its bound.
void readContent(const json& value, LayerContent& content);

/// @brief A path's anchors as JSON: [{"x", "y", "inX", "inY", "outX", "outY"}].
/// @param points The anchors.
[[nodiscard]] json pointsToJson(std::span<const PathPoint> points);

/// @brief A path's anchors from JSON (pointsToJson()'s; a point without handles is a corner).
/// @param value The array.
/// @return The anchors.
/// @throws DocumentError If it is malformed or beyond maxPathPoints.
[[nodiscard]] std::vector<PathPoint> pointsFromJson(const json& value);

} // namespace affresco::imaging
