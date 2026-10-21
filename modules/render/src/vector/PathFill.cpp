/// @file
/// @brief appendFill: the inside of contours as trapezoids, found band by band between the contours' points (split
/// again where edges cross), then a fading fringe outside each contour.

#include "affresco/render/VectorPath.hpp"

#include "VectorMath.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

using vector::minus;
using vector::plus;
using vector::times;

namespace {

/// @brief A contour's edge that isn't horizontal, from its top to its bottom.
struct Edge {
	Vec2 top;          ///< Its upper end.
	Vec2 bottom;       ///< Its lower end.
	int direction = 1; ///< +1 if the contour runs down along it, -1 if up.

	/// @brief Where it is at a height within it.
	[[nodiscard]] float xAt(float y) const noexcept {
		return top.x + (bottom.x - top.x) * (y - top.y) / (bottom.y - top.y);
	}
};

/// @brief An edge across a band: where it enters at the top and leaves at the bottom.
struct Span {
	float topX = 0.0f;    ///< At the band's top.
	float bottomX = 0.0f; ///< At the band's bottom.
	int direction = 1;    ///< The edge's direction.
};

/// @brief Tells whether a winding number is inside by a rule.
[[nodiscard]] bool isInside(int winding, FillRule rule) noexcept {
	return rule == FillRule::nonZero ? winding != 0 : (winding & 1) != 0;
}

/// @brief Calls @p visit with each edge of each contour (each closed): its two ends.
template <typename Visit> void forEachEdge(std::span<const VectorPath::Contour> contours, Visit&& visit) {
	for(const VectorPath::Contour& contour : contours)
		for(std::size_t index = 0; index < contour.points.size(); ++index)
			visit(contour.points[index], contour.points[(index + 1) % contour.points.size()]);
}

/// @brief Fills band after band of contours with trapezoids.
class Trapezoids {
public:
	/// @brief Prepares the edges.
	/// @param contours The contours.
	/// @param rule The fill rule.
	/// @param mesh Where the quads go.
	Trapezoids(std::span<const VectorPath::Contour> contours, FillRule rule, std::vector<MeshVertex>& mesh)
	    : rule_(rule), mesh_(mesh) {
		forEachEdge(contours, [&](Vec2 from, Vec2 to) {
			if(from.y < to.y) edges_.push_back({from, to, 1});
			else if(from.y > to.y) edges_.push_back({to, from, -1});
			heights_.push_back(from.y);
		});
		std::ranges::sort(edges_, {}, [](const Edge& edge) { return edge.top.y; });
		std::ranges::sort(heights_);
		heights_.erase(std::unique(heights_.begin(), heights_.end()), heights_.end());
	}

	/// @brief Fills every band.
	void run() {
		std::size_t entering = 0;
		for(std::size_t band = 0; band + 1 < heights_.size(); ++band) {
			const float top = heights_[band], bottom = heights_[band + 1];
			std::erase_if(active_, [&](const Edge& edge) { return edge.bottom.y <= top; });
			for(; entering < edges_.size() && edges_[entering].top.y <= top; ++entering)
				active_.push_back(edges_[entering]);
			if(!fillBand(top, bottom)) return;
		}
	}

private:
	/// @brief Fills a band, in pieces split where edges cross.
	/// @return False once a limit is reached.
	bool fillBand(float top, float bottom) {
		while(top < bottom) {
			work_ += active_.size();
			if(work_ > meshWorkLimit || vector::meshIsFull(mesh_, meshQuadLimit)) return false;
			float pieceBottom = bottom;
			while(true) { // spans by their middle; the first crossing of neighbours ends the piece
				spansAt(top, pieceBottom);
				const float crossing = firstCrossing(top, pieceBottom);
				if(crossing >= pieceBottom) break;
				pieceBottom = crossing;
				work_ += spans_.size();
				if(work_ > meshWorkLimit) return false;
			}
			emit(top, pieceBottom);
			top = pieceBottom;
		}
		return true;
	}

	/// @brief Places the active edges across a piece of band, sorted by where they cross its middle.
	void spansAt(float top, float bottom) {
		spans_.clear();
		for(const Edge& edge : active_) spans_.push_back({edge.xAt(top), edge.xAt(bottom), edge.direction});
		std::ranges::sort(spans_, {}, [](const Span& span) { return span.topX + span.bottomX; });
	}

	/// @brief The first height within a piece of band where neighbouring spans cross, or @p bottom if none does.
	[[nodiscard]] float firstCrossing(float top, float bottom) const noexcept {
		const float least = (bottom - top) * 1.0e-3f + 1.0e-4f; // closer crossings are drawn as they are
		float first = bottom;
		for(std::size_t index = 0; index + 1 < spans_.size(); ++index) {
			const float atTop = spans_[index].topX - spans_[index + 1].topX;
			const float atBottom = spans_[index].bottomX - spans_[index + 1].bottomX;
			const bool crosses = (atTop < 0.0f && atBottom > 0.0f) || (atTop > 0.0f && atBottom < 0.0f);
			if(!crosses) continue;
			const float y = top + (bottom - top) * atTop / (atTop - atBottom);
			if(y > top + least && y < bottom - least) first = std::min(first, y);
		}
		return first;
	}

