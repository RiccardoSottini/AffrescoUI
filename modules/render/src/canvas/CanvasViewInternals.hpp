#pragma once

/// @file
/// @brief What a CanvasView keeps out of its public header: the spatial index and the storage reused frame to
/// frame, shared by CanvasView.cpp (content and painting) and CanvasViewInput.cpp (the pointer and the keys).

#include "canvas/Rulers.hpp"
#include "Snapping.hpp"
#include "SpatialIndex.hpp"

#include <affresco/render/CanvasView.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

/// @brief The index and the scratch storage of a CanvasView.
struct CanvasView::Internals {
	SpatialIndex index;                                          ///< The model items' rectangles.
	std::vector<Rect> bounds;                                    ///< The model items' rectangles, as indexed.
	std::vector<std::pair<std::string_view, std::uint32_t>> ids; ///< Model item ids to indices, sorted by id.
	std::optional<Rect> extent;                                  ///< What the model items cover.
	std::vector<std::pair<const Panel*, const Panel*>> standIns; ///< Children standing in, and for whom.
	std::vector<std::uint32_t> found;                            ///< Query results.
	std::vector<Rect> rects;                                     ///< Rectangles: snap targets, the selection.
	std::vector<RulerTick> ticks;                                ///< The rulers' ticks.
	std::vector<CanvasTransform> pressed;                        ///< The selection at the press.
	std::array<std::optional<SnapLine>, 2> lines;                ///< The smart guides of the drag.
};

namespace canvas {

constexpr float snapPixels = 6.0f;     ///< How near, in screen units, a snap pulls.
constexpr float guidePixels = 4.0f;    ///< How near, in screen units, a guide is grabbed.
constexpr float clickPixels = 3.0f;    ///< A marquee smaller than this, in screen units, is a click.
constexpr float rotateOffset = 24.0f;  ///< How far above the selection the rotation handle is, in screen units.
constexpr float readablePixels = 6.0f; ///< Text shorter than this on screen draws as a bar.
constexpr float fitMargin = 40.0f;     ///< The margin fit() leaves, in screen units.
constexpr float stepZoom = 1.25f;      ///< The zoom factor of a key press.

/// @brief A rectangle with its edges in order.
/// @param a One corner.
/// @param b The opposite corner.
/// @return The rectangle.
[[nodiscard]] constexpr Rect spanning(Vec2 a, Vec2 b) noexcept {
	return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.x < b.x ? b.x : a.x, a.y < b.y ? b.y : a.y};
}

/// @brief Whether two rectangles meet, edges included.
/// @param a One.
/// @param b The other.
[[nodiscard]] constexpr bool meet(const Rect& a, const Rect& b) noexcept {
	return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

/// @brief The smallest rectangle around two.
/// @param a One.
/// @param b The other.
[[nodiscard]] constexpr Rect united(const Rect& a, const Rect& b) noexcept {
	return {a.left < b.left ? a.left : b.left, a.top < b.top ? a.top : b.top, a.right > b.right ? a.right : b.right,
	        a.bottom > b.bottom ? a.bottom : b.bottom};
}

/// @brief Maps a rectangle inside one group box to the same place inside another: how a move or resize of the
/// selection carries each selected item.
/// @param rect The item's rectangle.
/// @param from The group box at the press.
/// @param to The group box now.
/// @return The item's rectangle now.
[[nodiscard]] inline Rect carried(const Rect& rect, const Rect& from, const Rect& to) noexcept {
	const float scaleX = from.width() > 0.0f ? to.width() / from.width() : 1.0f;
	const float scaleY = from.height() > 0.0f ? to.height() / from.height() : 1.0f;
	return {to.left + (rect.left - from.left) * scaleX, to.top + (rect.top - from.top) * scaleY,
	        to.left + (rect.right - from.left) * scaleX, to.top + (rect.bottom - from.top) * scaleY};
}

/// @brief Where a selection handle is, on a group box on screen.
/// @param box The box, in window units.
/// @param handle 0 to 7 clockwise from the top-left, 8 for rotation.
/// @return The handle's centre.
[[nodiscard]] constexpr Vec2 handlePoint(const Rect& box, int handle) noexcept {
	const float middleX = (box.left + box.right) / 2.0f, middleY = (box.top + box.bottom) / 2.0f;
	constexpr std::array<std::array<int, 2>, 8> places{
	    {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}}};
	if(handle < 0 || handle > 7) return {middleX, box.top - rotateOffset};
	const auto [column, row] = places[static_cast<std::size_t>(handle)];
	return {column == 0   ? box.left
	        : column == 1 ? middleX
	                      : box.right,
	        row == 0   ? box.top
	        : row == 1 ? middleY
	                   : box.bottom};
}

} // namespace canvas
} // namespace affresco
