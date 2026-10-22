/// @file
/// @brief The `terminal` widget (TerminalView.hpp): its properties, its keys and the wheel, and how it draws.

#include "features/terminal/TerminalView.hpp"

#include <affresco/render/RichText.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace forge::terminal {

using affresco::Color;
using affresco::json;
using affresco::Key;

namespace {

/// @brief Whether a key stays a shortcut: with Super, or with Control and Shift together.
/// @param mods The modifiers.
/// @return Whether it does.
bool isShortcut(affresco::Mods mods) noexcept { return mods.super() || (mods.control() && mods.shift()); }

/// @brief Whether two cells are drawn in one run: the same colours and underline, both one column wide.
/// @param first The run's first cell.
/// @param next The next cell.
/// @return Whether they are.
bool sameRun(const TerminalCell& first, const TerminalCell& next) noexcept {
	return first.width == 1 && next.width == 1 && first.defaultForeground == next.defaultForeground &&
	       first.foreground == next.foreground && first.defaultBackground == next.defaultBackground &&
	       first.background == next.background && first.reverse == next.reverse && first.underline == next.underline;
}

} // namespace

/// @brief Applies the node's properties.
/// @param screen The screen, for the font and the colours.
/// @param properties `session`, `font`, `textColor`, `cursorColor`, `scrollback`, `allowTitle`.
void TerminalView::apply(const affresco::Screen& screen, const json& properties) {
	id_ = properties.value("session", std::string());
	const std::string fontName = properties.value("font", std::string());
	font_ = fontName.empty() ? nullptr : screen.font(fontName);
	textColor_ = screen.resolveColor(properties.value("textColor", std::string("#cccccc")));
	cursorColor_ = screen.resolveColor(properties.value("cursorColor", std::string("#aeafad")));
	scrollback_ = std::min(mostScrollbackLines,
	                       static_cast<std::size_t>(std::max(
	                           0.0, properties.value("scrollback", static_cast<double>(defaultScrollbackLines)))));
	allowTitle_ = properties.value("allowTitle", false);
	if(TerminalSession* shown = session()) configure(*shown->model);
	invalidate();
}

/// @brief The font it draws with.
/// @return The `font` property's, else the window's; nullptr before it has a window.
const affresco::Font* TerminalView::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

/// @brief One cell's size, in window units.
/// @return Its width and height; 0 without a font.
affresco::Vec2 TerminalView::cellSize() const noexcept {
	const affresco::Font* drawn = font();
	if(!drawn) return {0.0f, 0.0f};
	return {std::max(1.0f, drawn->measure(U"M")), std::max(1.0f, drawn->lineHeight())};
}

/// @brief The session it shows.
/// @return It, or nullptr (none attached, or closed).
TerminalSession* TerminalView::session() const noexcept {
	return attached_.empty() ? nullptr : sessions_->find(attached_);
}

/// @brief Gives a session's screen this view's settings.
/// @param model The screen.
void TerminalView::configure(TerminalModel& model) const {
	model.setScrollbackLimit(scrollback_);
	model.setAllowTitle(allowTitle_);
}

/// @brief Sizes the session to the view: attaches at the first layout, resizes the pty when the size changes.
void TerminalView::arrange() {
	const affresco::Vec2 cell = cellSize();
	const affresco::Rect& frame = bounds();
	if(cell.x <= 0.0f || frame.width() < cell.x || frame.height() < cell.y) return; // hidden: keep the size it had
	const int columns = std::clamp(static_cast<int>(frame.width() / cell.x), 1, mostTerminalCells);
	const int rows = std::clamp(static_cast<int>(frame.height() / cell.y), 1, mostTerminalCells);
	if(id_.empty()) return;
	if(id_ != attached_) {
		sessions_->detach(attached_, *this);
		attached_ = id_;
		scrolledBack_ = 0;
		if(TerminalSession* made = sessions_->attach(id_, *this, rows, columns)) configure(*made->model);
		return;
	}
	if(TerminalSession* shown = session()) sessions_->resize(*shown, rows, columns);
}

