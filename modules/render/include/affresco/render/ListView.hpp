#pragma once

/// @file
/// @brief ListView: a virtualised list of rows with selection, keyboard navigation and type-to-find, the base of the
/// tree and the table.

#include "affresco/render/Panel.hpp"
#include "affresco/render/RowWindow.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

class Font;

/// @brief One item of a list, a tree or a table.
struct ListItem {
	std::string id;                 ///< What names it in the selection and in signals; unique within the list.
	std::vector<std::string> cells; ///< Its text, UTF-8: the label first; a table has one per column.
	std::uint32_t depth = 0;        ///< Its level in a tree: 0 for a root.
	std::size_t end = 0;            ///< One past its last descendant, in the model's order (its own index + 1 for none).
	bool expandable = false;        ///< Has children, or may get them: a tree shows a chevron.

	/// @brief Equal when every field is.
	bool operator==(const ListItem&) const = default;
};

/// @brief Where the items of a list come from: the seam a data model plugs into. A tree's items are in pre-order, each
/// with its depth and the end of its subtree.
class ListModel {
public:
	/// @brief Destroys the model.
	virtual ~ListModel() = default;
	/// @brief How many items.
	[[nodiscard]] virtual std::size_t size() const noexcept = 0;
	/// @brief One item.
	/// @param index Less than size().
	[[nodiscard]] virtual const ListItem& item(std::size_t index) const noexcept = 0;
};

/// @brief A model that holds its items.
class ItemList : public ListModel {
public:
	/// @brief A model of these items.
	/// @param items In order; a tree's in pre-order.
	explicit ItemList(std::vector<ListItem> items) : items_(std::move(items)) {}
	[[nodiscard]] std::size_t size() const noexcept override { return items_.size(); }
	[[nodiscard]] const ListItem& item(std::size_t index) const noexcept override { return items_[index]; }

private:
	std::vector<ListItem> items_; ///< The items.
};

/// @brief How many items the user can select.
enum class SelectionMode : std::uint8_t {
	None,     ///< None; the cursor still moves and Enter opens.
	Single,   ///< One at a time.
	Multiple, ///< Any: Cmd or Ctrl adds or removes one, Shift selects a range.
};

/// @brief Where something dragged over a list would land, relative to a row.
enum class DropPlace : std::uint8_t {
	Before, ///< Above the row.
	Inside, ///< Into it (a tree's item).
	After,  ///< Below the row.
};

/// @brief A place something dragged over a list would land.
struct DropTarget {
	std::size_t row = 0;              ///< The row, or the number of rows past the last.
	DropPlace place = DropPlace::After; ///< Where, relative to it.

	/// @brief Equal when both fields are.
	bool operator==(const DropTarget&) const = default;
};

/// @brief A list of rows of which only those in view are laid out and drawn, so it holds a million items as easily
/// as ten. The items come from a ListModel; the selection is given (setSelection()) and the list reports what the user
/// does (onSelectionChanged, onOpened), so it always shows its owner's state.
///
/// A click selects; with SelectionMode::Multiple, Cmd or Ctrl toggles a row and Shift selects from the anchor. The
/// keyboard moves the cursor (Up, Down, Page Up, Page Down, Home, End; Shift extends), Enter or a double click opens,
/// and typing jumps to the next row whose label starts with what was typed in the last second.
class ListView : public Panel {
public:
	/// @brief An empty list with the default look.
	ListView();

