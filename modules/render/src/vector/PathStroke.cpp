/// @file
/// @brief appendStroke: contours cut into dashes, then each segment, join and cap drawn as a convex polygon with a
/// fading fringe on its outer edges.

#include "affresco/render/VectorPath.hpp"

#include "VectorMath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace affresco {

using vector::minus;
using vector::plus;
using vector::times;

namespace {

constexpr std::size_t polygonLimit = 64; ///< The most corners a piece of stroke has (a circle's).

/// @brief Draws convex polygons into a mesh: fan quads inside, fringe quads on the edges asked for.
class PolygonWriter {
public:
	/// @brief Prepares to write.
	/// @param mesh Where the quads go.
	/// @param feather How wide the fringes are.
	PolygonWriter(std::vector<MeshVertex>& mesh, float feather) : mesh_(mesh), feather_(feather) {}

	/// @brief Tells whether the mesh holds as many quads as it may, or a piece didn't fit (no later piece is drawn).
	[[nodiscard]] bool full() const noexcept { return stopped_ || vector::meshIsFull(mesh_, meshQuadLimit); }

	/// @brief Draws a convex polygon.
	/// @param corners Its corners, in order around it (either way), at most polygonLimit.
	/// @param fringed For each edge (from corner i to corner i + 1), whether it gets a fringe.
	void polygon(std::span<const Vec2> corners, std::span<const bool> fringed) {
		const std::size_t count = std::min(corners.size(), polygonLimit);
		if(count < 3 || full()) return;
		const bool feathered = feather_ > 0.0f;
		const auto fringes = feathered ? static_cast<std::size_t>(std::ranges::count(fringed.first(count), true)) : 0;
		if(mesh_.size() / 4 + count - 2 + fringes > meshQuadLimit) { // a piece is drawn whole or not at all
			stopped_ = true;
			return;
		}
		fan(corners.first(count));
		if(feathered) fringe(corners.first(count), fringed);
	}

	/// @brief Draws a circle (a polygon fine enough for its size), fringed all round.
	void circle(Vec2 center, float radius) {
		const float step = radius > 0.25f ? 2.0f * std::acos(1.0f - 0.25f / radius) : std::numbers::pi_v<float>;
		const auto count = static_cast<std::size_t>(std::clamp(std::ceil(2.0f * std::numbers::pi_v<float> / step), 8.0f, 64.0f));
		std::array<Vec2, polygonLimit> corners{};
		for(std::size_t index = 0; index < count; ++index) {
			const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(index) / static_cast<float>(count);
			corners[index] = {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
		}
		std::array<bool, polygonLimit> fringed{};
		fringed.fill(true);
		polygon(std::span(corners).first(count), fringed);
	}

private:
	/// @brief Fills a convex polygon with a fan of quads from its first corner.
	/// @param corners Its corners, at least 3.
	void fan(std::span<const Vec2> corners) {
		for(std::size_t index = 1; index + 1 < corners.size(); ++index)
			vector::pushQuad(mesh_, {MeshVertex{corners[0]}, MeshVertex{corners[index]}, MeshVertex{corners[index + 1]},
			                         MeshVertex{corners[index + 1]}});
	}

	/// @brief The outward normals of a convex polygon's edges.
	/// @param corners Its corners, at least 3 and at most polygonLimit.
	/// @return Per edge (from corner i to corner i + 1), its normal pointing out of the polygon.
	[[nodiscard]] static std::array<Vec2, polygonLimit> outwardNormals(std::span<const Vec2> corners) {
		const std::size_t count = corners.size();
		float area = 0.0f;
		for(std::size_t index = 0; index < count; ++index) area += vector::cross(corners[index], corners[(index + 1) % count]);
		const float sign = area > 0.0f ? -1.0f : 1.0f; // outside is on the right of a counter-clockwise polygon
		std::array<Vec2, polygonLimit> normals{};
		for(std::size_t index = 0; index < count; ++index)
			normals[index] = times(vector::leftOf(vector::normalized(minus(corners[(index + 1) % count], corners[index]))), sign);
		return normals;
	}

	/// @brief Draws the fringes of a convex polygon's edges, fading out from them.
	/// @param corners Its corners, at least 3 and at most polygonLimit.
	/// @param fringed For each edge, whether it gets a fringe.
	void fringe(std::span<const Vec2> corners, std::span<const bool> fringed) {
		const std::size_t count = corners.size();
		const std::array<Vec2, polygonLimit> normals = outwardNormals(corners);
		for(std::size_t index = 0; index < count; ++index) {
			if(!fringed[index]) continue;
			const std::size_t previous = (index + count - 1) % count, next = (index + 1) % count;
			const Vec2 start = offset(normals[index], fringed[previous] ? normals[previous] : normals[index]);
			const Vec2 end = offset(normals[index], fringed[next] ? normals[next] : normals[index]);
			vector::pushQuad(mesh_, {MeshVertex{corners[index]}, MeshVertex{corners[next]},
			                         MeshVertex{plus(corners[next], end), 0.0f}, MeshVertex{plus(corners[index], start), 0.0f}});
		}
	}

	/// @brief Where a fringe's outer corner is: its edge's normal, averaged with the neighbour's if that has a fringe.
	[[nodiscard]] Vec2 offset(Vec2 normal, Vec2 neighbour) const noexcept {
		const Vec2 average = times(plus(normal, neighbour), 0.5f);
		return times(average, feather_ / std::max(vector::dot(average, average), 0.25f));
	}

	std::vector<MeshVertex>& mesh_; ///< Where the quads go.
	float feather_;                 ///< How wide the fringes are.
	bool stopped_ = false;          ///< Whether a piece didn't fit.
};

/// @brief Strokes open and closed runs of points.
class Stroker {
public:
	/// @brief Prepares to stroke.
	Stroker(const StrokeStyle& style, std::vector<MeshVertex>& mesh, float feather)
	    : style_(style), half_(style.width / 2.0f), writer_(mesh, feather) {}

