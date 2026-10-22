#pragma once

/// @file
/// @brief diffSequences() and diffLines(): the shortest edit script between two texts (Myers' O(ND) algorithm, in
/// linear space), within a cost limit, for diff views, changed-line markers and merges.
///
/// The common start and end are taken off each range first, then the range is split at its middle snake and both
/// halves are diffed the same way (from an explicit stack: no recursion). Each step counts against DiffLimits::maxCost;
/// once it runs out, or when the edit distance of a range passes DiffLimits::maxDistance, what is left of that range
/// is one deletion then one insertion: still a correct script, only a longer one. So two texts from anywhere are
/// diffed in bounded time and memory.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco {

/// @brief What a diff may spend.
struct DiffLimits {
	std::size_t maxCost = 20'000'000;  ///< Steps (diagonals tried and elements compared), over the whole diff.
	std::size_t maxDistance = 100'000; ///< Edits one range is searched for, at most (memory: 16 bytes each).
};

/// @brief A run of the edit script: elements kept, deleted from the original, or inserted from the modified text.
struct DiffEdit {
	/// @brief What the run does.
	enum class Kind : std::uint8_t {
		Equal,  ///< In both texts.
		Delete, ///< Only in the original.
		Insert, ///< Only in the modified text.
	};

	Kind kind = Kind::Equal;  ///< What the run does.
	std::size_t original = 0; ///< Where it starts in the original (for Insert: the element it goes before).
	std::size_t modified = 0; ///< Where it starts in the modified text (for Delete: the element it comes before).
	std::size_t count = 0;    ///< How many elements, at least one.

	/// @brief Whether two runs are the same.
	bool operator==(const DiffEdit&) const = default;
};

namespace detail {

/// @brief Diffs two sequences of numbers: the work of diffSequences(), with the arrays it reuses.
class Differ {
public:
	/// @brief Prepares a diff.
	/// @param original The first sequence.
	/// @param modified The second.
	/// @param limits What it may spend.
	Differ(std::span<const std::uint32_t> original, std::span<const std::uint32_t> modified, DiffLimits limits) noexcept
	    : a_(original), b_(modified), limits_(limits), budget_(limits.maxCost) {}

	/// @brief The edit script: runs in order, neighbours of one kind merged, deletions before insertions.
	[[nodiscard]] std::vector<DiffEdit> run() {
		std::vector<Task> tasks{{0, a_.size(), 0, b_.size(), false}};
		while(!tasks.empty()) {
			Task t = tasks.back();
			tasks.pop_back();
			if(t.equal) {
				emit(DiffEdit::Kind::Equal, t.a0, t.b0, t.a1 - t.a0);
				continue;
			}
			trim(t, tasks);
			divide(t, tasks);
		}
		return changesInOrder(std::move(edits_));
	}

private:
	/// @brief A range of both sequences left to diff.
	struct Task {
		std::size_t a0, a1, b0, b1; ///< The ranges.
		bool equal;                 ///< Only emit them as equal (a common end found earlier).
	};

	/// @brief Takes the common start of a range off (emitted now) and its common end (a task, emitted after it).
	/// @param t The range; it shrinks.
	/// @param tasks Gets the common end.
	void trim(Task& t, std::vector<Task>& tasks) {
		std::size_t prefix = 0, suffix = 0;
		while(t.a0 + prefix < t.a1 && t.b0 + prefix < t.b1 && a_[t.a0 + prefix] == b_[t.b0 + prefix]) ++prefix;
		emit(DiffEdit::Kind::Equal, t.a0, t.b0, prefix);
		t.a0 += prefix;
		t.b0 += prefix;
		while(t.a1 - suffix > t.a0 && t.b1 - suffix > t.b0 && a_[t.a1 - suffix - 1] == b_[t.b1 - suffix - 1]) ++suffix;
		t.a1 -= suffix;
		t.b1 -= suffix;
		if(suffix > 0) tasks.push_back({t.a1, t.a1 + suffix, t.b1, t.b1 + suffix, true});
	}

