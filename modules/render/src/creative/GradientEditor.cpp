/// @file
/// @brief GradientEditor and the gradient maths: normalising, sampling and the position of a point.

#include "affresco/render/GradientEditor.hpp"
#include "affresco/render/ColorPicker.hpp"
#include "affresco/render/Window.hpp"

#include "CreativePixels.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

constexpr float barHeight = 16.0f;                            ///< The height of the stops' bar.
constexpr float handleHeight = 14.0f;                         ///< The height of the handles' row.
constexpr float handleWidth = 10.0f;                          ///< The width of a handle.
constexpr float removeDistance = 24.0f;                       ///< How far off the bar a dragged stop is removed.
constexpr float partGap = 8.0f;                               ///< The space between the preview and the bar.
constexpr int previewPixels = 96;                             ///< The side of the preview's bitmap.
constexpr int barPixels = 256;                                ///< The width of the bar's bitmap.
constexpr double degreesToRadians = 3.14159265358979 / 180.0; ///< Degrees to radians.

/// @brief A cache key for a gradient.
/// @param gradient The gradient.
/// @param withShape Whether its kind and angle count (the bar ignores them).
/// @return The key.
[[nodiscard]] std::uint64_t gradientKey(const Gradient& gradient, bool withShape) noexcept {
	std::uint64_t key = detail::firstKey;
	if(withShape) key = detail::mixKey(detail::mixKey(key, static_cast<double>(gradient.kind)), gradient.angle);
	for(const GradientStop& stop : gradient.stops) {
		key = detail::mixKey(detail::mixKey(detail::mixKey(key, stop.at), stop.color.r), stop.color.g);
		key = detail::mixKey(detail::mixKey(key, stop.color.b), stop.color.a);
	}
	return key;
}

/// @brief Wraps an angle to 0..360.
/// @param degrees The angle.
/// @return The wrapped angle (0 when not a number).
[[nodiscard]] double wrapAngle(double degrees) noexcept {
	if(!std::isfinite(degrees)) return 0.0;
	const double wrapped = std::fmod(degrees, 360.0);
	return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
}

} // namespace

void normalizeGradient(Gradient& gradient) {
	auto& stops = gradient.stops;
	if(stops.size() > maxGradientStops) stops.resize(maxGradientStops);
	for(GradientStop& stop : stops) stop.at = std::isfinite(stop.at) ? std::clamp(stop.at, 0.0, 1.0) : 0.0;
	std::ranges::stable_sort(stops, {}, &GradientStop::at);
	if(stops.empty()) stops = {{0.0, colors::black}, {1.0, colors::white}};
	if(stops.size() == 1) stops = {{0.0, stops.front().color}, {1.0, stops.front().color}};
	gradient.angle = wrapAngle(gradient.angle);
}

Color sampleGradient(std::span<const GradientStop> stops, double at) noexcept {
	if(stops.empty()) return colors::transparent;
	if(at <= stops.front().at) return stops.front().color;
	for(std::size_t index = 1; index < stops.size(); ++index) {
		const GradientStop& next = stops[index];
		if(at > next.at) continue;
		const GradientStop& previous = stops[index - 1];
		const double span = next.at - previous.at;
		const double t = span > 0.0 ? (at - previous.at) / span : 1.0;
		return Color::mix(previous.color, next.color, static_cast<float>(t));
	}
	return stops.back().color;
}

double gradientPosition(const Gradient& gradient, double x, double y) noexcept {
	const double dx = x - 0.5, dy = y - 0.5;
	if(gradient.kind == Gradient::Kind::Radial) return std::min(1.0, std::hypot(dx, dy) / std::sqrt(0.5));
	const double radians = gradient.angle * degreesToRadians;
	const double across = std::sin(radians), down = -std::cos(radians);
	const double length = std::abs(across) + std::abs(down); // the gradient line reaches the corners, as in CSS
	return std::clamp(0.5 + (dx * across + dy * down) / length, 0.0, 1.0);
}

