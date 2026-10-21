/// @file
/// @brief ListView: rows from a model, the selection, the keyboard, type-to-find, and drawing the rows in view.

#include "affresco/render/ListView.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"
#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco {

namespace {

constexpr std::size_t decodeMargin = 16; ///< Rows decoded beyond the view, so a short scroll decodes nothing.
constexpr double doubleClickTime = 0.4;  ///< Seconds between two presses on a row that open it.
constexpr double typeToFindTime = 1.0;   ///< Seconds after which typing starts a new search.
constexpr std::size_t maxTyped = 64;     ///< The most bytes type-to-find keeps.
constexpr float scrollBarWidth = 4.0f;   ///< The scroll bar's thumb, in pixels.
constexpr float minimumThumb = 16.0f;    ///< The shortest thumb, in pixels.

/// @brief Whether text starts with a prefix, ignoring the case of ASCII letters.
/// @param text UTF-8 text.
/// @param prefix UTF-8 text.
[[nodiscard]] bool startsWithFolded(std::string_view text, std::string_view prefix) noexcept {
	if(prefix.size() > text.size()) return false;
	const auto fold = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	for(std::size_t i = 0; i < prefix.size(); ++i)
		if(fold(text[i]) != fold(prefix[i])) return false;
	return true;
}

/// @brief Draws a rectangle's outline.
/// @param painter Where.
/// @param area The rectangle.
/// @param width The line's width.
/// @param color Its colour.
void outline(Painter& painter, const Rect& area, float width, Color color) {
	painter.fillRect({area.left, area.top, area.right, area.top + width}, color);
	painter.fillRect({area.left, area.bottom - width, area.right, area.bottom}, color);
	painter.fillRect({area.left, area.top, area.left + width, area.bottom}, color);
	painter.fillRect({area.right - width, area.top, area.right, area.bottom}, color);
}

} // namespace

ListView::ListView() { setColor(colors::white); }

void ListView::setModel(std::shared_ptr<const ItemModel> model) {
	if(model == model_) return;
	const auto idOf = [this](std::size_t index) {
		return model_ && index < model_->size() ? model_->item(index).id : std::string();
	};
	const std::size_t topRow = rows_.empty() || scroll_ <= 0.0f ? rows_.size() : window_.rowAt(scroll_);
	const std::string topId = topRow < rows_.size() ? idOf(rows_[topRow]) : std::string();
	const float topOffset = topRow < rows_.size() ? scroll_ - window_.offsetOf(topRow) : 0.0f;
	const std::string cursorId = idOf(cursor_), anchorId = idOf(anchor_);
	model_ = std::move(model);
	const std::size_t size = model_ ? model_->size() : 0;
	cursor_ = anchor_ = size;
	std::size_t top = size;
	std::vector<std::string> kept;
	for(std::size_t i = 0; i < size; ++i) {
		const std::string& id = model_->item(i).id;
		if(id == cursorId && !cursorId.empty()) cursor_ = i;
		if(id == anchorId && !anchorId.empty()) anchor_ = i;
		if(id == topId && !topId.empty()) top = i;
		if(!selection_.empty() && std::binary_search(selection_.begin(), selection_.end(), id)) kept.push_back(id);
	}
	std::sort(kept.begin(), kept.end());
	kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
	selection_ = std::move(kept);
	rebuildRowsKeeping(top, topOffset);
}

void ListView::setSelection(std::vector<std::string> ids) {
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	setLook(selection_, std::move(ids));
}

bool ListView::isSelected(std::string_view id) const noexcept {
	const auto at = std::lower_bound(selection_.begin(), selection_.end(), id,
	                                 [](const std::string& a, std::string_view b) { return a < b; });
	return at != selection_.end() && *at == id;
}

void ListView::setRowHeight(float height) {
	height = std::max(height, 1.0f);
	if(height == rowHeight_) return;
	rowHeight_ = height;
	window_.reset(rows_.size(), rowHeight_);
	decoded_ = {};
	invalidate();
}

void ListView::setFont(std::shared_ptr<const Font> font) { setProperty(font_, std::move(font)); }

void ListView::setColors(Color text, Color selectedText, Color selectedBackground, Color accent) {
	setLook(text_, text);
	setLook(selectedText_, selectedText);
	setLook(selectedBackground_, selectedBackground);
	setLook(accent_, accent);
}

