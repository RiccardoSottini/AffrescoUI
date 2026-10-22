/// @file
/// @brief History: recording tile changes, the list of steps and its budget, applying a step.

#include "affresco/imaging/History.hpp"

#include <utility>

namespace affresco::imaging {

namespace {

/// @brief Packs an image id and a tile's place into one key (ids below 2^24, places below 2^20).
/// @param id The image's id.
/// @param column The column.
/// @param row The row.
[[nodiscard]] std::uint64_t pack(std::uint64_t id, int column, int row) noexcept {
	return (id << 40) ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(column)) << 20) ^
	       static_cast<std::uint32_t>(row);
}

/// @brief The memory of a tile, or 0 for null.
/// @param tile The tile.
[[nodiscard]] std::size_t bytesOf(const std::shared_ptr<const Tile>& tile) noexcept {
	return tile ? tile->memoryBytes() : 0;
}

} // namespace

std::size_t HistoryStep::memoryBytes() const noexcept {
	std::size_t bytes = sizeof(HistoryStep) + name.size() + structureBytes;
	for(const TileChange& change : tiles) bytes += sizeof(TileChange) + bytesOf(change.before) + bytesOf(change.after);
	return bytes;
}

void TileRecorder::touch(std::uint64_t id, const Image& image, int column, int row) {
	const std::uint64_t key = pack(id, column, row);
	if(index_.contains(key)) return;
	index_.emplace(key, changes_.size());
	changes_.push_back(TileChange{id, column, row, image.tile({0, column, row}), nullptr});
}

bool TileRecorder::touched(std::uint64_t id, int column, int row) const noexcept {
	return index_.contains(pack(id, column, row));
}

std::vector<TileChange> TileRecorder::finish(const std::function<const Image*(std::uint64_t)>& imageOf) {
	std::vector<TileChange> changes;
	changes.reserve(changes_.size());
	for(TileChange& change : changes_) {
		const Image* image = imageOf(change.image);
		if(image == nullptr) continue;
		change.after = image->tile({0, change.column, change.row});
		if(change.after == change.before) continue; // never written: the same shared tile
		changes.push_back(std::move(change));
	}
	changes_.clear();
	index_.clear();
	return changes;
}

bool History::amendLast(std::string_view name, std::shared_ptr<const void> structureAfter) noexcept {
	if(steps_.empty() || position_ != steps_.size()) return false;
	HistoryStep& last = steps_.back();
	if(last.name != name || !last.tiles.empty() || !last.structureAfter) return false;
	last.structureAfter = std::move(structureAfter);
	return true;
}

void History::push(HistoryStep step) {
	while(steps_.size() > position_) {
		bytes_ -= steps_.back().memoryBytes();
		steps_.pop_back();
	}
	bytes_ += step.memoryBytes();
	steps_.push_back(std::move(step));
	position_ = steps_.size();
	std::size_t drop = 0;
	std::size_t remaining = bytes_;
	// The newest step stays even beyond the budget: an edit can always be undone once
	while(steps_.size() - drop > 1 && (remaining > budget_ || steps_.size() - drop > maximumSteps_))
		remaining -= steps_[drop++].memoryBytes();
	if(drop == 0) return;
	steps_.erase(steps_.begin(), steps_.begin() + static_cast<std::ptrdiff_t>(drop));
	bytes_ = remaining;
	position_ -= drop;
	dropped_ += drop;
}

const HistoryStep* History::undo() noexcept {
	if(position_ == 0) return nullptr;
	return &steps_[--position_];
}

const HistoryStep* History::redo() noexcept {
	if(position_ == steps_.size()) return nullptr;
	return &steps_[position_++];
}

void History::clear() noexcept {
	steps_.clear();
	position_ = 0;
	bytes_ = 0;
}

void applyTiles(const HistoryStep& step, bool after, const std::function<Image*(std::uint64_t)>& imageOf) {
	const std::size_t count = step.tiles.size();
	for(std::size_t index = 0; index < count; ++index) {
		const TileChange& change = step.tiles[after ? index : count - 1 - index];
		if(Image* image = imageOf(change.image))
			image->setTile(change.column, change.row, after ? change.after : change.before);
	}
}

} // namespace affresco::imaging
