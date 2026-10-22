#pragma once

/// @file
/// @brief History: undo and redo by tile differences. A step keeps only the tiles it changed, before and after, and
/// the history drops its oldest steps beyond a memory budget.

#include "affresco/imaging/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::imaging {

/// @brief One tile a step changed: which image (by the id its owner gives), where, and the tile before and after.
struct TileChange {
	std::uint64_t image = 0;            ///< The image's id in its document.
	int column = 0;                     ///< The tile's column.
	int row = 0;                        ///< The tile's row.
	std::shared_ptr<const Tile> before; ///< The tile before, or null (transparent).
	std::shared_ptr<const Tile> after;  ///< The tile after, or null (transparent).
};

/// @brief One step: its name (the History panel's row), its tile changes and, for a change of the layers themselves,
/// the document's structure before and after (opaque to the history; shared, so cheap).
struct HistoryStep {
	std::string name;                            ///< What was done, e.g. "Brush".
	std::vector<TileChange> tiles;               ///< The tiles changed.
	std::shared_ptr<const void> structureBefore; ///< The structure before, or null if unchanged.
	std::shared_ptr<const void> structureAfter;  ///< The structure after, or null if unchanged.
	std::size_t structureBytes = 0;              ///< An estimate of what the two structures take.

	/// @brief The memory it holds: its tiles (shared tiles counted in full) and its structures.
	[[nodiscard]] std::size_t memoryBytes() const noexcept;
};

/// @brief Records the tiles a step changes: the first time each is touched, its tile before; when done, after.
///
/// Call touch() before writing a tile; tiles shared with the image stay intact, as writing copies them first.
class TileRecorder {
public:
	/// @brief Notes a tile before it is first written in this step; later calls for it do nothing.
	/// @param id The image's id.
	/// @param image The image.
	/// @param column The tile's column.
	/// @param row The tile's row.
	void touch(std::uint64_t id, const Image& image, int column, int row);
	/// @brief Whether a tile was touched.
	/// @param id The image's id.
	/// @param column The tile's column.
	/// @param row The tile's row.
	[[nodiscard]] bool touched(std::uint64_t id, int column, int row) const noexcept;
	/// @brief The changes, with each tile after as the images hold it now; tiles left as they were are dropped.
	/// @param imageOf Finds an image by id (null if it is gone).
	/// @return The changes.
	[[nodiscard]] std::vector<TileChange> finish(const std::function<const Image*(std::uint64_t)>& imageOf);
	/// @brief Whether nothing was touched.
	[[nodiscard]] bool empty() const noexcept { return changes_.empty(); }

private:
	std::vector<TileChange> changes_;                      ///< The tiles touched, in order, with their tile before.
	std::unordered_map<std::uint64_t, std::size_t> index_; ///< (image, column, row) packed, to its change.
};

/// @brief The undo history: a list of steps and the place in it.
///
/// Undo gives the step to revert and moves back; redo gives the step to apply again and moves forward; a new step
/// drops the steps undone. Beyond the memory budget or the step count, the oldest steps are dropped.
class History {
public:
	/// @brief An empty history.
	/// @param budgetBytes The memory its steps may hold.
	/// @param maximumSteps The most steps it keeps.
	explicit History(std::size_t budgetBytes = std::size_t{512} << 20, std::size_t maximumSteps = 200) noexcept
	    : budget_(budgetBytes), maximumSteps_(maximumSteps) {}

	/// @brief Adds a step after the current place, dropping those undone and the oldest beyond the budget.
	/// @param step The step.
	void push(HistoryStep step);
	/// @brief The step to revert, moving back, or null at the start.
	[[nodiscard]] const HistoryStep* undo() noexcept;
	/// @brief The step to apply again, moving forward, or null at the end.
	[[nodiscard]] const HistoryStep* redo() noexcept;
	/// @brief Whether there is a step to undo.
	[[nodiscard]] bool canUndo() const noexcept { return position_ > 0; }
	/// @brief Whether there is a step to redo.
	[[nodiscard]] bool canRedo() const noexcept { return position_ < steps_.size(); }
	/// @brief The steps, oldest first.
	[[nodiscard]] const std::vector<HistoryStep>& steps() const noexcept { return steps_; }
	/// @brief How many steps are applied: those before this place.
	[[nodiscard]] std::size_t position() const noexcept { return position_; }
	/// @brief How many old steps were dropped since the start.
	[[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }
	/// @brief The memory its steps hold.
	[[nodiscard]] std::size_t memoryBytes() const noexcept { return bytes_; }
	/// @brief Replaces the structure after the newest step, when it is the step undone next and changes no tiles (a
	/// slider dragged: one step, not one per move).
	/// @param name The step's name, which must match.
	/// @param structureAfter The new structure after.
	/// @return Whether it was replaced.
	bool amendLast(std::string_view name, std::shared_ptr<const void> structureAfter) noexcept;
	/// @brief Forgets every step.
	void clear() noexcept;

private:
	std::vector<HistoryStep> steps_; ///< See steps().
	std::size_t position_ = 0;       ///< See position().
	std::size_t bytes_ = 0;          ///< See memoryBytes().
	std::size_t dropped_ = 0;        ///< See dropped().
	std::size_t budget_;             ///< The memory budget.
	std::size_t maximumSteps_;       ///< The most steps.
};

/// @brief Puts each changed tile of a step back as it was before (undo) or after (redo), last change first on undo.
/// @param step The step.
/// @param after true to redo, false to undo.
/// @param imageOf Finds an image by id (null if it is gone: its changes are skipped).
void applyTiles(const HistoryStep& step, bool after, const std::function<Image*(std::uint64_t)>& imageOf);

} // namespace affresco::imaging
