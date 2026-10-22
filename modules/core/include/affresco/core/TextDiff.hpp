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
	std::size_t maxCost = 20'000'000;   ///< Steps (diagonals tried and elements compared), over the whole diff.
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

	Kind kind = Kind::Equal; ///< What the run does.
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
		struct Task {
			std::size_t a0, a1, b0, b1; ///< The ranges.
			bool equal;                 ///< Only emit them as equal (a common end found earlier).
		};
		std::vector<Task> tasks{{0, a_.size(), 0, b_.size(), false}};
		while(!tasks.empty()) {
			Task t = tasks.back();
			tasks.pop_back();
			if(t.equal) {
				emit(DiffEdit::Kind::Equal, t.a0, t.b0, t.a1 - t.a0);
				continue;
			}
			std::size_t prefix = 0, suffix = 0;
			while(t.a0 + prefix < t.a1 && t.b0 + prefix < t.b1 && a_[t.a0 + prefix] == b_[t.b0 + prefix]) ++prefix;
			emit(DiffEdit::Kind::Equal, t.a0, t.b0, prefix);
			t.a0 += prefix;
			t.b0 += prefix;
			while(t.a1 - suffix > t.a0 && t.b1 - suffix > t.b0 && a_[t.a1 - suffix - 1] == b_[t.b1 - suffix - 1]) ++suffix;
			t.a1 -= suffix;
			t.b1 -= suffix;
			if(suffix > 0) tasks.push_back({t.a1, t.a1 + suffix, t.b1, t.b1 + suffix, true});
			const std::optional<std::pair<std::size_t, std::size_t>> split =
			    t.a0 == t.a1 || t.b0 == t.b1 ? std::nullopt : bisect(t.a0, t.a1, t.b0, t.b1);
			// A split at either corner would not shrink the range
			if(!split || (split->first == t.a0 && split->second == t.b0) || (split->first == t.a1 && split->second == t.b1)) {
				emit(DiffEdit::Kind::Delete, t.a0, t.b0, t.a1 - t.a0);
				emit(DiffEdit::Kind::Insert, t.a1, t.b0, t.b1 - t.b0);
				continue;
			}
			tasks.push_back({split->first, t.a1, split->second, t.b1, false});
			tasks.push_back({t.a0, split->first, t.b0, split->second, false});
		}
		return changesInOrder(std::move(edits_));
	}

private:
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
		const std::ptrdiff_t most = std::min<std::ptrdiff_t>((n + m + 1) / 2, static_cast<std::ptrdiff_t>(limits_.maxDistance));
		const std::ptrdiff_t offset = most, length = 2 * most + 2, delta = n - m;
		for(std::ptrdiff_t i = 0; i < length; i += 64)
			if(!spend()) return std::nullopt; // clearing the arrays costs too
		forward_.assign(static_cast<std::size_t>(length), -1);
		backward_.assign(static_cast<std::size_t>(length), -1);
		forward_[static_cast<std::size_t>(offset + 1)] = 0;
		backward_[static_cast<std::size_t>(offset + 1)] = 0;
		const bool odd = (delta & 1) != 0;
		const auto at = [](std::vector<std::ptrdiff_t>& v, std::ptrdiff_t i) -> std::ptrdiff_t& {
			return v[static_cast<std::size_t>(i)];
		};
		// Diagonals that ran off the bottom or the right edge are not tried again
		std::ptrdiff_t forwardStart = 0, forwardEnd = 0, backwardStart = 0, backwardEnd = 0;
		for(std::ptrdiff_t d = 0; d < most; ++d) {
			for(std::ptrdiff_t k = -d + forwardStart; k <= d - forwardEnd; k += 2) {
				if(!spend()) return std::nullopt;
				const std::ptrdiff_t i = offset + k;
				std::ptrdiff_t x = k == -d || (k != d && at(forward_, i - 1) < at(forward_, i + 1)) ? at(forward_, i + 1)
				                                                                                   : at(forward_, i - 1) + 1;
				std::ptrdiff_t y = x - k;
				while(x < n && y < m && a_[a0 + static_cast<std::size_t>(x)] == b_[b0 + static_cast<std::size_t>(y)]) {
					if(!spend()) return std::nullopt;
					++x;
					++y;
				}
				at(forward_, i) = x;
				if(x > n) forwardEnd += 2;
				else if(y > m) forwardStart += 2;
				else if(odd) {
					const std::ptrdiff_t j = offset + delta - k;
					if(j >= 0 && j < length && at(backward_, j) != -1 && x >= n - at(backward_, j))
						return std::pair{a0 + static_cast<std::size_t>(x), b0 + static_cast<std::size_t>(y)};
				}
			}
			for(std::ptrdiff_t k = -d + backwardStart; k <= d - backwardEnd; k += 2) {
				if(!spend()) return std::nullopt;
				const std::ptrdiff_t i = offset + k;
				std::ptrdiff_t x = k == -d || (k != d && at(backward_, i - 1) < at(backward_, i + 1))
				                       ? at(backward_, i + 1)
				                       : at(backward_, i - 1) + 1;
				std::ptrdiff_t y = x - k;
				while(x < n && y < m &&
				      a_[a1 - 1 - static_cast<std::size_t>(x)] == b_[b1 - 1 - static_cast<std::size_t>(y)]) {
					if(!spend()) return std::nullopt;
					++x;
					++y;
				}
				at(backward_, i) = x;
				if(x > n) backwardEnd += 2;
				else if(y > m) backwardStart += 2;
				else if(!odd) {
					const std::ptrdiff_t j = offset + delta - k;
					if(j >= 0 && j < length && at(forward_, j) != -1) {
						const std::ptrdiff_t forwardX = at(forward_, j), forwardY = forwardX - (j - offset);
						if(forwardX >= n - x)
							return std::pair{a0 + static_cast<std::size_t>(forwardX), b0 + static_cast<std::size_t>(forwardY)};
					}
				}
			}
		}
		return std::nullopt;
	}

	std::span<const std::uint32_t> a_, b_;  ///< The sequences.
	DiffLimits limits_;                     ///< What it may spend.
	std::size_t budget_;                    ///< Steps left.
	std::vector<DiffEdit> edits_;           ///< The script so far.
	std::vector<std::ptrdiff_t> forward_;   ///< The furthest x on each diagonal, from the start.
	std::vector<std::ptrdiff_t> backward_;  ///< The furthest x on each diagonal, from the end.
};

} // namespace detail

/// @brief The shortest edit script between two sequences, within limits (past them, a longer but correct one).
/// @param original The first sequence (e.g. line or code point numbers).
/// @param modified The second.
/// @param limits What it may spend.
/// @return Runs in order, covering both sequences; neighbours of one kind merged, deletions before insertions.
[[nodiscard]] inline std::vector<DiffEdit> diffSequences(std::span<const std::uint32_t> original,
                                                         std::span<const std::uint32_t> modified, DiffLimits limits = {}) {
	return detail::Differ(original, modified, limits).run();
}

/// @brief Splits a text into lines at "\n" (a "\r" before it is dropped); a text ending in "\n" ends in an empty line.
/// @param text The text.
/// @return Views into it, at least one.
[[nodiscard]] inline std::vector<std::string_view> splitLines(std::string_view text) {
	std::vector<std::string_view> lines;
	for(std::size_t start = 0;;) {
		const std::size_t end = text.find('\n', start);
		std::string_view line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
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
                                                     std::span<const std::string_view> modified, DiffLimits limits = {}) {
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