	/// @brief Strokes a run of points.
	/// @param points Its points, no two in a row equal.
	/// @param closed Whether it is a loop.
	void run(std::span<const Vec2> points, bool closed) {
		if(points.size() == 1) {
			dot(points.front());
			return;
		}
		const std::size_t segments = closed ? points.size() : points.size() - 1;
		for(std::size_t index = 0; index < segments && !writer_.full(); ++index) {
			const bool first = !closed && index == 0, last = !closed && index + 1 == segments;
			segment(points[index], points[(index + 1) % points.size()], first, last);
			if(!last)
				join(points[index], points[(index + 1) % points.size()], points[(index + 2) % points.size()]);
		}
		if(closed) return;
		cap(points[0], vector::normalized(minus(points[0], points[1])));
		cap(points.back(), vector::normalized(minus(points.back(), points[points.size() - 2])));
	}

private:
	/// @brief Draws a segment's rectangle; its ends get a fringe where they end the stroke with a butt cap.
	void segment(Vec2 from, Vec2 to, bool startsStroke, bool endsStroke) {
		const Vec2 side = times(vector::leftOf(vector::normalized(minus(to, from))), half_);
		const std::array corners{plus(from, side), plus(to, side), minus(to, side), minus(from, side)};
		const bool butt = style_.cap == LineCap::butt;
		const std::array fringed{true, endsStroke && butt, true, startsStroke && butt};
		writer_.polygon(corners, fringed);
	}

	/// @brief Draws the join at @p corner between the segments from @p before and to @p after.
	void join(Vec2 before, Vec2 corner, Vec2 after) {
		const Vec2 in = vector::normalized(minus(corner, before)), out = vector::normalized(minus(after, corner));
		const float turn = vector::cross(in, out);
		if(std::abs(turn) < 1.0e-6f && vector::dot(in, out) > 0.0f) return; // straight on
		if(style_.join == LineJoin::round) {
			writer_.circle(corner, half_);
			return;
		}
		const float outer = turn > 0.0f ? -1.0f : 1.0f; // the side away from the turn
		const Vec2 inSide = times(vector::leftOf(in), outer), outSide = times(vector::leftOf(out), outer);
		const Vec2 a = plus(corner, times(inSide, half_)), b = plus(corner, times(outSide, half_));
		const Vec2 bisector = vector::normalized(plus(inSide, outSide));
		const float cosHalf = vector::dot(bisector, inSide);
		if(style_.join == LineJoin::miter && cosHalf > 1.0e-4f && 1.0f / cosHalf <= style_.miterLimit) {
			const std::array corners{corner, a, plus(corner, times(bisector, half_ / cosHalf)), b};
			writer_.polygon(corners, std::array{false, true, true, false});
			return;
		}
		writer_.polygon(std::array{corner, a, b}, std::array{false, true, false});
	}

	/// @brief Draws a cap at an end of an open run.
	/// @param end The end point.
	/// @param away The direction out of the run there.
	void cap(Vec2 end, Vec2 away) {
		if(style_.cap == LineCap::round) writer_.circle(end, half_);
		else if(style_.cap == LineCap::square) {
			const Vec2 side = times(vector::leftOf(away), half_), reach = times(away, half_);
			const std::array corners{plus(end, side), plus(plus(end, side), reach), plus(minus(end, side), reach),
			                         minus(end, side)};
			writer_.polygon(corners, std::array{true, true, true, false});
		}
	}

	/// @brief Draws a run of no length: a dot for round and square caps, nothing for butt ones (like SVG).
	void dot(Vec2 point) {
		if(style_.cap == LineCap::round) writer_.circle(point, half_);
		else if(style_.cap == LineCap::square) {
			const std::array corners{Vec2{point.x - half_, point.y - half_}, Vec2{point.x + half_, point.y - half_},
			                         Vec2{point.x + half_, point.y + half_}, Vec2{point.x - half_, point.y + half_}};
			writer_.polygon(corners, std::array{true, true, true, true});
		}
	}

