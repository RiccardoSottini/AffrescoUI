#pragma once

/// @file
/// @brief detail::layoutFlex: flexible layout of a row or a column, as a pure function of measured sizes.

#include "affresco/render/Layout.hpp"

#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace affresco::detail {

/// @brief One child of a row or a column, as the container measured it. "Main" is along the container's direction,
/// "cross" across it.
struct FlexItem {
	float base = 0.0f;                                       ///< Its main size before flexing: its basis or its own.
	float minMain = 0.0f;                                    ///< Its minimum main size.
	float maxMain = std::numeric_limits<float>::infinity();  ///< Its maximum main size.
	float cross = 0.0f;                                      ///< Its own cross size (read only when placed across).
	float minCross = 0.0f;                                   ///< Its minimum cross size.
	float maxCross = std::numeric_limits<float>::infinity(); ///< Its maximum cross size.
	float grow = 0.0f;                                       ///< Its share of the room left over.
	float shrink = 0.0f;                                     ///< How much it gives up when the line overflows.
	std::optional<CrossAlign> align;                         ///< Where it sits across; nullopt keeps its own place.
	bool sized = false; ///< The container gives it its main size even when it doesn't flex (it has a basis).
};

/// @brief The container.
struct FlexOptions {
	float main = 0.0f;                ///< Its size along its direction.
	float cross = 0.0f;               ///< Its size across.
	float gap = 0.0f;                 ///< Between children along the direction.
	float lineGap = 0.0f;             ///< Between lines, when it wraps.
	Justify justify = Justify::Start; ///< How the children of a line are spread.
	bool wrap = false;                ///< A child that doesn't fit starts a new line.
};

/// @brief Where one child goes, in the container's terms: what a Stack turns into anchors and sizes.
struct FlexPlacement {
	float main = 0.0f;                ///< Its offset from the container's start edge, or from its end edge.
	bool fromEnd = false;             ///< Whether @ref main is measured from the end edge (Justify::End on one line).
	std::optional<float> mainSize;    ///< The main size the container gives it; nullopt: its own.
	std::optional<float> crossOffset; ///< Its offset from the cross start edge; nullopt: its own place across.
	std::optional<float> crossSize;   ///< The cross size the container gives it (Stretch); nullopt: its own.
};

/// @brief Lays the children of a row or a column out, like CSS flexbox: each child's hypothetical size is its base
/// within its minimum and maximum; the room left over on a line goes to the growing children, and an overflow is taken
/// from the shrinking ones in proportion to their size, each within its bounds; then each line is spread by the
/// justification and its children placed across.
///
/// It reads only its arguments: tests give it tables of sizes and compare the placements.
/// @param options The container.
/// @param items Its children, in order.
/// @param placements Receives one placement per child (cleared first; its capacity is reused).
void layoutFlex(const FlexOptions& options, std::span<const FlexItem> items, std::vector<FlexPlacement>& placements);

} // namespace affresco::detail
