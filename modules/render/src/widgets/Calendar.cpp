/// @file
/// @brief Calendar: the month view's state, layout, keyboard, mouse and drawing.

#include "affresco/render/Calendar.hpp"

#include "CreativePixels.hpp"

#include "affresco/render/RichText.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco {

namespace {

constexpr int daysShown = Calendar::weeksShown * 7; ///< The days drawn: six weeks.

/// @brief The English months, the names until setNames() gives others.
constexpr std::array<const char*, 12> englishMonths = {"January", "February", "March",     "April",   "May",      "June",
                                                       "July",    "August",   "September", "October", "November", "December"};

/// @brief The English days of the week, Sunday first.
constexpr std::array<const char*, 7> englishDays = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

/// @brief Writes a number with ASCII digits.
/// @param number The number.
/// @return The text.
std::u32string numberText(int number) {
	const std::string digits = std::to_string(number);
	return {digits.begin(), digits.end()};
}

/// @brief Draws a text centred in a rectangle.
/// @param painter Where to draw.
/// @param font The font.
/// @param text The text.
/// @param rect The rectangle.
/// @param color The text's colour.
void centred(Painter& painter, const Font& font, std::u32string_view text, const Rect& rect, Color color) {
	const float width = font.measure(text);
	const float baseline = detail::centredBaseline(rect.top, rect.height(), font.lineHeight(), font.ascent());
	painter.drawText(font, text, {std::round(rect.left + (rect.width() - width) / 2.0f), baseline}, color);
}

} // namespace

Calendar::Calendar() {
	Panel::setColor(colors::transparent);
	setCursor(Cursor::Hand);
	for(std::size_t index = 0; index < months_.size(); ++index) months_[index] = codepointsOf(englishMonths[index]);
	for(std::size_t index = 0; index < days_.size(); ++index) days_[index] = codepointsOf(englishDays[index]);
}

void Calendar::setValue(std::optional<CivilDate> value) {
	if(value && !isValid(*value)) value.reset();
	if(value == value_) return;
	value_ = value;
	if(value_) focused_ = *value_;
	invalidatePaint();
}

void Calendar::setRange(std::optional<CivilDate> earliest, std::optional<CivilDate> latest) {
	setLook(earliest_, earliest && isValid(*earliest) ? earliest : std::nullopt);
	setLook(latest_, latest && isValid(*latest) ? latest : std::nullopt);
}

void Calendar::setToday(std::optional<CivilDate> today) { setLook(today_, today && isValid(*today) ? today : std::nullopt); }

void Calendar::setMarked(std::vector<CivilDate> marked) {
	std::ranges::sort(marked);
	setLook(marked_, std::move(marked));
}

void Calendar::setFirstDayOfWeek(int day) { setLook(firstDayOfWeek_, (day % 7 + 7) % 7); }

void Calendar::setNames(const std::vector<std::string>& months, const std::vector<std::string>& days) {
	std::array<std::u32string, 12> monthNames;
	std::array<std::u32string, 7> dayNames;
	for(std::size_t index = 0; index < monthNames.size(); ++index)
		monthNames[index] = codepointsOf(index < months.size() ? months[index] : englishMonths[index]);
	for(std::size_t index = 0; index < dayNames.size(); ++index)
		dayNames[index] = codepointsOf(index < days.size() ? days[index] : englishDays[index]);
	setLook(months_, std::move(monthNames));
	setLook(days_, std::move(dayNames));
}

void Calendar::setFont(std::shared_ptr<const Font> font) { setLook(font_, std::move(font)); }

void Calendar::setColors(Color text, Color accent, Color onAccent) {
	setLook(text_, text);
	setLook(accent_, accent);
	setLook(onAccent_, onAccent);
}

void Calendar::setFocusedDay(CivilDate day) {
	if(!isValid(day)) return;
	setLook(focused_, clamped(day));
}

bool Calendar::isEnabled(CivilDate day) const noexcept {
	return (!earliest_ || day >= *earliest_) && (!latest_ || day <= *latest_);
}

CivilDate Calendar::clamped(CivilDate day) const noexcept {
	if(earliest_ && day < *earliest_) return *earliest_;
	if(latest_ && day > *latest_) return *latest_;
	return day;
}

void Calendar::pick(CivilDate day) {
	if(!isValid(day) || !isEnabled(day)) return;
	focused_ = day;
	invalidatePaint();
	if(value_ == day) return;
	value_ = day;
	onValueChanged(day);
}

const Font* Calendar::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

CivilDate Calendar::firstShown() const noexcept { return startOfWeek(shownMonth(), firstDayOfWeek_); }

int Calendar::screenColumn(int column) const noexcept { return rightToLeft() ? 6 - column : column; }

Calendar::Layout Calendar::layout() const noexcept {
	const Rect& f = frame();
	const Font* text = font();
	const float line = text ? std::ceil(text->lineHeight()) : 16.0f;
	Layout out;
	const float header = line + 12.0f;
	const float button = std::min(header, f.width() / 7.0f);
	out.title = {f.left + button, f.top, f.right - button, f.top + header};
	const Rect left{f.left, f.top, f.left + button, f.top + header}, right{f.right - button, f.top, f.right, f.top + header};
	out.previous = rightToLeft() ? right : left;
	out.next = rightToLeft() ? left : right;
	out.dayNames = {f.left, out.title.bottom, f.right, out.title.bottom + line + 6.0f};
	out.days = {f.left, out.dayNames.bottom, f.right, f.bottom};
	out.cell = f.width() / 7.0f;
	out.row = std::max(0.0f, out.days.height() / static_cast<float>(weeksShown));
	return out;
}

