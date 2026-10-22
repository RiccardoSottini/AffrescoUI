/// @file
/// @brief ColorPicker's drawing: the plane, the strips, the fields and the swatches, from cached CPU bitmaps, and
/// the font it draws in.

#include "affresco/render/ColorPicker.hpp"
#include "affresco/render/Window.hpp"

#include "CreativePixels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace affresco {

namespace {

constexpr float pi = std::numbers::pi_v<float>; ///< Half a turn, in radians.
constexpr int stripPixels = 128;                ///< The width of a strip's bitmap.
constexpr int planePixels = 128;                ///< The side of the plane's bitmap.

/// @brief Outlines the part the keyboard is on.
/// @param painter Where to draw.
/// @param rect The part.
/// @param color The outline's colour.
void outline(Painter& painter, const Rect& rect, Color color) {
	const Rect around{rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f};
	painter.strokeRoundedRect(around, CornerRadii::all(3.0f), 1.5f, color);
}

/// @brief The colour of a pixel of the hue-saturation wheel at full value.
/// @param x The pixel's column.
/// @param y Its row.
/// @param value The value (brightness) of the wheel.
/// @return The colour, transparent outside the circle.
[[nodiscard]] Color wheelPixel(int x, int y, double value) noexcept {
	const double radius = planePixels / 2.0;
	const double dx = x + 0.5 - radius, dy = y + 0.5 - radius;
	const double distance = std::hypot(dx, dy) / radius;
	if(distance > 1.0) return colors::transparent;
	const double hue = std::fmod(std::atan2(-dy, dx) * 180.0 / pi + 360.0, 360.0);
	const float edge = static_cast<float>(std::clamp((1.0 - distance) * radius, 0.0, 1.0)); // a soft rim
	return detail::fromRgb(color::fromHsv({hue, distance, value}), edge);
}

} // namespace

const Font* ColorPicker::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void ColorPicker::paint(Painter& painter) const {
	Panel::paint(painter);
	const Layout area = layout();
	if(area.plane.width() > 0.0f && area.plane.height() > 0.0f) paintPlane(painter, area);
	paintStrips(painter, area);
	if(showsFields_) paintFields(painter, area);
	if(swatchCount() > 0) paintSwatches(painter, area);
}

void ColorPicker::paintPlane(Painter& painter, const Layout& area) const {
	const Rect& plane = area.plane;
	Vec2 marker{};
	if(mode_ == Mode::Square) {
		const double hue = hsv_.hue;
		const Bitmap& bitmap =
		    planeBitmap_->get(detail::mixKey(detail::firstKey, hue), planePixels, planePixels, [hue](int x, int y) {
			    const double saturation = x / (planePixels - 1.0);
			    const double value = 1.0 - y / (planePixels - 1.0);
			    return detail::fromRgb(color::fromHsv({hue, saturation, value}));
		    });
		painter.drawImage(bitmap, plane);
		marker = {plane.left + static_cast<float>(hsv_.saturation) * plane.width(),
		          plane.top + (1.0f - static_cast<float>(hsv_.value)) * plane.height()};
	} else {
		const double value = hsv_.value;
		const Bitmap& bitmap = planeBitmap_->get(detail::mixKey(detail::firstKey + 1, value), planePixels, planePixels,
		                                         [value](int x, int y) { return wheelPixel(x, y, value); });
		painter.drawImage(bitmap, plane);
		const float radius = plane.width() / 2.0f, angle = static_cast<float>(hsv_.hue) * pi / 180.0f;
		const float reach = static_cast<float>(hsv_.saturation) * radius;
		marker = {plane.left + radius + std::cos(angle) * reach, plane.top + radius - std::sin(angle) * reach};
	}
	detail::drawMarker(painter, marker, 6.0f);
	if(hasFocus() && active_ == Part::Plane) outline(painter, plane, accent_);
}

