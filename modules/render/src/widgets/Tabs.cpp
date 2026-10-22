/// @file
/// @brief Tabs: places the tabs, scrolls the strip, reorders by dragging, closes, and draws.

#include "affresco/render/Tabs.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"
#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace affresco {

namespace {

constexpr float padding = 12.0f;      ///< Pixels before the label and after the last thing in a tab.
constexpr float closeSize = 16.0f;    ///< The close button's square side.
constexpr float closeGap = 6.0f;      ///< Pixels between the label and the close button.
constexpr float dragThreshold = 4.0f; ///< Pixels the pointer moves before a press becomes a drag.
constexpr float underline = 2.0f;     ///< The active tab's line, in pixels.
constexpr char32_t closeMark = U'×';

/// @brief Whether a tab has room for a close button or a marker.
/// @param tab The tab.
[[nodiscard]] bool hasButton(const Tab& tab) noexcept { return tab.closable || tab.marker; }

/// @brief Whether a mouse button event is the left button doing something.
/// @param event The event.
/// @param action What it should do.
/// @return true if it is.
constexpr bool isLeft(const MouseButtonEvent& event, Action action) noexcept {
	return event.button == MouseButton::Left && event.action == action;
}

/// @brief Where a moved tab goes, from where it was dropped: the place counts the moved tab, the result doesn't.
/// @param to The place it was dropped at, among all the tabs.
/// @param from Where it was.
/// @return Its index once moved.
constexpr std::size_t withoutMoved(std::size_t to, std::size_t from) noexcept { return to > from ? to - 1 : to; }

/// @brief The tab a key moves the active one to.
/// @param key The key.
/// @param active The active tab's index, or count if there is none.
/// @param count How many tabs there are; at least one.
/// @return Its index, or nothing for another key.
constexpr std::optional<std::size_t> steppedTab(Key key, std::size_t active, std::size_t count) noexcept {
	switch(key) {
	case Key::Left:
		return active == 0 || active >= count ? 0 : active - 1;
	case Key::Right:
		return active >= count ? 0 : std::min(active + 1, count - 1);
	case Key::Home:
		return 0;
	case Key::End:
		return count - 1;
	default:
		return std::nullopt;
	}
}

} // namespace

Tabs::Tabs() {
	setColor(colors::track);
	setCursor(Cursor::Hand);
}

void Tabs::setTabs(std::vector<Tab> tabs) {
	if(tabs == tabs_) return;
	tabs_ = std::move(tabs);
	places_.clear();
	hovered_ = pressed_ = tabs_.size();
	reordering_ = false;
	keepActiveInView_ = true;
	invalidate();
}

void Tabs::setActive(std::string id) {
	if(id == active_) return;
	active_ = std::move(id);
	keepActiveInView_ = true;
	invalidate();
}

void Tabs::setFont(std::shared_ptr<const Font> font) { setProperty(font_, std::move(font)); }

void Tabs::setColors(Color text, Color activeText, Color activeBackground, Color accent) {
	setLook(text_, text);
	setLook(activeText_, activeText);
	setLook(activeBackground_, activeBackground);
	setLook(accent_, accent);
}

const Font* Tabs::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void Tabs::arrange() {
	const Font* labels = font();
	places_.resize(tabs_.size());
	float left = 0.0f;
	for(std::size_t i = 0; i < tabs_.size(); ++i) {
		Place& place = places_[i];
		place.text = detail::fromUtf8(tabs_[i].label);
		const float text = labels ? std::ceil(labels->measure(place.text)) : 0.0f;
		place.left = left;
		place.width = padding + text + (hasButton(tabs_[i]) ? closeGap + closeSize : 0.0f) + padding;
		left += place.width;
	}
	contentWidth_ = left;
	scroll_ = std::clamp(scroll_, 0.0f, std::max(contentWidth_ - resolvedSize().x, 0.0f));
	if(keepActiveInView_) {
		keepActiveInView_ = false;
		keepInView(activeIndex());
	}
}

std::size_t Tabs::activeIndex() const noexcept {
	for(std::size_t i = 0; i < tabs_.size(); ++i)
		if(tabs_[i].id == active_) return i;
	return tabs_.size();
}

void Tabs::keepInView(std::size_t index) {
	if(index >= places_.size()) return;
	const Place& place = places_[index];
	const float width = resolvedSize().x;
	if(place.left + place.width - scroll_ > width) scroll_ = place.left + place.width - width;
	if(place.left < scroll_) scroll_ = place.left;
	invalidatePaint();
}

std::size_t Tabs::tabAt(Vec2 position) const noexcept {
	const float x = position.x - frame().left + scroll_;
	if(!frame().contains(position)) return tabs_.size();
	for(std::size_t i = 0; i < places_.size(); ++i)
		if(x >= places_[i].left && x < places_[i].left + places_[i].width) return i;
	return tabs_.size();
}

std::size_t Tabs::insertionAt(Vec2 position) const noexcept {
	const float x = position.x - frame().left + scroll_;
	for(std::size_t i = 0; i < places_.size(); ++i)
		if(x < places_[i].left + places_[i].width / 2.0f) return i;
	return places_.size();
}

void Tabs::showDropIndicator(std::optional<std::size_t> index) { setLook(dropIndicator_, index); }

