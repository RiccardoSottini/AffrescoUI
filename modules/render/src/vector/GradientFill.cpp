/// @file
/// @brief appendGradientFill: each triangle of a fill clipped against the convex cells of a gradient (strips, or ring
/// sectors cut by chords), the pieces gathered by colour.

#include "affresco/render/GradientFill.hpp"
#include "affresco/render/VectorPath.hpp"

#include "VectorMath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace affresco {

using vector::dot;
using vector::length;

namespace {

constexpr int linearStrips = 128;      ///< The bands of a linear gradient.
constexpr int radialRings = 96;        ///< The bands of a radial gradient inside its farthest corner.
constexpr int radialWedges = 32;       ///< The sectors each ring is cut into, so that its sides are straight chords.
constexpr std::size_t cornerLimit = 8; ///< A triangle cut by at most 4 half-planes has at most 7 corners.
constexpr float farCorner = std::numbers::sqrt2_v<float> / 2.0f; ///< The distance to a box's corner, in box units.

/// @brief The index of the cell a value falls in, cells [k, k + 1) from 0 to a count, the first and last unbounded.
/// @param value The value, in cells (NaN in the first).
/// @param last The last cell's index.
/// @return The index.
[[nodiscard]] int cellOf(float value, int last) noexcept {
	if(!(value >= 0.0f)) return 0;
	if(value >= static_cast<float>(last)) return last;
	return static_cast<int>(value);
}

/// @brief A convex polygon: a triangle and what is left of it after clipping.
struct Polygon {
	std::array<MeshVertex, cornerLimit> corners{}; ///< Its corners, in order around it.
	std::size_t count = 0;                         ///< How many of them there are.
};

/// @brief The points p with dot(normal, p) >= offset, in box units.
struct HalfPlane {
	Vec2 normal;         ///< Pointing inside.
	float offset = 0.0f; ///< The inside's distance along the normal.
};

/// @brief Clips a convex polygon to a half-plane, the coverage interpolated along the edges it cuts.
/// @param polygon The polygon, changed in place.
/// @param plane The half-plane it keeps.
void clip(Polygon& polygon, const HalfPlane& plane) {
	Polygon kept;
	for(std::size_t index = 0; index < polygon.count; ++index) {
		const MeshVertex& from = polygon.corners[index];
		const MeshVertex& to = polygon.corners[(index + 1) % polygon.count];
		const float fromSide = dot(plane.normal, from.position) - plane.offset;
		const float toSide = dot(plane.normal, to.position) - plane.offset;
		if(fromSide >= 0.0f && kept.count < cornerLimit) kept.corners[kept.count++] = from;
		if((fromSide >= 0.0f) != (toSide >= 0.0f) && kept.count < cornerLimit) {
			const float along = fromSide / (fromSide - toSide);
			kept.corners[kept.count++] = {{from.position.x + (to.position.x - from.position.x) * along,
			                               from.position.y + (to.position.y - from.position.y) * along},
			                              from.coverage + (to.coverage - from.coverage) * along};
		}
	}
	polygon = kept;
}

/// @brief Cuts the fill into bands: maps it into box units (the box's middle at 0, its sides at -0.5 and 0.5), clips,
/// and maps the pieces back.
class Cutter {
public:
	/// @brief A cutter for a gradient over a box.
	/// @param gradient The gradient.
	/// @param box The box (its width and height above 0).
	Cutter(const Gradient& gradient, Rect box) : gradient_(gradient), box_(box) {
		const bool radial = gradient.kind == Gradient::Kind::Radial;
		bands_.resize(radial ? radialRings + 1 : linearStrips);
		for(std::size_t band = 0; band < bands_.size(); ++band)
			bands_[band].color =
			    sampleGradient(gradient.stops, radial && band == radialRings
			                                       ? 1.0
			                                       : (static_cast<double>(band) + 0.5) /
			                                             static_cast<double>(radial ? radialRings : linearStrips));
		const double angle = gradient.angle * std::numbers::pi / 180.0;
		direction_ = {static_cast<float>(std::sin(angle)), static_cast<float>(-std::cos(angle))};
		reach_ = std::abs(direction_.x) + std::abs(direction_.y);
	}

	/// @brief Cuts one triangle of the fill into the bands.
	/// @param corners Its corners, in the fill's units.
	void add(const std::array<MeshVertex, 3>& corners) {
		Polygon triangle;
		for(const MeshVertex& corner : corners) triangle.corners[triangle.count++] = toBox(corner);
		if(quads_ >= 4 * meshQuadLimit) { // past the budget, uncut in the colour at its middle
			const Vec2 middle{
			    (triangle.corners[0].position.x + triangle.corners[1].position.x + triangle.corners[2].position.x) /
			        3.0f,
			    (triangle.corners[0].position.y + triangle.corners[1].position.y + triangle.corners[2].position.y) /
			        3.0f};
			emit(triangle, bandAt(middle));
			return;
		}
		if(gradient_.kind == Gradient::Kind::Radial) addRadial(triangle);
		else addLinear(triangle);
	}

