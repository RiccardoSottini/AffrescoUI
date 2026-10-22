/// @file
/// @brief Snapping on a canvas: finding the nearest target on each axis.

#include "spatial/Snapping.hpp"

#include <algorithm>
#include <cmath>

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

	/// @brief Keeps a candidate if it pulls less than the best so far and within the tolerance.
	/// @param from The moving coordinate.
	/// @param to The target coordinate.
	/// @param tolerance The largest pull.
	/// @param item The item the target belongs to, if any.
	/// @param onGrid Whether the target is a grid line.
	/// @param index Which moving coordinate it is.
	void consider(float from, float to, float tolerance, const Rect* item, bool onGrid, std::size_t index) noexcept {
		const float gap = std::abs(to - from);
		if(!(gap <= tolerance) || gap >= distance) return;
		*this = {to - from, gap, to, item, onGrid, index};
	}
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
		if(targets.grid > 0.0f)
			pull.consider(from, std::round(from / targets.grid) * targets.grid, tolerance, nullptr, true, index);
		for(const float guide : guides) pull.consider(from, guide, tolerance, nullptr, false, index);
		for(const Rect& other : targets.others)
			for(const float to : stops(other, vertical)) pull.consider(from, to, tolerance, &other, false, index);
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

} // namespace

SnapResult snapMove(Rect moving, const SnapTargets& targets, float tolerance) noexcept {
	SnapResult result{moving, {}};
	if(!(tolerance > 0.0f)) return result;
	for(const bool vertical : {true, false}) {
		const std::array<float, 3> coordinates = stops(moving, vertical);
		const Pull pull = bestPull(coordinates, vertical, targets, tolerance);
		if(std::isinf(pull.distance)) continue;
		(vertical ? result.rect.left : result.rect.top) += pull.delta;
		(vertical ? result.rect.right : result.rect.bottom) += pull.delta;
	}
	for(const bool vertical : {true, false}) {
		const std::array<float, 3> coordinates = stops(result.rect, vertical);
		const Pull pull = bestPull(coordinates, vertical, targets, 1e-3f * std::max(tolerance, 1.0f));
		result.lines[vertical ? 0 : 1] = lineOf(pull, vertical, result.rect);
	}
	return result;
}

SnapResult snapResize(Rect resized, SnapEdges edges, const SnapTargets& targets, float tolerance) noexcept {
	SnapResult result{resized, {}};
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
