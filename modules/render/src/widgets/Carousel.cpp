/// @file
/// @brief Carousel: placing the pages, the arrows and dots, and turning pages by mouse, keyboard and trackpad.

#include "affresco/render/Carousel.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace affresco {

namespace {

constexpr float dotSize = 8.0f;      ///< A dot's diameter.
constexpr float dotGap = 8.0f;       ///< Pixels between dots.
constexpr float arrowSize = 7.0f;    ///< Half the height of an arrow's chevron.
constexpr float swipeTurn = 60.0f;   ///< Horizontal scrolling that turns a page.
constexpr std::size_t mostDots = 40; ///< Beyond this many pages, no dots are drawn (they wouldn't fit).

/// @brief Where a dot sits, from the carousel's left edge.
/// @param index The dot.
/// @param count The number of dots.
/// @param width The carousel's width.
/// @return Its centre's x.
float dotCentre(std::size_t index, std::size_t count, float width) noexcept {
	const float row = static_cast<float>(count) * dotSize + static_cast<float>(count - 1) * dotGap;
	return (width - row) / 2.0f + static_cast<float>(index) * (dotSize + dotGap) + dotSize / 2.0f;
}

/// @brief The dot under a point of the dots' strip.
/// @param x The point's x, from the carousel's left edge.
/// @param count The number of dots.
/// @param width The carousel's width.
/// @param rightToLeft Whether the dots run right to left.
/// @return The dot, or nothing between and around them.
std::optional<std::size_t> dotAt(float x, std::size_t count, float width, bool rightToLeft) noexcept {
	for(std::size_t dot = 0; dot < count; ++dot) {
		const float centre = dotCentre(rightToLeft ? count - 1 - dot : dot, count, width);
		if(std::abs(x - centre) <= (dotSize + dotGap) / 2.0f) return dot;
	}
	return std::nullopt;
}

} // namespace

Carousel::Carousel() {
	setColor(colors::transparent);
	setCursor(Cursor::Hand);
}

std::vector<Panel*> Carousel::pages() const {
	std::vector<Panel*> result;
	result.reserve(children().size());
	for(const auto& child : children())
		if(child->isVisible() && !child->isOverlay()) result.push_back(child.get());
	return result;
}

std::size_t Carousel::pageCount() const noexcept {
	return static_cast<std::size_t>(
	    std::ranges::count_if(children(), [](const auto& child) { return child->isVisible() && !child->isOverlay(); }));
}

void Carousel::setIndex(std::size_t index) {
	if(index == index_) return;
	index_ = index;
	invalidate();
}

void Carousel::setArrows(bool arrows) {
	if(arrows == arrows_) return;
	arrows_ = arrows;
	invalidate();
}

void Carousel::setDots(bool dots) {
	if(dots == dots_) return;
	dots_ = dots;
	invalidate();
}

void Carousel::setColors(Color arrow, Color dot, Color activeDot) {
	setLook(arrowColor_, arrow);
	setLook(dotColor_, dot);
	setLook(activeDotColor_, activeDot);
}

Rect Carousel::pageArea() const noexcept {
	const Vec2 size = resolvedSize();
	const float side = arrows_ ? gutter : 0.0f;
	const float below = dots_ ? strip : 0.0f;
	return {side, 0.0f, std::max(side, size.x - side), std::max(0.0f, size.y - below)};
}

void Carousel::arrange() {
	const std::vector<Panel*> shown = pages();
	if(!shown.empty()) index_ = std::min(index_, shown.size() - 1);
	const Rect area = pageArea();
	for(std::size_t page = 0; page < shown.size(); ++page) {
		Panel& panel = *shown[page];
		// The others sit a whole carousel away on either side, where clipping hides them
		const float offset = (static_cast<float>(page) - static_cast<float>(index_)) * (resolvedSize().x + gutter);
		panel.clearAnchor(Edge::Right);
		panel.clearAnchor(Edge::Bottom);
		panel.setAnchor(Edge::Left, px(area.left + offset));
		panel.setAnchor(Edge::Top, px(area.top));
		arrangeSize(panel, area.width(), area.height());
	}
}

