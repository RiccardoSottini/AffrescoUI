#pragma once

/// @file
/// @brief GradientEditor: a linear or radial gradient edited as colour stops on a bar, with a preview, and each stop's
/// colour chosen in a ColorPicker popover.

#include "affresco/render/Color.hpp"
#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace affresco {

class ColorPicker;

namespace detail {
class BitmapSlot;
} // namespace detail

/// @brief One colour stop of a gradient.
struct GradientStop {
	double at = 0.0; ///< Where it is along the gradient, 0..1.
	Color color;     ///< Its colour (straight alpha).

	/// @brief Equal when both members are equal.
	bool operator==(const GradientStop&) const noexcept = default;
};

/// @brief The most stops a gradient keeps.
inline constexpr std::size_t maxGradientStops = 32;

/// @brief A gradient: its kind, its angle and its stops, sorted by position.
struct Gradient {
	/// @brief Its shape.
	enum class Kind {
		Linear, ///< Along a line through the centre, at angle().
		Radial, ///< Outwards from the centre to the farthest corner.
	};

	Kind kind = Kind::Linear;          ///< Its shape.
	double angle = 90.0;               ///< The direction of a linear gradient, in degrees (0 up, 90 right, like CSS).
	std::vector<GradientStop> stops{}; ///< The stops, sorted by at (see normalizeGradient()).

	/// @brief Equal when every member is equal.
	bool operator==(const Gradient&) const = default;
};

/// @brief Brings a gradient within bounds: positions clamped to 0..1 and sorted, at most maxGradientStops (the first
/// ones), at least two (black to white when there were none, the one colour twice when there was one), the angle
/// wrapped to 0..360.
/// @param gradient The gradient, changed in place.
void normalizeGradient(Gradient& gradient);

/// @brief The colour of a gradient at a position, mixing the stops on each side in sRGB (as CSS does).
/// @param stops The stops, sorted by position.
/// @param at The position, 0..1.
/// @return The colour; transparent when there are no stops.
[[nodiscard]] Color sampleGradient(std::span<const GradientStop> stops, double at) noexcept;

/// @brief The position along a gradient of a point of a square it fills.
/// @param gradient Its kind and angle.
/// @param x The point across, 0..1.
/// @param y The point down, 0..1.
/// @return The position, 0..1.
[[nodiscard]] double gradientPosition(const Gradient& gradient, double x, double y) noexcept;

/// @brief A gradient editor. Mouse: click the bar to add a stop, drag a stop to move it (off the bar to remove it),
/// click a stop to edit its colour. Keyboard: Left and Right (Home, End) select a stop, Shift with the arrows moves it,
/// Insert or = adds one, Delete removes it, Enter or Space edits its colour (Escape closes the popover), K switches
/// linear and radial, [ and ] turn the angle.
class GradientEditor : public Panel {
public:
	/// @brief A black-to-white linear gradient.
	GradientEditor();
	/// @brief Frees the cached bitmaps.
	~GradientEditor() override;

	/// @brief Sets the gradient, without firing onGradientChanged.
	/// @param gradient The gradient (normalised, see normalizeGradient()).
	void setGradient(Gradient gradient);
	/// @brief The gradient.
	[[nodiscard]] const Gradient& gradient() const noexcept { return gradient_; }