/// @brief Sends what a session's screen has for the program, and shows the bottom.
/// @param session The session.
void TerminalView::typed(TerminalSession& session) {
	sessions_->send(session, session.model->takeOutput());
	scrolledBack_ = 0;
	invalidatePaint();
}

/// @brief Types a key: Control and a letter as its control code, Enter, Tab, the arrows and the other keys
/// TerminalModel::pressKey() knows as their sequences, Shift+Page Up and Down as scrolling back. Other keys, and
/// shortcuts, go to the document's keys.
/// @param event The event.
void TerminalView::keyEvent(const affresco::KeyEvent& event) {
	TerminalSession* shown = session();
	if(!shown || event.action == affresco::Action::Release || isShortcut(event.mods)) return Panel::keyEvent(event);
	const bool page = event.key == Key::PageUp || event.key == Key::PageDown;
	if(page && event.mods.shift() && !shown->model->alternateScreen()) {
		const long rows = shown->model->rows();
		return scrollLines(event.key == Key::PageUp ? rows : -rows);
	}
	const int code = static_cast<int>(event.key);
	if(event.mods.control() && code >= static_cast<int>(Key::A) && code <= static_cast<int>(Key::Z)) {
		shown->model->typeCharacter(static_cast<char32_t>(int{U'a'} + code - static_cast<int>(Key::A)), event.mods);
		return typed(*shown);
	}
	if(shown->model->pressKey(event.key, event.mods)) return typed(*shown);
	Panel::keyEvent(event);
}

/// @brief Types a character.
/// @param event The event.
void TerminalView::charEvent(const affresco::CharEvent& event) {
	TerminalSession* shown = session();
	if(!shown) return Panel::charEvent(event);
	shown->model->typeCharacter(event.codepoint);
	typed(*shown);
}

/// @brief Scrolls back by whole lines.
/// @param lines Lines towards the top; negative towards the bottom.
void TerminalView::scrollLines(long lines) {
	const TerminalSession* shown = session();
	if(!shown) return;
	const auto most = static_cast<long>(shown->model->scrollbackSize());
	scrolledBack_ = static_cast<std::size_t>(std::clamp(static_cast<long>(scrolledBack_) + lines, 0L, most));
	invalidatePaint();
}

/// @brief Scrolls the scrollback with the wheel (not the alternate screen, which has none).
/// @param delta Pixels; positive scrolls towards the end.
/// @return Whether it scrolled.
bool TerminalView::scrollContent(affresco::Vec2 delta) {
	const TerminalSession* shown = session();
	if(!shown || shown->model->alternateScreen() || shown->model->scrollbackSize() == 0) return false;
	wheel_ -= delta.y / cellSize().y;
	const float whole = std::trunc(wheel_);
	wheel_ -= whole;
	scrollLines(static_cast<long>(whole));
	return true;
}

/// @brief Draws the cursor as focused or not.
/// @param focused Whether it has the focus.
void TerminalView::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

/// @brief A cell's text colour.
/// @param cell The cell.
/// @return The colour.
Color TerminalView::foregroundOf(const TerminalCell& cell) const noexcept {
	if(cell.reverse)
		return cell.defaultBackground ? (color().a > 0.0f ? color() : Color::hex(0x1e1e1e))
		                              : Color::hex(cell.background);
	return cell.defaultForeground ? textColor_ : Color::hex(cell.foreground);
}

/// @brief A cell's background, if it isn't the view's.
/// @param cell The cell.
/// @return The colour, or std::nullopt.
std::optional<Color> TerminalView::backgroundOf(const TerminalCell& cell) const noexcept {
	if(cell.reverse) return cell.defaultForeground ? textColor_ : Color::hex(cell.foreground);
	if(cell.defaultBackground) return std::nullopt;
	return Color::hex(cell.background);
}

