/// @file
/// @brief detail::PaintRecorder: clipped quads and batch steps, recorded the way the renderer used to draw them.

#include "PaintRecorder.hpp"

#include "affresco/render/Font.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::detail {

namespace {

/// @brief Converts to a normalized 16-bit integer.
/// @param value In [0, 1]; clamped.
/// @return In [0, 65535].
std::uint16_t toUnorm16(float value) noexcept {
	return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
}

/// @brief The longest radius, outline or blur a vertex holds, in window units.
constexpr float longestShapeLength = 65535.0f / 16.0f;

/// @brief Converts a shape's length to the vertex's sixteenths of a window unit.
/// @param value In window units; clamped to [0, longestShapeLength].
/// @return In [0, 65535].
std::uint16_t toSixteenths(float value) noexcept {
	return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, longestShapeLength) * 16.0f));
}

/// @brief The part of a rectangle inside another one.
/// @param rect The rectangle.
/// @param clip The other one.
/// @return The intersection; empty (right <= left or bottom <= top) if they don't overlap.
Rect intersect(const Rect& rect, const Rect& clip) noexcept {
	return {std::max(rect.left, clip.left), std::max(rect.top, clip.top), std::min(rect.right, clip.right),
	        std::min(rect.bottom, clip.bottom)};
}

} // namespace

// Consecutive fills (rectangles and shapes alike) are one step: they replay the same way
void PaintRecorder::beginStep(PaintStep step, const Font* font, std::uint32_t page) {
	if(step == PaintStep::fill && !record_.ops.empty() && record_.ops.back().step == PaintStep::fill &&
	   record_.ops.size() > record_.overStart)
		return;
	const auto firstQuad = static_cast<std::uint32_t>(record_.vertices.size() / verticesPerQuad);
	record_.ops.push_back({step, page, font, firstQuad, 0});
	if(step == PaintStep::colorGlyph) record_.color = true;
}

void PaintRecorder::addQuad(const std::array<Vertex, 4>& corners) {
	record_.vertices.insert(record_.vertices.end(), corners.begin(), corners.end());
	++record_.ops.back().quads;
	addTo(record_.extent, {corners[0].x, corners[0].y, corners[2].x, corners[2].y});
}

void PaintRecorder::fillRect(const Rect& rect, Color color) {
	beginStep(PaintStep::fill); // even if the quad is dropped: a first fill gives the frame a white texture
	pushQuad(rect, Rect{}, color);
}

void PaintRecorder::drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) {
	beginStep(PaintStep::text, &font);

	// Shaped glyphs come left to right, and spaces aren't among them
	for(const Font::ShapedGlyph& placed : font.shape(text).glyphs) {
		const float penX = origin.x + placed.x, baseline = origin.y + placed.y;
		if(penX > clip_.right) break; // the rest of the line is clipped anyway

		const Font::Glyph& glyph = *placed.glyph;
		const Rect quad{penX + glyph.quad.left, baseline + glyph.quad.top, penX + glyph.quad.right,
		                baseline + glyph.quad.bottom};
		if(glyph.color) {
			beginStep(PaintStep::colorGlyph, &font, glyph.page);
			pushQuad(quad, glyph.uv, Color{1.0f, 1.0f, 1.0f, color.a}, QuadKind::colorGlyph); // its own colours
			continue;
		}
		const PaintOp& last = record_.ops.back();
		if(last.step != PaintStep::glyphs || last.page != glyph.page) beginStep(PaintStep::glyphs, &font, glyph.page);
		pushQuad(quad, glyph.uv, color);
	}
}

// Clips the quad to the panel (adjusting the texture coordinates to match) and adds it
void PaintRecorder::pushQuad(const Rect& rect, const Rect& uv, Color color, QuadKind kind) {
	const auto [cr, cg, cb, ca] = color.toBytes();
	if(ca == 0) return; // invisible: e.g. pure layout containers

	const Rect q = intersect(rect, clip_);
	if(q.right <= q.left || q.bottom <= q.top) return;

	Rect t = uv;
	if(q != rect) {
		const auto lerp = [](float a, float b, float f) { return a + (b - a) * f; };
		t.left = lerp(uv.left, uv.right, (q.left - rect.left) / rect.width());
		t.right = lerp(uv.left, uv.right, (q.right - rect.left) / rect.width());
		t.top = lerp(uv.top, uv.bottom, (q.top - rect.top) / rect.height());
		t.bottom = lerp(uv.top, uv.bottom, (q.bottom - rect.top) / rect.height());
	}

	const std::uint16_t u0 = toUnorm16(t.left), v0 = toUnorm16(t.top);
	const std::uint16_t u1 = toUnorm16(t.right), v1 = toUnorm16(t.bottom);
	const auto marker = static_cast<std::uint16_t>(kind);
	addQuad({{{q.left, q.top, u0, v0, cr, cg, cb, ca, 0.0f, 0.0f, 0, 0, 0, marker},
	          {q.right, q.top, u1, v0, cr, cg, cb, ca, 0.0f, 0.0f, 0, 0, 0, marker},
	          {q.right, q.bottom, u1, v1, cr, cg, cb, ca, 0.0f, 0.0f, 0, 0, 0, marker},
	          {q.left, q.bottom, u0, v1, cr, cg, cb, ca, 0.0f, 0.0f, 0, 0, 0, marker}}});
}