	/// @brief Adds the trapezoids of a piece of band: from where the winding enters the inside to where it leaves.
	void emit(float top, float bottom) {
		int winding = 0;
		const Span* left = nullptr;
		for(const Span& span : spans_) {
			const bool wasInside = isInside(winding, rule_);
			winding += span.direction;
			const bool inside = isInside(winding, rule_);
			if(!wasInside && inside) left = &span;
			else if(wasInside && !inside && left != nullptr) {
				if(vector::meshIsFull(mesh_, meshQuadLimit)) return;
				vector::pushQuad(mesh_, {MeshVertex{{left->topX, top}}, MeshVertex{{span.topX, top}},
				                         MeshVertex{{span.bottomX, bottom}}, MeshVertex{{left->bottomX, bottom}}});
			}
		}
	}

	FillRule rule_;                ///< Which points are inside.
	std::vector<MeshVertex>& mesh_; ///< Where the quads go.
	std::vector<Edge> edges_;      ///< Every edge, by its top.
	std::vector<float> heights_;   ///< Every point's height, once each, in order.
	std::vector<Edge> active_;     ///< The edges across the current band.
	std::vector<Span> spans_;      ///< The active edges across the current piece of band.
	std::size_t work_ = 0;         ///< Edge visits so far.
};

/// @brief How many times contours wind around a point.
/// @param contours The contours.
/// @param point The point.
/// @return Its winding number.
[[nodiscard]] int windingAt(std::span<const VectorPath::Contour> contours, Vec2 point) {
	int winding = 0;
	forEachEdge(contours, [&](Vec2 from, Vec2 to) {
		const float side = vector::cross(minus(to, from), minus(point, from));
		if(from.y <= point.y && to.y > point.y && side > 0.0f) ++winding;
		else if(to.y <= point.y && from.y > point.y && side < 0.0f) --winding;
	});
	return winding;
}

/// @brief Tells whether a contour's inside is on the left of its edges: whether a point just left of its longest
/// edge is inside the fill (holes have their inside, the fill, on the other side).
[[nodiscard]] bool insideOnLeft(std::span<const VectorPath::Contour> contours, const VectorPath::Contour& contour,
                                FillRule rule) {
	std::size_t longest = 0;
	float longestLength = -1.0f;
	for(std::size_t index = 0; index < contour.points.size(); ++index) {
		const float size = vector::length(minus(contour.points[(index + 1) % contour.points.size()], contour.points[index]));
		if(size > longestLength) {
			longestLength = size;
			longest = index;
		}
	}
	const Vec2 from = contour.points[longest], to = contour.points[(longest + 1) % contour.points.size()];
	const Vec2 middle = times(plus(from, to), 0.5f);
	const Vec2 probe = plus(middle, times(vector::leftOf(vector::normalized(minus(to, from))), std::min(0.01f, longestLength)));
	return isInside(windingAt(contours, probe), rule);
}

/// @brief Adds the fringe outside one contour: a quad on each edge, fading from the edge outward.
/// @param contour The contour (closed).
/// @param outwardSign +1 if outside is on the left of its edges, -1 if on the right.
/// @param feather How wide.
/// @param mesh Where the quads go.
void appendFringe(const VectorPath::Contour& contour, float outwardSign, float feather, std::vector<MeshVertex>& mesh) {
	const std::size_t count = contour.points.size();
	const auto outward = [&](std::size_t index) {
		const Vec2 direction = minus(contour.points[(index + 1) % count], contour.points[index]);
		return times(vector::leftOf(vector::normalized(direction)), outwardSign);
	};
	// The fringe's outer corners: the neighbouring edges' normals averaged, longer at sharp corners (within 2x)
	const auto corner = [&](std::size_t index) {
		const Vec2 average = times(plus(outward((index + count - 1) % count), outward(index)), 0.5f);
		const float squared = std::max(vector::dot(average, average), 0.25f);
		return plus(contour.points[index], times(average, feather / squared));
	};
	Vec2 firstCorner = corner(0), previousCorner = firstCorner;
	for(std::size_t index = 0; index < count && !vector::meshIsFull(mesh, meshQuadLimit); ++index) {
		const std::size_t next = (index + 1) % count;
		const Vec2 nextCorner = next == 0 ? firstCorner : corner(next);
		vector::pushQuad(mesh, {MeshVertex{contour.points[index]}, MeshVertex{contour.points[next]},
		                        MeshVertex{nextCorner, 0.0f}, MeshVertex{previousCorner, 0.0f}});
		previousCorner = nextCorner;
	}
}

} // namespace

void appendFill(std::span<const VectorPath::Contour> contours, FillRule rule, std::vector<MeshVertex>& mesh,
                float feather) {
	if(!vector::finite(contours)) return;
	Trapezoids trapezoids(contours, rule, mesh);
	trapezoids.run();
	if(!(feather > 0.0f)) return;
	// Finding each contour's inside costs a pass over every edge: within the work limit, then no more fringes
	std::size_t edgeCount = 0, work = 0;
	for(const VectorPath::Contour& contour : contours) edgeCount += contour.points.size();
	for(const VectorPath::Contour& contour : contours) {
		if(contour.points.size() < 3) continue;
		work += edgeCount;
		if(work > meshWorkLimit || vector::meshIsFull(mesh, meshQuadLimit)) return;
		appendFringe(contour, insideOnLeft(contours, contour, rule) ? -1.0f : 1.0f, std::min(feather, 4.0f), mesh);
	}
}

} // namespace affresco
