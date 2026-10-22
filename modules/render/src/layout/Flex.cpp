/// @file
/// @brief detail::layoutFlex: lines, flexing within bounds, justification and cross placement.
///
/// The arithmetic follows the order the Stack used before it (sizes summed child by child, then the gaps; offsets
/// advanced by size plus gap), so rows and columns that use none of the newer properties land on exactly the same
/// pixels.

#include "layout/Flex.hpp"

#include <algorithm>
#include <cstddef>

namespace affresco::detail {

namespace {

/// @brief A child while its line is being flexed.
struct Work {
	float length = 0.0f; ///< Its main size so far.
	bool frozen = false; ///< It reached a bound, or doesn't flex: its length is final.
	bool flexed = false; ///< The line gave it a size of its own.
};

/// @brief A value within its bounds; the minimum wins over the maximum, as in CSS.
/// @param value The value.
/// @param minimum The minimum.
/// @param maximum The maximum.
/// @return The bounded value.
float bounded(float value, float minimum, float maximum) noexcept {
	return std::max(std::min(value, maximum), minimum);
}

/// @brief The main sizes of a line's children and the gaps between them.
/// @param work The children.
/// @param gap The gap.
/// @return The total.
float lineLength(std::span<const Work> work, float gap) noexcept {
	float total = 0.0f;
	for(const Work& child : work) total += child.length;
	if(!work.empty()) total += gap * static_cast<float>(work.size() - 1);
	return total;
}

/// @brief The grow factors of a line's children that still flex.
/// @param items The line's children.
/// @param work Their sizes.
/// @return Their sum.
float growShares(std::span<const FlexItem> items, std::span<const Work> work) noexcept {
	float shares = 0.0f;
	for(std::size_t i = 0; i < items.size(); ++i)
		if(!work[i].frozen) shares += items[i].grow;
	return shares;
}

/// @brief Freezes at their maximum the children that their share of the spare room would take past it.
/// @param items The line's children.
/// @param work Their sizes.
/// @param spare The room left over.
/// @param shares The grow factors of the children that still flex.
/// @return Whether any was frozen: the others then share what is left, from their hypothetical sizes again.
bool freezeAtMaximum(std::span<const FlexItem> items, std::span<Work> work, float spare, float shares) noexcept {
	bool reachedBound = false;
	for(std::size_t i = 0; i < items.size(); ++i) {
		if(work[i].frozen || work[i].length + spare * items[i].grow / shares <= items[i].maxMain) continue;
		work[i] = {items[i].maxMain, true, true};
		reachedBound = true;
	}
	return reachedBound;
}

/// @brief Shares the room left over on a line among its growing children, freezing those that reach their maximum.
/// @param items The line's children.
/// @param work Their sizes, hypothetical on entry.
/// @param room The container's main size.
/// @param gap The gap.
/// @return Whether the line now fills the room exactly.
bool growLine(std::span<const FlexItem> items, std::span<Work> work, float room, float gap) {
	for(std::size_t i = 0; i < items.size(); ++i) work[i].frozen = items[i].grow <= 0.0f;
	for(;;) {
		const float total = lineLength(work, gap);
		const float shares = growShares(items, work);
		if(shares <= 0.0f || room <= total) return false;

		const float spare = room - total;
		if(freezeAtMaximum(items, work, spare, shares)) continue;
		for(std::size_t i = 0; i < items.size(); ++i) {
			if(work[i].frozen) continue;
			work[i].length += spare * items[i].grow / shares;
			work[i].flexed = true;
		}
		return true;
	}
}

/// @brief The shrink factors times the sizes of a line's children that still flex.
/// @param items The line's children.
/// @param work Their sizes.
/// @return Their sum.
float shrinkShares(std::span<const FlexItem> items, std::span<const Work> work) noexcept {
	float scaled = 0.0f;
	for(std::size_t i = 0; i < items.size(); ++i)
		if(!work[i].frozen) scaled += items[i].shrink * work[i].length;
	return scaled;
}

/// @brief Freezes at their minimum the children that their share of the overflow would take below it.
/// @param items The line's children.
/// @param work Their sizes.
/// @param overflow How far the line is over the room.
/// @param scaled The shrink factors times the sizes of the children that still flex.
/// @return Whether any was frozen: the others then share what is left, from their hypothetical sizes again.
bool freezeAtMinimum(std::span<const FlexItem> items, std::span<Work> work, float overflow, float scaled) noexcept {
	bool reachedBound = false;
	for(std::size_t i = 0; i < items.size(); ++i) {
		if(work[i].frozen) continue;
		const float target = work[i].length - overflow * items[i].shrink * work[i].length / scaled;
		if(target >= items[i].minMain) continue;
		work[i] = {items[i].minMain, true, true};
		reachedBound = true;
	}
	return reachedBound;
}

/// @brief Takes a line's overflow from its shrinking children, in proportion to shrink times size, freezing those
/// that reach their minimum.
/// @param items The line's children.
/// @param work Their sizes, hypothetical on entry.
/// @param room The container's main size.
/// @param gap The gap.
/// @return Whether the line now fills the room exactly.
bool shrinkLine(std::span<const FlexItem> items, std::span<Work> work, float room, float gap) {
	for(std::size_t i = 0; i < items.size(); ++i) work[i].frozen = items[i].shrink <= 0.0f || work[i].length <= 0.0f;
	for(;;) {
		const float total = lineLength(work, gap);
		const float scaled = shrinkShares(items, work);
		if(scaled <= 0.0f || total <= room) return false;

		const float overflow = total - room;
		if(freezeAtMinimum(items, work, overflow, scaled)) continue;
		for(std::size_t i = 0; i < items.size(); ++i) {
			if(work[i].frozen) continue;
			work[i].length -= overflow * items[i].shrink * work[i].length / scaled;
			work[i].flexed = true;
		}
		return true;
	}
}

/// @brief Where the first child of a line starts and what is added between children, from the justification.
struct Spread {
	float start = 0.0f; ///< The first child's offset.
	float extra = 0.0f; ///< Added to each step, besides the gap.
};

/// @brief Spreads a line by its justification (Justify::End on a single line is placed from the end instead).
/// @param justify The justification.
/// @param free The room left over; negative when the line overflows.
/// @param count The line's children.
/// @return The spread.
Spread spread(Justify justify, float free, std::size_t count) noexcept {
	const auto children = static_cast<float>(count);
	switch(justify) {
	case Justify::Start:
		return {};
	case Justify::Center:
		return {std::max(free / 2.0f, 0.0f), 0.0f};
	case Justify::End:
		return {free, 0.0f};
	case Justify::SpaceBetween:
		return {0.0f, count > 1 && free > 0.0f ? free / (children - 1.0f) : 0.0f};
	case Justify::SpaceAround:
		if(free <= 0.0f || count == 0) return {};
		return {free / children / 2.0f, free / children};
	case Justify::SpaceEvenly:
		if(free <= 0.0f) return {};
		return {free / (children + 1.0f), free / (children + 1.0f)};
	}
	return {};
}

/// @brief Places a line's children across it.
/// @param items The line's children.
/// @param placements Their placements.
/// @param top Where the line starts across.
/// @param size How large the line is across.
/// @param wrapped Whether the container wraps: children without an alignment then sit at the line's start.
void placeAcross(std::span<const FlexItem> items, std::span<FlexPlacement> placements, float top, float size,
                 bool wrapped) {
	for(std::size_t i = 0; i < items.size(); ++i) {
		const FlexItem& item = items[i];
		const std::optional<CrossAlign> align = item.align ? item.align
		                                        : wrapped  ? std::optional(CrossAlign::Start)
		                                                   : std::nullopt;
		if(!align) continue;
		const float own = bounded(item.cross, item.minCross, item.maxCross);
		switch(*align) {
		case CrossAlign::Start:
			placements[i].crossOffset = top;
			break;
		case CrossAlign::Center:
			placements[i].crossOffset = top + (size - own) / 2.0f;
			break;
		case CrossAlign::End:
			placements[i].crossOffset = top + size - own;
			break;
		case CrossAlign::Stretch:
			placements[i].crossOffset = top;
			placements[i].crossSize = bounded(size, item.minCross, item.maxCross);
			break;
		}
	}
}

/// @brief Grows or shrinks a line's children to fill the container, if they flex that way.
/// @param options The container.
/// @param items The line's children.
/// @param work Their hypothetical sizes; flexed.
/// @return The line's length afterwards, gaps included.
float flexLine(const FlexOptions& options, std::span<const FlexItem> items, std::span<Work> work) {
	const float room = options.main;
	const float before = lineLength(work, options.gap);
	const bool grows = std::ranges::any_of(items, [](const FlexItem& item) { return item.grow > 0.0f; });
	const bool shrinks = std::ranges::any_of(items, [](const FlexItem& item) { return item.shrink > 0.0f; });
	const bool filled = before < room ? grows && growLine(items, work, room, options.gap)
	                                  : before > room && shrinks && shrinkLine(items, work, room, options.gap);
	return filled ? room : lineLength(work, options.gap);
}

/// @brief Places a line's children from the container's end back, so that an overflow is cut at the start.
/// @param work Their sizes.
/// @param placements Their placements.
/// @param gap The gap.
void placeFromEnd(std::span<const Work> work, std::span<FlexPlacement> placements, float gap) noexcept {
	float offset = 0.0f;
	for(std::size_t i = work.size(); i-- > 0;) {
		placements[i].main = offset;
		placements[i].fromEnd = true;
		offset += work[i].length + gap;
	}
}

/// @brief Lays one line out along the direction: flexing, then spreading.
/// @param options The container.
/// @param items The line's children.
/// @param work Their hypothetical sizes.
/// @param placements Their placements.
void placeAlong(const FlexOptions& options, std::span<const FlexItem> items, std::span<Work> work,
                std::span<FlexPlacement> placements) {
	const float total = flexLine(options, items, work);
	for(std::size_t i = 0; i < items.size(); ++i)
		if(work[i].flexed || items[i].sized) placements[i].mainSize = work[i].length;

	if(options.justify == Justify::End && !options.wrap) {
		placeFromEnd(work, placements, options.gap);
		return;
	}
	const auto [start, extra] = spread(options.justify, options.main - total, items.size());
	float offset = start;
	for(std::size_t i = 0; i < items.size(); ++i) {
		placements[i].main = offset;
		offset += work[i].length + options.gap + extra;
	}
}

} // namespace

void layoutFlex(const FlexOptions& options, std::span<const FlexItem> items, std::vector<FlexPlacement>& placements) {
	placements.assign(items.size(), FlexPlacement{});
	std::vector<Work> work(items.size());
	for(std::size_t i = 0; i < items.size(); ++i)
		work[i].length = bounded(items[i].base, items[i].minMain, items[i].maxMain);

	float lineTop = 0.0f;
	for(std::size_t first = 0; first < items.size();) {
		// A line ends before the child that would cross the end, unless nothing with a size comes before it
		std::size_t last = first + 1;
		if(options.wrap) {
			float x = work[first].length + options.gap;
			while(last < items.size() && (x <= 0.0f || x + work[last].length <= options.main))
				x += work[last++].length + options.gap;
		} else {
			last = items.size();
		}

		const auto count = last - first;
		const std::span<const FlexItem> line = items.subspan(first, count);
		placeAlong(options, line, std::span(work).subspan(first, count), std::span(placements).subspan(first, count));

		float lineSize = options.cross;
		if(options.wrap) {
			lineSize = 0.0f;
			for(const FlexItem& item : line)
				lineSize = std::max(lineSize, bounded(item.cross, item.minCross, item.maxCross));
		}
		placeAcross(line, std::span(placements).subspan(first, count), lineTop, lineSize, options.wrap);
		lineTop += lineSize + options.lineGap;
		first = last;
	}
}

} // namespace affresco::detail
