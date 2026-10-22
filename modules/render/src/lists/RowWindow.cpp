/// @file
/// @brief RowWindow: row offsets with a fixed height, or a Fenwick tree once a row is measured.

#include "affresco/render/RowWindow.hpp"

#include <algorithm>
#include <bit>

namespace affresco {

void RowWindow::reset(std::size_t count, float rowHeight) {
	count_ = count;
	rowHeight_ = std::max(rowHeight, 1.0f);
	heights_.clear();
	tree_.clear();
}

void RowWindow::setHeight(std::size_t row, float height) {
	if(row >= count_) return;
	height = std::max(height, 0.0f);
	if(tree_.empty()) {
		if(height == rowHeight_) return;
		// The first measured row: build the tree over equal heights in O(n)
		heights_.assign(count_, rowHeight_);
		tree_.assign(count_ + 1, 0.0f);
		for(std::size_t i = 1; i <= count_; ++i) {
			tree_[i] += rowHeight_;
			const std::size_t parent = i + (i & (~i + 1));
			if(parent <= count_) tree_[parent] += tree_[i];
		}
	}
	const float change = height - heights_[row];
	if(change == 0.0f) return;
	heights_[row] = height;
	for(std::size_t i = row + 1; i <= count_; i += i & (~i + 1)) tree_[i] += change;
}

float RowWindow::heightOf(std::size_t row) const noexcept {
	if(row >= count_) return 0.0f;
	return tree_.empty() ? rowHeight_ : heights_[row];
}

float RowWindow::offsetOf(std::size_t row) const noexcept {
	row = std::min(row, count_);
	if(tree_.empty()) return static_cast<float>(row) * rowHeight_;
	float sum = 0.0f;
	for(std::size_t i = row; i > 0; i -= i & (~i + 1)) sum += tree_[i];
	return sum;
}

std::size_t RowWindow::rowAt(float offset) const noexcept {
	if(count_ == 0) return 0;
	if(!(offset > 0.0f)) return 0; // NaN too
	if(tree_.empty()) {
		// Compared before the cast: a list as tall as 1e30 puts the row past size_t's range, where the cast is undefined
		const float row = offset / rowHeight_;
		return row < static_cast<float>(count_ - 1) ? static_cast<std::size_t>(row) : count_ - 1;
	}
	// Descend the tree for the last prefix whose sum is at most the offset
	std::size_t position = 0;
	float remaining = offset;
	for(std::size_t step = std::bit_floor(count_); step > 0; step /= 2) {
		const std::size_t next = position + step;
		if(next <= count_ && tree_[next] <= remaining) {
			position = next;
			remaining -= tree_[next];
		}
	}
	return std::min(position, count_ - 1);
}

RowRange RowWindow::visible(float scroll, float viewport, std::size_t margin) const noexcept {
	if(count_ == 0) return {};
	const std::size_t first = rowAt(scroll);
	std::size_t last = rowAt(scroll + std::max(viewport, 0.0f)) + 1;
	last = std::min(last + margin, count_);
	return {first > margin ? first - margin : 0, last};
}

} // namespace affresco
