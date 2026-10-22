#pragma once

/// @file
/// @brief Camera2D: the view of an infinite canvas, a world point and a zoom. A pure value type: world to screen and
/// back, zoom at a point, fit a rectangle, the visible rectangle.

#include "affresco/render/Geometry.hpp"

#include <algorithm>

namespace affresco {

/// @brief The camera of an infinite canvas: the world point at the view's top-left corner and the zoom (screen units
/// per world unit). Screen points are relative to the view's top-left corner.
struct Camera2D {
	static constexpr float minimumZoom = 0.01f;  ///< The smallest zoom: 100 world units a screen unit.
	static constexpr float maximumZoom = 256.0f; ///< The largest zoom: 256 screen units a world unit.

	float x = 0.0f;    ///< The world x at the view's left edge.
	float y = 0.0f;    ///< The world y at the view's top edge.
	float zoom = 1.0f; ///< Screen units per world unit, within [minimumZoom, maximumZoom].

	/// @brief A zoom brought within the range; not a number becomes 1.
	/// @param zoom The zoom asked for.
	/// @return The zoom used.
	[[nodiscard]] static constexpr float clampZoom(float zoom) noexcept {
		if(!(zoom == zoom)) return 1.0f; // NaN
		return std::clamp(zoom, minimumZoom, maximumZoom);
	}

	/// @brief Maps a world point to the screen.
	/// @param world The point in world units.
	/// @return The point in screen units, from the view's top-left.
	[[nodiscard]] constexpr Vec2 toScreen(Vec2 world) const noexcept {
		return {(world.x - x) * zoom, (world.y - y) * zoom};
	}
	/// @brief Maps a screen point to the world.
	/// @param screen The point in screen units, from the view's top-left.
	/// @return The point in world units.
	[[nodiscard]] constexpr Vec2 toWorld(Vec2 screen) const noexcept {
		return {screen.x / zoom + x, screen.y / zoom + y};
	}
	/// @brief Maps a world rectangle to the screen.
	/// @param world The rectangle in world units.
	/// @return The rectangle in screen units, from the view's top-left.
	[[nodiscard]] constexpr Rect toScreen(Rect world) const noexcept {
		const Vec2 from = toScreen(Vec2{world.left, world.top}), to = toScreen(Vec2{world.right, world.bottom});
		return {from.x, from.y, to.x, to.y};
	}
	/// @brief Maps a screen rectangle to the world.
	/// @param screen The rectangle in screen units, from the view's top-left.
	/// @return The rectangle in world units.
	[[nodiscard]] constexpr Rect toWorld(Rect screen) const noexcept {
		const Vec2 from = toWorld(Vec2{screen.left, screen.top}), to = toWorld(Vec2{screen.right, screen.bottom});
		return {from.x, from.y, to.x, to.y};
	}

	/// @brief The world rectangle a view of a size shows.
	/// @param viewSize The view's width and height in screen units.
	/// @return The visible rectangle, in world units.
	[[nodiscard]] constexpr Rect visible(Vec2 viewSize) const noexcept {
		return toWorld(Rect{0.0f, 0.0f, viewSize.x, viewSize.y});
	}

	/// @brief The camera zoomed so that the world point under a screen point stays under it.
	/// @param screenPoint The fixed point, in screen units from the view's top-left (e.g. the pointer).
	/// @param newZoom The zoom asked for; clamped.
	/// @return The new camera.
	[[nodiscard]] constexpr Camera2D zoomedAt(Vec2 screenPoint, float newZoom) const noexcept {
		const Vec2 world = toWorld(screenPoint);
		const float zoomed = clampZoom(newZoom);
		return {world.x - screenPoint.x / zoomed, world.y - screenPoint.y / zoomed, zoomed};
	}

	/// @brief The camera moved so that the world follows the pointer by a screen distance (a drag to the right shows
	/// what is to the left).
	/// @param screenDelta The distance in screen units.
	/// @return The new camera.
	[[nodiscard]] constexpr Camera2D pannedBy(Vec2 screenDelta) const noexcept {
		return {x - screenDelta.x / zoom, y - screenDelta.y / zoom, zoom};
	}

	/// @brief The camera that shows a world rectangle whole and centred in a view, with a margin around it.
	/// @param world The rectangle to show; an empty one is centred at zoom 1.
	/// @param viewSize The view's width and height in screen units.
	/// @param margin Screen units left free on each side (shrunk when the view is too small for it).
	/// @return The camera; its zoom is clamped, so a rectangle too large or too small may not fill the view.
	[[nodiscard]] static constexpr Camera2D fitted(Rect world, Vec2 viewSize, float margin) noexcept {
		const float room = std::max(std::min({margin, viewSize.x / 4.0f, viewSize.y / 4.0f}), 0.0f);
		const float width = std::max(viewSize.x - 2.0f * room, 1.0f), height = std::max(viewSize.y - 2.0f * room, 1.0f);
		const float zoom = world.width() > 0.0f && world.height() > 0.0f
		                       ? clampZoom(std::min(width / world.width(), height / world.height()))
		                       : 1.0f;
		const float centreX = (world.left + world.right) / 2.0f, centreY = (world.top + world.bottom) / 2.0f;
		return {centreX - viewSize.x / 2.0f / zoom, centreY - viewSize.y / 2.0f / zoom, zoom};
	}

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const Camera2D&) const noexcept = default;
};

} // namespace affresco
