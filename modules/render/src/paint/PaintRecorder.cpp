/// @file
/// @brief detail::PaintRecorder: clipped quads and batch steps, recorded the way the renderer used to draw them.

#include "paint/PaintRecorder.hpp"

#include "affresco/render/Font.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

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

/// @brief A mesh's quad clipped to a rectangle: a convex polygon (4 corners cut by 4 edges is at most 8; the room
/// past that keeps a quad that isn't convex in bounds).
struct MeshPolygon {
	std::array<MeshVertex, 12> corners{}; ///< Its corners, in order around it.
	std::size_t count = 0;                ///< How many there are.

	/// @brief Adds a corner, unless it is full.
	void add(const MeshVertex& corner) noexcept {
		if(count < corners.size()) corners[count++] = corner;
	}
};

/// @brief Cuts a polygon along a line, keeping the side where @p distance isn't negative (Sutherland-Hodgman).
/// @param polygon The polygon.
/// @param distance How far a point is inside the line.
/// @return What is kept, with the coverage interpolated at the new corners.
template <typename Distance> MeshPolygon cut(const MeshPolygon& polygon, Distance distance) {
	MeshPolygon kept;
	for(std::size_t index = 0; index < polygon.count; ++index) {
		const MeshVertex& from = polygon.corners[index];
		const MeshVertex& to = polygon.corners[(index + 1) % polygon.count];
		const float start = distance(from.position), end = distance(to.position);
		if(start >= 0.0f) kept.add(from);
		if((start < 0.0f) == (end < 0.0f)) continue;
		const float along = start / (start - end);
		kept.add({{from.position.x + (to.position.x - from.position.x) * along,
		           from.position.y + (to.position.y - from.position.y) * along},
		          from.coverage + (to.coverage - from.coverage) * along});
	}
	return kept;
}

/// @brief Clips a mesh's quad to a rectangle.
/// @param corners The quad.
/// @param clip The rectangle.
/// @return What is inside it.
MeshPolygon clipQuad(const std::array<MeshVertex, 4>& corners, const Rect& clip) {
	MeshPolygon polygon;
	for(const MeshVertex& corner : corners) polygon.add(corner);
	polygon = cut(polygon, [&](Vec2 point) { return point.x - clip.left; });
	polygon = cut(polygon, [&](Vec2 point) { return clip.right - point.x; });
	polygon = cut(polygon, [&](Vec2 point) { return point.y - clip.top; });
	return cut(polygon, [&](Vec2 point) { return clip.bottom - point.y; });
}

/// @brief Where the points of a rectangle fall along a gradient filling it, as in CSS.
struct GradientPlace {
	Vec2 centre; ///< The rectangle's centre.
	Vec2 step;   ///< Linear: the position's change per unit right and down. Radial: the ellipse's scales.
	bool radial; ///< Whether it is radial.

	/// @brief Places a gradient in a rectangle.
	/// @param gradient The gradient.
	/// @param rect The rectangle, not empty.
	GradientPlace(const Gradient& gradient, const Rect& rect) noexcept
	    : centre{(rect.left + rect.right) / 2.0f, (rect.top + rect.bottom) / 2.0f},
	      radial(gradient.kind == Gradient::Kind::Radial) {
		if(radial) { // an ellipse through the corners, its sides in the rectangle's proportions
			step = {std::numbers::sqrt2_v<float> / rect.width(), std::numbers::sqrt2_v<float> / rect.height()};
			return;
		}
		const double radians = gradient.angle * std::numbers::pi / 180.0;
		const double across = std::sin(radians), down = -std::cos(radians);
		const double length = std::abs(rect.width() * across) + std::abs(rect.height() * down);
		step = {static_cast<float>(across / length), static_cast<float>(down / length)};
	}

	/// @brief The position of a point.
	/// @param point The point.
	/// @return Its position along the gradient: 0 at the start, 1 at the end, beyond them outside.
	[[nodiscard]] float at(Vec2 point) const noexcept {
		const float x = point.x - centre.x, y = point.y - centre.y;
		return radial ? std::hypot(x * step.x, y * step.y) : 0.5f + x * step.x + y * step.y;
	}
};

/// @brief A rectangle as a polygon.
/// @param area The rectangle.
/// @return Its corners, clockwise from the top left.
MeshPolygon polygonOf(const Rect& area) noexcept {
	MeshPolygon result;
	result.add({{area.left, area.top}, 1.0f});
	result.add({{area.right, area.top}, 1.0f});
	result.add({{area.right, area.bottom}, 1.0f});
	result.add({{area.left, area.bottom}, 1.0f});
	return result;
}

