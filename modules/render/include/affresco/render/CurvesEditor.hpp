#pragma once

/// @file
/// @brief CurvesEditor: tone curves edited as control points on a 0..1 square, for RGB together or each channel,
/// over an optional histogram; and the curve maths (monotone cubic, linear or Bézier).

#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace affresco {

/// @brief A control point of a curve, both coordinates 0..1.
struct CurvePoint {
	double x = 0.0; ///< The input.
	double y = 0.0; ///< The output.

	/// @brief Equal when both coordinates are equal.
	bool operator==(const CurvePoint&) const noexcept = default;
};

/// @brief How a curve passes through (or near) its points.
enum class CurveInterpolation {
	Monotone, ///< A monotone cubic through the points (Fritsch–Butland): smooth, never overshooting.
	Linear,   ///< Straight lines between the points.
	Bezier,   ///< One Bézier curve with the points as its control polygon (through the ends only).
};

/// @brief The curves a CurvesEditor holds.
enum class CurveChannel {
	Rgb,   ///< All three channels together.
	Red,   ///< Red only.
	Green, ///< Green only.
	Blue,  ///< Blue only.
};

inline constexpr std::size_t maxCurvePoints = 32;     ///< The most points a curve keeps.
inline constexpr std::size_t maxHistogramBins = 1024; ///< The most histogram bins kept.
inline constexpr std::size_t curveChannelCount = 4;   ///< The number of channels.

/// @brief Brings a curve within bounds: coordinates clamped to 0..1, sorted by x, at most maxCurvePoints (the first
/// ones), at least two (the diagonal from (0, 0) to (1, 1) when there were none, a flat line when there was one).
/// @param points The points, changed in place.
void normalizeCurve(std::vector<CurvePoint>& points);

/// @brief A curve's value at an input. Allocates nothing.
/// @param points The points, sorted by x (at most maxCurvePoints are used).
/// @param interpolation How the curve passes through them.
/// @param x The input (clamped to the first and last point's x).
/// @return The output, clamped to 0..1; x itself when there are no points.
[[nodiscard]] double evaluateCurve(std::span<const CurvePoint> points, CurveInterpolation interpolation,
                                   double x) noexcept;

/// @brief A curves editor. Mouse: press on the plot to add a point (or on a point to pick it), drag to move it (out of
/// the plot to remove it), click a channel's tab. Keyboard: Left and Right (Home, End) select a point, Shift+Left and
/// Shift+Right move it across, Up and Down move it up and down (Shift for larger steps), Insert or = adds one, Delete
/// removes it, C switches the channel.
class CurvesEditor : public Panel {
public:
	/// @brief The diagonal on every channel, monotone, RGB only.
	CurvesEditor();
	/// @brief Frees the cached samples.
	~CurvesEditor() override;

	/// @brief Sets a channel's points, without firing onCurveChanged.
	/// @param channel The channel.
	/// @param points The points (normalised, see normalizeCurve()).
	void setCurve(CurveChannel channel, std::vector<CurvePoint> points);
	/// @brief A channel's points.
	/// @param channel The channel.
	[[nodiscard]] std::span<const CurvePoint> curve(CurveChannel channel) const noexcept;
	/// @brief The channel shown and edited.
	[[nodiscard]] CurveChannel channel() const noexcept { return channel_; }
	/// @brief Shows a channel (RGB when the channels are hidden).
	/// @param channel The channel.
	void setChannel(CurveChannel channel);
	/// @brief Shows or hides the channel tabs; hidden, only the RGB curve is edited.
	/// @param shown Whether they show.
	void setShowsChannels(bool shown);
	/// @brief Whether the channel tabs show.
	[[nodiscard]] bool showsChannels() const noexcept { return showsChannels_; }
	/// @brief Sets how the curves pass through their points.
	/// @param interpolation The interpolation.
	void setInterpolation(CurveInterpolation interpolation);
	/// @brief How the curves pass through their points.
	[[nodiscard]] CurveInterpolation interpolation() const noexcept { return interpolation_; }
	/// @brief Sets the histogram drawn behind the curve.
	/// @param bins Counts from dark to light; at most maxHistogramBins are kept, empty for none.
	void setHistogram(std::span<const double> bins);