	/// @brief The stop the keyboard acts on.
	[[nodiscard]] std::size_t selectedStop() const noexcept { return selected_; }
	/// @brief Selects a stop.
	/// @param index The stop (clamped to the last one).
	void selectStop(std::size_t index);
	/// @brief Adds a stop with the gradient's colour there, selects it and fires onGradientChanged.
	/// @param at Its position, 0..1.
	/// @return false when the gradient already has maxGradientStops.
	bool addStop(double at);
	/// @brief Removes a stop and fires onGradientChanged.
	/// @param index The stop.
	/// @return false when it doesn't exist or only two stops are left.
	bool removeStop(std::size_t index);
	/// @brief Moves a stop, keeping the stops sorted (the selection follows it), and fires onGradientChanged.
	/// @param index The stop.
	/// @param at Its new position (clamped to 0..1).
	/// @return Its index after sorting.
	std::size_t moveStop(std::size_t index, double at);
	/// @brief Changes a stop's colour and fires onGradientChanged.
	/// @param index The stop.
	/// @param color Its new colour.
	void setStopColor(std::size_t index, Color color);
	/// @brief Changes the kind and fires onGradientChanged.
	/// @param kind The kind.
	void changeKind(Gradient::Kind kind);
	/// @brief Changes the angle and fires onGradientChanged.
	/// @param angle In degrees (wrapped to 0..360).
	void changeAngle(double angle);

	/// @brief Opens the colour popover for the selected stop and focuses it.
	void openPopover();
	/// @brief Closes the colour popover.
	/// @param refocus Whether to give the focus back to the editor.
	void closePopover(bool refocus);
	/// @brief Whether the colour popover is open.
	[[nodiscard]] bool isPopoverOpen() const noexcept;
	/// @brief The popover's picker, to style it (its font and colours).
	[[nodiscard]] ColorPicker& picker() noexcept { return *picker_; }

	/// @brief Sets the colour of the selected stop's outline.
	/// @param accent The colour.
	void setAccent(Color accent);

	Signal<const Gradient&> onGradientChanged; ///< The user changed the gradient (not setGradient()).

protected:
	void keyEvent(const KeyEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Where the parts are, in window units.
	struct Layout {
		Rect preview; ///< The gradient filling a square; empty when the editor is too short.
		Rect bar;     ///< The stops' bar, left to right 0..1.
		Rect handles; ///< The row of stop handles under the bar.
	};

	/// @brief Computes where the parts are within frame().
	[[nodiscard]] Layout layout() const noexcept;
	/// @brief A stop's handle.
	/// @param area The layout.
	/// @param index The stop.
	[[nodiscard]] Rect handleRect(const Layout& area, std::size_t index) const noexcept;
	/// @brief The stop whose handle (or bar tick) is under a point, or the stop count if none.
	/// @param area The layout.
	/// @param position In window units.
	[[nodiscard]] std::size_t stopAt(const Layout& area, Vec2 position) const noexcept;
	/// @brief The position along the bar of a point.
	/// @param area The layout.
	/// @param x In window units.
	[[nodiscard]] double barPosition(const Layout& area, float x) const noexcept;
	/// @brief Notes a change: repaints and fires onGradientChanged.
	void changed();
	/// @brief Handles the keys that select or move a stop.
	/// @param event The key.
	/// @return Whether it was handled.
	bool stopKey(const KeyEvent& event);
	/// @brief Whether the focused panel is this editor or within it (its popover).
	[[nodiscard]] bool focusWithin() const noexcept;

	Gradient gradient_;                                 ///< See gradient().
	std::size_t selected_ = 0;                          ///< See selectedStop().
	Color accent_ = colors::accent;                     ///< See setAccent().
	bool dragging_ = false;                             ///< The button is held on a stop.
	bool dragMoved_ = false;                            ///< The held stop has moved.
	bool pressedStop_ = false;                          ///< The press was on a stop that already existed.
	bool offBar_ = false;                               ///< The held stop is dragged off the bar (removed on release).
	Vec2 pressedAt_{};                                  ///< Where the button was pressed.
	Panel* popover_ = nullptr;                          ///< The popover (a child overlay), hidden when closed.
	ColorPicker* picker_ = nullptr;                     ///< The popover's picker.
	std::unique_ptr<detail::BitmapSlot> previewBitmap_; ///< The preview, cached.
	std::unique_ptr<detail::BitmapSlot> barBitmap_;     ///< The bar, cached.
};

} // namespace affresco