void Carousel::choose(std::size_t index) {
	if(index == index_) return;
	setIndex(index);
	onIndexChanged(index);
}

void Carousel::turn(int step) {
	const std::size_t count = pageCount();
	if(count == 0) return;
	const auto last = static_cast<long long>(count) - 1;
	long long next = static_cast<long long>(index_) + step;
	if(loop_) next = next < 0 ? last : (next > last ? 0 : next);
	choose(static_cast<std::size_t>(std::clamp(next, 0LL, last)));
}

int Carousel::controlAt(Vec2 position) const noexcept {
	const Rect box = frame();
	const float x = position.x - box.left;
	const float y = position.y - box.top;
	const int forward = rightToLeft() ? -1 : 1;
	if(arrows_ && y < box.height() - (dots_ ? strip : 0.0f)) {
		if(x < gutter) return -forward;
		if(x >= box.width() - gutter) return forward;
	}
	const std::size_t count = pageCount();
	if(!dots_ || count == 0 || count > mostDots || y < box.height() - strip) return 0;
	const std::optional<std::size_t> dot = dotAt(x, count, box.width(), rightToLeft());
	return dot ? static_cast<int>(*dot) + 2 : 0;
}

void Carousel::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button == MouseButton::Left && event.action == Action::Press) {
		const int control = controlAt(event.position);
		if(control == -1 || control == 1) turn(control);
		else if(control >= 2) choose(static_cast<std::size_t>(control - 2));
	}
	Panel::mouseButtonEvent(event);
}

void Carousel::keyEvent(const KeyEvent& event) {
	if(event.action != Action::Release) {
		const int forward = rightToLeft() ? -1 : 1;
		if(event.key == Key::Right) turn(forward);
		else if(event.key == Key::Left) turn(-forward);
		else if(event.key == Key::Home) choose(0);
		else if(event.key == Key::End && pageCount() > 0) choose(pageCount() - 1);
	}
	Panel::keyEvent(event);
}

bool Carousel::scrollContent(Vec2 delta) {
	if(std::abs(delta.x) <= std::abs(delta.y)) return false; // vertical scrolling is the page's or the parent's
	swipe_ += delta.x;
	if(std::abs(swipe_) >= swipeTurn) {
		const int step = (swipe_ > 0.0f) != rightToLeft() ? 1 : -1;
		swipe_ = 0.0f;
		turn(step);
	}
	return true;
}

void Carousel::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect box = frame();
	const std::size_t count = pageCount();
	if(arrows_ && count > 1) {
		const float middle = std::round(box.top + pageArea().height() / 2.0f);
		for(const int side : {-1, 1}) {
			const float centre = side < 0 ? box.left + gutter / 2.0f : box.right - gutter / 2.0f;
			// A chevron of two strokes, pointing out
			const float tip = centre + static_cast<float>(side) * arrowSize / 2.0f;
			const float back = centre - static_cast<float>(side) * arrowSize / 2.0f;
			for(int y = 0; y < static_cast<int>(arrowSize); ++y) {
				const float t = static_cast<float>(y) / arrowSize;
				const float x = tip + (back - tip) * t;
				painter.fillRect(
				    {x - 1.0f, middle - static_cast<float>(y) - 1.0f, x + 1.0f, middle - static_cast<float>(y) + 1.0f},
				    arrowColor_);
				painter.fillRect(
				    {x - 1.0f, middle + static_cast<float>(y) - 1.0f, x + 1.0f, middle + static_cast<float>(y) + 1.0f},
				    arrowColor_);
			}
		}
	}
	if(!dots_ || count < 2 || count > mostDots) return;
	const float centreY = box.bottom - strip / 2.0f;
	for(std::size_t dot = 0; dot < count; ++dot) {
		const float centre = box.left + dotCentre(rightToLeft() ? count - 1 - dot : dot, count, box.width());
		const Rect disc{centre - dotSize / 2.0f, centreY - dotSize / 2.0f, centre + dotSize / 2.0f,
		                centreY + dotSize / 2.0f};
		painter.fillRoundedRect(disc, CornerRadii::all(dotSize / 2.0f), dot == index_ ? activeDotColor_ : dotColor_);
	}
}

} // namespace affresco