	/// @brief Splits a trimmed range at its middle snake into two tasks, or emits it as one change when it can't be.
	/// @param t The range.
	/// @param tasks Gets the halves, the first on top.
	void divide(const Task& t, std::vector<Task>& tasks) {
		const std::optional<std::pair<std::size_t, std::size_t>> split =
		    t.a0 == t.a1 || t.b0 == t.b1 ? std::nullopt : bisect(t.a0, t.a1, t.b0, t.b1);
		// A split at either corner would not shrink the range
		if(!split || (split->first == t.a0 && split->second == t.b0) ||
		   (split->first == t.a1 && split->second == t.b1)) {
			emit(DiffEdit::Kind::Delete, t.a0, t.b0, t.a1 - t.a0);
			emit(DiffEdit::Kind::Insert, t.a1, t.b0, t.b1 - t.b0);
			return;
		}
		tasks.push_back({split->first, t.a1, split->second, t.b1, false});
		tasks.push_back({t.a0, split->first, t.b0, split->second, false});
	}

	/// @brief Turns each change between two kept runs into one deletion then one insertion (the halves of a split can
	/// leave an insertion before a deletion, or several of each).
	/// @param edits The runs, in order.
	[[nodiscard]] static std::vector<DiffEdit> changesInOrder(std::vector<DiffEdit> edits) {
		std::vector<DiffEdit> out;
		out.reserve(edits.size());
		for(std::size_t e = 0; e < edits.size();) {
			if(edits[e].kind == DiffEdit::Kind::Equal) {
				out.push_back(edits[e++]);
				continue;
			}
			const std::size_t original = edits[e].original, modified = edits[e].modified;
			std::size_t deleted = 0, inserted = 0;
			for(; e < edits.size() && edits[e].kind != DiffEdit::Kind::Equal; ++e)
				(edits[e].kind == DiffEdit::Kind::Delete ? deleted : inserted) += edits[e].count;
			if(deleted > 0) out.push_back({DiffEdit::Kind::Delete, original, modified, deleted});
			if(inserted > 0) out.push_back({DiffEdit::Kind::Insert, original + deleted, modified, inserted});
		}
		return out;
	}

	/// @brief Adds a run, or lengthens the last one when it is of the same kind and just before it.
	/// @param kind What it does.
	/// @param original Where it starts in the original.
	/// @param modified Where it starts in the modified text.
	/// @param count How many elements (none: nothing is added).
	void emit(DiffEdit::Kind kind, std::size_t original, std::size_t modified, std::size_t count) {
		if(count == 0) return;
		if(!edits_.empty() && edits_.back().kind == kind) {
			DiffEdit& last = edits_.back();
			const bool next = kind == DiffEdit::Kind::Insert ? last.modified + last.count == modified
			                                                 : last.original + last.count == original;
			if(next) {
				last.count += count;
				return;
			}
		}
		edits_.push_back({kind, original, modified, count});
	}

	/// @brief Spends one step of the budget.
	/// @return false when none is left.
	[[nodiscard]] bool spend() noexcept {
		if(budget_ == 0) return false;
		--budget_;
		return true;
	}

	/// @brief The two ranges a bisect searches, and the size of its arrays.
	struct Ranges {
		std::size_t a0, a1, b0, b1;    ///< The ranges.
		std::ptrdiff_t n, m;           ///< Their lengths.
		std::ptrdiff_t offset, length; ///< Diagonal k is at offset + k in arrays of this length.
	};

	/// @brief A search from one end of the ranges.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): bisect() sets every field
	struct Front {
		std::vector<std::ptrdiff_t>* furthest; ///< The furthest x on each diagonal.
		bool backward;                         ///< From the end.
		std::ptrdiff_t start = 0, end = 0; ///< Diagonals that ran off the bottom or the right edge, not tried again.
	};