GradientEditor::GradientEditor()
    : previewBitmap_(std::make_unique<detail::BitmapSlot>()), barBitmap_(std::make_unique<detail::BitmapSlot>()) {
	Panel::setColor(colors::transparent);
	setCursor(Cursor::Hand);
	normalizeGradient(gradient_);
	auto popover = std::make_unique<Panel>();
	popover->setColor(colors::white);
	popover->setBorder(1.0f, colors::black.withAlpha(0.15f));
	popover->setCornerRadii(CornerRadii::all(6.0f));
	popover->setShadow({{0.0f, 4.0f}, 12.0f, colors::black.withAlpha(0.2f)});
	popover->setSize(px(240.0f), px(320.0f));
	popover->setOverlay("", Placement::Below);
	popover->setVisible(false);
	auto picker = std::make_unique<ColorPicker>();
	picker->setPosition(px(8.0f), px(8.0f));
	picker->setSize(px(224.0f), px(304.0f));
	picker->onValueChanged.connect([this](Color color) { setStopColor(selected_, color); });
	picker->onKey.connect([this](const KeyEvent& event) {
		if(event.action != Action::Release && (event.key == Key::Escape || event.key == Key::Tab)) closePopover(true);
	});
	picker->onFocusChanged.connect([this](bool focused) {
		if(!focused && !focusWithin()) closePopover(false);
	});
	picker_ = &static_cast<ColorPicker&>(popover->addChild(std::move(picker)));
	popover_ = &addChild(std::move(popover));
}

GradientEditor::~GradientEditor() = default;

void GradientEditor::setGradient(Gradient gradient) {
	normalizeGradient(gradient);
	if(gradient == gradient_) return;
	gradient_ = std::move(gradient);
	selected_ = std::min(selected_, gradient_.stops.size() - 1);
	if(isPopoverOpen()) picker_->setValue(gradient_.stops[selected_].color);
	invalidatePaint();
}

void GradientEditor::selectStop(std::size_t index) {
	index = std::min(index, gradient_.stops.size() - 1);
	if(index == selected_) return;
	selected_ = index;
	if(isPopoverOpen()) picker_->setValue(gradient_.stops[selected_].color);
	invalidatePaint();
}

bool GradientEditor::addStop(double at) {
	if(gradient_.stops.size() >= maxGradientStops) return false;
	at = std::clamp(at, 0.0, 1.0);
	const GradientStop stop{at, sampleGradient(gradient_.stops, at)};
	const auto place = std::ranges::upper_bound(gradient_.stops, at, {}, &GradientStop::at);
	selected_ = static_cast<std::size_t>(place - gradient_.stops.begin());
	gradient_.stops.insert(place, stop);
	changed();
	return true;
}

bool GradientEditor::removeStop(std::size_t index) {
	if(index >= gradient_.stops.size() || gradient_.stops.size() <= 2) return false;
	gradient_.stops.erase(gradient_.stops.begin() + static_cast<std::ptrdiff_t>(index));
	selected_ = std::min(selected_ > index ? selected_ - 1 : selected_, gradient_.stops.size() - 1);
	changed();
	return true;
}

std::size_t GradientEditor::moveStop(std::size_t index, double at) {
	auto& stops = gradient_.stops;
	if(index >= stops.size()) return index;
	at = std::isfinite(at) ? std::clamp(at, 0.0, 1.0) : stops[index].at;
	if(at == stops[index].at) return index;
	const bool follows = index == selected_;
	stops[index].at = at;
	for(; index > 0 && stops[index - 1].at > at; --index) std::swap(stops[index - 1], stops[index]);
	for(; index + 1 < stops.size() && stops[index + 1].at < at; ++index) std::swap(stops[index + 1], stops[index]);
	if(follows) selected_ = index;
	changed();
	return index;
}

void GradientEditor::setStopColor(std::size_t index, Color color) {
	if(index >= gradient_.stops.size() || gradient_.stops[index].color == color) return;
	gradient_.stops[index].color = color;
	changed();
}

void GradientEditor::changeKind(Gradient::Kind kind) {
	if(kind == gradient_.kind) return;
	gradient_.kind = kind;
	changed();
}

void GradientEditor::changeAngle(double angle) {
	angle = wrapAngle(angle);
	if(angle == gradient_.angle) return;
	gradient_.angle = angle;
	changed();
}

void GradientEditor::openPopover() {
	picker_->setValue(gradient_.stops[selected_].color);
	popover_->setVisible(true);
	if(window()) window()->setFocus(picker_);
	invalidatePaint();
}

void GradientEditor::closePopover(bool refocus) {
	if(!isPopoverOpen()) return;
	picker_->rememberValue();
	popover_->setVisible(false);
	if(refocus && window()) window()->setFocus(this);
	invalidatePaint();
}

