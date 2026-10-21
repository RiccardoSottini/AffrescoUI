/// @file
/// @brief Split: panes along an axis, sizes kept within minimums, sashes moved by the pointer or the keyboard.

#include "affresco/render/Split.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <numeric>

namespace affresco {

namespace {

constexpr float keyStep = 8.0f;            ///< Pixels an arrow key moves a sash.
constexpr float bigKeyStep = 32.0f;        ///< With Shift.
constexpr float sashSlop = 2.0f;           ///< Pixels on each side of a sash that still grab it.
constexpr double doubleClickSeconds = 0.4; ///< At most this long between the presses of a double click.

/// @brief The children a split lays out: visible, not overlays.
/// @param panel The split.
/// @return Its panes, in order.
std::vector<Panel*> panesOf(const Panel& panel) {
	std::vector<Panel*> result;
	result.reserve(panel.children().size());
	for(const auto& child : panel.children())
		if(child->isVisible() && !child->isOverlay()) result.push_back(child.get());
	return result;
}

} // namespace

Split::Split(Direction direction) : direction_(direction) {
	setColor(colors::transparent);
	setCursor(direction == Direction::Row ? Cursor::ResizeHorizontal : Cursor::ResizeVertical);
}

void Split::setDirection(Direction direction) {
	setProperty(direction_, direction);
	setCursor(direction == Direction::Row ? Cursor::ResizeHorizontal : Cursor::ResizeVertical);
}

void Split::setSizes(std::vector<float> sizes) { setProperty(wanted_, std::move(sizes)); }

void Split::setDefaultSizes(std::vector<float> sizes) { setProperty(defaults_, std::move(sizes)); }

void Split::setMinimumSizes(std::vector<float> minimums) { setProperty(minimums_, std::move(minimums)); }

void Split::setSash(float thickness, Color idle, Color active) {
	setProperty(thickness_, std::max(thickness, 0.0f));
	setLook(idle_, idle);
	setLook(active_, active);
}

void Split::reset() {
	wanted_ = defaults_;
	laidOut_ = fit(wanted_, laidOut_.size(), std::accumulate(laidOut_.begin(), laidOut_.end(), 0.0f));
	invalidate();
	onSizesChanged(laidOut_);
}

float Split::minimumOf(std::size_t pane) const noexcept {
	if(minimums_.empty()) return 0.0f;
	return std::max(pane < minimums_.size() ? minimums_[pane] : minimums_.back(), 0.0f);
}

std::vector<float> Split::fit(const std::vector<float>& wanted, std::size_t panes, float room) const {
	std::vector<float> sizes(panes, 0.0f);
	if(panes == 0) return sizes;

	// The sizes given, then the panes without one (the last included) share what is left
	float given = 0.0f;
	std::size_t missing = 1;
	for(std::size_t i = 0; i + 1 < panes; ++i) {
		if(i < wanted.size() && wanted[i] >= 0.0f) given += sizes[i] = wanted[i];
		else ++missing;
	}
	const float share = std::max(room - given, 0.0f) / static_cast<float>(missing);
	for(std::size_t i = 0; i + 1 < panes; ++i)
		sizes[i] = std::max(i < wanted.size() && wanted[i] >= 0.0f ? sizes[i] : share, minimumOf(i));

	// The last pane takes the rest; if that is under its minimum, the panes before it give way, the latest first
	float rest = room - std::accumulate(sizes.begin(), sizes.end() - 1, 0.0f);
	for(std::size_t i = panes - 1; i-- > 0 && rest < minimumOf(panes - 1);) {
		const float give = std::min(sizes[i] - minimumOf(i), minimumOf(panes - 1) - rest);
		sizes[i] -= give;
		rest += give;
	}
	sizes.back() = std::max(rest, 0.0f);
	return sizes;
}

void Split::arrange() {
	const bool row = direction_ == Direction::Row;
	const std::vector<Panel*> panes = panesOf(*this);
	const Vec2 size = resolvedSize();
	const float sashes = panes.empty() ? 0.0f : thickness_ * static_cast<float>(panes.size() - 1);
	laidOut_ =
	    fit(wanted_.empty() ? defaults_ : wanted_, panes.size(), std::max((row ? size.x : size.y) - sashes, 0.0f));

	float position = 0.0f;
	for(std::size_t i = 0; i < panes.size(); ++i) {
		Panel& pane = *panes[i];
		pane.clearAnchor(row ? Edge::Right : Edge::Bottom);
		pane.clearAnchor(row ? Edge::Bottom : Edge::Right);
		pane.setAnchor(row ? Edge::Left : Edge::Top, px(position));
		pane.setAnchor(row ? Edge::Top : Edge::Left, px(0.0f));
		if(row) arrangeSize(pane, laidOut_[i], size.y);
		else arrangeSize(pane, size.x, laidOut_[i]);
		position += laidOut_[i] + thickness_;
	}
}

float Split::sashStart(std::size_t sash) const noexcept {
	float start = static_cast<float>(sash) * thickness_;
	for(std::size_t i = 0; i <= sash && i < laidOut_.size(); ++i) start += laidOut_[i];
	return start;
}

std::size_t Split::sashAt(Vec2 position) const noexcept {
	const std::size_t count = laidOut_.empty() ? 0 : laidOut_.size() - 1;
	const float along = direction_ == Direction::Row ? position.x - frame().left : position.y - frame().top;
	for(std::size_t sash = 0; sash < count; ++sash) {
		const float start = sashStart(sash);
		if(along >= start - sashSlop && along < start + thickness_ + sashSlop) return sash;
	}
	return count;
}

void Split::moveSash(std::size_t sash, float delta) {
	if(sash + 1 >= laidOut_.size()) return;
	// Both panes beside the sash stay at least at their minimums; the others keep their sizes
	const float lowest = minimumOf(sash) - laidOut_[sash], highest = laidOut_[sash + 1] - minimumOf(sash + 1);
	const float moved = lowest > highest ? 0.0f : std::clamp(delta, lowest, highest);
	if(moved == 0.0f) return;
	laidOut_[sash] += moved;
	laidOut_[sash + 1] -= moved;
	wanted_ = laidOut_;
	invalidate();
	onSizesChanged(laidOut_);
}

void Split::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button == MouseButton::Left && event.action == Action::Press) {
		const std::size_t sash = sashAt(event.position);
		if(sash + 1 < laidOut_.size()) {
			const double now = window() ? window()->clock().seconds() : 0.0;
			const float along =
			    direction_ == Direction::Row ? event.position.x - frame().left : event.position.y - frame().top;
			dragging_ = true;
			dragged_ = activeSash_ = sash;
			grab_ = along - sashStart(sash);
			if(lastPressed_ == sash && lastPress_ >= 0.0 && now - lastPress_ < doubleClickSeconds) {
				dragging_ = false;
				lastPress_ = -1.0;
				reset();
			} else {
				lastPress_ = now;
				lastPressed_ = sash;
			}
			invalidatePaint();
		}
	} else if(event.button == MouseButton::Left && event.action == Action::Release && dragging_) {
		dragging_ = false;
		invalidatePaint();
	}
	Panel::mouseButtonEvent(event);
}

