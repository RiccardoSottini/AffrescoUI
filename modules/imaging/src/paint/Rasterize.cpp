/// @file
/// @brief Shapes rasterised into selection coverage: an even-odd scanline fill with four rows per pixel and exact
/// horizontal coverage, kept sparse (no tile outside the shape, one shared white tile inside it).

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Selection.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace affresco::imaging {

namespace {

constexpr int subrows = 4; ///< Scanlines per pixel row.

/// @brief An edge of the polygon, top to bottom.
struct Edge {
	float top = 0.0f;    ///< Its top.
	float bottom = 0.0f; ///< Its bottom.
	float x = 0.0f;      ///< Its x at the top.
	float slope = 0.0f;  ///< x per y.
};

/// @brief One band of tile rows being filled.
struct Band {
	std::vector<std::shared_ptr<Tile>> tiles; ///< The band's tiles, made at their first covered row.
	std::vector<int> fullRows;                ///< Rows of each tile entirely covered.
};

/// @brief The polygon's edges, its horizontal ones left out, sorted by their top.
/// @param points The corners.
[[nodiscard]] std::vector<Edge> edgesOf(std::span<const Point> points) {
	std::vector<Edge> edges;
	edges.reserve(points.size());
	for(std::size_t index = 0; index < points.size(); ++index) {
		Point from = points[index], to = points[(index + 1) % points.size()];
		if(!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y)) continue;
		if(from.y == to.y) continue;
		if(from.y > to.y) std::swap(from, to);
		edges.push_back({from.y, to.y, from.x, (to.x - from.x) / (to.y - from.y)});
	}
	std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) { return left.top < right.top; });
	return edges;
}

/// @brief Adds a span's coverage to a row: fractions at its ends, whole pixels through the running sum.
/// @param left The span's left.
/// @param right The span's right.
/// @param partial Fractions, per pixel.
/// @param whole Steps of the running sum, per pixel (one more than the width).
void addSpan(float left, float right, std::vector<float>& partial, std::vector<float>& whole) {
	const auto width = static_cast<float>(partial.size());
	left = std::clamp(left, 0.0f, width), right = std::clamp(right, 0.0f, width);
	if(right <= left) return;
	constexpr float share = 1.0f / subrows;
	const auto first = static_cast<std::size_t>(left), last = static_cast<std::size_t>(right);
	if(first == last) {
		partial[first] += (right - left) * share;
		return;
	}
	partial[first] += (static_cast<float>(first + 1) - left) * share;
	whole[first + 1] += share, whole[last] -= share;
	if(last < partial.size()) partial[last] += (right - static_cast<float>(last)) * share;
}

/// @brief Fills one pixel row's coverage from the edges crossing its scanlines.
/// @param y The row.
/// @param edges The edges, sorted by their top.
/// @param next The first edge not yet active, advanced.
/// @param active The active edges, changed.
/// @param crossings Scratch.
/// @param partial The row's fractions, added to.
/// @param whole The row's running-sum steps, added to.
void scanRow(int y, const std::vector<Edge>& edges, std::size_t& next, std::vector<const Edge*>& active,
             std::vector<float>& crossings, std::vector<float>& partial, std::vector<float>& whole) {
	for(int sub = 0; sub < subrows; ++sub) {
		const float scan = static_cast<float>(y) + (static_cast<float>(sub) + 0.5f) / subrows;
		while(next < edges.size() && edges[next].top <= scan) active.push_back(&edges[next++]);
		std::erase_if(active, [scan](const Edge* edge) { return edge->bottom <= scan; });
		crossings.clear();
		for(const Edge* edge : active)
			if(edge->top <= scan) crossings.push_back(edge->x + (scan - edge->top) * edge->slope);
		std::sort(crossings.begin(), crossings.end());
		for(std::size_t index = 0; index + 1 < crossings.size(); index += 2)
			addSpan(crossings[index], crossings[index + 1], partial, whole);
	}
}