	const StrokeStyle& style_; ///< The stroke.
	float half_;               ///< Half its width.
	PolygonWriter writer_;     ///< Draws the pieces.
};

/// @brief The dash pattern to walk, or an empty one for a solid stroke: an odd list repeated, a broken one ignored.
/// @param dash The pattern.
/// @param totalLength The length of the contour to dash (a pattern too fine for it is ignored too).
[[nodiscard]] std::vector<float> dashPattern(const std::vector<float>& dash, float totalLength) {
	float sum = 0.0f;
	for(const float length : dash) {
		if(!std::isfinite(length) || length < 0.0f) return {};
		sum += length;
	}
	if(!(sum > 0.0f) || !(totalLength / sum <= 100000.0f)) return {}; // NaN too: it would never run out
	std::vector<float> pattern = dash;
	if(pattern.size() % 2 != 0) pattern.insert(pattern.end(), dash.begin(), dash.end());
	return pattern;
}

/// @brief Cuts a contour into its dashes.
/// @param contour The contour.
/// @param pattern The dash pattern (even, valid).
/// @return The dashes, open runs of points.
[[nodiscard]] std::vector<std::vector<Vec2>> dashesOf(const VectorPath::Contour& contour, std::span<const float> pattern) {
	std::vector<std::vector<Vec2>> dashes;
	std::size_t phase = 0;
	float left = pattern[0];
	std::vector<Vec2> current{contour.points.front()};
	const std::size_t segments = contour.closed ? contour.points.size() : contour.points.size() - 1;
	for(std::size_t index = 0; index < segments; ++index) {
		Vec2 from = contour.points[index];
		const Vec2 to = contour.points[(index + 1) % contour.points.size()];
		float remaining = vector::length(minus(to, from));
		while(remaining > left) { // the dash or gap ends within this segment
			from = plus(from, times(minus(to, from), left / remaining));
			remaining -= left;
			if(phase % 2 == 0) {
				current.push_back(from);
				dashes.push_back(std::move(current));
			}
			current = {from};
			phase = (phase + 1) % pattern.size();
			left = pattern[phase];
		}
		left -= remaining;
		if(phase % 2 == 0) current.push_back(to);
	}
	if(phase % 2 == 0 && current.size() > 1) dashes.push_back(std::move(current));
	return dashes;
}

/// @brief The length of a contour, closed or not.
[[nodiscard]] float lengthOf(const VectorPath::Contour& contour) {
	float total = 0.0f;
	const std::size_t count = contour.points.size();
	for(std::size_t index = 0; index + 1 < count; ++index)
		total += vector::length(minus(contour.points[index + 1], contour.points[index]));
	if(contour.closed && count > 1) total += vector::length(minus(contour.points.front(), contour.points.back()));
	return total;
}

/// @brief Tells whether a stroke draws anything that can be measured: a positive, finite width along finite points.
/// @param contours The contours.
/// @param style The stroke.
/// @return true if it does.
bool strokable(std::span<const VectorPath::Contour> contours, const StrokeStyle& style) {
	return style.width > 0.0f && std::isfinite(style.width) && vector::finite(contours);
}

/// @brief A run of points with each point that repeats the one before it left out.
/// @param points The points.
/// @return The run.
[[nodiscard]] std::vector<Vec2> withoutRepeats(std::span<const Vec2> points) {
	std::vector<Vec2> run;
	for(const Vec2 point : points)
		if(run.empty() || !(run.back() == point)) run.push_back(point);
	return run;
}

/// @brief Strokes one contour, whole or cut into its dashes.
/// @param stroker What draws it.
/// @param contour The contour, with a point at least.
/// @param style The stroke.
void strokeContour(Stroker& stroker, const VectorPath::Contour& contour, const StrokeStyle& style) {
	// A closed contour whose last point is its first is one point shorter
	std::span<const Vec2> points = contour.points;
	if(contour.closed && points.size() > 1 && points.back() == points.front()) points = points.first(points.size() - 1);
	const std::vector<float> pattern = style.dash.empty() ? std::vector<float>{} : dashPattern(style.dash, lengthOf(contour));
	if(pattern.empty() || points.size() == 1) {
		stroker.run(points, contour.closed && points.size() > 2);
		return;
	}
	const VectorPath::Contour trimmed{{points.begin(), points.end()}, contour.closed};
	for(const std::vector<Vec2>& dash : dashesOf(trimmed, pattern)) stroker.run(withoutRepeats(dash), false);
}

} // namespace

void appendStroke(std::span<const VectorPath::Contour> contours, const StrokeStyle& style, std::vector<MeshVertex>& mesh,
                  float feather) {
	if(!strokable(contours, style)) return;
	Stroker stroker(style, mesh, std::min(std::max(feather, 0.0f), 4.0f));
	for(const VectorPath::Contour& contour : contours)
		if(!contour.points.empty()) strokeContour(stroker, contour, style);
}

} // namespace affresco