/// @brief The area of a polygon (shoelace).
/// @param polygon The polygon.
/// @return Its area; 0 for a line or a point, which a cut along an edge leaves.
float areaOf(const MeshPolygon& polygon) noexcept {
	float twice = 0.0f;
	for(std::size_t index = 0; index < polygon.count; ++index) {
		const Vec2 from = polygon.corners[index].position;
		const Vec2 to = polygon.corners[(index + 1) % polygon.count].position;
		twice += from.x * to.y - to.x * from.y;
	}
	return std::abs(twice) / 2.0f;
}

/// @brief A gradient's stops ready to be mixed in straight alpha, as CSS mixes them premultiplied: a transparent
/// stop takes the colour of its neighbour on each side (two stops at one place), so a fade to transparent doesn't
/// darken through black.
/// @param stops The stops, sorted.
/// @return The stops to mix.
std::vector<GradientStop> fadingStops(std::span<const GradientStop> stops) {
	std::vector<GradientStop> result;
	result.reserve(stops.size() * 2);
	for(std::size_t index = 0; index < stops.size(); ++index) {
		const GradientStop& stop = stops[index];
		if(stop.color.a > 0.0f || stops.size() == 1) {
			result.push_back(stop);
			continue;
		}
		if(index > 0) result.push_back({stop.at, stops[index - 1].color.withAlpha(0.0f)});
		if(index + 1 < stops.size()) result.push_back({stop.at, stops[index + 1].color.withAlpha(0.0f)});
	}
	return result;
}

/// @brief Cuts a piece of a linear gradient where its colour stops change, each part mixing two stops' colours
/// linearly (before the first stop and after the last, one colour). Each stop's line cuts what is left, so
/// neighbouring parts share their corners exactly.
/// @param piece The piece.
/// @param place Where its points fall along the gradient.
/// @param stops The stops, sorted.
/// @param add Called with each part and its colour at a point.
template <typename Add>
void linearParts(const Rect& piece, const GradientPlace& place, std::span<const GradientStop> stops, const Add& add) {
	MeshPolygon rest = polygonOf(piece);
	for(std::size_t index = 0; index <= stops.size() && rest.count >= 3; ++index) {
		const GradientStop& low = stops[index == 0 ? 0 : index - 1];
		const GradientStop& high = stops[std::min(index, stops.size() - 1)];
		if(index > 0 && index < stops.size() && !(high.at > low.at)) continue; // two stops at one place: no part
		MeshPolygon part = rest;
		if(index < stops.size()) {
			const auto line = static_cast<float>(high.at);
			part = cut(rest, [&](Vec2 point) { return line - place.at(point); });
			rest = cut(rest, [&](Vec2 point) { return place.at(point) - line; });
		}
		if(!(areaOf(part) > 0.0f)) continue; // a cut along the piece's edge
		const double span = high.at - low.at;
		add(part, [&](Vec2 point) {
			const double along = span > 0.0 ? std::clamp((place.at(point) - low.at) / span, 0.0, 1.0) : 0.0;
			return Color::mix(low.color, high.color, static_cast<float>(along));
		});
	}
}

/// @brief Cuts a piece of a radial gradient into cells, about one every 12 units along each side (at most 12), small
/// enough that the colours sampled at their corners, mixed, look the same.
/// @param piece The piece.
/// @param place Where its points fall along the gradient.
/// @param stops The stops, sorted.
/// @param add Called with each cell and its colour at a point.
template <typename Add>
void radialCells(const Rect& piece, const GradientPlace& place, std::span<const GradientStop> stops, const Add& add) {
	const auto cells = [](float length) {
		return static_cast<std::size_t>(std::clamp(std::ceil(length / 12.0f), 1.0f, 12.0f));
	};
	const std::size_t columns = cells(piece.width()), rows = cells(piece.height());
	const float width = piece.width() / static_cast<float>(columns);
	const float height = piece.height() / static_cast<float>(rows);
	const auto colorAt = [&](Vec2 point) { return sampleGradient(stops, place.at(point)); };
	for(std::size_t row = 0; row < rows; ++row)
		for(std::size_t column = 0; column < columns; ++column) {
			const float left = piece.left + width * static_cast<float>(column);
			const float top = piece.top + height * static_cast<float>(row);
			const float right = column + 1 == columns ? piece.right : left + width;
			const float bottom = row + 1 == rows ? piece.bottom : top + height;
			add(polygonOf({left, top, right, bottom}), colorAt);
		}
}

} // namespace