	/// @brief Sets the items. The selection and the cursor keep the items whose ids are still there.
	/// @param model The items, or nullptr for none.
	void setModel(std::shared_ptr<const ListModel> model);
	/// @brief The items.
	[[nodiscard]] const std::shared_ptr<const ListModel>& model() const noexcept { return model_; }
	/// @brief Sets how many items the user can select; selected items beyond that stay selected until the next change.
	/// @param mode The mode.
	void setSelectionMode(SelectionMode mode) noexcept { selectionMode_ = mode; }
	/// @brief How many items the user can select.
	[[nodiscard]] SelectionMode selectionMode() const noexcept { return selectionMode_; }
	/// @brief Sets the selection, without firing onSelectionChanged.
	/// @param ids The selected items' ids.
	void setSelection(std::vector<std::string> ids);
	/// @brief The selected items' ids, sorted.
	[[nodiscard]] const std::vector<std::string>& selection() const noexcept { return selection_; }
	/// @brief Whether an item is selected.
	/// @param id Its id.
	[[nodiscard]] bool isSelected(std::string_view id) const noexcept;
	/// @brief Sets every row's height.
	/// @param height In pixels; at least 1.
	void setRowHeight(float height);
	/// @brief Sets the font of the rows.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the colours.
	/// @param text The rows' text.
	/// @param selectedText A selected row's text.
	/// @param selectedBackground A selected row's background.
	/// @param accent The cursor's outline, the drop indicator and a tree's guides.
	void setColors(Color text, Color selectedText, Color selectedBackground, Color accent);