	/// @brief Moves the bands that have quads into a list.
	/// @param bands The list.
	void finish(std::vector<GradientBand>& bands) {
		for(GradientBand& band : bands_)
			if(!band.quads.empty()) bands.push_back(std::move(band));
	}

private:
	/// @brief The position along a linear gradient times its reach, minus half of it: 0 in the middle.
	[[nodiscard]] float along(Vec2 point) const noexcept { return dot(direction_, point); }

	/// @brief The band a point falls in.
	[[nodiscard]] std::size_t bandAt(Vec2 point) const noexcept {
		if(gradient_.kind == Gradient::Kind::Radial)
			return static_cast<std::size_t>(cellOf(length(point) / farCorner * radialRings, radialRings));
		return static_cast<std::size_t>(cellOf((0.5f + along(point) / reach_) * linearStrips, linearStrips - 1));
	}

	/// @brief Cuts a triangle into the strips of a linear gradient it crosses.
	void addLinear(const Polygon& triangle) {
		float low = along(triangle.corners[0].position), high = low;
		for(std::size_t index = 1; index < 3; ++index) {
			low = std::min(low, along(triangle.corners[index].position));
			high = std::max(high, along(triangle.corners[index].position));
		}
		const auto stripAt = [this](float value) {
			return cellOf((0.5f + value / reach_) * linearStrips, linearStrips - 1);
		};
		const int first = stripAt(low), last = stripAt(high);
		for(int strip = first; strip <= last; ++strip) {
			Polygon piece = triangle;
			// The strip's sides, as values of along(): position k / strips
			const auto side = [this](int k) {
				return (static_cast<float>(k) / static_cast<float>(linearStrips) - 0.5f) * reach_;
			};
			if(strip > first) clip(piece, {direction_, side(strip)});
			if(strip < last) clip(piece, {{-direction_.x, -direction_.y}, -side(strip + 1)});
			emit(piece, static_cast<std::size_t>(strip));
		}
	}

	/// @brief Cuts a triangle into the ring sectors of a radial gradient it crosses.
	void addRadial(const Polygon& triangle) {
		const std::array<Vec2, 3> p{triangle.corners[0].position, triangle.corners[1].position,
		                            triangle.corners[2].position};
		// Whether the middle is inside (or on) the triangle: then it touches every wedge, and the rings from 0
		const float turn = vector::cross(vector::minus(p[1], p[0]), vector::minus(p[2], p[0]));
		bool aroundMiddle = true;
		for(std::size_t index = 0; index < 3; ++index) {
			const float side =
			    vector::cross(vector::minus(p[(index + 1) % 3], p[index]), vector::times(p[index], -1.0f));
			if(side * turn < 0.0f) aroundMiddle = false;
		}
		float nearest = 0.0f, farthest = 0.0f;
		for(const Vec2 point : p) farthest = std::max(farthest, length(point));
		if(!aroundMiddle) {
			nearest = farthest;
			for(std::size_t index = 0; index < 3; ++index) {
				const Vec2 from = p[index], edge = vector::minus(p[(index + 1) % 3], from);
				const float span = dot(edge, edge);
				const float at = span > 0.0f ? std::clamp(-dot(from, edge) / span, 0.0f, 1.0f) : 0.0f;
				nearest = std::min(nearest, length(vector::plus(from, vector::times(edge, at))));
			}
		}
		const float step = farCorner / radialRings;
		const float chord = std::cos(std::numbers::pi_v<float> / radialWedges); // a chord's distance, per radius
		const int firstRing = cellOf(nearest / step, radialRings);
		const int lastRing = cellOf(farthest / (step * chord), radialRings);
		// The wedges between the corners' angles (a triangle away from the middle spans less than half a turn)
		int firstWedge = 0, lastWedge = radialWedges - 1;
		const float wedge = 2.0f * std::numbers::pi_v<float> / radialWedges;
		if(!aroundMiddle) {
			const float start = std::atan2(p[0].y, p[0].x);
			float low = 0.0f, high = 0.0f;
			for(std::size_t index = 1; index < 3; ++index) {
				float delta = std::atan2(p[index].y, p[index].x) - start;
				if(delta > std::numbers::pi_v<float>) delta -= 2.0f * std::numbers::pi_v<float>;
				if(delta < -std::numbers::pi_v<float>) delta += 2.0f * std::numbers::pi_v<float>;
				low = std::min(low, delta);
				high = std::max(high, delta);
			}
			// The angles lie within -1.5 to 1.5 turns: shifted by two turns, they are cells from 0
			firstWedge = cellOf((start + low) / wedge + 2.0f * radialWedges, 4 * radialWedges) - 2 * radialWedges - 1;
			lastWedge = cellOf((start + high) / wedge + 2.0f * radialWedges, 4 * radialWedges) - 2 * radialWedges + 1;
			if(lastWedge - firstWedge >= radialWedges) lastWedge = firstWedge + radialWedges - 1;
		}
		for(int index = firstWedge; index <= lastWedge; ++index) {
			const int sector = ((index % radialWedges) + radialWedges) % radialWedges;
			const float from = static_cast<float>(sector) * wedge, to = from + wedge, middle = from + wedge / 2.0f;
			Polygon slice = triangle;
			clip(slice, {{-std::sin(from), std::cos(from)}, 0.0f});
			clip(slice, {{std::sin(to), -std::cos(to)}, 0.0f});
			if(slice.count < 3) continue;
			const Vec2 out{std::cos(middle), std::sin(middle)};
			for(int ring = firstRing; ring <= lastRing; ++ring) {
				Polygon piece = slice;
				if(ring > firstRing) clip(piece, {out, static_cast<float>(ring) * step * chord});
				if(ring < lastRing) clip(piece, {{-out.x, -out.y}, -static_cast<float>(ring + 1) * step * chord});
				emit(piece, static_cast<std::size_t>(ring));
			}
		}
	}

