#pragma once

/// @file
/// @brief What a panel drew last, kept until it changes: its quads, and the steps that put them in batches
/// (detail::PaintRecord), and what that drawing depended on (detail::PaintKey). Plain data: no OpenGL here.

#include "affresco/render/Geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace affresco {

class Bitmap;
class Font;

namespace detail {

/// @brief What a quad is, for the fragment shader (the vertex's last field).
enum class QuadKind : std::uint16_t {
	plain = 0,      ///< A rectangle or a glyph of coverage.
	shape = 1,      ///< A rounded rectangle, an outline or a shadow.
	colorGlyph = 2, ///< A glyph from a colour page, or an image: its texels are its colours.
};

/// @brief One vertex, 32 bytes, as the GPU reads it.
struct Vertex {
	float x, y;              ///< Position, in window units.
	std::uint16_t u, v;      ///< Texture coordinates, normalized by the GPU to [0, 1].
	std::uint8_t r, g, b, a; ///< Colour, normalized by the GPU to [0, 1].
	/// Shapes: how far inside the shape's nearest vertical and horizontal edges it is (negative outside).
	float insideX, insideY;
	/// Shapes: the corner's radius, the outline's width (0 for a filled shape) and the blur, in sixteenths of a window
	/// unit.
	std::uint16_t radius, outline, blur;
	std::uint16_t kind; ///< A QuadKind: plain quads skip the shape's work and ignore the shape's fields.
};
static_assert(sizeof(Vertex) == 32);

constexpr std::size_t verticesPerQuad = 4; ///< Corners.

/// @brief What a step of a recorded drawing does to the frame's batches when it is replayed.
enum class PaintStep : std::uint8_t {
	fill,       ///< Rectangles and shapes: the current texture's white texel (a white texture if there is none yet).
	text,       ///< A text starts: carry on with its font's page in use, or its first page.
	glyphs,     ///< Glyphs of coverage from one atlas page of the text's font.
	colorGlyph, ///< A colour glyph from one colour page of the text's font.
	image,      ///< An image: its texture on the colour unit, next to the coverage page in use.
};

/// @brief One step of a recorded drawing, and the quads it adds.
struct PaintOp {
	PaintStep step = PaintStep::fill; ///< What it does.
	std::uint32_t page = 0;           ///< glyphs, colorGlyph: the atlas page.
	const Font* font = nullptr;       ///< text, glyphs, colorGlyph: the font (alive while the panel holds it).
	std::uint32_t firstQuad = 0;      ///< Its first quad in the record's vertices.
	std::uint32_t quads = 0;          ///< Its quads (0 when all were clipped away: the step still changes batches).
	const Bitmap* bitmap = nullptr;   ///< image: the image (alive while the panel holds it).
};

/// @brief What a panel's paint() and paintOver() drew: the quads, already clipped, and the steps that place them in
/// batches. Replaying a record into a frame gives the batches and vertices drawing it directly gave, wherever the
/// panel falls in the frame (fills take the white texel of whatever texture is in use then).
struct PaintRecord {
	std::vector<Vertex> vertices; ///< 4 per quad; fills and shapes get their texture coordinates when replayed.
	std::vector<PaintOp> ops;     ///< The steps, in order.
	std::size_t overStart = 0;    ///< The first step of paintOver(), drawn after the children.
	std::optional<Rect> extent;   ///< What the quads cover, if any.
	bool shapes = false;          ///< A shape quad is in it: the frame needs the shader with shapes.
	bool color = false;           ///< A colour page or an image is in it: the frame needs the colour shader.

	/// @brief Empties it, keeping its capacity.
	void clear() noexcept {
		vertices.clear();
		ops.clear();
		overStart = 0;
		extent.reset();
		shapes = color = false;
	}
};

/// @brief What a record depended on besides the panel's own properties (whose changes call invalidate()): when it is
/// the same, the panel draws the same.
struct PaintKey {
	Rect frame;                   ///< The panel's frame.
	Rect bounds;                  ///< Its bounds: what clips paint().
	Rect outerClip;               ///< What clips its shadow: its parent's bounds, or the window.
	Vec2 scroll;                  ///< Its scroll offset (the scroll bar).
	Vec2 contentMin, contentMax;  ///< Its children's extent (the scroll bar).
	std::uint64_t generation = 0; ///< The window's paint generation (Window::invalidate() starts a new one).

	/// @brief Equal when every member is equal.
	bool operator==(const PaintKey&) const = default;
};

/// @brief What a panel keeps of its drawing, owned by the panel; the renderer fills it.
struct PanelGeometry {
	PaintRecord record;               ///< What it drew.
	PaintKey key;                     ///< What that depended on.
	bool recorded = false;            ///< record and key hold a drawing (the panel was shown when last refreshed).
	bool subtreeShapes = false;       ///< It or a descendant (not an overlay) is shown and has shapes.
	bool subtreeColor = false;        ///< It or a descendant (not an overlay) is shown and uses colour pages.
	bool cached = false;              ///< It is shown and cached (Panel::setCached()): drawn as one colour quad.
	std::uint64_t id = 0;             ///< Unique among geometries: names its subtree's texture.
	std::uint64_t subtreeVersion = 0; ///< For a cached panel: changes when its subtree's drawing may have.
};

/// @brief Whether two rectangles overlap (touching edges don't).
/// @param a One.
/// @param b The other.
/// @return true if they share an area.
constexpr bool overlaps(const Rect& a, const Rect& b) noexcept {
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

/// @brief The smallest rectangle holding both.
/// @param a One.
/// @param b The other.
/// @return Their union.
constexpr Rect unite(const Rect& a, const Rect& b) noexcept {
	return {a.left < b.left ? a.left : b.left, a.top < b.top ? a.top : b.top, a.right > b.right ? a.right : b.right,
	        a.bottom > b.bottom ? a.bottom : b.bottom};
}

/// @brief The smallest rectangle holding a quad's vertices (a mesh's quads are any convex shape, not rectangles).
/// @param corners Its vertices.
/// @return Their bounds.
constexpr Rect boundsOf(std::span<const Vertex> corners) noexcept {
	Rect bounds{corners[0].x, corners[0].y, corners[0].x, corners[0].y};
	for(const Vertex& corner : corners.subspan(1)) bounds = unite(bounds, {corner.x, corner.y, corner.x, corner.y});
	return bounds;
}

/// @brief Adds a rectangle to a damage region, kept as one rectangle.
/// @param damage The region; nullopt when empty.
/// @param rect What to add.
constexpr void addTo(std::optional<Rect>& damage, const Rect& rect) noexcept {
	if(!(rect.right > rect.left) || !(rect.bottom > rect.top)) return;
	damage = damage ? unite(*damage, rect) : rect;
}

} // namespace detail
} // namespace affresco