	/// @brief How many rows are shown, scrolled into view or not.
	[[nodiscard]] std::size_t rowCount() const noexcept { return rows_.size(); }
	/// @brief The item a row shows.
	/// @param row Less than rowCount().
	[[nodiscard]] std::size_t itemAt(std::size_t row) const noexcept { return rows_[row]; }
	/// @brief The row under a point.
	/// @param position In window units.
	/// @return The row, or rowCount() for none.
	[[nodiscard]] std::size_t rowAt(Vec2 position) const noexcept;
	/// @brief The row that shows an item.
	/// @param item The item's index in the model.
	/// @return The row, or rowCount() when the item isn't shown (e.g. inside a collapsed folder).
	[[nodiscard]] virtual std::size_t rowOf(std::size_t item) const noexcept;
	/// @brief The cursor: the item the keyboard acts on.
	/// @return Its index in the model, or the model's size for none.
	[[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
	/// @brief How far the rows are scrolled, in pixels.
	[[nodiscard]] float scrollPosition() const noexcept { return scroll_; }
	/// @brief Scrolls so a row is fully in view.
	/// @param row The row.
	void scrollToRow(std::size_t row);
	/// @brief Where something dropped at a point would land.
	/// @param position In window units.
	[[nodiscard]] DropTarget dropTargetAt(Vec2 position) const noexcept;
	/// @brief Shows where something dragged over the list would land.
	/// @param target A target from dropTargetAt(), or std::nullopt to hide it.
	void showDropIndicator(std::optional<DropTarget> target);

	Signal<const std::vector<std::string>&> onSelectionChanged; ///< The user changed the selection (the ids, sorted).
	Signal<const std::string&> onOpened; ///< The user pressed Enter on an item or double-clicked it (its id).

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	/// @brief Repaints: the cursor's outline shows only while the list has the focus.
	/// @param focused Whether it gained the focus.
	void focusEvent(bool focused) override;
	bool scrollContent(Vec2 delta) override;
	void paint(Painter& painter) const override;

	/// @brief Fills the rows from the model: every item, in order. A tree skips collapsed subtrees, a table sorts.
	/// @param rows Empty; gets the items' indices.
	virtual void buildRows(std::vector<std::size_t>& rows) const;
	/// @brief Draws a row's content over its background.
	/// @param painter Where.
	/// @param font The font.
	/// @param row The row.
	/// @param area Its rectangle.
	/// @param color Its text colour.
	virtual void paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const;
	/// @brief Handles a key before the list does (a tree's Left and Right).
	/// @param event The key.
	/// @return Whether it was handled.
	virtual bool rowKey(const KeyEvent& event);
	/// @brief Handles a press on a row before it selects (a tree's chevron).
	/// @param row The row.
	/// @param position In window units.
	/// @return Whether it was handled.
	virtual bool rowPress(std::size_t row, Vec2 position);
	/// @brief Whether a drop can land inside a row, not only between rows.
	[[nodiscard]] virtual bool dropsInside() const noexcept { return false; }
	/// @brief The height above the rows a subclass draws in (a table's header).
	[[nodiscard]] virtual float headerHeight() const noexcept { return 0.0f; }
	/// @brief How many cells of each item the rows draw: 1, or a table's columns.
	[[nodiscard]] virtual std::size_t shownCells() const noexcept { return 1; }

	/// @brief Rebuilds the rows (buildRows()), keeping the cursor, and lays out again.
	void rebuildRows();
	/// @brief The font the rows use: setFont()'s, or the window's default.
	[[nodiscard]] const Font* font() const noexcept;
	/// @brief A visible row's cell, decoded for drawing.
	/// @param row The row, within the rows drawn.
	/// @param cell The cell.
	/// @return Its text, or empty for a row not decoded yet.
	[[nodiscard]] std::u32string_view shownText(std::size_t row, std::size_t cell) const noexcept;
	/// @brief Moves the cursor to a row and selects as a click would.
	/// @param row The row.
	/// @param mods The modifier keys held.
	void moveCursor(std::size_t row, Mods mods);
	/// @brief The left padding of a row's text.
	[[nodiscard]] static constexpr float padding() noexcept { return 8.0f; }
	/// @brief The colour of a tree's guides and a table's lines.
	[[nodiscard]] Color accent() const noexcept { return accent_; }
	/// @brief The text colour.
	[[nodiscard]] Color textColor() const noexcept { return text_; }

	std::vector<std::size_t> rows_; ///< The items shown, in order: see itemAt().

private:
	/// @brief Decodes the text of the rows in view, plus a margin, when they changed.
	void decodeShown();
	/// @brief Clamps the scroll position to the rows.
	void clampScroll() noexcept;
	/// @brief Replaces the selection and fires onSelectionChanged when it changed.
	/// @param ids The new selection, any order.
	void changeSelection(std::vector<std::string> ids);
	/// @brief Selects the rows from the anchor to a row.
	/// @param row The row.
	void selectRange(std::size_t row);
	/// @brief Finds the next row whose label starts with the typed text.
	/// @param from The first row to look at.
	/// @return The row, or rowCount().
	[[nodiscard]] std::size_t findTyped(std::size_t from) const noexcept;
	/// @brief The height of the viewport the rows scroll in.
	[[nodiscard]] float viewport() const noexcept;
	/// @brief Draws the drop indicator and the scroll bar.
	/// @param painter Where.
	void paintOverRows(Painter& painter) const;

	std::shared_ptr<const ListModel> model_;          ///< See model().
	RowWindow window_;                                ///< Where each row sits.
	SelectionMode selectionMode_ = SelectionMode::Single; ///< See setSelectionMode().
	std::vector<std::string> selection_;              ///< See selection().
	std::size_t cursor_ = 0;                          ///< See cursor().
	std::size_t anchor_ = 0;                          ///< Where a Shift range starts: an item index.
	float rowHeight_ = 24.0f;                         ///< See setRowHeight().
	std::shared_ptr<const Font> font_;                ///< See setFont().
	Color text_ = colors::black;                      ///< See setColors().
	Color selectedText_ = colors::white;              ///< See setColors().
	Color selectedBackground_ = colors::accent;       ///< See setColors().
	Color accent_ = colors::accent;                   ///< See setColors().
	float scroll_ = 0.0f;                             ///< See scrollPosition().
	RowRange decoded_;                                ///< The rows whose text shown_ holds.
	std::vector<std::u32string> shown_;               ///< Their cells, shownCells() per row.
	std::optional<DropTarget> dropIndicator_;         ///< See showDropIndicator().
	std::string typed_;                               ///< What type-to-find has, UTF-8.
	double typedAt_ = -1.0;                           ///< When the last character was typed, on the window's clock.
	std::size_t lastPressRow_ = 0;                    ///< The row of the last press, for double clicks.
	double lastPress_ = -1.0;                         ///< When it was, on the window's clock.
};

} // namespace affresco
