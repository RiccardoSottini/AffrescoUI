#pragma once

/// @file
/// @brief TreeView: a virtualised tree: a ListView whose rows are the expanded part of a pre-order list of items.

#include "affresco/render/ListView.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief A folder the user opened or closed.
struct ExpandChange {
	std::string id;        ///< The item's id.
	bool expanded = false; ///< Whether it is open now.
};

/// @brief A tree of items of which only the rows in view are drawn. The items come in pre-order with their depth and
/// the end of their subtree (ListItem), so expanding or collapsing rebuilds the rows in one pass over the items,
/// skipping closed subtrees whole. Which folders are open is given (setExpanded()) and the tree reports what the user
/// does (onExpandedChanged): Right opens a folder or moves to its first child, Left closes it or moves to its parent,
/// and a click on the chevron toggles it. Drops can land between rows or inside a folder.
///
/// With sticky rows (setStickyRows()), the open folders the top row is in stay pinned at the top as their children
/// scroll under them, outermost first; a click on one scrolls back to it.
class TreeView : public ListView {
public:
	/// @brief Sets which items are open; the others are closed. Call it again after setModel(): the flags are per item.
	/// @param ids Their ids.
	void setExpanded(const std::vector<std::string>& ids);
	/// @brief Whether an item is open.
	/// @param item Its index in the model.
	[[nodiscard]] bool isExpanded(std::size_t item) const noexcept {
		return item < expanded_.size() && expanded_[item] != 0;
	}
	/// @brief Opens or closes an item, as the user would, firing onExpandedChanged.
	/// @param item Its index in the model.
	/// @param expanded Whether to open it.
	void expand(std::size_t item, bool expanded);

	/// @brief Sets how many folders the top row is in stay pinned at the top as it scrolls.
	/// @param count At most this many, outermost first; 0 (the default) pins none.
	void setStickyRows(std::size_t count);
	/// @brief How many folders stay pinned at the top at most.
	[[nodiscard]] std::size_t stickyRows() const noexcept { return stickyRows_; }
	/// @brief The items pinned at the top now, outermost first.
	[[nodiscard]] const std::vector<std::size_t>& stuck() const noexcept { return stuck_; }

	Signal<const ExpandChange&> onExpandedChanged; ///< The user opened or closed an item.

protected:
	void buildRows(std::vector<std::size_t>& rows) override;
	void paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const override;
	bool rowKey(const KeyEvent& event) override;
	bool rowPress(std::size_t row, Vec2 position) override;
	[[nodiscard]] bool dropsInside() const noexcept override { return true; }
	void paintOverlay(Painter& painter, const Font& font) const override;
	/// @brief Finds the folders to pin at the top and decodes their labels.
	void viewChanged() override;

private:
	/// @brief How far one level indents, in pixels.
	[[nodiscard]] static constexpr float indent() noexcept { return 16.0f; }
	/// @brief Where an item's chevron starts.
	/// @param item Its index in the model.
	/// @param left The row's left edge.
	[[nodiscard]] float chevronLeft(std::size_t item, float left) const noexcept;
	/// @brief The parent of an item.
	/// @param item Its index in the model.
	/// @return Its index, or the model's size for a root.
	[[nodiscard]] std::size_t parentOf(std::size_t item) const noexcept;
	/// @brief Draws an item's chevron, pointing right when closed and down when open.
	/// @param painter Where.
	/// @param left Its left edge.
	/// @param middle The row's vertical middle.
	/// @param open Whether the item is open.
	/// @param color Its colour.
	static void paintChevron(Painter& painter, float left, float middle, bool open, Color color);
	/// @brief Draws an item's guides, chevron and label.
	/// @param painter Where.
	/// @param font The font.
	/// @param item Its index in the model.
	/// @param label Its label, decoded.
	/// @param area Its row's rectangle.
	/// @param color Its text colour.
	void paintItem(Painter& painter, const Font& font, std::size_t item, std::u32string_view label, const Rect& area,
	               Color color) const;

	std::vector<char> expanded_;             ///< Per item, whether it is open.
	std::vector<std::string> lastGiven_;     ///< The ids setExpanded() was last given, to skip repeats.
	const ItemModel* expandedFor_ = nullptr; ///< The model those ids were matched against.
	std::size_t stickyRows_ = 0;             ///< See setStickyRows().
	std::vector<std::size_t> stuck_;         ///< See stuck().
	std::vector<std::u32string> stuckText_;  ///< Their labels, decoded; the storage is reused.
};

} // namespace affresco