void PaintRecorder::fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) {
	pushShape(rect, radii, {.color = color}, clip_);
}

void PaintRecorder::strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) {
	if(width > 0.0f) pushShape(rect, radii, {.color = color, .outline = width}, clip_);
}

// A shadow falls outside its panel, so it is clipped to what clips the panel instead
void PaintRecorder::drawShadow(const Rect& rect, const CornerRadii& radii, const Shadow& shadow) {
	const Rect moved{rect.left + shadow.offset.x, rect.top + shadow.offset.y, rect.right + shadow.offset.x,
	                 rect.bottom + shadow.offset.y};
	pushShape(moved, radii, {.color = shadow.color, .blur = std::clamp(shadow.blur, 0.0f, longestShapeLength)},
	          outerClip_);
}

// Four quarters meeting at the centre: inside a quarter, the distance to the shape's edge depends only on the two
// nearest edges and their corner, so each vertex needs just those. Shapes never break a batch: they take the current
// texture's white texel when replayed, like fillRect()
void PaintRecorder::pushShape(const Rect& rect, const CornerRadii& radii, const ShapeStyle& style, const Rect& clip) {
	if(!(rect.width() > 0.0f) || !(rect.height() > 0.0f) || style.color.toBytes()[3] == 0) return; // also drops NaN
	beginStep(PaintStep::fill);

	// A radius past the centre would reach into the next quarter; an outline that wide is the whole shape
	const float largest = std::min(rect.width(), rect.height()) / 2.0f;
	ShapeStyle clamped = style;
	if(clamped.outline >= largest) clamped.outline = 0.0f;

	// Room past the edges for the anti-aliased edge, or three standard deviations of the blur
	const float margin = 1.0f + style.blur * 1.5f;
	const float centreX = (rect.left + rect.right) / 2.0f, centreY = (rect.top + rect.bottom) / 2.0f;
	const std::array<Quarter, 4> quarters{{
	    {{rect.left - margin, rect.top - margin, centreX, centreY}, radii.topLeft, true, true},
	    {{centreX, rect.top - margin, rect.right + margin, centreY}, radii.topRight, false, true},
	    {{centreX, centreY, rect.right + margin, rect.bottom + margin}, radii.bottomRight, false, false},
	    {{rect.left - margin, centreY, centreX, rect.bottom + margin}, radii.bottomLeft, true, false},
	}};
	for(Quarter quarter : quarters) {
		quarter.radius = std::clamp(quarter.radius, 0.0f, largest);
		pushQuarter(rect, quarter, clamped, clip);
	}
}

void PaintRecorder::pushQuarter(const Rect& rect, const Quarter& quarter, const ShapeStyle& style, const Rect& clip) {
	const Rect q = intersect(quarter.piece, clip);
	if(q.right <= q.left || q.bottom <= q.top) return;

	// Linear in the position, so the GPU interpolates them exactly
	const auto insideX = [&](float x) { return quarter.left ? x - rect.left : rect.right - x; };
	const auto insideY = [&](float y) { return quarter.top ? y - rect.top : rect.bottom - y; };

	const auto [cr, cg, cb, ca] = style.color.toBytes();
	const std::uint16_t radius = toSixteenths(quarter.radius), outline = toSixteenths(style.outline),
	                    blur = toSixteenths(style.blur);
	const auto shape = static_cast<std::uint16_t>(QuadKind::shape);
	const auto corner = [&](float x, float y) {
		return Vertex{x, y, 0, 0, cr, cg, cb, ca, insideX(x), insideY(y), radius, outline, blur, shape};
	};
	addQuad({corner(q.left, q.top), corner(q.right, q.top), corner(q.right, q.bottom), corner(q.left, q.bottom)});
	record_.shapes = true;
}

} // namespace affresco::detail