// Consecutive fills (rectangles and shapes alike) are one step: they replay the same way
void PaintRecorder::beginStep(PaintStep step, const Font* font, std::uint32_t page) {
	if(step == PaintStep::fill && !record_.ops.empty() && record_.ops.back().step == PaintStep::fill &&
	   record_.ops.size() > record_.overStart)
		return;
	const auto firstQuad = static_cast<std::uint32_t>(record_.vertices.size() / verticesPerQuad);
	record_.ops.push_back({step, page, font, firstQuad, 0});
	if(step == PaintStep::colorGlyph || step == PaintStep::image || step == PaintStep::surface) record_.color = true;
}

void PaintRecorder::addQuad(const std::array<Vertex, 4>& corners) {
	record_.vertices.insert(record_.vertices.end(), corners.begin(), corners.end());
	++record_.ops.back().quads;
	addTo(record_.extent, boundsOf(corners));
}

void PaintRecorder::smoothEdges(const Rect& frame) noexcept {
	smooth_ = true;
	if(clip_.left == frame.left) shapeClip_.left -= 1.0f;
	if(clip_.top == frame.top) shapeClip_.top -= 1.0f;
	if(clip_.right == frame.right) shapeClip_.right += 1.0f;
	if(clip_.bottom == frame.bottom) shapeClip_.bottom += 1.0f;
}

void PaintRecorder::fillRect(const Rect& rect, Color color) {
	beginStep(PaintStep::fill); // even if the quad is dropped: a first fill gives the frame a white texture
	if(smooth_) pushShape(rect, {}, {.color = color}, shapeClip_);
	else pushQuad(rect, Rect{}, color);
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

// Each quad on its own: inside the panel as it is, across its edge clipped and drawn as a fan of quads
void PaintRecorder::fillMesh(std::span<const MeshVertex> quads, Vec2 offset, Color color) {
	if(color.toBytes()[3] == 0) return;
	beginStep(PaintStep::fill);
	for(std::size_t first = 0; first + verticesPerQuad <= quads.size(); first += verticesPerQuad) {
		std::array<MeshVertex, 4> corners{};
		for(std::size_t index = 0; index < corners.size(); ++index) {
			const MeshVertex& corner = quads[first + index];
			corners[index] = {{corner.position.x + offset.x, corner.position.y + offset.y}, corner.coverage};
		}
		pushMeshQuad(corners, color);
	}
}

void PaintRecorder::pushMeshQuad(const std::array<MeshVertex, 4>& corners, Color color) {
	const auto [cr, cg, cb, ca] = color.toBytes();
	const auto vertex = [&](const MeshVertex& corner) {
		const auto alpha = static_cast<std::uint8_t>(std::lround(std::clamp(corner.coverage, 0.0f, 1.0f) * ca));
		return Vertex{corner.position.x, corner.position.y, 0, 0, cr, cg, cb, alpha, 0.0f, 0.0f, 0, 0, 0, 0};
	};
	const auto add = [&](const MeshVertex& a, const MeshVertex& b, const MeshVertex& c, const MeshVertex& d) {
		const std::array<Vertex, 4> quad{vertex(a), vertex(b), vertex(c), vertex(d)};
		if(quad[0].a != 0 || quad[1].a != 0 || quad[2].a != 0 || quad[3].a != 0) addQuad(quad);
	};

	Rect bounds{corners[0].position.x, corners[0].position.y, corners[0].position.x, corners[0].position.y};
	for(const MeshVertex& corner : corners)
		bounds = unite(bounds, {corner.position.x, corner.position.y, corner.position.x, corner.position.y});
	if(!overlaps(bounds, clip_)) return; // also drops NaN
	if(intersect(bounds, clip_) == bounds) return add(corners[0], corners[1], corners[2], corners[3]);

	const MeshPolygon polygon = clipQuad(corners, clip_);
	for(std::size_t index = 1; index + 1 < polygon.count; index += 2) {
		const std::size_t last = std::min(index + 2, polygon.count - 1);
		add(polygon.corners[0], polygon.corners[index], polygon.corners[index + 1], polygon.corners[last]);
	}
}

// Its coordinates are within the image: where it lies in its texture is known only when it is replayed
void PaintRecorder::drawImage(const Bitmap& bitmap, const Rect& rect) {
	beginStep(PaintStep::image);
	record_.ops.back().bitmap = &bitmap;
	pushQuad(rect, {0.0f, 0.0f, 1.0f, 1.0f}, colors::white, QuadKind::colorGlyph);
}

// The framebuffer's bottom row (v = 0) is the rectangle's bottom
void PaintRecorder::drawSurface(const SurfaceView& view, const Rect& rect) {
	beginStep(PaintStep::surface);
	record_.ops.back().surface = &view;
	pushQuad(rect, {0.0f, 1.0f, 1.0f, 0.0f}, colors::white, QuadKind::colorGlyph);
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
	pushShape(rect, radii, {.color = color}, shapeClip_);
}

void PaintRecorder::strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) {
	if(width > 0.0f) pushShape(rect, radii, {.color = color, .outline = width}, shapeClip_);
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
	for(const Quarter& quarter : quartersOf(rect, radii, 1.0f + style.blur * 1.5f))
		pushQuarter(rect, quarter, clamped, clip);
}

