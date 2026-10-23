#pragma once

/// @file
/// @brief ViewRows: how many rows each line of a TextEditor takes on screen (several when wrapped, none when folded),
/// with the row of a line and the line of a row found in O(log n).

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace affresco::detail {

/// @brief The rows of each line, in a Fenwick tree: a line's first row is the sum of the rows before it.
class ViewRows {
public:
	/// @brief Sets every line's rows at once, in O(n).
	/// @param counts The rows of each line.
	void assign(const std::vector<std::uint32_t>& counts) {
		counts_ = counts;
		tree_.assign(counts_.size() + 1, 0);
		for(std::size_t i = 1; i <= counts_.size(); ++i) {
			tree_[i] += counts_[i - 1];
			const std::size_t parent = i + (i & (~i + 1));
			if(parent < tree_.size()) tree_[parent] += tree_[i];
		}
		total_ = 0;
		for(const std::uint32_t count : counts_) total_ += count;
	}

	/// @brief Changes one line's rows, in O(log n).
	/// @param line The line, below lines().
	/// @param count Its rows.
	void set(std::size_t line, std::uint32_t count) {
		const auto delta = static_cast<std::ptrdiff_t>(count) - static_cast<std::ptrdiff_t>(counts_[line]);
		counts_[line] = count;
		total_ = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(total_) + delta);
		for(std::size_t i = line + 1; i < tree_.size(); i += i & (~i + 1))
			tree_[i] = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(tree_[i]) + delta);
	}

	/// @brief The lines.
	[[nodiscard]] std::size_t lines() const noexcept { return counts_.size(); }
	/// @brief The rows of all lines.
	[[nodiscard]] std::size_t total() const noexcept { return total_; }
	/// @brief A line's rows.
	/// @param line The line, below lines().
	[[nodiscard]] std::uint32_t count(std::size_t line) const noexcept { return counts_[line]; }

	/// @brief The first row of a line: the rows of the lines before it.
	/// @param line The line, at most lines().
	[[nodiscard]] std::size_t rowOf(std::size_t line) const noexcept {
		std::size_t sum = 0;
		for(std::size_t i = line; i > 0; i -= i & (~i + 1)) sum += tree_[i];
		return sum;
	}

	/// @brief The line a row is on, and which of its rows it is.
	/// @param row The row, below total().
	[[nodiscard]] std::pair<std::size_t, std::size_t> lineAt(std::size_t row) const noexcept {
		std::size_t position = 0, step = 1;
		while(step * 2 < tree_.size()) step *= 2;
		// The last line whose rows before it are at most the row, then the first shown from there
		for(; step > 0; step /= 2)
			if(position + step < tree_.size() && tree_[position + step] <= row) {
				position += step;
				row -= tree_[position];
			}
		while(position < counts_.size() && counts_[position] == 0) ++position;
		if(position >= counts_.size()) return {counts_.empty() ? 0 : counts_.size() - 1, 0};
		return {position, row};
	}

private:
	std::vector<std::uint32_t> counts_; ///< The rows of each line.
	std::vector<std::size_t> tree_;     ///< The Fenwick tree, from 1.
	std::size_t total_ = 0;             ///< See total().
};

} // namespace affresco::detail
