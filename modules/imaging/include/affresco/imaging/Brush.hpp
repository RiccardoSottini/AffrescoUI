#pragma once

/// @file
/// @brief Brushes: round stamps with a size, hardness, spacing, opacity, flow and pressure, painted, erased or cloned
/// along a stroke as one undoable step, within the selection; and the presets the options bar offers.

#include "affresco/imaging/Document.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::imaging {

/// @brief A brush's tip and how it lays paint.
struct BrushTip {
	float size = 20.0f;           ///< Its diameter in pixels, 1 to brushSize.
	float hardness = 0.8f;        ///< The share of its radius fully painted, 0 to 1; the rest fades out.
	float spacing = 0.25f;        ///< Between stamps, a share of the diameter: 0.01 to 10.
	float opacity = 1.0f;         ///< The most a stroke covers, 0 to 1.
	float flow = 1.0f;            ///< What each stamp adds towards the opacity, 0 to 1.
	bool pressureSize = true;     ///< Whether pressure scales the size.
	bool pressureOpacity = false; ///< Whether pressure scales the opacity.

	/// @brief Clamps the settings into their ranges and the limits.
	/// @param limits The limits (brushSize bounds the size).
	void normalize(const Limits& limits) noexcept;
};

/// @brief A named tip, as the options bar lists them.
struct BrushPreset {
	std::string_view name; ///< Its name.
	BrushTip tip;          ///< Its tip.
};

/// @brief The built-in presets: hard and soft round brushes of a few sizes, an airbrush and a pencil-like one.
[[nodiscard]] std::span<const BrushPreset> brushPresets() noexcept;

/// @brief What a stroke does.
enum class StrokeMode : std::uint8_t {
	paint, ///< Lays the colour.
	erase, ///< Takes away opacity (on a mask or the selection: hides, deselects).
	clone, ///< Copies the image from an offset, as it was when the stroke began.
};

/// @brief A point of a stroke and the pen's pressure there.
struct StrokePoint {
	float x = 0.0f;        ///< Across, in image pixels.
	float y = 0.0f;        ///< Down.
	float pressure = 1.0f; ///< 0 to 1 (a mouse is 1).
};

/// @brief A stroke being painted: an open step of the document that each point extends, committed by finish() or put
/// back by cancel(). The document must outlive the stroke and change only through it while it lasts.
class Stroke {
public:
	/// @brief Starts a stroke (and a step).
	/// @param document The document.
	/// @param target The image painted: a pixel layer's, a mask, or the selection (quick mask).
	/// @param tip The tip, normalised.
	/// @param mode What it does.
	/// @param colour paint: the colour (on a mask or the selection, its lightness).
	/// @param name The step's name.
	/// @throws DocumentError when the target can't be painted.
	Stroke(Document& document, std::uint64_t target, const BrushTip& tip, StrokeMode mode, const Pixel& colour,
	       std::string name);
	Stroke(const Stroke&) = delete;
	Stroke& operator=(const Stroke&) = delete;
	/// @brief Cancels the stroke if it wasn't finished.
	~Stroke();

	/// @brief clone: where it copies from, relative to where it paints.
	/// @param x Across.
	/// @param y Down.
	void setCloneOffset(float x, float y) noexcept { cloneX_ = x, cloneY_ = y; }
	/// @brief Extends the stroke to a point, stamping along the way; points that aren't finite are left out.
	/// @param point The point.
	void add(const StrokePoint& point);
	/// @brief Ends the stroke, committing its step.
	void finish();
	/// @brief Ends the stroke, putting back what it painted.
	void cancel();
	/// @brief The stamps laid so far.
	[[nodiscard]] std::size_t stamps() const noexcept { return stamps_; }

private:
	/// @brief One tile's share of the stroke.
	struct Touched {
		std::shared_ptr<const Tile> original; ///< The tile before the stroke (a mask's absent tile: white).
		std::vector<float> coverage;          ///< What the stroke covers of each pixel, 0 to opacity.
	};

	/// @brief Lays one stamp.
	/// @param x Its centre across.
	/// @param y Its centre down.
	/// @param pressure The pressure there.
	void stamp(float x, float y, float pressure);
	/// @brief Lays a stamp's share in one tile.
	/// @param column The tile's column.
	/// @param row The tile's row.
	/// @param x The stamp's centre across.
	/// @param y Its centre down.
	/// @param radius Its radius.
	/// @param cap The most it covers.
	void stampTile(int column, int row, float x, float y, float radius, float cap);
	/// @brief A tile's share of the stroke, made at its first stamp.
	/// @param column Its column.
	/// @param row Its row.
	Touched& touched(int column, int row);
	/// @brief A pixel of the target before the stroke (for clone).
	/// @param x Across.
	/// @param y Down.
	[[nodiscard]] Pixel original(int x, int y) const;
	/// @brief A pixel as the stroke leaves it.
	/// @param under The pixel before the stroke.
	/// @param x Across, for clone.
	/// @param y Down.
	/// @param coverage The stroke's coverage there.
	[[nodiscard]] Pixel painted(const Pixel& under, int x, int y, float coverage) const;

	Document* document_;                                        ///< The document, null once ended.
	std::uint64_t target_;                                      ///< The image painted.
	BrushTip tip_;                                              ///< The tip.
	StrokeMode mode_;                                           ///< What it does.
	Pixel colour_;                                              ///< paint: the colour.
	bool mask_;                                                 ///< Whether the target is a mask or the selection.
	bool selected_;                                             ///< Whether the selection limits the stroke.
	float cloneX_ = 0.0f;                                       ///< clone: the offset across.
	float cloneY_ = 0.0f;                                       ///< clone: the offset down.
	bool started_ = false;                                      ///< Whether a point was added.
	StrokePoint last_;                                          ///< The last point.
	float travelled_ = 0.0f;                                    ///< The distance since the last stamp.
	std::size_t stamps_ = 0;                                    ///< See stamps().
	std::unordered_map<TileKey, Touched, TileKeyHash> touched_; ///< The tiles the stroke touched.
};

/// @brief The coverage of a round stamp at a distance from its centre: 1 within hardness, fading out to its radius
/// (over at least a pixel, so edges are smooth).
/// @param distance The distance from the centre.
/// @param radius The radius.
/// @param hardness The hardness, 0 to 1.
[[nodiscard]] float stampCoverage(float distance, float radius, float hardness) noexcept;

} // namespace affresco::imaging
