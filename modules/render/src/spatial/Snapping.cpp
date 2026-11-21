/// @file
/// @brief Snapping on a canvas: finding the nearest target on each axis.

#include "spatial/Snapping.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco {
namespace {

/// @brief The best pull found on one axis so far.
struct Pull {
	float delta = 0.0f;          ///< How far the rectangle moves.
	float distance = INFINITY;   ///< |delta|; infinite while nothing was found.
	float target = 0.0f;         ///< Where it lands.
	const Rect* other = nullptr; ///< The item it lands on, if any (for the guide's extent).
	bool grid = false;           ///< It lands on the grid (no guide is drawn).
	std::size_t source = 0;      ///< Which moving coordinate lands.
};

/// @brief The three coordinates of a rectangle along an axis: start, centre, end.
/// @param rect The rectangle.
/// @param vertical The x axis (for vertical lines) or the y axis.
/// @return Start, centre, end.
std::array<float, 3> stops(const Rect& rect, bool vertical) noexcept {
	const float start = vertical ? rect.left : rect.top, end = vertical ? rect.right : rect.bottom;
	return {start, (start + end) / 2.0f, end};
}

/// @brief Finds the best pull on one axis for some moving coordinates.
/// @param moving The coordinates that move (edges or the centre); NaN ones are skipped.
/// @param vertical The x axis or the y axis.
/// @param targets What they snap to.
/// @param tolerance The largest pull.
/// @return The pull (an infinite distance when nothing is near).
Pull bestPull(std::span<const float> moving, bool vertical, const SnapTargets& targets, float tolerance) noexcept {
	Pull pull;
	const std::span<const float> guides = vertical ? targets.vertical : targets.horizontal;
	for(std::size_t index = 0; index < moving.size(); ++index) {
		const float from = moving[index];
		if(std::isnan(from)) continue;
		// Keeps a target if it pulls less than the best so far and within the tolerance
		const auto consider = [&](float to, const Rect* item, bool onGrid) {
			const float gap = std::abs(to - from);
			if(gap <= tolerance && gap < pull.distance) pull = {to - from, gap, to, item, onGrid, index};
		};
		if(targets.grid > 0.0f) consider(std::round(from / targets.grid) * targets.grid, nullptr, true);
		for(const float guide : guides) consider(guide, nullptr, false);
		for(const Rect& other : targets.others)
			for(const float to : stops(other, vertical)) consider(to, &other, false);
	}
	return pull;
}

/// @brief The guide to draw for a pull: along the other axis, over the rectangle and the item it met.
/// @param pull The pull.
/// @param vertical The axis.
/// @param rect The snapped rectangle.
/// @return The line, or nothing for the grid or no pull.
std::optional<SnapLine> lineOf(const Pull& pull, bool vertical, const Rect& rect) noexcept {
	if(std::isinf(pull.distance) || pull.grid) return std::nullopt;
	float from = vertical ? rect.top : rect.left, to = vertical ? rect.bottom : rect.right;
	if(pull.other) {
		from = std::min(from, vertical ? pull.other->top : pull.other->left);
		to = std::max(to, vertical ? pull.other->bottom : pull.other->right);
	}
	return SnapLine{vertical, pull.target, from, to};
}

/// @brief Whether two rectangles overlap across an axis, so they sit in one row (along x) or one column (along y).
/// @param a One rectangle.
/// @param b The other.
/// @param vertical The x axis (a row) or the y axis (a column).
/// @return true when their spans on the other axis overlap.
bool sameRow(const Rect& a, const Rect& b, bool vertical) noexcept {
	return vertical ? a.top < b.bottom && b.top < a.bottom : a.left < b.right && b.left < a.right;
}

/// @brief The gaps between neighbouring items in the moving rectangle's row (or column): each item's end to the start
/// of the next item that begins after it.
/// @param moving The moving rectangle.
/// @param vertical The x axis or the y axis.
/// @param others The other items.
/// @param visit Called with the earlier item and the later one of each gap.
template <typename Visit>
void eachGap(const Rect& moving, bool vertical, std::span<const Rect> others, const Visit& visit) {
	for(const Rect& a : others) {
		if(!sameRow(a, moving, vertical)) continue;
		const Rect* next = nullptr;
		const float end = stops(a, vertical)[2];
		for(const Rect& b : others) {
			const float start = stops(b, vertical)[0];
			if(&a != &b && sameRow(b, moving, vertical) && start >= end && (!next || start < stops(*next, vertical)[0]))
				next = &b;
		}
		if(next && stops(*next, vertical)[0] > end) visit(a, *next);
	}
}

/// @brief The best pull on one axis that gives the moving rectangle a gap to an item of its row equal to a gap
/// between two items there, or equal gaps to its two neighbours.
/// @param moving The moving rectangle.
/// @param vertical The x axis or the y axis.
/// @param others The other items.
/// @param tolerance The largest pull.
/// @return The pull (an infinite distance when nothing is near).
Pull gapPull(const Rect& moving, bool vertical, std::span<const Rect> others, float tolerance) noexcept {
	Pull pull;
	const std::array<float, 3> own = stops(moving, vertical);
	const auto consider = [&](float delta) {
		if(std::abs(delta) <= tolerance && std::abs(delta) < pull.distance)
			pull = {delta, std::abs(delta), 0.0f, nullptr, false, 0};
	};
	eachGap(moving, vertical, others, [&](const Rect& a, const Rect& b) {
		const float gap = stops(b, vertical)[0] - stops(a, vertical)[2];
		for(const Rect& item : others) {
			if(!sameRow(item, moving, vertical)) continue;
			const std::array<float, 3> at = stops(item, vertical);
			consider(at[2] + gap - own[0]); // after it, by that gap
			consider(at[0] - gap - own[2]); // before it, by that gap
		}
	});
	// Between two neighbours, as far from each
	eachGap(moving, vertical, others, [&](const Rect& a, const Rect& b) {
		const float room = stops(b, vertical)[0] - stops(a, vertical)[2], size = own[2] - own[0];
		if(room > size) consider(stops(a, vertical)[2] + (room - size) / 2.0f - own[0]);
	});
	return pull;
}

/// @brief A distance indicator: a segment across the gap from `from` to `to` along an axis, halfway down the span the
/// two rectangles share on the other axis.
/// @param a The rectangle before the gap.
/// @param b The rectangle after it.
/// @param vertical The x axis (the segment is horizontal) or the y axis.
/// @return The segment.
SnapLine gapLine(const Rect& a, const Rect& b, bool vertical) noexcept {
	const float shared = vertical ? (std::max(a.top, b.top) + std::min(a.bottom, b.bottom)) / 2.0f
	                              : (std::max(a.left, b.left) + std::min(a.right, b.right)) / 2.0f;
	return SnapLine{!vertical, shared, stops(a, vertical)[2], stops(b, vertical)[0]};
}

/// @brief How near two gaps must be to count as equal, in world units.
constexpr float same = 0.5f;

/// @brief A rectangle's nearest neighbours in its row (or column): the item that ends last before it, and the one
/// that starts first after it.
/// @param rect The rectangle.
/// @param vertical The x axis or the y axis.
/// @param others The other items.
/// @return The one before and the one after (nullptr where there is none).
std::pair<const Rect*, const Rect*> neighbours(const Rect& rect, bool vertical, std::span<const Rect> others) noexcept {
	const std::array<float, 3> own = stops(rect, vertical);
	const Rect* before = nullptr;
	const Rect* after = nullptr;
	for(const Rect& item : others) {
		if(!sameRow(item, rect, vertical)) continue;
		const std::array<float, 3> at = stops(item, vertical);
		if(at[2] <= own[0] + same && (!before || at[2] > stops(*before, vertical)[2])) before = &item;
		if(at[0] >= own[2] - same && (!after || at[0] < stops(*after, vertical)[0])) after = &item;
	}
	return {before, after};
}

/// @brief The distance indicators of a rectangle where it landed, along one axis: its gap to a neighbour, and the
/// equal gap it matches (another pair's, or its other neighbour's).
/// @param rect The rectangle.
/// @param vertical The x axis or the y axis.
/// @param others The other items.
/// @param gaps The two slots to fill.
void indicate(const Rect& rect, bool vertical, std::span<const Rect> others,
              std::span<std::optional<SnapLine>, 2> gaps) noexcept {
	const std::array<float, 3> own = stops(rect, vertical);
	const auto [before, after] = neighbours(rect, vertical, others);
	const float left = before ? own[0] - stops(*before, vertical)[2] : -1.0f;
	const float right = after ? stops(*after, vertical)[0] - own[2] : -1.0f;
	if(left > same && right > same && std::abs(left - right) < same) {
		gaps[0] = gapLine(*before, rect, vertical);
		gaps[1] = gapLine(rect, *after, vertical);
		return;
	}
	eachGap(rect, vertical, others, [&](const Rect& a, const Rect& b) {
		if(gaps[0]) return;
		const float gap = stops(b, vertical)[0] - stops(a, vertical)[2];
		if(left > same && std::abs(gap - left) < same) gaps[0] = gapLine(*before, rect, vertical);
		else if(right > same && std::abs(gap - right) < same) gaps[0] = gapLine(rect, *after, vertical);
		if(gaps[0]) gaps[1] = gapLine(a, b, vertical);
	});
}

} // namespace