	/// @brief The point the keyboard acts on, in the shown channel.
	[[nodiscard]] std::size_t selectedPoint() const noexcept { return selected_; }
	/// @brief Selects a point of the shown channel.
	/// @param index The point (clamped to the last one).
	void selectPoint(std::size_t index);
	/// @brief Adds a point to the shown channel, selects it and fires onCurveChanged.
	/// @param x Its input, 0..1.
	/// @param y Its output, 0..1.
	/// @return false when the curve already has maxCurvePoints.
	bool addPoint(double x, double y);
	/// @brief Removes a point of the shown channel and fires onCurveChanged.
	/// @param index The point.
	/// @return false when it doesn't exist or only two points are left.
	bool removePoint(std::size_t index);
	/// @brief Moves a point of the shown channel, its x kept between its neighbours', and fires onCurveChanged.
	/// @param index The point.
	/// @param x Its new input.
	/// @param y Its new output.
	void movePoint(std::size_t index, double x, double y);

	/// @brief Sets the font of the channel tabs.
	/// @param font The font; nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the colours of the grid, the RGB curve and the tab labels, and of the selection.
	/// @param text The grid, the RGB curve and the labels.
	/// @param accent The selected point and tab.
	void setColors(Color text, Color accent);

	Signal<CurveChannel> onCurveChanged; ///< The user changed a channel's curve (not setCurve()).

protected:
	void keyEvent(const KeyEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Where the parts are, in window units.
	struct Layout {
		Rect tabs; ///< The channel tabs; empty when hidden.
		Rect plot; ///< The 0..1 square (stretched to fit).
	};
	/// @brief The curve sampled for drawing, rebuilt when the points or the interpolation change.
	struct Samples {
		std::uint64_t key = 0;     ///< What the samples were built for.
		std::vector<float> values; ///< The outputs at evenly spaced inputs.
	};

	/// @brief Computes where the parts are within frame().
	[[nodiscard]] Layout layout() const noexcept;
	/// @brief The font of the tabs, or nullptr.
	[[nodiscard]] const Font* font() const noexcept;
	/// @brief The shown channel's points.
	[[nodiscard]] std::vector<CurvePoint>& points() noexcept;
	/// @brief The shown channel's points.
	[[nodiscard]] const std::vector<CurvePoint>& points() const noexcept;
	/// @brief A point in window units.
	/// @param area The layout.
	/// @param point The point.
	[[nodiscard]] Vec2 toWindow(const Layout& area, CurvePoint point) const noexcept;
	/// @brief A window position as a curve point (not clamped).
	/// @param area The layout.
	/// @param position In window units.
	[[nodiscard]] CurvePoint fromWindow(const Layout& area, Vec2 position) const noexcept;
	/// @brief The point of the shown channel near a position, or the point count if none.
	/// @param area The layout.
	/// @param position In window units.
	[[nodiscard]] std::size_t pointAt(const Layout& area, Vec2 position) const noexcept;
	/// @brief Notes a change of the shown channel: repaints and fires onCurveChanged.
	void changed();
	/// @brief Handles the keys that select or move a point.
	/// @param event The key.
	/// @return Whether it was handled.
	bool pointKey(const KeyEvent& event);
	/// @brief Draws the grid and the histogram.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintBackground(Painter& painter, const Layout& area) const;
	/// @brief Draws the curve and its points.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintCurve(Painter& painter, const Layout& area) const;
	/// @brief Draws the channel tabs.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintTabs(Painter& painter, const Layout& area) const;

	std::array<std::vector<CurvePoint>, curveChannelCount> curves_;   ///< Each channel's points.
	CurveChannel channel_ = CurveChannel::Rgb;                        ///< See channel().
	bool showsChannels_ = false;                                      ///< See showsChannels().
	CurveInterpolation interpolation_ = CurveInterpolation::Monotone; ///< See interpolation().
	std::vector<float> histogram_;                                    ///< The bins, scaled to 0..1.
	std::size_t selected_ = 0;                                        ///< See selectedPoint().
	std::shared_ptr<const Font> font_;                                ///< See setFont().
	Color text_ = colors::black;                                      ///< See setColors().
	Color accent_ = colors::accent;                                   ///< See setColors().
	bool dragging_ = false;                                           ///< The button is held on a point.
	bool offPlot_ = false;                                            ///< The held point is out of the plot.
	std::unique_ptr<Samples> samples_;                                ///< The shown curve, sampled.
};

} // namespace affresco
