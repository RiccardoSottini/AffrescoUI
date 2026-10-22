#pragma once

/// @file
/// @brief Selections: shapes (rectangle, ellipse, lasso, polygon) rasterised with soft edges, the magic wand and
/// colour range, and their combination with a document's selection (replace, add, subtract, intersect, invert).

#include "affresco/imaging/Composite.hpp"
#include "affresco/imaging/Document.hpp"

#include <cstdint>
#include <array>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace affresco::imaging {

/// @brief A point in image pixels (the top left of the image 0, 0).
struct Point {
	float x = 0.0f; ///< Across.
	float y = 0.0f; ///< Down.
};

/// @brief How a new selection combines with the one there.
enum class SelectionMode : std::uint8_t {
	replace,   ///< The new one alone.
	add,       ///< Either.
	subtract,  ///< The old one but not the new one.
	intersect, ///< Both.
};

/// @brief A selection's tiles by (column, row), mask8; absent tiles select nothing.
using Coverage = std::map<std::pair<int, int>, std::shared_ptr<const Tile>>;

/// @brief The pixels within a polygon (a lasso's points or a polygon's corners), even-odd, its edges antialiased.
/// @param width The image's width.
/// @param height The image's height.
/// @param points The corners, closed back to the first.
/// @param limits The limits (polygonPoints).
/// @throws DocumentError beyond polygonPoints.
[[nodiscard]] Coverage polygonCoverage(int width, int height, std::span<const Point> points, const Limits& limits);

/// @brief The pixels within a rectangle between two corners.
/// @param width The image's width.
/// @param height The image's height.
/// @param from One corner.
/// @param to The opposite corner.
[[nodiscard]] Coverage rectangleCoverage(int width, int height, Point from, Point to);

/// @brief The pixels within the ellipse a rectangle between two corners encloses.
/// @param width The image's width.
/// @param height The image's height.
/// @param from One corner.
/// @param to The opposite corner.
[[nodiscard]] Coverage ellipseCoverage(int width, int height, Point from, Point to);

/// @brief What the magic wand, colour range and the fill sample.
struct SampleOptions {
	int tolerance = 32;     ///< The largest difference of a channel from the colour clicked, 0 to 255.
	bool contiguous = true; ///< Only pixels connected to the one clicked.
	bool allLayers = false; ///< The flattened image rather than the active layer.
};

/// @brief The pixels like the one at a point: the magic wand (and the fill's region).
/// @param document The document.
/// @param x The point across.
/// @param y The point down.
/// @param options Tolerance, contiguity and what is sampled.
/// @throws DocumentError when a contiguous region reaches floodPixels.
[[nodiscard]] Coverage wandCoverage(const Document& document, int x, int y, const SampleOptions& options);

/// @brief The pixels near a colour, softly: 1 at the colour, falling to 0 at the fuzziness (colour range).
/// @param document The document.
/// @param colour The colour.
/// @param fuzziness The difference of a channel where the selection reaches 0, 1 to 255.
/// @param allLayers The flattened image rather than the active layer.
[[nodiscard]] Coverage colorRangeCoverage(const Document& document, const Pixel& colour, int fuzziness,
                                          bool allLayers);

/// @brief Combines a coverage with a document's selection as one undoable step.
/// @param document The document.
/// @param name The step's name ("Rectangular Marquee", "Magic Wand", …).
/// @param coverage The new pixels.
/// @param mode How it combines.
void select(Document& document, const std::string& name, const Coverage& coverage, SelectionMode mode);

/// @brief Selects the whole image, one step.
/// @param document The document.
void selectAll(Document& document);

/// @brief Selects nothing, one step (nothing when there is no selection).
/// @param document The document.
void deselect(Document& document);

/// @brief Selects what wasn't and unselects what was, one step.
/// @param document The document.
void invertSelection(Document& document);

/// @brief Whether a selection's image selects anything: any tile written and not transparent.
/// @param selection The selection's image.
[[nodiscard]] bool selectsAnything(const Image& selection);

/// @brief The selection's bounds in pixels, by its tiles (empty when nothing is selected).
/// @param document The document.
/// @return x, y, width and height.
[[nodiscard]] std::array<int, 4> selectionBounds(const Document& document);

} // namespace affresco::imaging