const Font* ListView::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void ListView::buildRows(std::vector<std::size_t>& rows) {
	const std::size_t size = model_ ? model_->size() : 0;
	rows.resize(size);
	for(std::size_t i = 0; i < size; ++i) rows[i] = i;
}

void ListView::rebuildRows(bool anchored) {
	// Anchoring starts once scrolled: a list at its top stays there as rows are added above
	const std::size_t row = !anchored || rows_.empty() || scroll_ <= 0.0f ? rows_.size() : window_.rowAt(scroll_);
	if(row >= rows_.size()) rebuildRowsKeeping(model_ ? model_->size() : 0, 0.0f);
	else rebuildRowsKeeping(rows_[row], scroll_ - window_.offsetOf(row));
}

void ListView::rebuildRowsKeeping(std::size_t item, float offset) {
	rows_.clear();
	if(model_) buildRows(rows_);
	window_.reset(rows_.size(), rowHeight_);
	decoded_ = {};
	const std::size_t row = model_ && item < model_->size() ? rowOf(item) : rows_.size();
	if(row < rows_.size()) scroll_ = window_.offsetOf(row) + offset; // arrange() clamps it
	invalidate();
}

std::size_t ListView::rowOf(std::size_t item) const noexcept {
	// Lists and trees show their items in the model's order
	const auto at = std::lower_bound(rows_.begin(), rows_.end(), item);
	return at != rows_.end() && *at == item ? static_cast<std::size_t>(at - rows_.begin()) : rows_.size();
}

float ListView::viewport() const noexcept { return std::max(resolvedSize().y - headerHeight(), 0.0f); }

void ListView::clampScroll() noexcept {
	scroll_ = std::clamp(scroll_, 0.0f, std::max(window_.total() - viewport(), 0.0f));
}

void ListView::arrange() {
	clampScroll();
	decodeShown();
}

void ListView::decodeShown() {
	const RowRange range = window_.visible(scroll_, viewport(), decodeMargin);
	if(range == decoded_ || !model_) {
		viewChanged();
		return;
	}
	// Decode a little more than the view, so a short scroll finds its rows ready
	const std::size_t cells = shownCells();
	shown_.resize((range.last - range.first) * cells);
	for(std::size_t row = range.first; row < range.last; ++row) {
		const ListItem& item = model_->item(rows_[row]);
		for(std::size_t cell = 0; cell < cells; ++cell)
			detail::decodeInto(shown_[(row - range.first) * cells + cell],
			                   cell < item.cells.size() ? std::string_view(item.cells[cell]) : std::string_view());
	}
	decoded_ = range;
	viewChanged();
}

std::u32string_view ListView::shownText(std::size_t row, std::size_t cell) const noexcept {
	if(row < decoded_.first || row >= decoded_.last || cell >= shownCells()) return {};
	return shown_[(row - decoded_.first) * shownCells() + cell];
}

std::size_t ListView::rowAt(Vec2 position) const noexcept {
	if(!frame().contains(position)) return rows_.size();
	const float y = position.y - frame().top - headerHeight();
	if(y < 0.0f || y + scroll_ >= window_.total()) return rows_.size();
	return window_.rowAt(y + scroll_);
}

void ListView::scrollToRow(std::size_t row) {
	if(row >= rows_.size()) return;
	const float top = window_.offsetOf(row), bottom = top + window_.heightOf(row);
	if(bottom - scroll_ > viewport()) scroll_ = bottom - viewport();
	if(top < scroll_) scroll_ = top;
	clampScroll();
	decodeShown();
	invalidatePaint();
}

DropTarget ListView::dropTargetAt(Vec2 position) const noexcept {
	const std::size_t row = rowAt(position);
	if(row >= rows_.size()) {
		const bool above = position.y < frame().top + headerHeight();
		return {above ? 0 : rows_.size(), DropPlace::Before};
	}
	const float top = frame().top + headerHeight() + window_.offsetOf(row) - scroll_;
	const float along = (position.y - top) / window_.heightOf(row);
	if(dropsInside()) {
		if(along < 0.25f) return {row, DropPlace::Before};
		return {row, along > 0.75f ? DropPlace::After : DropPlace::Inside};
	}
	return {row, along < 0.5f ? DropPlace::Before : DropPlace::After};
}