bool GradientEditor::isPopoverOpen() const noexcept { return popover_->isVisible(); }

void GradientEditor::setAccent(Color accent) {
	setLook(accent_, accent);
	picker_->setColors(colors::black, accent);
}

void GradientEditor::changed() {
	invalidatePaint();
	onGradientChanged(gradient_);
}

bool GradientEditor::focusWithin() const noexcept {
	const Panel* focused = window() ? window()->focusedPanel() : nullptr;
	while(focused && focused != this) focused = focused->parent();
	return focused == this;
}

GradientEditor::Layout GradientEditor::layout() const noexcept {
	const Rect& f = frame();
	Layout out;
	const float inset = handleWidth / 2.0f; // so the end handles fit
	out.handles = {f.left + inset, f.bottom - handleHeight, f.right - inset, f.bottom};
	out.bar = {out.handles.left, out.handles.top - barHeight, out.handles.right, out.handles.top};
	const float previewSide = std::min(f.width(), out.bar.top - partGap - f.top);
	if(previewSide >= barHeight) out.preview = {f.left, f.top, f.left + previewSide, f.top + previewSide};
	return out;
}

Rect GradientEditor::handleRect(const Layout& area, std::size_t index) const noexcept {
	const float x = area.bar.left + static_cast<float>(gradient_.stops[index].at) * area.bar.width();
	return {x - handleWidth / 2.0f, area.handles.top + 2.0f, x + handleWidth / 2.0f, area.handles.bottom};
}

std::size_t GradientEditor::stopAt(const Layout& area, Vec2 position) const noexcept {
	const std::size_t count = gradient_.stops.size();
	if(position.y < area.bar.top || position.y > area.handles.bottom) return count;
	std::size_t best = count;
	float bestDistance = handleWidth / 2.0f + 1.0f;
	for(std::size_t index = 0; index < count; ++index) { // the nearest, the selected one winning ties
		const Rect handle = handleRect(area, index);
		const float distance = std::abs(position.x - (handle.left + handle.right) / 2.0f);
		if(distance < bestDistance || (distance == bestDistance && index == selected_)) {
			best = index;
			bestDistance = distance;
		}
	}
	const bool onHandles = position.y >= area.handles.top;
	return onHandles || bestDistance <= 3.0f ? best : count; // on the bar, only right on a stop's tick
}

double GradientEditor::barPosition(const Layout& area, float x) const noexcept {
	return area.bar.width() > 0.0f ? std::clamp((x - area.bar.left) / area.bar.width(), 0.0f, 1.0f) : 0.0;
}

bool GradientEditor::stopKey(const KeyEvent& event) {
	const std::size_t last = gradient_.stops.size() - 1;
	const bool shift = event.mods.shift();
	switch(event.key) {
	case Key::Left:
	case Key::Right: {
		const int direction = event.key == Key::Right ? 1 : -1;
		if(shift) moveStop(selected_, gradient_.stops[selected_].at + direction * 0.01);
		else selectStop(direction > 0 ? std::min(selected_ + 1, last) : (selected_ == 0 ? 0 : selected_ - 1));
		return true;
	}
	case Key::Up:
	case Key::Down:
		if(!shift) return false;
		moveStop(selected_, gradient_.stops[selected_].at + (event.key == Key::Up ? 0.1 : -0.1));
		return true;
	case Key::Home:
		selectStop(0);
		return true;
	case Key::End:
		selectStop(last);
		return true;
	default:
		return false;
	}
}

void GradientEditor::keyEvent(const KeyEvent& event) {
	if(event.action == Action::Release || stopKey(event)) return Panel::keyEvent(event);
	switch(event.key) {
	case Key::Insert:
	case Key::Equal:
	case Key::KeypadAdd: {
		const auto& stops = gradient_.stops;
		const double at = selected_ + 1 < stops.size() ? (stops[selected_].at + stops[selected_ + 1].at) / 2.0
		                                               : (stops[selected_ - 1].at + stops[selected_].at) / 2.0;
		static_cast<void>(addStop(at)); // full: nothing to add
		return;
	}
	case Key::Delete:
	case Key::Backspace:
		static_cast<void>(removeStop(selected_));
		return;
	case Key::Enter:
	case Key::KeypadEnter:
	case Key::Space:
		openPopover();
		return;
	case Key::K:
		changeKind(gradient_.kind == Gradient::Kind::Linear ? Gradient::Kind::Radial : Gradient::Kind::Linear);
		return;
	case Key::LeftBracket:
		changeAngle(gradient_.angle - 15.0);
		return;
	case Key::RightBracket:
		changeAngle(gradient_.angle + 15.0);
		return;
	default:
		Panel::keyEvent(event);
	}
}

