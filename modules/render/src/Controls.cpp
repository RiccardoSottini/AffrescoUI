#include "affresco/render/Controls.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

/* Toggle */

Toggle::Toggle(Look look) : look_(look) {
	setColor(colors::transparent);
	setCursor(Cursor::Hand);
}

void Toggle::setChecked(bool checked) {
	checked_ = checked;
	invalidate();
}

void Toggle::setOnColor(Color color) {
	onColor_ = color;
	invalidate();
}

void Toggle::setOffColor(Color color) {
	offColor_ = color;
	invalidate();
}

void Toggle::setKnobColor(Color color) {
	knobColor_ = color;
	invalidate();
}

void Toggle::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button == MouseButton::Left && event.action == Action::Release && bounds().contains(event.position)) {
		checked_ = !checked_;
		invalidate();
		onToggled(checked_);
	}
	Panel::mouseButtonEvent(event);
}

void Toggle::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();

	if(look_ == Look::Checkbox) {
		// A square box at the left, as tall as the panel: a border, then the inside, then the check
		const float side = std::min(f.width(), f.height());
		const Rect box{f.left, f.top + (f.height() - side) / 2.0f, f.left + side, f.top + (f.height() + side) / 2.0f};
		painter.fillRect(box, checked_ ? onColor_ : offColor_);
		painter.fillRect({box.left + 2.0f, box.top + 2.0f, box.right - 2.0f, box.bottom - 2.0f},
		                 checked_ ? onColor_ : knobColor_);
		if(checked_) {
			const float inset = std::round(side * 0.3f);
			painter.fillRect({box.left + inset, box.top + inset, box.right - inset, box.bottom - inset}, knobColor_);
		}
		return;
	}

	painter.fillRect(f, checked_ ? onColor_ : offColor_);
	const float inset = 3.0f, knob = f.height() - 2.0f * inset;
	const float left = checked_ ? f.right - inset - knob : f.left + inset;
	painter.fillRect({left, f.top + inset, left + knob, f.bottom - inset}, knobColor_);
}

/* Slider */

Slider::Slider() {
	setColor(colors::transparent);
	setCursor(Cursor::Hand);
}

void Slider::setValue(double value) {
	value_ = std::clamp(value, std::min(minimum_, maximum_), std::max(minimum_, maximum_));
	invalidate();
}

void Slider::setRange(double minimum, double maximum, double step) {
	minimum_ = minimum;
	maximum_ = maximum;
	step_ = std::max(step, 0.0);
	setValue(value_);
}

void Slider::setColors(Color track, Color fill, Color knob) {
	track_ = track;
	fill_ = fill;
	knob_ = knob;
	invalidate();
}

void Slider::pick(float x) {
	const Rect& f = frame();
	const double t = f.width() > 0.0f ? std::clamp((x - f.left) / f.width(), 0.0f, 1.0f) : 0.0;
	double value = minimum_ + t * (maximum_ - minimum_);
	if(step_ > 0.0) value = minimum_ + std::round((value - minimum_) / step_) * step_;
	value = std::clamp(value, std::min(minimum_, maximum_), std::max(minimum_, maximum_));
	if(value == value_) return;
	value_ = value;
	invalidate();
	onValueChanged(value_);
}

void Slider::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button == MouseButton::Left) {
		dragging_ = event.action == Action::Press;
		if(dragging_) pick(event.position.x);
	}
	Panel::mouseButtonEvent(event);
}

void Slider::mouseMoveEvent(const MouseMoveEvent& event) {
	if(dragging_) pick(event.position.x);
	Panel::mouseMoveEvent(event);
}

void Slider::paint(Painter& painter) const {
	Panel::paint(painter);
	const Rect& f = frame();
	const double span = maximum_ - minimum_;
	const auto t = static_cast<float>(span != 0.0 ? (value_ - minimum_) / span : 0.0);

	// A 4 px track across the middle, filled up to a knob as tall as the panel (at most 16 px)
	const float middle = std::round(f.top + f.height() / 2.0f);
	const float knob = std::min(f.height(), 16.0f);
	const float x = std::round(f.left + t * (f.width() - knob));
	painter.fillRect({f.left, middle - 2.0f, f.right, middle + 2.0f}, track_);
	painter.fillRect({f.left, middle - 2.0f, x + knob / 2.0f, middle + 2.0f}, fill_);
	painter.fillRect({x, middle - knob / 2.0f, x + knob, middle + knob / 2.0f}, fill_);
	painter.fillRect({x + 2.0f, middle - knob / 2.0f + 2.0f, x + knob - 2.0f, middle + knob / 2.0f - 2.0f}, knob_);
}

} // namespace affresco
