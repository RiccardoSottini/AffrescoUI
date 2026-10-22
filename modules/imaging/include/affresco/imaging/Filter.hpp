#pragma once

/// @file
/// @brief Filters: Gaussian blur, sharpen, unsharp mask, add noise and motion blur, tile by tile with a halo, within
/// the selection; the feather of a selection; and runs a live preview filters tile by tile before committing.

#include "affresco/imaging/Document.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::imaging {

/// @brief What a filter does.
enum class FilterKind : std::uint8_t {
	gaussianBlur, ///< Softens by a radius.
	sharpen,      ///< Sharpens the finest detail.
	unsharpMask,  ///< Sharpens by an amount, a radius and a threshold.
	addNoise,     ///< Adds random noise by an amount.
	motionBlur,   ///< Smears along an angle by a distance.
};

/// @brief A filter's kind's names as stored and named in screens: "gaussian-blur", "sharpen", …
[[nodiscard]] std::span<const std::string_view> filterKindNames() noexcept;

/// @brief A filter kind by name.
/// @param name The name.
/// @return The kind, or nothing for an unknown name.
[[nodiscard]] std::optional<FilterKind> filterKindOf(std::string_view name) noexcept;

/// @brief A filter and its settings.
struct Filter {
	FilterKind kind = FilterKind::gaussianBlur; ///< What it does.
	float radius = 2.0f;                        ///< gaussianBlur, unsharpMask: the blur's standard deviation in pixels.
	float amount = 1.0f;                        ///< unsharpMask: 0 to 5 (500 %); addNoise: 0 to 1.
	int threshold = 0;                          ///< unsharpMask: the smallest difference sharpened, 0 to 255.
	float angle = 0.0f;                         ///< motionBlur: degrees, anticlockwise from across.
	float distance = 10.0f;                     ///< motionBlur: pixels.
	bool monochrome = false;                    ///< addNoise: the same noise in each channel.
	std::uint32_t seed = 1;                     ///< addNoise: the noise's seed.

	/// @brief Clamps the settings into their ranges and the limits.
	/// @param limits The limits (filterRadius bounds the radius and the distance).
	void normalize(const Limits& limits) noexcept;
};

/// @brief The pixels around a tile a filter reads.
/// @param filter The filter.
[[nodiscard]] int haloOf(const Filter& filter) noexcept;

/// @brief A filter applied to a layer tile by tile, in an open step of the document: what a live preview shows,
/// committed by finish() or put back by cancel(). The document must outlive the run and change only through it.
class FilterRun {
public:
	/// @brief Starts a run (and a step named after the filter).
	/// @param document The document.
	/// @param target The image filtered: a pixel layer's, a mask, or the selection (feather).
	/// @param filter The filter, normalised.
	/// @param name The step's name.
	/// @throws DocumentError when the target can't be painted.
	FilterRun(Document& document, std::uint64_t target, const Filter& filter, std::string name);
	FilterRun(const FilterRun&) = delete;
	FilterRun& operator=(const FilterRun&) = delete;
	/// @brief Cancels the run if it wasn't finished.
	~FilterRun();

	/// @brief Filters the tiles a region touches that aren't yet.
	/// @param x The region's left.
	/// @param y Its top.
	/// @param width Its width.
	/// @param height Its height.
	/// @param most The most tiles filtered by this call (a preview's budget).
	/// @return Whether every tile of the region is now filtered.
	bool runRegion(int x, int y, int width, int height, std::size_t most = SIZE_MAX);
	/// @brief Filters the rest and commits the step.
	void finish();
	/// @brief Puts back every tile filtered and ends the step.
	void cancel();

private:
	/// @brief Filters one tile.
	/// @param column Its column.
	/// @param row Its row.
	void runTile(int column, int row);
	/// @brief Filters the region read into after_, its centre copied into before_.
	/// @param column The tile's column.
	/// @param row The tile's row.
	void filterTile(int column, int row);
	/// @brief Stores after_ as the tile, mixed with before_ by the selection.
	/// @param column The tile's column.
	/// @param row The tile's row.
	/// @param selected The selection's tile, or null for everywhere.
	void storeTile(int column, int row, const Tile* selected);
	/// @brief A tile of the target as it was before the run.
	/// @param column Its column.
	/// @param row Its row.
	[[nodiscard]] std::shared_ptr<const Tile> original(int column, int row) const;
	/// @brief Reads the region around a tile, premultiplied, its edges extended.
	/// @param column The tile's column.
	/// @param row The tile's row.
	/// @return Whether anything in it isn't transparent.
	bool readRegion(int column, int row);

	Document* document_;                 ///< The document, null once finished.
	std::uint64_t target_;               ///< The image filtered.
	Filter filter_;                      ///< The filter.
	int halo_;                           ///< haloOf(filter_).
	std::set<std::pair<int, int>> done_; ///< The tiles filtered.
	std::unordered_map<TileKey, std::shared_ptr<const Tile>, TileKeyHash> originals_; ///< Tiles before the run.
	std::vector<float> region_;  ///< A tile and its halo, premultiplied RGBA.
	std::vector<float> scratch_; ///< The kernels' scratch.
	std::vector<float> before_;  ///< A tile before filtering, premultiplied RGBA.
	std::vector<float> after_;   ///< The tile filtered, premultiplied RGBA.
};

/// @brief Applies a filter to a layer as one step (within the selection when there is one).
/// @param document The document.
/// @param target The image.
/// @param filter The filter.
/// @throws DocumentError when the target can't be painted.
void applyFilter(Document& document, std::uint64_t target, const Filter& filter);

/// @brief Softens the selection's edge by a radius, one step.
/// @param document The document.
/// @param radius The radius in pixels, at most filterRadius.
void featherSelection(Document& document, float radius);

} // namespace affresco::imaging