void GradientEditor::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button != MouseButton::Left) return Panel::mouseButtonEvent(event);
	const Layout area = layout();
	if(event.action == Action::Press) {
		closePopover(false);
		pressedAt_ = event.position;
		dragMoved_ = offBar_ = false;
		const std::size_t hit = stopAt(area, event.position);
		pressedStop_ = hit < gradient_.stops.size();
		if(pressedStop_) selectStop(hit);
		else if(area.bar.contains(event.position)) static_cast<void>(addStop(barPosition(area, event.position.x)));
		dragging_ = pressedStop_ || area.bar.contains(event.position);
	} else if(dragging_) {
		dragging_ = false;
		if(offBar_ && removeStop(selected_)) offBar_ = false;
		else if(pressedStop_ && !dragMoved_) openPopover();
		offBar_ = false;
		invalidatePaint();
	}
	Panel::mouseButtonEvent(event);
}

void GradientEditor::mouseMoveEvent(const MouseMoveEvent& event) {
	if(dragging_) {
		const Layout area = layout();
		const Vec2 delta{event.position.x - pressedAt_.x, event.position.y - pressedAt_.y};
		if(!dragMoved_ && std::max(std::abs(delta.x), std::abs(delta.y)) > 2.0f) dragMoved_ = true;
		if(dragMoved_) {
			const float away = std::max(area.bar.top - event.position.y, event.position.y - area.handles.bottom);
			const bool off = away > removeDistance && gradient_.stops.size() > 2;
			if(off != offBar_) {
				offBar_ = off;
				invalidatePaint();
			}
			moveStop(selected_, barPosition(area, event.position.x));
		}
	}
	Panel::mouseMoveEvent(event);
}

void GradientEditor::focusEvent(bool focused) {
	if(!focused && !focusWithin()) closePopover(false);
	invalidatePaint();
	Panel::focusEvent(focused);
}

void GradientEditor::paint(Painter& painter) const {
	Panel::paint(painter);
	const Layout area = layout();
	if(area.preview.width() > 0.0f) {
		const Gradient& gradient = gradient_;
		const Bitmap& preview =
		    previewBitmap_->get(gradientKey(gradient, true), previewPixels, previewPixels, [&gradient](int x, int y) {
			    const double at = gradientPosition(gradient, (x + 0.5) / previewPixels, (y + 0.5) / previewPixels);
			    return detail::over(sampleGradient(gradient.stops, at), detail::checker(x, y, 8));
		    });
		painter.drawImage(preview, area.preview);
	}
	const auto& stops = gradient_.stops;
	const Bitmap& bar =
	    barBitmap_->get(gradientKey(gradient_, false), barPixels, static_cast<int>(barHeight), [&stops](int x, int y) {
		    const Color color = sampleGradient(stops, x / (barPixels - 1.0));
		    return detail::over(color, detail::checker(x, y, 4));
	    });
	painter.drawImage(bar, area.bar);
	const bool focused = hasFocus();
	for(std::size_t index = 0; index < stops.size(); ++index) {
		const Rect handle = handleRect(area, index);
		const bool selected = index == selected_;
		const float fade = selected && offBar_ ? 0.3f : 1.0f;
		const float middle = std::round((handle.left + handle.right) / 2.0f);
		painter.fillRect({middle - 0.5f, area.bar.top, middle + 0.5f, area.bar.bottom}, colors::white.withAlpha(fade));
		painter.fillRoundedRect(handle, CornerRadii::all(2.0f),
		                        detail::over(stops[index].color, colors::white).withAlpha(fade));
		const Color edge = selected && focused ? accent_ : colors::black.withAlpha(0.6f);
		painter.strokeRoundedRect(handle, CornerRadii::all(2.0f), selected ? 2.0f : 1.0f,
		                          edge.withAlpha(edge.a * fade));
	}
}

} // namespace affresco