bool Tabs::onCloseButton(std::size_t index, Vec2 position) const noexcept {
	if(index >= places_.size() || !tabs_[index].closable) return false;
	const Place& place = places_[index];
	const float right = frame().left + place.left + place.width - scroll_ - padding;
	const float middle = frame().top + frame().height() / 2.0f;
	return position.x >= right - closeSize && position.x < right && std::abs(position.y - middle) <= closeSize / 2.0f;
}

void Tabs::activate(std::size_t index) {
	if(index >= tabs_.size() || tabs_[index].id == active_) return;
	active_ = tabs_[index].id;
	keepInView(index);
	invalidatePaint();
	onActivated(active_);
}

void Tabs::mouseButtonEvent(const MouseButtonEvent& event) {
	if(isLeft(event, Action::Press)) {
		pressed_ = tabAt(event.position);
		pressX_ = event.position.x;
		reordering_ = false;
		if(pressed_ < tabs_.size() && !onCloseButton(pressed_, event.position)) activate(pressed_);
	} else if(isLeft(event, Action::Release) && pressed_ < tabs_.size()) {
		const std::size_t from = pressed_;
		pressed_ = tabs_.size();
		if(reordering_) {
			reordering_ = false;
			const std::size_t to = withoutMoved(insertionAt(event.position), from);
			showDropIndicator(std::nullopt);
			// Outside the strip it isn't a reorder (it may be a drop on another strip)
			if(to != from && frame().contains(event.position)) onMoved(TabMove{tabs_[from].id, from, to});
		} else if(onCloseButton(from, event.position)) {
			const std::string id = tabs_[from].id; // the handler may change the tabs
			onCloseRequested(id);
		}
	}
	Panel::mouseButtonEvent(event);
}

void Tabs::mouseMoveEvent(const MouseMoveEvent& event) {
	const std::size_t hovered = tabAt(event.position);
	if(hovered != hovered_) {
		hovered_ = hovered;
		invalidatePaint();
	}
	if(pressed_ < tabs_.size() && (reordering_ || std::abs(event.position.x - pressX_) > dragThreshold)) {
		reordering_ = true;
		showDropIndicator(frame().contains(event.position) ? std::optional(insertionAt(event.position)) : std::nullopt);
	}
	Panel::mouseMoveEvent(event);
}

void Tabs::hoverEvent(bool hovered) {
	if(!hovered && hovered_ != tabs_.size()) {
		hovered_ = tabs_.size();
		invalidatePaint();
	}
	Panel::hoverEvent(hovered);
}

void Tabs::keyEvent(const KeyEvent& event) {
	const std::size_t active = activeIndex(), count = tabs_.size();
	if(count > 0 && event.action != Action::Release) {
		if(const std::optional<std::size_t> to = steppedTab(event.key, active, count)) activate(*to);
		else if(event.key == Key::Delete && active < count && tabs_[active].closable) {
			const std::string id = tabs_[active].id;
			onCloseRequested(id);
		}
	}
	Panel::keyEvent(event);
}

bool Tabs::scrollContent(Vec2 delta) {
	const float before = scroll_;
	// A vertical wheel scrolls the strip sideways too
	const float along = std::abs(delta.x) > std::abs(delta.y) ? delta.x : delta.y;
	scroll_ = std::clamp(scroll_ + along, 0.0f, std::max(contentWidth_ - resolvedSize().x, 0.0f));
	if(scroll_ == before) return false;
	invalidatePaint();
	return true;
}

void Tabs::paint(Painter& painter) const {
	Panel::paint(painter);
	const Font* labels = font();
	if(!labels) return;
	const float left = frame().left - scroll_, right = frame().right;
	for(std::size_t i = 0; i < places_.size(); ++i)
		if(left + places_[i].left < right && left + places_[i].left + places_[i].width > frame().left)
			paintTab(painter, *labels, i);
	if(dropIndicator_) {
		const float x =
		    std::round(left + (*dropIndicator_ < places_.size() ? places_[*dropIndicator_].left : contentWidth_));
		painter.fillRect({x - 1.0f, frame().top, x + 1.0f, frame().bottom}, accent_);
	}
}

void Tabs::paintTab(Painter& painter, const Font& font, std::size_t index) const {
	const Rect& f = frame();
	const Place& place = places_[index];
	const Tab& tab = tabs_[index];
	const bool active = tab.id == active_, hovered = index == hovered_;
	const float left = std::round(f.left + place.left - scroll_);
	const Rect area{left, f.top, left + place.width, f.bottom};
	if(active) {
		painter.fillRect(area, activeBackground_);
		painter.fillRect({area.left, area.bottom - underline, area.right, area.bottom}, accent_);
	}
	const float baseline = std::round(f.top + (f.height() - font.lineHeight()) / 2.0f + font.ascent());
	painter.drawText(font, place.text, {left + padding, baseline}, active ? activeText_ : text_);

	// The close button when it can be used now, else the marker
	const float buttonLeft = area.right - padding - closeSize, middle = f.top + f.height() / 2.0f;
	if(tab.closable && (active || hovered)) {
		const std::u32string_view mark(&closeMark, 1);
		const float x = std::round(buttonLeft + (closeSize - font.measure(mark)) / 2.0f);
		painter.drawText(font, mark, {x, baseline}, active ? activeText_ : text_);
	} else if(tab.marker) {
		const float dot = 8.0f, x = buttonLeft + (closeSize - dot) / 2.0f, y = std::round(middle - dot / 2.0f);
		painter.fillRoundedRect({x, y, x + dot, y + dot}, CornerRadii::all(dot / 2.0f), accent_);
	}
}

} // namespace affresco
