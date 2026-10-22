/// @file
/// @brief ColorPicker: its state, layout, fields, keyboard and pointer (its drawing is in ColorPickerPaint.cpp).

#include "affresco/render/ColorPicker.hpp"
#include "affresco/render/Window.hpp"

#include "image/CreativePixels.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <optional>

namespace affresco {

namespace {

constexpr float partGap = 8.0f;      ///< The space between parts.
constexpr float stripHeight = 12.0f; ///< The height of the hue, value and alpha strips.
constexpr float swatchSide = 16.0f;  ///< The side of a swatch.
constexpr float swatchGap = 4.0f;    ///< The space between swatches.
constexpr std::size_t maxTyped = 32; ///< The longest text a field takes.

/// @brief Whether a part is one of the typed fields.
/// @param part The part.
/// @return Whether it is.
[[nodiscard]] bool isField(ColorPicker::Part part) noexcept {
	return part >= ColorPicker::Part::Hex && part <= ColorPicker::Part::Oklch;
}

/// @brief A field's position among the fields.
/// @param part Hex, Rgb, Hsl, Hsv or Oklch.
/// @return 0..4.
[[nodiscard]] std::size_t fieldIndex(ColorPicker::Part part) noexcept {
	return static_cast<std::size_t>(part) - static_cast<std::size_t>(ColorPicker::Part::Hex);
}

/// @brief Up to four numbers in a text, whatever separates them (commas, spaces, % signs).
struct Numbers {
	std::array<double, 4> values{}; ///< The numbers found.
	std::size_t count = 0;          ///< How many.
};

/// @brief Reads the numbers in a field's text.
/// @param text The text.
/// @return Up to four numbers; count is 5 when there are more (refused).
[[nodiscard]] Numbers numbersIn(std::string_view text) {
	Numbers out;
	const std::string copy(text); // strtod needs a terminated string
	const char* at = copy.c_str();
	while(*at != '\0') {
		const char c = *at;
		if((c < '0' || c > '9') && c != '.' && c != '-' && c != '+') {
			++at;
			continue;
		}
		char* end = nullptr;
		const double value = std::strtod(at, &end);
		if(end == at || !std::isfinite(value)) return {{}, 5};
		if(out.count == out.values.size()) return {{}, 5};
		out.values[out.count++] = value;
		at = end;
	}
	return out;
}

/// @brief A colour typed in a field.
struct Typed {
	color::Rgb rgb;              ///< Its red, green and blue.
	std::optional<double> alpha; ///< Its opacity, when the text gave one.
};

/// @brief Reads a hex colour, with or without its #.
/// @param text The text.
/// @return The colour, or std::nullopt.
[[nodiscard]] std::optional<Typed> parseHex(std::string_view text) {
	while(!text.empty() && text.front() == ' ') text.remove_prefix(1);
	while(!text.empty() && text.back() == ' ') text.remove_suffix(1);
	const std::string hex = text.starts_with('#') ? std::string(text) : "#" + std::string(text);
	const std::optional<Color> parsed = Color::parseHex(hex);
	if(!parsed) return std::nullopt;
	return Typed{detail::toRgb(*parsed), parsed->a};
}

/// @brief Reads a colour typed in a field.
/// @param field Hex, Rgb, Hsl, Hsv or Oklch.
/// @param text The text.
/// @return The colour, or std::nullopt if the text isn't one in that form.
[[nodiscard]] std::optional<Typed> parseField(ColorPicker::Part field, std::string_view text) {
	using Part = ColorPicker::Part;
	if(field == Part::Hex) return parseHex(text);
	const Numbers numbers = numbersIn(text);
	if(numbers.count < 3 || numbers.count > 4) return std::nullopt;
	const auto& n = numbers.values;
	Typed out;
	if(numbers.count == 4) out.alpha = std::clamp(n[3], 0.0, 1.0);
	switch(field) {
	case Part::Rgb:
		out.rgb = color::clampRgb({n[0] / 255.0, n[1] / 255.0, n[2] / 255.0});
		break;
	case Part::Hsl:
		out.rgb = color::fromHsl({n[0], n[1] / 100.0, n[2] / 100.0});
		break;
	case Part::Hsv:
		out.rgb = color::fromHsv({n[0], n[1] / 100.0, n[2] / 100.0});
		break;
	default:
		out.rgb = color::gamutMapOkLch({n[0] > 1.0 ? n[0] / 100.0 : n[0], n[1], n[2]});
		break;
	}
	return out;
}

/// @brief Widens an ASCII text.
/// @param text The text.
/// @return The same characters as UTF-32.
[[nodiscard]] std::u32string widen(std::string_view text) { return {text.begin(), text.end()}; }

/// @brief Whether two colours look the same (every component within half a byte step).
/// @param left One colour.
/// @param right The other.
/// @return Whether they match.
[[nodiscard]] bool sameColor(Color left, Color right) noexcept {
	constexpr float halfStep = 0.5f / 255.0f;
	return std::abs(left.r - right.r) < halfStep && std::abs(left.g - right.g) < halfStep &&
	       std::abs(left.b - right.b) < halfStep && std::abs(left.a - right.a) < halfStep;
}

} // namespace

ColorPicker::ColorPicker()
    : planeBitmap_(std::make_unique<detail::BitmapSlot>()), stripBitmap_(std::make_unique<detail::BitmapSlot>()),
      alphaBitmap_(std::make_unique<detail::BitmapSlot>()) {
	Panel::setColor(colors::transparent);
	setCursor(Cursor::Hand);
	refreshTexts();
}

ColorPicker::~ColorPicker() = default;

color::Rgb ColorPicker::rgb() const noexcept { return color::fromHsv(hsv_); }

Color ColorPicker::value() const noexcept { return detail::fromRgb(rgb(), static_cast<float>(alpha_)); }

void ColorPicker::setValue(Color value) {
	if(sameColor(value, this->value())) return;
	color::Hsv hsv = color::toHsv(detail::toRgb(value));
	if(hsv.saturation <= 0.0 || hsv.value <= 0.0) hsv.hue = hsv_.hue; // a grey has no hue: keep the last one
	if(hsv.value <= 0.0) hsv.saturation = hsv_.saturation;
	hsv_ = hsv;
	alpha_ = showsAlpha_ ? std::clamp(static_cast<double>(value.a), 0.0, 1.0) : 1.0;
	typing_ = false;
	refreshTexts();
	invalidatePaint();
}

void ColorPicker::setMode(Mode mode) { setLook(mode_, mode); }

void ColorPicker::setShowsAlpha(bool shown) {
	setLook(showsAlpha_, shown);
	if(!shown && alpha_ != 1.0) {
		alpha_ = 1.0;
		refreshTexts();
	}
	if(!hasPart(active_)) active_ = Part::Plane;
}

void ColorPicker::setShowsFields(bool shown) {
	setLook(showsFields_, shown);
	if(!hasPart(active_)) active_ = Part::Plane;
}

void ColorPicker::setSwatches(std::span<const Color> swatches) {
	const std::span<const Color> kept = swatches.first(std::min(swatches.size(), maxSwatches));
	if(std::ranges::equal(kept, swatches_)) return;
	swatches_.assign(kept.begin(), kept.end());
	swatch_ = std::min(swatch_, swatchCount() == 0 ? 0 : swatchCount() - 1);
	if(!hasPart(active_)) active_ = Part::Plane; // no swatch left for Enter to pick
	invalidatePaint();
}

void ColorPicker::rememberValue() {
	const Color current = value();
	std::erase_if(recent_, [current](Color color) { return sameColor(color, current); });
	recent_.insert(recent_.begin(), current);
	if(recent_.size() > maxRecent) recent_.resize(maxRecent);
	invalidatePaint();
}

void ColorPicker::setFont(std::shared_ptr<const Font> font) { setLook(font_, std::move(font)); }

void ColorPicker::setColors(Color text, Color accent) {
	setLook(text_, text);
	setLook(accent_, accent);
}

const Font* ColorPicker::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

std::string ColorPicker::fieldText(Part field) const {
	if(!isField(field)) return {};
	const std::u32string& text = typing_ && field == active_ ? editing_ : texts_[fieldIndex(field)];
	std::string out;
	for(const char32_t c : text) out += static_cast<char>(c);
	return out;
}

bool ColorPicker::applyFieldText(Part field, std::string_view text) {
	if(!isField(field)) return false;
	const std::optional<Typed> typed = parseField(field, text);
	if(!typed) return false;
	changeRgb(typed->rgb, showsAlpha_ ? typed->alpha.value_or(alpha_) : 1.0);
	rememberValue();
	return true;
}

void ColorPicker::refreshTexts() {
	const color::Rgb srgb = rgb();
	const color::Hsl hsl = color::toHsl(srgb);
	const color::OkLch lch = color::toOkLch(srgb);
	const Color current = value();
	const auto byte = [](double component) { return static_cast<int>(detail::toByte(component)); };
	texts_[0] = widen(current.a < 1.0f ? current.toHex() : current.withAlpha(1.0f).toHex());
	texts_[1] = widen(std::format("{}, {}, {}", byte(srgb.red), byte(srgb.green), byte(srgb.blue)));
	texts_[2] = widen(std::format("{:.0f}, {:.0f}%, {:.0f}%", hsl.hue, hsl.saturation * 100.0, hsl.lightness * 100.0));
	texts_[3] = widen(std::format("{:.0f}, {:.0f}%, {:.0f}%", hsv_.hue, hsv_.saturation * 100.0, hsv_.value * 100.0));
	texts_[4] = widen(std::format("{:.3f} {:.3f} {:.0f}", lch.lightness, lch.chroma, lch.hue));
}

void ColorPicker::change(color::Hsv hsv, double alpha) {
	hsv.hue = std::clamp(hsv.hue, 0.0, 360.0);
	hsv.saturation = std::clamp(hsv.saturation, 0.0, 1.0);
	hsv.value = std::clamp(hsv.value, 0.0, 1.0);
	alpha = showsAlpha_ ? std::clamp(alpha, 0.0, 1.0) : 1.0;
	if(hsv.hue == hsv_.hue && hsv.saturation == hsv_.saturation && hsv.value == hsv_.value && alpha == alpha_) return;
	hsv_ = hsv;
	alpha_ = alpha;
	refreshTexts();
	invalidatePaint();
	onValueChanged(value());
}

void ColorPicker::changeRgb(color::Rgb srgb, double alpha) {
	color::Hsv hsv = color::toHsv(srgb);
	if(hsv.saturation <= 0.0 || hsv.value <= 0.0) hsv.hue = hsv_.hue;
	if(hsv.value <= 0.0) hsv.saturation = hsv_.saturation;
	change(hsv, alpha);
}

ColorPicker::Layout ColorPicker::layout() const noexcept {
	const Rect& f = frame();
	Layout out;
	float bottom = f.bottom;
	if(swatchCount() > 0) {
		out.perRow =
		    std::max<std::size_t>(1, static_cast<std::size_t>((f.width() + swatchGap) / (swatchSide + swatchGap)));
		const std::size_t rows = (swatchCount() + out.perRow - 1) / out.perRow;
		const float height = static_cast<float>(rows) * (swatchSide + swatchGap) - swatchGap;
		out.swatches = {f.left, bottom - height, f.right, bottom};
		bottom -= height + partGap;
	}
	if(showsFields_) {
		const Font* text = font();
		const float row = text ? std::ceil(text->lineHeight()) + 6.0f : 20.0f;
		for(std::size_t index = fieldCount; index-- > 0;) {
			out.fields[index] = {f.left, bottom - row, f.right, bottom};
			bottom -= row + 2.0f;
		}
		bottom -= partGap - 2.0f;
	}
	const float stripsTop = bottom - (showsAlpha_ ? 2.0f * stripHeight + partGap : stripHeight);
	out.preview = {f.right - (bottom - stripsTop), stripsTop, f.right, bottom};
	out.strip = {f.left, stripsTop, out.preview.left - partGap, stripsTop + stripHeight};
	if(showsAlpha_) out.alpha = {f.left, bottom - stripHeight, out.preview.left - partGap, bottom};
	const float planeBottom = std::max(f.top, stripsTop - partGap);
	out.plane = {f.left, f.top, f.right, planeBottom};
	if(mode_ == Mode::Wheel) {
		const float side = std::min(f.width(), planeBottom - f.top);
		const float left = f.left + (f.width() - side) / 2.0f;
		out.plane = {left, f.top, left + side, f.top + side};
	}
	return out;
}

Color ColorPicker::swatchAt(std::size_t index) const noexcept {
	return index < swatches_.size() ? swatches_[index] : recent_[index - swatches_.size()];
}

Rect ColorPicker::swatchRect(const Layout& area, std::size_t index) const noexcept {
	const float left = area.swatches.left + static_cast<float>(index % area.perRow) * (swatchSide + swatchGap);
	const float top = area.swatches.top + static_cast<float>(index / area.perRow) * (swatchSide + swatchGap);
	return {left, top, left + swatchSide, top + swatchSide};
}

bool ColorPicker::hasPart(Part part) const noexcept {
	if(part == Part::Alpha) return showsAlpha_;
	if(isField(part)) return showsFields_;
	if(part == Part::Swatches) return swatchCount() > 0;
	return true;
}

bool ColorPicker::stepPart(bool forward) {
	auto index = static_cast<int>(active_);
	const int last = static_cast<int>(Part::Swatches);
	for(index += forward ? 1 : -1; index >= 0 && index <= last; index += forward ? 1 : -1) {
		if(hasPart(static_cast<Part>(index))) {
			active_ = static_cast<Part>(index);
			return true;
		}
	}
	active_ = Part::Plane; // leaving: come back at the first part
	return false;
}

void ColorPicker::pick(Part part, Vec2 position) {
	const Layout area = layout();
	const auto along = [position](const Rect& rect) {
		return rect.width() > 0.0f ? std::clamp((position.x - rect.left) / rect.width(), 0.0f, 1.0f) : 0.0f;
	};
	color::Hsv hsv = hsv_;
	double alpha = alpha_;
	if(part == Part::Plane && mode_ == Mode::Square) {
		const Rect& plane = area.plane;
		hsv.saturation = along(plane);
		hsv.value = plane.height() > 0.0f ? 1.0f - std::clamp((position.y - plane.top) / plane.height(), 0.0f, 1.0f)
		                                  : hsv.value;
	} else if(part == Part::Plane) {
		const float radius = area.plane.width() / 2.0f;
		const float dx = position.x - (area.plane.left + radius), dy = position.y - (area.plane.top + radius);
		if(dx != 0.0f || dy != 0.0f) hsv.hue = std::fmod(std::atan2(-dy, dx) * 180.0 / 3.14159265358979 + 360.0, 360.0);
		hsv.saturation = radius > 0.0f ? std::min(1.0f, std::hypot(dx, dy) / radius) : 0.0f;
	} else if(part == Part::Strip && mode_ == Mode::Square) {
		hsv.hue = along(area.strip) * 360.0;
	} else if(part == Part::Strip) {
		hsv.value = along(area.strip);
	} else if(part == Part::Alpha) {
		alpha = along(area.alpha);
	}
	change(hsv, alpha);
}

bool ColorPicker::arrowKey(const KeyEvent& event) {
	const int dx = event.key == Key::Right ? 1 : event.key == Key::Left ? -1 : 0;
	const int dy = event.key == Key::Up ? 1 : event.key == Key::Down ? -1 : 0;
	if(active_ == Part::Swatches) {
		if(event.key == Key::Enter || event.key == Key::KeypadEnter || event.key == Key::Space) {
			changeRgb(detail::toRgb(swatchAt(swatch_)), swatchAt(swatch_).a);
			rememberValue();
			return true;
		}
		const auto perRow = static_cast<long>(layout().perRow);
		const long moved = static_cast<long>(swatch_) + dx - dy * perRow;
		if((dx == 0 && dy == 0) || moved < 0 || moved >= static_cast<long>(swatchCount())) return dx != 0 || dy != 0;
		swatch_ = static_cast<std::size_t>(moved);
		invalidatePaint();
		return true;
	}
	if(dx == 0 && dy == 0) return false;
	const double step = event.mods.shift() ? 0.1 : 0.01, degrees = event.mods.shift() ? 10.0 : 1.0;
	color::Hsv hsv = hsv_;
	double alpha = alpha_;
	if(active_ == Part::Plane && mode_ == Mode::Square) {
		hsv.saturation += dx * step;
		hsv.value += dy * step;
	} else if(active_ == Part::Plane) {
		hsv.hue = std::fmod(hsv.hue + dx * degrees + 360.0, 360.0);
		hsv.saturation += dy * step;
	} else if(active_ == Part::Strip && mode_ == Mode::Square) {
		hsv.hue += (dx + dy) * degrees;
	} else if(active_ == Part::Strip) {
		hsv.value += (dx + dy) * step;
	} else {
		alpha += (dx + dy) * step;
	}
	change(hsv, alpha);
	return true;
}

bool ColorPicker::fieldKey(const KeyEvent& event) {
	switch(event.key) {
	case Key::Enter:
	case Key::KeypadEnter:
		commitField();
		return true;
	case Key::Escape:
		if(!typing_) return false;
		typing_ = false;
		editing_.clear();
		invalidatePaint();
		return true;
	case Key::Backspace:
		if(!typing_) editing_ = texts_[fieldIndex(active_)];
		typing_ = true;
		if(!editing_.empty()) editing_.pop_back();
		invalidatePaint();
		return true;
	default:
		return false;
	}
}

void ColorPicker::commitField() {
	if(!typing_ || !isField(active_)) return;
	typing_ = false;
	std::string text;
	for(const char32_t c : editing_) text += static_cast<char>(c);
	editing_.clear();
	static_cast<void>(applyFieldText(active_, text)); // a text that isn't a colour is dropped
	invalidatePaint();
}

void ColorPicker::keyEvent(const KeyEvent& event) {
	if(event.action == Action::Release) return Panel::keyEvent(event);
	if(event.key == Key::Tab && !event.mods.control()) {
		commitField();
		if(stepPart(!event.mods.shift())) {
			invalidatePaint();
			return;
		}
		invalidatePaint();
		return Panel::keyEvent(event); // past the last part: the focus moves on
	}
	if(isField(active_) ? fieldKey(event) : arrowKey(event)) return;
	Panel::keyEvent(event);
}

void ColorPicker::charEvent(const CharEvent& event) {
	if(!isField(active_) || event.codepoint < 0x20 || event.codepoint > 0x7e) return Panel::charEvent(event);
	if(!typing_) editing_.clear();
	typing_ = true;
	if(editing_.size() < maxTyped) editing_.push_back(event.codepoint);
	invalidatePaint();
}

void ColorPicker::pickFromWindow(Vec2 position) {
	const Panel* hit = window() ? window()->panelAt(position) : nullptr;
	while(hit && hit->color().a <= 0.0f) hit = hit->parent();
	if(!hit || hit == this) return;
	changeRgb(detail::toRgb(hit->color()), alpha_);
	rememberValue();
}

void ColorPicker::mouseButtonEvent(const MouseButtonEvent& event) {
	if(event.button != MouseButton::Left) return Panel::mouseButtonEvent(event);
	if(event.action == Action::Release) {
		if(dragging_) rememberValue();
		if(eyedropping_ && !layout().preview.contains(event.position)) pickFromWindow(event.position);
		dragging_ = eyedropping_ = false;
		return Panel::mouseButtonEvent(event);
	}
	commitField();
	const Layout area = layout();
	const Vec2 at = event.position;
	for(std::size_t index = 0; showsFields_ && index < fieldCount; ++index) {
		if(area.fields[index].contains(at)) active_ = static_cast<Part>(static_cast<std::size_t>(Part::Hex) + index);
	}
	for(std::size_t index = 0; index < swatchCount(); ++index) {
		if(!swatchRect(area, index).contains(at)) continue;
		active_ = Part::Swatches;
		swatch_ = index;
		changeRgb(detail::toRgb(swatchAt(index)), swatchAt(index).a);
		rememberValue();
	}
	eyedropping_ = area.preview.contains(at);
	for(const Part part : {Part::Plane, Part::Strip, Part::Alpha}) {
		const Rect& rect = part == Part::Plane ? area.plane : part == Part::Strip ? area.strip : area.alpha;
		if(!hasPart(part) || !rect.contains(at)) continue;
		active_ = dragPart_ = part;
		dragging_ = true;
		pick(part, at);
	}
	invalidatePaint();
	Panel::mouseButtonEvent(event);
}

void ColorPicker::mouseMoveEvent(const MouseMoveEvent& event) {
	if(dragging_) pick(dragPart_, event.position);
	Panel::mouseMoveEvent(event);
}

void ColorPicker::focusEvent(bool focused) {
	if(!focused) commitField();
	invalidatePaint();
	Panel::focusEvent(focused);
}

} // namespace affresco