void Split::mouseMoveEvent(const MouseMoveEvent& event) {
	if(dragging_) {
		const float along =
		    direction_ == Direction::Row ? event.position.x - frame().left : event.position.y - frame().top;
		const float before = sashStart(dragged_);
		moveSash(dragged_, along - grab_ - before);
		if(sashStart(dragged_) != before) lastPress_ = -1.0; // a drag is not the first click of a double click
	}
	Panel::mouseMoveEvent(event);
}

void Split::keyEvent(const KeyEvent& event) {
	const std::size_t sashes = laidOut_.empty() ? 0 : laidOut_.size() - 1;
	if(sashes > 0 && event.action != Action::Release) {
		const bool row = direction_ == Direction::Row;
		const float step = event.mods.shift() ? bigKeyStep : keyStep;
		activeSash_ = std::min(activeSash_, sashes - 1);
		if(event.key == (row ? Key::Left : Key::Up)) moveSash(activeSash_, -step);
		else if(event.key == (row ? Key::Right : Key::Down)) moveSash(activeSash_, step);
		else if(event.key == Key::PageUp) activeSash_ = activeSash_ == 0 ? sashes - 1 : activeSash_ - 1;
		else if(event.key == Key::PageDown) activeSash_ = (activeSash_ + 1) % sashes;
		else if(event.key == Key::Home) reset();
		invalidatePaint();
	}
	Panel::keyEvent(event);
}

void Split::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();
	const bool row = direction_ == Direction::Row, focused = hasFocus();
	for(std::size_t sash = 0; sash + 1 < laidOut_.size(); ++sash) {
		const float start = (row ? f.left : f.top) + sashStart(sash);
		const bool active = dragging_ ? dragged_ == sash : focused && activeSash_ == sash;
		const Rect area =
		    row ? Rect{start, f.top, start + thickness_, f.bottom} : Rect{f.left, start, f.right, start + thickness_};
		painter.fillRect(area, active ? active_ : idle_);
	}
}

} // namespace affresco
