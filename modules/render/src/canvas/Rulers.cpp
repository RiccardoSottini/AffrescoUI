/// @file
/// @brief The rulers of a canvas: choosing the step, the ticks and their labels, and drawing them.

#include "canvas/Rulers.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/Painter.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace affresco {
namespace {

constexpr float labelSpacing = 64.0f;      ///< Screen units between labelled ticks, at least.
constexpr std::size_t maximumTicks = 4096; ///< Ticks a ruler draws at most, whatever its length.

/// @brief Draws the ticks and labels of one ruler.
/// @param pen The painter, the labels' font (or nullptr) and the style.
/// @param ruler The ruler's rectangle, in window units.
/// @param horizontal The top ruler (ticks along x) or the left one.
/// @param step The labelled ticks' step.
/// @param ticks The ticks.
void paintRuler(const RulerPen& pen, const Rect& ruler, bool horizontal, double step,
                const std::vector<RulerTick>& ticks) {
	Painter& painter = pen.painter;
	const Font* font = pen.font;
	const RulerStyle& style = pen.style;
	painter.fillRect(ruler, style.background);
	const float thickness = horizontal ? ruler.height() : ruler.width();
	std::array<char32_t, 32> buffer{};
	for(const RulerTick& tick : ticks) {
		const float length = tick.major ? thickness : thickness / 3.0f;
		const float at = std::round((horizontal ? ruler.left : ruler.top) + tick.screen);
		painter.fillRect(horizontal ? Rect{at, ruler.bottom - length, at + 1.0f, ruler.bottom}
		                            : Rect{ruler.right - length, at, ruler.right, at + 1.0f},
		                 style.tick);
		if(!tick.major || !font) continue;
		const std::u32string_view label = rulerLabel(tick.world, step, buffer);
		if(horizontal) {
			painter.drawText(*font, label, {at + 3.0f, ruler.top + font->ascent() + 1.0f}, style.text);
			continue;
		}
		// Down the left ruler, one character a line, so that long numbers fit its width
		for(std::size_t i = 0; i < label.size(); ++i)
			painter.drawText(
			    *font, label.substr(i, 1),
			    {ruler.left + 2.0f, at + 2.0f + font->ascent() + font->lineHeight() * 0.8f * static_cast<float>(i)},
			    style.text);
	}
	painter.fillRect(horizontal ? Rect{ruler.left, ruler.bottom - 1.0f, ruler.right, ruler.bottom}
	                            : Rect{ruler.right - 1.0f, ruler.top, ruler.right, ruler.bottom},
	                 style.tick);
}

} // namespace

double rulerStep(float zoom, float minimumSpacing) noexcept {
	const double worldSpacing = std::max(double{minimumSpacing}, 1.0) / std::max(double{zoom}, 1e-6);
	const double power = std::pow(10.0, std::floor(std::log10(worldSpacing)));
	for(const double factor : {1.0, 2.0, 5.0, 10.0})
		if(power * factor >= worldSpacing) return power * factor;
	return power * 10.0;
}

void rulerTicks(double worldStart, float zoom, float length, std::vector<RulerTick>& ticks) {
	ticks.clear();
	if(!(zoom > 0.0f) || !(length > 0.0f) || !std::isfinite(worldStart)) return;
	const double step = rulerStep(zoom, labelSpacing);
	// Five minor ticks a step for 1 and 5, four for 2, so that each lands on a round number
	const double leading = step / std::pow(10.0, std::floor(std::log10(step)));
	const int minors = std::abs(leading - 2.0) < 0.5 ? 4 : 5;
	const double minor = step / minors;
	const double first = std::floor(worldStart / minor);
	if(!(std::abs(first) < 1e15)) return; // too far out for whole ticks
	auto index = static_cast<long long>(first);
	for(std::size_t count = 0; count < maximumTicks; ++count, ++index) {
		const double world = static_cast<double>(index) * minor;
		const auto screen = static_cast<float>((world - worldStart) * zoom);
		if(screen > length) break;
		if(screen < 0.0f) continue;
		ticks.push_back({screen, world, index % minors == 0});
	}
}

std::u32string_view rulerLabel(double value, double step, std::array<char32_t, 32>& buffer) noexcept {
	const int decimals = std::clamp(static_cast<int>(-std::floor(std::log10(step))), 0, 6);
	if(std::abs(value) < step / 1e6) value = 0.0; // no "-0"
	std::array<char, 32> text{};
	const auto [end, error] =
	    std::to_chars(text.data(), text.data() + text.size(), value, std::chars_format::fixed, decimals);
	if(error != std::errc{}) return {};
	const auto length = static_cast<std::size_t>(end - text.data());
	for(std::size_t i = 0; i < length; ++i) buffer[i] = static_cast<char32_t>(text[i]);
	return {buffer.data(), length};
}

void paintRulers(const RulerPen& pen, const Rect& frame, const Camera2D& camera, std::vector<RulerTick>& ticks) {
	const float thickness = std::min({pen.style.thickness, frame.width(), frame.height()});
	if(!(thickness > 0.0f)) return;
	const double step = rulerStep(camera.zoom, labelSpacing);
	rulerTicks(camera.x + double{thickness} / camera.zoom, camera.zoom, frame.width() - thickness, ticks);
	for(RulerTick& tick : ticks) tick.screen += thickness;
	paintRuler(pen, {frame.left, frame.top, frame.right, frame.top + thickness}, true, step, ticks);
	rulerTicks(camera.y + double{thickness} / camera.zoom, camera.zoom, frame.height() - thickness, ticks);
	for(RulerTick& tick : ticks) tick.screen += thickness;
	paintRuler(pen, {frame.left, frame.top, frame.left + thickness, frame.bottom}, false, step, ticks);
	pen.painter.fillRect({frame.left, frame.top, frame.left + thickness, frame.top + thickness}, pen.style.background);
}

} // namespace affresco