void ListView::showDropIndicator(std::optional<DropTarget> target) { setLook(dropIndicator_, target); }

void ListView::changeSelection(std::vector<std::string> ids) {
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	if(ids == selection_) return;
	selection_ = std::move(ids);
	invalidatePaint();
	const std::vector<std::string> reported = selection_; // a handler may set the selection again
	onSelectionChanged(reported);
}

void ListView::selectRange(std::size_t row) {
	std::size_t from = rowOf(anchor_);
	if(from >= rows_.size()) from = row;
	const std::size_t first = std::min(from, row), last = std::max(from, row);
	std::vector<std::string> ids;
	ids.reserve(last - first + 1);
	for(std::size_t r = first; r <= last; ++r) ids.push_back(model_->item(rows_[r]).id);
	changeSelection(std::move(ids));
}

void ListView::moveCursor(std::size_t row, Mods mods) {
	if(row >= rows_.size()) return;
	const std::size_t item = rows_[row];
	const std::string& id = model_->item(item).id;
	cursor_ = item;
	if(selectionMode_ == SelectionMode::Multiple && mods.shift()) {
		selectRange(row);
	} else if(selectionMode_ == SelectionMode::Multiple && (mods.super() || mods.control())) {
		std::vector<std::string> ids = selection_;
		if(isSelected(id)) std::erase(ids, id);
		else ids.push_back(id);
		anchor_ = item;
		changeSelection(std::move(ids));
	} else {
		anchor_ = item;
		if(selectionMode_ != SelectionMode::None) changeSelection({id});
	}
	scrollToRow(row);
}

void ListView::mouseButtonEvent(const MouseButtonEvent& event) {
	const std::size_t row = rowAt(event.position);
	if(event.button == MouseButton::Left && event.action == Action::Press && row < rows_.size() &&
	   !rowPress(row, event.position)) {
		const double now = window() ? window()->clock().seconds() : 0.0;
		if(row == lastPressRow_ && lastPress_ >= 0.0 && now - lastPress_ < doubleClickTime) {
			lastPress_ = -1.0;
			const std::string id = model_->item(rows_[row]).id; // the handler may change the items
			onOpened(id);
		} else {
			moveCursor(row, event.mods);
			lastPress_ = now;
			lastPressRow_ = row;
		}
	}
	Panel::mouseButtonEvent(event);
}

void ListView::keyEvent(const KeyEvent& event) {
	const std::size_t count = rows_.size();
	if(count > 0 && event.action != Action::Release && !rowKey(event)) {
		const std::size_t row = rowOf(cursor_);
		const bool none = row >= count;
		// At most the row count before the cast: a list 1e30 tall is past size_t's range, where the cast is undefined
		const float rows = std::min(std::max(1.0f, viewport() / rowHeight_), static_cast<float>(count));
		const auto page = static_cast<std::size_t>(rows);
		const Mods shift{event.mods.bits & 0x0001};
		if(event.key == Key::Up) moveCursor(none || row == 0 ? 0 : row - 1, shift);
		else if(event.key == Key::Down) moveCursor(none ? 0 : std::min(row + 1, count - 1), shift);
		else if(event.key == Key::PageUp) moveCursor(none || row < page ? 0 : row - page, shift);
		else if(event.key == Key::PageDown) moveCursor(none ? 0 : std::min(row + page, count - 1), shift);
		else if(event.key == Key::Home) moveCursor(0, shift);
		else if(event.key == Key::End) moveCursor(count - 1, shift);
		else if((event.key == Key::Enter || event.key == Key::KeypadEnter) && !none) {
			const std::string id = model_->item(cursor_).id;
			onOpened(id);
		}
	}
	Panel::keyEvent(event);
}

void ListView::charEvent(const CharEvent& event) {
	const double now = window() ? window()->clock().seconds() : 0.0;
	if(now - typedAt_ > typeToFindTime) typed_.clear();
	const bool continuing = !typed_.empty();
	if(typed_.size() < maxTyped) detail::appendUtf8(typed_, event.codepoint);
	typedAt_ = now;
	if(!rows_.empty()) {
		std::size_t from = rowOf(cursor_);
		if(from >= rows_.size()) from = 0;
		else if(!continuing) from = (from + 1) % rows_.size(); // a new search starts after the cursor
		const std::size_t found = findTyped(from);
		if(found < rows_.size()) moveCursor(found, {});
	}
	Panel::charEvent(event);
}