	/// @brief Maps a vertex into box units.
	[[nodiscard]] MeshVertex toBox(const MeshVertex& vertex) const noexcept {
		return {{(vertex.position.x - box_.left) / box_.width() - 0.5f,
		         (vertex.position.y - box_.top) / box_.height() - 0.5f},
		        vertex.coverage};
	}

	/// @brief Maps a vertex back into the fill's units.
	[[nodiscard]] MeshVertex fromBox(const MeshVertex& vertex) const noexcept {
		return {{box_.left + (vertex.position.x + 0.5f) * box_.width(),
		         box_.top + (vertex.position.y + 0.5f) * box_.height()},
		        vertex.coverage};
	}

	/// @brief Adds a convex polygon to a band, as a fan of quads (the last one's two last corners the same when the
	/// fan has an odd number of triangles); one with no area (a sliver at a cut) adds nothing.
	void emit(const Polygon& piece, std::size_t band) {
		if(piece.count < 3) return;
		float twiceArea = 0.0f; // from the first corner, so that the products stay small
		const Vec2 origin = piece.corners[0].position;
		for(std::size_t index = 1; index + 1 < piece.count; ++index)
			twiceArea += vector::cross(vector::minus(piece.corners[index].position, origin),
			                           vector::minus(piece.corners[index + 1].position, origin));
		if(!(std::abs(twiceArea) > 1e-9f)) return; // of the box's area: below what a pixel shows
		std::vector<MeshVertex>& quads = bands_[band].quads;
		for(std::size_t index = 1; index + 1 < piece.count; index += 2) {
			const std::size_t next = std::min(index + 2, piece.count - 1);
			vector::pushQuad(quads, {fromBox(piece.corners[0]), fromBox(piece.corners[index]),
			                         fromBox(piece.corners[index + 1]), fromBox(piece.corners[next])});
			++quads_;
		}
	}

	const Gradient& gradient_;        ///< The gradient.
	Rect box_;                        ///< The box it spans.
	Vec2 direction_;                  ///< A linear gradient's direction (0 degrees up, 90 right).
	float reach_ = 1.0f;              ///< How far along direction_ the box reaches, corner to corner.
	std::vector<GradientBand> bands_; ///< The bands, by strip or ring.
	std::size_t quads_ = 0;           ///< The quads made so far.
};

} // namespace

void appendGradientFill(std::span<const MeshVertex> fill, const Gradient& gradient, Rect box,
                        std::vector<GradientBand>& bands) {
	if(gradient.stops.empty() || fill.size() < 4) return;
	if(!(box.width() > 0.0f) || !(box.height() > 0.0f) || !std::isfinite(box.width()) ||
	   !std::isfinite(box.height())) { // no box to spread it over: the colour at its start
		bands.push_back({sampleGradient(gradient.stops, 0.0), {fill.begin(), fill.end() - fill.size() % 4}});
		return;
	}
	Cutter cutter(gradient, box);
	for(std::size_t first = 0; first + 4 <= fill.size(); first += 4) {
		cutter.add({fill[first], fill[first + 1], fill[first + 2]});
		cutter.add({fill[first + 2], fill[first + 3], fill[first]});
	}
	cutter.finish(bands);
}

} // namespace affresco
