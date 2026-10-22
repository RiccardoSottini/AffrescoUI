#pragma once

/// @file
/// @brief Painter: what a panel draws with, and the shapes it can draw (rounded corners, outlines, shadows).

#include "affresco/render/Color.hpp"
#include "affresco/render/Geometry.hpp"

#include <span>
#include <string_view>

namespace affresco {

class Bitmap;
class Font;
class SurfaceView;

/// @brief A corner of a quad in a mesh: where it is, and how much of the mesh's colour it takes there.
struct MeshVertex {
	Vec2 position;         ///< In window units, before the mesh's offset.
	float coverage = 1.0f; ///< In [0, 1]: 1 inside the shape, 0 at the outer edge of its fringe.

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const MeshVertex&) const noexcept = default;
};

/// @brief The radius of each corner of a rounded rectangle, in window units. All zero is a plain rectangle.
struct CornerRadii {
	float topLeft = 0.0f;     ///< Top-left corner.
	float topRight = 0.0f;    ///< Top-right corner.
	float bottomRight = 0.0f; ///< Bottom-right corner.
	float bottomLeft = 0.0f;  ///< Bottom-left corner.

	/// @brief The same radius on every corner.
	/// @param radius The radius.
	/// @return The radii.
	[[nodiscard]] static constexpr CornerRadii all(float radius) noexcept { return {radius, radius, radius, radius}; }

	/// @brief Tells whether every corner is square.
	[[nodiscard]] constexpr bool isSquare() const noexcept {
		return topLeft <= 0.0f && topRight <= 0.0f && bottomRight <= 0.0f && bottomLeft <= 0.0f;
	}

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const CornerRadii&) const noexcept = default;
};

/// @brief A soft shadow under a panel, like CSS box-shadow: the panel's shape, moved and blurred.
struct Shadow {
	Vec2 offset{};                     ///< How far it is moved from the panel (positive: right and down).
	float blur = 0.0f;                 ///< How soft its edge is, like CSS: the Gaussian's standard deviation is half.
	Color color = colors::transparent; ///< Its colour; transparent (the default) means no shadow.

	/// @brief Tells whether it draws anything.
	[[nodiscard]] constexpr bool isVisible() const noexcept { return color.a > 0.0f; }

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const Shadow&) const noexcept = default;
};

/// @brief What a panel draws with in Panel::paint. Everything drawn is clipped to the panel's visible bounds.
///
/// The shape functions have fallbacks for painters that only fill rectangles: square corners, a square outline and no
/// shadow.
class Painter {
public:
	virtual ~Painter() = default;

	/// @brief Fills a rectangle.
	/// @param rect In window units.
	/// @param color The colour; transparent rectangles are skipped.
	virtual void fillRect(const Rect& rect, Color color) = 0;

	/// @brief Draws one line of text.
	/// @param font The font.
	/// @param text The text (no line breaking here).
	/// @param origin Left end of the baseline, in window units.
	/// @param color The colour.
	virtual void drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) = 0;

	/// @brief Draws an image, stretched to a rectangle. Painters that can't draw images draw nothing.
	/// @param bitmap The image.
	/// @param rect Where it goes, in window units; the part outside the panel is clipped off.
	virtual void drawImage(const Bitmap& bitmap, const Rect& rect) {
		static_cast<void>(bitmap);
		static_cast<void>(rect);
	}

	/// @brief Draws a panel's GPU surface, its framebuffer stretched to a rectangle. Painters that can't draw surfaces
	/// draw nothing.
	/// @param view The panel showing it.
	/// @param rect Where it goes, in window units; the part outside the panel is clipped off.
	virtual void drawSurface(const SurfaceView& view, const Rect& rect) {
		static_cast<void>(view);
		static_cast<void>(rect);
	}

	/// @brief Fills a rectangle with rounded corners, with anti-aliased edges.
	/// @param rect In window units.
	/// @param radii Each corner's radius; each is clamped to half the shorter side.
	/// @param color The colour; transparent shapes are skipped.
	virtual void fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) {
		static_cast<void>(radii);
		fillRect(rect, color);
	}

	/// @brief Draws the outline of a rectangle with rounded corners, inside its edges (like a CSS border).
	/// @param rect In window units.
	/// @param radii Each corner's outer radius, as in fillRoundedRect(); the inner corners are that much smaller.
	/// @param width How thick the outline is.
	/// @param color The colour; transparent outlines are skipped.
	virtual void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) {
		static_cast<void>(radii);
		fillRect({rect.left, rect.top, rect.right, rect.top + width}, color);
		fillRect({rect.left, rect.bottom - width, rect.right, rect.bottom}, color);
		fillRect({rect.left, rect.top + width, rect.left + width, rect.bottom - width}, color);
		fillRect({rect.right - width, rect.top + width, rect.right, rect.bottom - width}, color);
	}

	/// @brief Draws the shadow of a rounded rectangle. Unlike the rest, it is clipped to the area that clips the
	/// panel (its parent's), not to the panel, so it can fall outside it.
	/// @param rect The shape casting it, in window units, before the shadow's offset.
	/// @param radii Its corners, as in fillRoundedRect().
	/// @param shadow The shadow.
	virtual void drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) {
		static_cast<void>(rect);
		static_cast<void>(radii);
		static_cast<void>(shadow);
	}

	/// @brief Fills a mesh of convex quads (a vector path's fill or stroke), each vertex taking the colour times its
	/// coverage. Painters that can't draw meshes draw nothing.
	/// @param quads The quads, 4 vertices each, in order around the quad (a triangle repeats its last vertex).
	/// @param offset Added to every position.
	/// @param color The colour; transparent meshes are skipped.
	virtual void fillMesh(std::span<const MeshVertex> quads, Vec2 offset, Color color) {
		static_cast<void>(quads);
		static_cast<void>(offset);
		static_cast<void>(color);
	}
};

} // namespace affresco