void ListView::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

std::size_t ListView::findTyped(std::size_t from) const noexcept {
	const std::size_t count = rows_.size();
	for(std::size_t k = 0; k < count; ++k) {
		const std::size_t row = (from + k) % count;
		const ListItem& item = model_->item(rows_[row]);
		if(!item.cells.empty() && startsWithFolded(item.cells.front(), typed_)) return row;
	}
	return count;
}

bool ListView::scrollContent(Vec2 delta) {
	const float before = scroll_;
	scroll_ += delta.y;
	clampScroll();
	if(scroll_ == before) return false;
	decodeShown();
	invalidatePaint();
	return true;
}

bool ListView::rowKey(const KeyEvent& event) {
	static_cast<void>(event);
	return false;
}

bool ListView::rowPress(std::size_t row, Vec2 position) {
	static_cast<void>(row);
	static_cast<void>(position);
	return false;
}

void ListView::paint(Painter& painter) const {
	Panel::paint(painter);
	const Font* text = font();
	if(!text || !model_) return;
	const Rect& f = frame();
	const float top = f.top + headerHeight();
	const bool focused = window() && window()->focusedPanel() == this;
	const RowRange range = window_.visible(scroll_, viewport(), 0);
	for(std::size_t row = range.first; row < range.last; ++row) {
		const float y = std::round(top + window_.offsetOf(row) - scroll_);
		const Rect area{f.left, y, f.right, y + std::round(window_.heightOf(row))};
		const ListItem& item = model_->item(rows_[row]);
		const bool selected = isSelected(item.id);
		if(selected) painter.fillRect(area, selectedBackground_);
		paintRow(painter, *text, row, area, selected ? selectedText_ : text_);
		if(focused && rows_[row] == cursor_) outline(painter, area, 1.0f, accent_);
	}
	paintOverlay(painter, *text);
	paintOverRows(painter);
}

void ListView::paintOverlay(Painter& painter, const Font& font) const {
	static_cast<void>(painter);
	static_cast<void>(font);
}

void ListView::paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const {
	const float baseline = std::round(area.top + (area.height() - font.lineHeight()) / 2.0f + font.ascent());
	const Vec2 origin{area.left + padding(), baseline};
	const float shift = paintIcon(painter, font, model_->item(itemAt(row)), origin, color);
	painter.drawText(font, shownText(row, 0), {origin.x + shift, baseline}, color);
}

float ListView::paintIcon(Painter& painter, const Font& font, const ListItem& item, Vec2 origin, Color color) const {
	if(item.icon.empty()) return 0.0f;
	const char32_t glyph = font.codepointNamed(item.icon);
	if(glyph == 0) return 0.0f;
	const std::u32string_view shown(&glyph, 1);
	painter.drawText(font, shown, origin, item.iconColor.value_or(color));
	return std::round(font.measure(shown) + font.measure(U" "));
}

void ListView::paintOverRows(Painter& painter) const {
	const Rect& f = frame();
	const float top = f.top + headerHeight(), total = window_.total(), view = viewport();
	if(dropIndicator_) {
		const std::size_t row = dropIndicator_->row;
		const float y = std::round(top + window_.offsetOf(row) - scroll_);
		if(row < rows_.size() && dropIndicator_->place == DropPlace::Inside)
			outline(painter, {f.left, y, f.right, y + std::round(window_.heightOf(row))}, 2.0f, accent_);
		else {
			const float at =
			    row < rows_.size() && dropIndicator_->place == DropPlace::After ? y + window_.heightOf(row) : y;
			painter.fillRect({f.left, std::round(at) - 1.0f, f.right, std::round(at) + 1.0f}, accent_);
		}
	}
	if(total > view && view > 0.0f) {
		const float thumb = std::max(view * view / total, minimumThumb);
		const float y = std::round(top + (view - thumb) * scroll_ / (total - view));
		painter.fillRoundedRect({f.right - scrollBarWidth - 2.0f, y, f.right - 2.0f, y + std::round(thumb)},
		                        CornerRadii::all(scrollBarWidth / 2.0f), colors::muted.withAlpha(0.6f));
	}
}

} // namespace affresco