/// @brief Writes a row's coverage into its band's tiles, then clears the row.
/// @param y The row.
/// @param partial The row's fractions.
/// @param whole The row's running-sum steps.
/// @param band The band.
void storeRow(int y, std::vector<float>& partial, std::vector<float>& whole, Band& band) {
	const auto width = static_cast<int>(partial.size());
	float running = 0.0f;
	for(int column = 0; column * tileSize < width; ++column) {
		const int left = column * tileSize, right = std::min(width, left + tileSize);
		bool full = true, any = false;
		std::array<std::uint8_t, tileSize> bytes{};
		for(int x = left; x < right; ++x) {
			const auto at = static_cast<std::size_t>(x);
			running += whole[at];
			bytes[static_cast<std::size_t>(x - left)] = byteFromUnit(running + partial[at]);
			full = full && bytes[static_cast<std::size_t>(x - left)] == 255;
			any = any || bytes[static_cast<std::size_t>(x - left)] != 0;
			partial[at] = 0.0f, whole[at] = 0.0f;
		}
		if(!any) continue;
		auto& tile = band.tiles[static_cast<std::size_t>(column)];
		if(!tile) tile = std::make_shared<Tile>(PixelFormat::mask8);
		std::copy(bytes.begin(), bytes.begin() + (right - left),
		          tile->bytes().begin() + static_cast<std::ptrdiff_t>((y % tileSize) * tileSize));
		if(full) ++band.fullRows[static_cast<std::size_t>(column)];
	}
	whole[partial.size()] = 0.0f;
}

/// @brief Moves a finished band's tiles into the coverage, sharing one white tile for those entirely covered.
/// @param row The band's tile row.
/// @param rows The image's rows in the band.
/// @param band The band, emptied.
/// @param white The white tile.
/// @param coverage The coverage, added to.
void storeBand(int row, int rows, Band& band, const std::shared_ptr<const Tile>& white, Coverage& coverage) {
	for(std::size_t column = 0; column < band.tiles.size(); ++column) {
		if(!band.tiles[column]) continue;
		const bool full = band.fullRows[column] == rows;
		coverage[{static_cast<int>(column), row}] = full ? white : std::move(band.tiles[column]);
		band.tiles[column].reset(), band.fullRows[column] = 0;
	}
}

} // namespace

Coverage polygonCoverage(int width, int height, std::span<const Point> points, const Limits& limits) {
	if(points.size() > limits.polygonPoints)
		throw DocumentError("imaging: a polygon of " + std::to_string(points.size()) + " points, beyond the limit of " +
		                    std::to_string(limits.polygonPoints));
	Coverage coverage;
	const std::vector<Edge> edges = edgesOf(points);
	if(edges.empty() || width <= 0 || height <= 0) return coverage;
	float top = edges.front().top, bottom = top;
	for(const Edge& edge : edges) bottom = std::max(bottom, edge.bottom);
	const int firstRow = std::clamp(static_cast<int>(std::floor(top)), 0, height);
	const int lastRow = std::clamp(static_cast<int>(std::ceil(bottom)), 0, height);
	const auto columns = static_cast<std::size_t>((width + tileSize - 1) / tileSize);
	Band band{std::vector<std::shared_ptr<Tile>>(columns), std::vector<int>(columns, 0)};
	std::vector<float> partial(static_cast<std::size_t>(width), 0.0f), whole(partial.size() + 1, 0.0f), crossings;
	std::vector<const Edge*> active;
	std::size_t next = 0;
	const std::shared_ptr<const Tile> white = Tile::filled(PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f});
	for(int y = firstRow; y < lastRow; ++y) {
		scanRow(y, edges, next, active, crossings, partial, whole);
		storeRow(y, partial, whole, band);
		if(y % tileSize == tileSize - 1 || y == lastRow - 1)
			storeBand(y / tileSize, std::min(tileSize, height - (y / tileSize) * tileSize), band, white, coverage);
	}
	return coverage;
}

Coverage rectangleCoverage(int width, int height, Point from, Point to) {
	const std::array<Point, 4> corners{{{from.x, from.y}, {to.x, from.y}, {to.x, to.y}, {from.x, to.y}}};
	return polygonCoverage(width, height, corners, Limits{});
}

Coverage ellipseCoverage(int width, int height, Point from, Point to) {
	const float centreX = (from.x + to.x) / 2.0f, centreY = (from.y + to.y) / 2.0f;
	const float radiusX = std::abs(to.x - from.x) / 2.0f, radiusY = std::abs(to.y - from.y) / 2.0f;
	if(!std::isfinite(radiusX) || !std::isfinite(radiusY)) return {};
	// Corners close enough that the polygon strays from the ellipse by under a tenth of a pixel
	const float radius = std::min(std::max(radiusX, radiusY), 1e6f);
	const int count = std::clamp(
	    static_cast<int>(std::ceil(std::numbers::pi_v<float> / std::sqrt(0.2f / std::max(radius, 1.0f)))), 16, 8192);
	std::vector<Point> corners(static_cast<std::size_t>(count));
	for(int index = 0; index < count; ++index) {
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(index) / static_cast<float>(count);
		corners[static_cast<std::size_t>(index)] = {centreX + radiusX * std::cos(angle),
		                                            centreY + radiusY * std::sin(angle)};
	}
	return polygonCoverage(width, height, corners, Limits{});
}

} // namespace affresco::imaging