Rect Calendar::dayRect(const Layout& area, int index) const noexcept {
	const auto column = static_cast<float>(screenColumn(index % 7)), week = static_cast<float>(index / 7);
	const float left = area.days.left + column * area.cell, top = area.days.top + week * area.row;
	return {left, top, left + area.cell, top + area.row};
}

bool Calendar::moveKey(const KeyEvent& event) {
	const int forward = rightToLeft() ? -1 : 1;
	const bool shift = event.mods.shift();
	switch(event.key) {
	case Key::Left: setFocusedDay(addDays(focused_, -forward)); return true;
	case Key::Right: setFocusedDay(addDays(focused_, forward)); return true;
	case Key::Up: setFocusedDay(addDays(focused_, -7)); return true;
	case Key::Down: setFocusedDay(addDays(focused_, 7)); return true;
	case Key::PageUp: setFocusedDay(addMonths(focused_, shift ? -12 : -1)); return true;
	case Key::PageDown: setFocusedDay(addMonths(focused_, shift ? 12 : 1)); return true;
	case Key::Home: setFocusedDay(startOfWeek(focused_, firstDayOfWeek_)); return true;
	case Key::End: setFocusedDay(addDays(startOfWeek(focused_, firstDayOfWeek_), 6)); return true;
	default: return false;
	}
}

void Calendar::keyEvent(const KeyEvent& event) {
	if(event.action == Action::Release || event.mods.control() || event.mods.super()) return Panel::keyEvent(event);
	if(event.key == Key::Enter || event.key == Key::KeypadEnter || event.key == Key::Space) return pick(focused_);
	if(!moveKey(event)) Panel::keyEvent(event);
}

void Calendar::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button != MouseButton::Left || event.action != Action::Press) return Panel::mouseButtonEvent(event);
	const Layout area = layout();
	if(area.previous.contains(event.position)) setFocusedDay(addMonths(focused_, -1));
	if(area.next.contains(event.position)) setFocusedDay(addMonths(focused_, 1));
	const CivilDate first = firstShown();
	for(int index = 0; index < daysShown; ++index)
		if(dayRect(area, index).contains(event.position)) pick(addDays(first, index));
	Panel::mouseButtonEvent(event);
}

void Calendar::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

void Calendar::paint(Painter& painter) const {
	Panel::paint(painter);
	const Layout area = layout();
	paintHeader(painter, area);
	for(int index = 0; index < daysShown; ++index) paintDay(painter, area, index);
}

void Calendar::paintHeader(Painter& painter, const Layout& area) const {
	const Font* text = font();
	if(!text) return;
	const std::u32string title = months_[static_cast<std::size_t>(focused_.month - 1)] + U" " + numberText(focused_.year);
	centred(painter, *text, title, area.title, text_);
	const bool canGoBack = !earliest_ || addMonths(shownMonth(), -1) >= CivilDate{earliest_->year, earliest_->month, 1};
	const bool canGoOn = !latest_ || addMonths(shownMonth(), 1) <= *latest_;
	centred(painter, *text, rightToLeft() ? U">" : U"<", area.previous, text_.withAlpha(canGoBack ? 1.0f : 0.3f));
	centred(painter, *text, rightToLeft() ? U"<" : U">", area.next, text_.withAlpha(canGoOn ? 1.0f : 0.3f));
	for(int column = 0; column < 7; ++column) {
		const auto left = area.dayNames.left + static_cast<float>(screenColumn(column)) * area.cell;
		const Rect cell{left, area.dayNames.top, left + area.cell, area.dayNames.bottom};
		const std::u32string& name = days_[static_cast<std::size_t>((firstDayOfWeek_ + column) % 7)];
		centred(painter, *text, name, cell, text_.withAlpha(0.6f));
	}
}

void Calendar::paintDay(Painter& painter, const Layout& area, int index) const {
	const CivilDate day = addDays(firstShown(), index);
	const Rect cell = dayRect(area, index);
	const float side = std::max(0.0f, std::min(cell.width(), cell.height()) - 4.0f);
	const Rect disc{std::round(cell.left + (cell.width() - side) / 2.0f), std::round(cell.top + (cell.height() - side) / 2.0f),
	                0.0f, 0.0f};
	const Rect spot{disc.left, disc.top, disc.left + side, disc.top + side};
	const CornerRadii round = CornerRadii::all(side / 2.0f);
	const bool picked = value_ == day, enabled = isEnabled(day), inMonth = day.month == focused_.month;
	if(picked) painter.fillRoundedRect(spot, round, accent_);
	if(today_ == day && !picked) painter.strokeRoundedRect(spot, round, 1.0f, text_.withAlpha(0.5f));
	if(hasFocus() && focused_ == day) painter.strokeRoundedRect(spot, round, 2.0f, picked ? onAccent_ : accent_);
	if(std::ranges::binary_search(marked_, day)) {
		const float dot = 2.0f, middle = std::round(cell.left + cell.width() / 2.0f);
		painter.fillRoundedRect({middle - dot, spot.bottom - 3.0f * dot, middle + dot, spot.bottom - dot},
		                        CornerRadii::all(dot), picked ? onAccent_ : accent_);
	}
	const Font* text = font();
	if(!text) return;
	const float alpha = !enabled ? 0.25f : inMonth ? 1.0f : 0.45f;
	centred(painter, *text, numberText(day.day), cell, picked ? onAccent_ : text_.withAlpha(text_.a * alpha));
}

} // namespace affresco
