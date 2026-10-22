#pragma once

/// @file
/// @brief The rulers of a canvas: ticks in world units that follow the zoom, labelled, on the top and left edges.

#include <affresco/render/Camera2D.hpp>
#include <affresco/render/Color.hpp>
#include <affresco/render/Geometry.hpp>

#include <array>
#include <string_view>
#include <vector>

namespace affresco {

class Font;
class Painter;

/// @brief One tick of a ruler.
struct RulerTick {
	float screen = 0.0f; ///< Where it is along the ruler, in screen units from the view's edge.
	double world = 0.0;  ///< The world coordinate it marks.
	bool major = false;  ///< A labelled tick.
};

/// @brief How rulers look.
struct RulerStyle {
	float thickness = 20.0f;                 ///< The rulers' width, in screen units.
	Color background = Color::hex(0xf3f4f6); ///< Their fill.
	Color tick = Color::hex(0x9ca3af);       ///< Ticks and the edge line.
	Color text = Color::hex(0x4b5563);       ///< Labels.
};

/// @brief The world distance between labelled ticks: 1, 2 or 5 times a power of ten, the smallest that leaves at
/// least a spacing on screen.
/// @param zoom Screen units per world unit (positive).
/// @param minimumSpacing Screen units between labelled ticks, at least.
/// @return The step, in world units.
[[nodiscard]] double rulerStep(float zoom, float minimumSpacing) noexcept;

/// @brief The ticks of a ruler along one axis.
/// @param worldStart The world coordinate at the ruler's start (the camera's x or y).
/// @param zoom The camera's zoom.
/// @param length The ruler's length, in screen units.
/// @param ticks Cleared, then filled in order; its storage is reused, so steady redraws allocate nothing.
void rulerTicks(double worldStart, float zoom, float length, std::vector<RulerTick>& ticks);

/// @brief A tick's label: the number with the decimals its step needs and no more.
/// @param value The world coordinate.
/// @param step The step between labelled ticks.
/// @param buffer Where the characters go.
/// @return The label, a view into buffer.
[[nodiscard]] std::u32string_view rulerLabel(double value, double step, std::array<char32_t, 32>& buffer) noexcept;

/// @brief Draws both rulers over a view: the top one, the left one, and the square where they meet.
/// @param painter The painter.
/// @param font The labels' font; nullptr draws ticks only.
/// @param frame The view's frame, in window units.
/// @param camera The view's camera.
/// @param style How they look.
/// @param ticks Scratch storage for the ticks.
void paintRulers(Painter& painter, const Font* font, const Rect& frame, const Camera2D& camera, const RulerStyle& style,
                 std::vector<RulerTick>& ticks);

} // namespace affresco