/// @brief Draws a run of cells: their background, text and underline.
/// @param painter The painter.
/// @param cells The line's cells.
/// @param start The run's first column.
/// @param end Past its last column.
/// @param top The line's top.
void TerminalView::paintRun(affresco::Painter& painter, const std::vector<TerminalCell>& cells, int start, int end,
                            float top) const {
	const affresco::Vec2 cell = cellSize();
	const TerminalCell& first = cells[static_cast<std::size_t>(start)];
	const float left = bounds().left + static_cast<float>(start) * cell.x;
	const float right = left + static_cast<float>(end - start) * cell.x;
	if(const std::optional<Color> background = backgroundOf(first))
		painter.fillRect({left, top, right, top + cell.y}, *background);
	std::u32string text;
	bool blank = true;
	for(int column = start; column < end; ++column) {
		const char32_t character = cells[static_cast<std::size_t>(column)].character;
		blank = blank && (character == 0 || character == U' ');
		text.push_back(character == 0 ? U' ' : character);
	}
	const Color foreground = foregroundOf(first);
	const float baseline = top + font()->ascent();
	if(!blank) painter.drawText(*font(), text, {left, baseline}, foreground);
	if(first.underline) painter.fillRect({left, baseline + 1.0f, right, baseline + 2.0f}, foreground);
}

/// @brief Draws one line of the scrollback or the screen.
/// @param painter The painter.
/// @param model The screen.
/// @param line The line (TerminalModel::cell()).
/// @param top Its top.
void TerminalView::paintRow(affresco::Painter& painter, const TerminalModel& model, std::size_t line, float top) const {
	const int columns = model.columns();
	std::vector<TerminalCell> cells;
	cells.reserve(static_cast<std::size_t>(columns));
	for(int column = 0; column < columns; ++column) cells.push_back(model.cell(line, column));
	for(int start = 0; start < columns;) {
		const TerminalCell& first = cells[static_cast<std::size_t>(start)];
		int end = start + 1;
		if(first.width == 2) end = std::min(columns, start + 2);
		else
			while(end < columns && sameRun(first, cells[static_cast<std::size_t>(end)])) ++end;
		if(first.width != 0) paintRun(painter, cells, start, end, top);
		start = end;
	}
}

/// @brief Draws the cursor: a block while focused, an outline otherwise.
/// @param painter The painter.
/// @param model The screen.
/// @param first The first line shown.
void TerminalView::paintCursor(affresco::Painter& painter, const TerminalModel& model, std::size_t first) const {
	const TerminalCursor cursor = model.cursor();
	const std::size_t line = model.scrollbackSize() + static_cast<std::size_t>(cursor.row);
	if(!cursor.visible || line < first || line >= first + static_cast<std::size_t>(model.rows())) return;
	const affresco::Vec2 cell = cellSize();
	const float left = bounds().left + static_cast<float>(cursor.column) * cell.x;
	const float top = bounds().top + static_cast<float>(line - first) * cell.y;
	const affresco::Rect block{left, top, left + cell.x, top + cell.y};
	if(!hasFocus()) return painter.strokeRoundedRect(block, {}, 1.0f, cursorColor_);
	painter.fillRect(block, cursorColor_);
	const char32_t character = model.cell(line, cursor.column).character;
	if(character != 0)
		painter.drawText(*font(), std::u32string(1, character), {left, top + font()->ascent()},
		                 color().a > 0.0f ? color() : Color::hex(0x1e1e1e));
}

/// @brief Draws the background, the visible lines, the cursor, and the status when no program runs.
/// @param painter Clips to bounds().
void TerminalView::paint(affresco::Painter& painter) const {
	Panel::paint(painter);
	const TerminalSession* shown = session();
	if(!shown || !font()) return;
	const TerminalModel& model = *shown->model;
	const std::size_t rows = static_cast<std::size_t>(model.rows());
	const std::size_t back = std::min(scrolledBack_, model.scrollbackSize());
	const std::size_t first = model.scrollbackSize() - back; // the screen's top is line scrollbackSize()
	const float height = cellSize().y;
	for(std::size_t row = 0; row < rows; ++row)
		paintRow(painter, model, first + row, bounds().top + static_cast<float>(row) * height);
	paintCursor(painter, model, first);
	if(shown->status.empty()) return;
	Color dim = textColor_;
	dim.a *= 0.6f;
	painter.drawText(*font(), affresco::codepointsOf(shown->status),
	                 {bounds().left, bounds().bottom - height + font()->ascent()}, dim);
}

} // namespace forge::terminal