void ColorPicker::paintStrips(Painter& painter, const Layout& area) const {
	const color::Hsv hsv = hsv_;
	const bool square = mode_ == Mode::Square;
	const std::uint64_t stripKey =
	    square ? detail::firstKey : detail::mixKey(detail::mixKey(detail::firstKey + 2, hsv.hue), hsv.saturation);
	const Bitmap& strip = stripBitmap_->get(stripKey, stripPixels, 1, [hsv, square](int x, int) {
		const double along = x / (stripPixels - 1.0);
		return detail::fromRgb(square ? color::fromHsv({along * 360.0, 1.0, 1.0})
		                              : color::fromHsv({hsv.hue, hsv.saturation, along}));
	});
	painter.drawImage(strip, area.strip);
	const float stripAt = square ? static_cast<float>(hsv.hue / 360.0) : static_cast<float>(hsv.value);
	const float middle = (area.strip.top + area.strip.bottom) / 2.0f;
	detail::drawMarker(painter, {area.strip.left + stripAt * area.strip.width(), middle}, area.strip.height() / 2.0f);
	if(hasFocus() && active_ == Part::Strip) outline(painter, area.strip, accent_);
	const color::Rgb srgb = rgb();
	if(showsAlpha_) {
		const std::uint64_t alphaKey =
		    detail::mixKey(detail::mixKey(detail::mixKey(detail::firstKey + 3, srgb.red), srgb.green), srgb.blue);
		const auto height = static_cast<int>(std::max(1.0f, area.alpha.height()));
		const Bitmap& alpha = alphaBitmap_->get(alphaKey, stripPixels, height, [srgb](int x, int y) {
			const Color top = detail::fromRgb(srgb, static_cast<float>(x / (stripPixels - 1.0)));
			return detail::over(top, detail::checker(x, y, 4));
		});
		painter.drawImage(alpha, area.alpha);
		const float alphaMiddle = (area.alpha.top + area.alpha.bottom) / 2.0f;
		detail::drawMarker(painter, {area.alpha.left + static_cast<float>(alpha_) * area.alpha.width(), alphaMiddle},
		                   area.strip.height() / 2.0f);
		if(hasFocus() && active_ == Part::Alpha) outline(painter, area.alpha, accent_);
	}
	painter.fillRect(area.preview, detail::checker(0, 0, 1));
	painter.fillRect(area.preview, value());
	painter.strokeRoundedRect(area.preview, CornerRadii::all(0.0f), 1.0f, text_.withAlpha(0.3f));
}

void ColorPicker::paintFields(Painter& painter, const Layout& area) const {
	const Font* text = font();
	for(std::size_t index = 0; index < fieldCount; ++index) {
		const Rect& field = area.fields[index];
		const auto part = static_cast<Part>(static_cast<std::size_t>(Part::Hex) + index);
		const bool active = hasFocus() && active_ == part;
		painter.strokeRoundedRect(field, CornerRadii::all(3.0f), active ? 1.5f : 1.0f,
		                          active ? accent_ : text_.withAlpha(0.25f));
		if(!text) continue;
		const std::u32string& shown = typing_ && active_ == part ? editing_ : texts_[index];
		const float baseline = detail::centredBaseline(field.top, field.height(), text->lineHeight(), text->ascent());
		painter.drawText(*text, shown, {field.left + 6.0f, baseline}, text_);
		if(active && typing_) {
			const float caret = field.left + 6.0f + text->measure(shown);
			painter.fillRect({caret, baseline - text->ascent(), caret + 1.0f, baseline + 2.0f}, accent_);
		}
	}
}

void ColorPicker::paintSwatches(Painter& painter, const Layout& area) const {
	for(std::size_t index = 0; index < swatchCount(); ++index) {
		const Rect cell = swatchRect(area, index);
		const Color swatch = swatchAt(index);
		if(swatch.a < 1.0f) painter.fillRect(cell, detail::checker(0, 0, 1));
		painter.fillRoundedRect(cell, CornerRadii::all(3.0f), swatch);
		painter.strokeRoundedRect(cell, CornerRadii::all(3.0f), 1.0f, text_.withAlpha(0.2f));
		if(hasFocus() && active_ == Part::Swatches && index == swatch_) outline(painter, cell, accent_);
	}
}

} // namespace affresco