	/// @brief Finds where the shortest script of two non-empty ranges crosses its middle (the forward and backward
	/// searches meet), as in Myers' paper and diff-match-patch's bisect.
	/// @param a0 The original's range start.
	/// @param a1 Its end.
	/// @param b0 The modified range's start.
	/// @param b1 Its end.
	/// @return The point to split both ranges at, or nothing when the limits ran out first.
	[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> bisect(std::size_t a0, std::size_t a1,
	                                                                        std::size_t b0, std::size_t b1) {
		const auto n = static_cast<std::ptrdiff_t>(a1 - a0), m = static_cast<std::ptrdiff_t>(b1 - b0);
		const std::ptrdiff_t most =
		    std::min<std::ptrdiff_t>((n + m + 1) / 2, static_cast<std::ptrdiff_t>(limits_.maxDistance));
		const Ranges r{a0, a1, b0, b1, n, m, most, 2 * most + 2};
		for(std::ptrdiff_t i = 0; i < r.length; i += 64)
			if(!spend()) return std::nullopt; // clearing the arrays costs too
		forward_.assign(static_cast<std::size_t>(r.length), -1);
		backward_.assign(static_cast<std::size_t>(r.length), -1);
		forward_[static_cast<std::size_t>(r.offset + 1)] = 0;
		backward_[static_cast<std::size_t>(r.offset + 1)] = 0;
		const bool odd = ((n - m) & 1) != 0;
		Front forward{&forward_, false}, backward{&backward_, true};
		for(std::ptrdiff_t d = 0; d < most; ++d)
			for(Front* front : {&forward, &backward}) {
				// The searches can meet in the forward one when the distance is odd, else in the backward one
				const auto [stop, split] = pass(*front, r, d, front->backward != odd);
				if(stop) return split;
			}
		return std::nullopt;
	}

	/// @brief Takes each diagonal of a front one edit further, at one distance.
	/// @param front The front.
	/// @param r The ranges.
	/// @param d The distance.
	/// @param overlap Whether to look for the other front.
	/// @return Whether to stop (the limits ran out, or the fronts met), and where they met.
	[[nodiscard]] std::pair<bool, std::optional<std::pair<std::size_t, std::size_t>>>
	pass(Front& front, const Ranges& r, std::ptrdiff_t d, bool overlap) {
		for(std::ptrdiff_t k = -d + front.start; k <= d - front.end; k += 2) {
			const std::optional<bool> inside = extend(front, r, d, k);
			if(!inside) return {true, std::nullopt};
			if(!*inside || !overlap) continue;
			if(const auto met = meeting(r, k, front.backward)) return {true, met};
		}
		return {false, std::nullopt};
	}

	/// @brief Takes one diagonal of a front one edit further, then along the elements both ranges share.
	/// @param front The front; a diagonal that runs off an edge is dropped from it.
	/// @param r The ranges.
	/// @param d The distance.
	/// @param k The diagonal.
	/// @return Whether it stays inside both ranges, or nothing when the limits ran out.
	[[nodiscard]] std::optional<bool> extend(Front& front, const Ranges& r, std::ptrdiff_t d, std::ptrdiff_t k) {
		if(!spend()) return std::nullopt;
		std::vector<std::ptrdiff_t>& v = *front.furthest;
		const auto at = [&v](std::ptrdiff_t i) -> std::ptrdiff_t& { return v[static_cast<std::size_t>(i)]; };
		const std::ptrdiff_t i = r.offset + k;
		std::ptrdiff_t x = k == -d || (k != d && at(i - 1) < at(i + 1)) ? at(i + 1) : at(i - 1) + 1;
		std::ptrdiff_t y = x - k;
		while(x < r.n && y < r.m && same(r, front.backward, x, y)) {
			if(!spend()) return std::nullopt;
			++x;
			++y;
		}
		at(i) = x;
		if(x > r.n) front.end += 2;
		else if(y > r.m) front.start += 2;
		return x <= r.n && y <= r.m;
	}

	/// @brief Whether the elements at a point are the same, counted from the ranges' start or end.
	/// @param r The ranges.
	/// @param backward From their end.
	/// @param x How far into the original's range.
	/// @param y How far into the modified one.
	[[nodiscard]] bool same(const Ranges& r, bool backward, std::ptrdiff_t x, std::ptrdiff_t y) const noexcept {
		const auto ux = static_cast<std::size_t>(x), uy = static_cast<std::size_t>(y);
		return backward ? a_[r.a1 - 1 - ux] == b_[r.b1 - 1 - uy] : a_[r.a0 + ux] == b_[r.b0 + uy];
	}

	/// @brief Where a diagonal just extended meets the other front, if it does.
	/// @param r The ranges.
	/// @param k The diagonal.
	/// @param backward Whether it was extended from the end.
	/// @return The split point, from the forward front's end on that diagonal.
	[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> meeting(const Ranges& r, std::ptrdiff_t k,
	                                                                         bool backward) const {
		const std::ptrdiff_t j = r.offset + (r.n - r.m) - k;
		if(j < 0 || j >= r.length) return std::nullopt;
		const std::vector<std::ptrdiff_t>& other = backward ? forward_ : backward_;
		const std::ptrdiff_t otherX = other[static_cast<std::size_t>(j)];
		if(otherX == -1) return std::nullopt;
		const std::ptrdiff_t x = (backward ? backward_ : forward_)[static_cast<std::size_t>(r.offset + k)];
		if(x < r.n - otherX) return std::nullopt;
		// The forward front's furthest point: on diagonal k going forward, or on j coming back
		const std::ptrdiff_t forwardX = backward ? otherX : x, forwardY = backward ? otherX - (j - r.offset) : x - k;
		return std::pair{r.a0 + static_cast<std::size_t>(forwardX), r.b0 + static_cast<std::size_t>(forwardY)};
	}

	std::span<const std::uint32_t> a_, b_; ///< The sequences.
	DiffLimits limits_;                    ///< What it may spend.
	std::size_t budget_;                   ///< Steps left.
	std::vector<DiffEdit> edits_;          ///< The script so far.
	std::vector<std::ptrdiff_t> forward_;  ///< The furthest x on each diagonal, from the start.
	std::vector<std::ptrdiff_t> backward_; ///< The furthest x on each diagonal, from the end.
};

} // namespace detail

/// @brief The shortest edit script between two sequences, within limits (past them, a longer but correct one).
/// @param original The first sequence (e.g. line or code point numbers).
/// @param modified The second.
/// @param limits What it may spend.
/// @return Runs in order, covering both sequences; neighbours of one kind merged, deletions before insertions.
[[nodiscard]] inline std::vector<DiffEdit> diffSequences(std::span<const std::uint32_t> original,
                                                         std::span<const std::uint32_t> modified,
                                                         DiffLimits limits = {}) {
	return detail::Differ(original, modified, limits).run();
}

/// @brief Splits a text into lines at "\n" (a "\r" before it is dropped); a text ending in "\n" ends in an empty line.
/// @param text The text.
/// @return Views into it, at least one.
[[nodiscard]] inline std::vector<std::string_view> splitLines(std::string_view text) {
	std::vector<std::string_view> lines;
	for(std::size_t start = 0;;) {
		const std::size_t end = text.find('\n', start);
		std::string_view line =
		    text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if(line.ends_with('\r') && end != std::string_view::npos) line.remove_suffix(1);
		lines.push_back(line);
		if(end == std::string_view::npos) return lines;
		start = end + 1;
	}
}

/// @brief The edit script between two lists of lines (each line numbered by its text, then diffSequences()).
/// @param original The original's lines.
/// @param modified The modified lines.
/// @param limits What it may spend.
[[nodiscard]] inline std::vector<DiffEdit> diffLines(std::span<const std::string_view> original,
                                                     std::span<const std::string_view> modified,
                                                     DiffLimits limits = {}) {
	std::unordered_map<std::string_view, std::uint32_t> numbers;
	numbers.reserve(original.size() + modified.size());
	const auto number = [&numbers](std::span<const std::string_view> lines) {
		std::vector<std::uint32_t> out;
		out.reserve(lines.size());
		for(const std::string_view line : lines)
			out.push_back(numbers.try_emplace(line, static_cast<std::uint32_t>(numbers.size())).first->second);
		return out;
	};
	const std::vector<std::uint32_t> a = number(original), b = number(modified);
	return diffSequences(a, b, limits);
}

} // namespace affresco