SnapResult snapMove(Rect moving, const SnapTargets& targets, float tolerance) noexcept {
	SnapResult result{moving, {}, {}};
	if(!(tolerance > 0.0f)) return result;
	for(const bool vertical : {true, false}) {
		const std::array<float, 3> coordinates = stops(moving, vertical);
		// An edge, a centre, a guide or the grid comes first; an equal gap only where none of them is near
		Pull pull = bestPull(coordinates, vertical, targets, tolerance);
		if(std::isinf(pull.distance)) pull = gapPull(moving, vertical, targets.others, tolerance);
		if(std::isinf(pull.distance)) continue;
		(vertical ? result.rect.left : result.rect.top) += pull.delta;
		(vertical ? result.rect.right : result.rect.bottom) += pull.delta;
	}
	for(const bool vertical : {true, false}) {
		const std::array<float, 3> coordinates = stops(result.rect, vertical);
		const Pull pull = bestPull(coordinates, vertical, targets, 1e-3f * std::max(tolerance, 1.0f));
		result.lines[vertical ? 0 : 1] = lineOf(pull, vertical, result.rect);
		indicate(result.rect, vertical, targets.others,
		         std::span<std::optional<SnapLine>, 2>(result.gaps.data() + (vertical ? 0 : 2), 2));
	}
	return result;
}

SnapResult snapResize(Rect resized, SnapEdges edges, const SnapTargets& targets, float tolerance) noexcept {
	SnapResult result{resized, {}, {}};
	if(!(tolerance > 0.0f)) return result;
	for(const bool vertical : {true, false}) {
		const bool startMoves = vertical ? edges.left : edges.top, endMoves = vertical ? edges.right : edges.bottom;
		float& start = vertical ? result.rect.left : result.rect.top;
		float& end = vertical ? result.rect.right : result.rect.bottom;
		const std::array<float, 2> coordinates{startMoves ? start : NAN, endMoves ? end : NAN};
		const Pull pull = bestPull(coordinates, vertical, targets, tolerance);
		if(std::isinf(pull.distance)) continue;
		// The edge that lands moves alone; a snap never turns the rectangle inside out
		if(pull.source == 0) start = std::min(pull.target, end);
		else end = std::max(pull.target, start);
		result.lines[vertical ? 0 : 1] = lineOf(pull, vertical, result.rect);
	}
	return result;
}

} // namespace affresco