std::array<PaintRecorder::Quarter, 4> PaintRecorder::quartersOf(const Rect& rect, const CornerRadii& radii,
                                                                float margin) noexcept {
	const float largest = std::min(rect.width(), rect.height()) / 2.0f;
	const auto radius = [&](float value) { return std::clamp(value, 0.0f, largest); };
	const float centreX = (rect.left + rect.right) / 2.0f, centreY = (rect.top + rect.bottom) / 2.0f;
	return {{
	    {{rect.left - margin, rect.top - margin, centreX, centreY}, radius(radii.topLeft), true, true},
	    {{centreX, rect.top - margin, rect.right + margin, centreY}, radius(radii.topRight), false, true},
	    {{centreX, centreY, rect.right + margin, rect.bottom + margin}, radius(radii.bottomRight), false, false},
	    {{rect.left - margin, centreY, centreX, rect.bottom + margin}, radius(radii.bottomLeft), true, false},
	}};
}

// Like pushShape(), as four quarters anti-aliased by distance; square and drawn straight, one plain rectangle, as
// fillRect() draws
void PaintRecorder::fillGradient(const Rect& rect, const CornerRadii& radii, const Gradient& gradient) {
	if(!(rect.width() > 0.0f) || !(rect.height() > 0.0f) || gradient.stops.empty()) return; // also drops NaN
	beginStep(PaintStep::fill);
	const Gradient fading{gradient.kind, gradient.angle, fadingStops(gradient.stops)};
	const auto shown = [](const Rect& piece) { return piece.right > piece.left && piece.bottom > piece.top; };
	if(radii.isSquare() && !smooth_) {
		const Quarter whole{intersect(rect, clip_), 0.0f, true, true};
		if(shown(whole.piece)) pushGradientPiece(rect, whole, fading, QuadKind::plain);
		return;
	}
	for(Quarter quarter : quartersOf(rect, radii, 1.0f)) {
		quarter.piece = intersect(quarter.piece, shapeClip_);
		if(shown(quarter.piece)) pushGradientPiece(rect, quarter, fading, QuadKind::shape);
	}
	record_.shapes = true;
}

// Each part's corners take the gradient's colours there, which the GPU mixes across it
void PaintRecorder::pushGradientPiece(const Rect& rect, const Quarter& quarter, const Gradient& gradient,
                                      QuadKind kind) {
	const auto insideX = [&](float x) { return quarter.left ? x - rect.left : rect.right - x; };
	const auto insideY = [&](float y) { return quarter.top ? y - rect.top : rect.bottom - y; };
	const std::uint16_t radius = toSixteenths(quarter.radius);
	const auto marker = static_cast<std::uint16_t>(kind);
	const GradientPlace place(gradient, rect);
	const auto add = [&](const MeshPolygon& part, const auto& colorAt) {
		const auto corner = [&](const MeshVertex& point) {
			const auto [cr, cg, cb, ca] = colorAt(point.position).toBytes();
			const float x = point.position.x, y = point.position.y;
			return Vertex{x, y, 0, 0, cr, cg, cb, ca, insideX(x), insideY(y), radius, 0, 0, marker};
		};
		for(std::size_t index = 1; index + 1 < part.count; index += 2) {
			const std::size_t last = std::min(index + 2, part.count - 1);
			const std::array<Vertex, 4> quad{corner(part.corners[0]), corner(part.corners[index]),
			                                 corner(part.corners[index + 1]), corner(part.corners[last])};
			if(quad[0].a != 0 || quad[1].a != 0 || quad[2].a != 0 || quad[3].a != 0) addQuad(quad);
		}
	};
	if(place.radial) radialCells(quarter.piece, place, gradient.stops, add);
	else linearParts(quarter.piece, place, gradient.stops, add);
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
