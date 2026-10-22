#pragma once

/// @file
/// @brief Value widgets: Toggle (a switch or a checkbox) and Slider.

#include "affresco/render/Panel.hpp"

namespace affresco {

/// @brief An on/off control: a click flips it. Drawn as a switch (a track with a knob) or as a checkbox.
class Toggle : public Panel {
public:
	/// @brief How it looks.
	enum class Look {
		Switch,   ///< A track with a knob that moves to the right when on.
		Checkbox, ///< A box, filled with a check when on.
	};

	/// @brief An off toggle with a transparent background and a hand cursor.
	/// @param look How it looks.
	explicit Toggle(Look look = Look::Switch);

	/// @brief Sets the value, without firing onToggled.
	/// @param checked On or off.
	void setChecked(bool checked);
	/// @brief The value.
	[[nodiscard]] bool isChecked() const noexcept { return checked_; }

	/// @brief Sets the colour of the track or the check when on.
	/// @param color The colour.
	void setOnColor(Color color);
	/// @brief Sets the colour of the track or the box when off.
	/// @param color The colour.
	void setOffColor(Color color);
	/// @brief Sets the colour of the knob, or of the box's inside.
	/// @param color The colour.
	void setKnobColor(Color color);

	Signal<bool> onToggled; ///< The user flipped it (not setChecked()).

protected:
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void paint(Painter& painter) const override;

private:
	Look look_;                       ///< See Look.
	bool checked_ = false;            ///< See isChecked().
	Color onColor_ = colors::accent;  ///< See setOnColor().
	Color offColor_ = colors::idle;   ///< See setOffColor().
	Color knobColor_ = colors::white; ///< See setKnobColor().
};

/// @brief A value in a range, set by clicking or dragging along a track.
class Slider : public Panel {
public:
	/// @brief A slider from 0 to 100 at 0, with a transparent background and a hand cursor.
	Slider();

	/// @brief Sets the value, without firing onValueChanged.
	/// @param value Clamped to the range.
	void setValue(double value);
	/// @brief The value.
	[[nodiscard]] double value() const noexcept { return value_; }
	/// @brief The value at the left end.
	[[nodiscard]] double minimum() const noexcept { return minimum_; }
	/// @brief The value at the right end.
	[[nodiscard]] double maximum() const noexcept { return maximum_; }
	/// @brief The step values snap to; 0 for none.
	[[nodiscard]] double step() const noexcept { return step_; }

	/// @brief Sets the range and the step.
	/// @param minimum The value at the left end.
	/// @param maximum The value at the right end.
	/// @param step Values snap to multiples of it from the minimum; 0 for none.
	void setRange(double minimum, double maximum, double step);

	/// @brief Sets the colours.
	/// @param track The empty part of the track.
	/// @param fill The part before the knob.
	/// @param knob The knob.
	void setColors(Color track, Color fill, Color knob);

	Signal<double> onValueChanged; ///< The user moved it (not setValue()).

protected:
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Sets the value from a pointer position and fires onValueChanged if it changed.
	/// @param x The pointer's x, in window units.
	void pick(float x);

	double value_ = 0.0, minimum_ = 0.0, maximum_ = 100.0, step_ = 0.0; ///< See setValue() and setRange().
	bool dragging_ = false;                                             ///< The button is held on it.
	Color track_ = colors::track;                                       ///< See setColors().
	Color fill_ = colors::accent;                                       ///< See setColors().
	Color knob_ = colors::white;                                        ///< See setColors().
};

} // namespace affresco
