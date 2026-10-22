/// @file
/// @brief Adjustments and gradients: the formulas, their tables, and the hue/saturation GLSL.

#include "affresco/imaging/Adjustment.hpp"

#include "Luminosity.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace affresco::imaging {

namespace {

/// @brief The curve's output at an input: monotone cubic (Fritsch-Carlson) through its points.
/// @param points The points, sorted, at least two.
/// @param input 0 to 1.
[[nodiscard]] float curveAt(const std::vector<std::pair<float, float>>& points, float input) noexcept {
	if(points.empty()) return input;
	if(input <= points.front().first) return points.front().second;
	if(input >= points.back().first) return points.back().second;
	std::size_t index = 1;
	while(points[index].first < input) ++index;
	const auto slope = [&](std::size_t at) {
		const auto secant = [&](std::size_t from) {
			const float width = points[from + 1].first - points[from].first;
			return width > 0.0f ? (points[from + 1].second - points[from].second) / width : 0.0f;
		};
		if(at == 0) return secant(0);
		if(at + 1 == points.size()) return secant(at - 1);
		const float left = secant(at - 1), right = secant(at);
		return left * right <= 0.0f ? 0.0f : 2.0f / (1.0f / left + 1.0f / right);
	};
	const auto& [x0, y0] = points[index - 1];
	const auto& [x1, y1] = points[index];
	const float width = x1 - x0;
	if(width <= 0.0f) return y1;
	const float t = (input - x0) / width, t2 = t * t, t3 = t2 * t;
	return clamped((2 * t3 - 3 * t2 + 1) * y0 + (t3 - 2 * t2 + t) * width * slope(index - 1) + (-2 * t3 + 3 * t2) * y1 +
	               (t3 - t2) * width * slope(index));
}

/// @brief One channel through a per-channel adjustment.
/// @param adjustment The adjustment (levels, curves, colorBalance, brightnessContrast or invert).
/// @param channel 0 red, 1 green, 2 blue.
/// @param value 0 to 1.
[[nodiscard]] float adjustChannel(const Adjustment& adjustment, std::size_t channel, float value) noexcept {
	switch(adjustment.kind) {
	case AdjustmentKind::levels: {
		const float range = std::max(adjustment.inputWhite - adjustment.inputBlack, 1e-4f);
		const float normal = std::pow(clamped((value - adjustment.inputBlack) / range), 1.0f / adjustment.gamma);
		return adjustment.outputBlack + normal * (adjustment.outputWhite - adjustment.outputBlack);
	}
	case AdjustmentKind::curves:
		return curveAt(adjustment.curve, value);
	case AdjustmentKind::colorBalance: // midtones: strongest at 0.5, none at black and white
		return clamped(value + adjustment.balance[channel] * 0.5f * 4.0f * value * (1.0f - value));
	case AdjustmentKind::brightnessContrast: {
		const float factor = adjustment.contrast >= 0.0f ? 1.0f / std::max(1.0f - adjustment.contrast, 0.01f)
		                                                 : 1.0f + adjustment.contrast;
		return clamped((value + adjustment.brightness * 0.5f - 0.5f) * factor + 0.5f);
	}
	case AdjustmentKind::invert:
		return 1.0f - value;
	default:
		return value;
	}
}

/// @brief A colour turned in hue, saturated and lightened (the hue/saturation adjustment).
/// @param adjustment The adjustment.
/// @param pixel The colour.
[[nodiscard]] Pixel hueSaturation(const Adjustment& adjustment, const Pixel& pixel) noexcept {
	// Turning the hue: a rotation about the grey axis
	const float angle = adjustment.hue * std::numbers::pi_v<float> / 180.0f;
	const float cosine = std::cos(angle), sine = std::sin(angle), third = 1.0f / 3.0f, root = std::sqrt(third);
	const float a = cosine + (1.0f - cosine) * third, b = third * (1.0f - cosine) - root * sine,
	            c = third * (1.0f - cosine) + root * sine;
	Pixel turned{a * pixel.red + b * pixel.green + c * pixel.blue, c * pixel.red + a * pixel.green + b * pixel.blue,
	             b * pixel.red + c * pixel.green + a * pixel.blue, pixel.alpha};
	const float grey = detail::luminosityOf(turned), scale = 1.0f + adjustment.saturation;
	for(float* channel : {&turned.red, &turned.green, &turned.blue}) {
		*channel = grey + (*channel - grey) * scale;
		*channel = adjustment.lightness >= 0.0f ? *channel + (1.0f - *channel) * adjustment.lightness
		                                        : *channel * (1.0f + adjustment.lightness);
		*channel = clamped(*channel);
	}
	return turned;
}

} // namespace

Pixel Gradient::at(float position) const noexcept {
	if(stops.empty()) return {};
	if(position <= stops.front().position) return stops.front().color;
	if(position >= stops.back().position) return stops.back().color;
	std::size_t index = 1;
	while(stops[index].position < position) ++index;
	const GradientStop& low = stops[index - 1];
	const GradientStop& high = stops[index];
	const float span = high.position - low.position;
	const float t = span > 0.0f ? (position - low.position) / span : 1.0f;
	const auto mix = [t](float from, float to) { return from + (to - from) * t; };
	return {mix(low.color.red, high.color.red), mix(low.color.green, high.color.green),
	        mix(low.color.blue, high.color.blue), mix(low.color.alpha, high.color.alpha)};
}

void Gradient::normalize() {
	if(stops.size() > maxStops) stops.resize(maxStops);
	for(GradientStop& stop : stops) {
		stop.position = clamped(stop.position);
		stop.color = {clamped(stop.color.red), clamped(stop.color.green), clamped(stop.color.blue),
		              clamped(stop.color.alpha)};
	}
	std::ranges::stable_sort(
	    stops, [](const GradientStop& left, const GradientStop& right) { return left.position < right.position; });
	if(stops.empty()) stops.push_back({0.0f, {0.0f, 0.0f, 0.0f, 1.0f}});
}

const std::array<std::string_view, adjustmentKindCount>& adjustmentKindNames() noexcept {
	static constexpr std::array<std::string_view, adjustmentKindCount> names{
	    "levels", "curves",    "hue-saturation", "color-balance", "brightness-contrast",
	    "invert", "threshold", "gradient-map"};
	return names;
}

std::optional<AdjustmentKind> adjustmentKindOf(std::string_view name) noexcept {
	const auto& names = adjustmentKindNames();
	const auto* const found = std::ranges::find(names, name);
	if(found == names.end()) return std::nullopt;
	return static_cast<AdjustmentKind>(found - names.begin());
}

void Adjustment::normalize() {
	inputBlack = clamped(inputBlack);
	inputWhite = std::max(clamped(inputWhite), inputBlack + 0.001f);
	gamma = std::isfinite(gamma) ? std::clamp(gamma, 0.1f, 10.0f) : 1.0f;
	outputBlack = clamped(outputBlack);
	outputWhite = clamped(outputWhite);
	if(curve.size() > maxCurvePoints) curve.resize(maxCurvePoints);
	for(auto& [input, output] : curve) {
		input = clamped(input);
		output = clamped(output);
	}
	std::ranges::stable_sort(curve, [](const auto& left, const auto& right) { return left.first < right.first; });
	if(curve.size() < 2) curve = {{0.0f, 0.0f}, {1.0f, 1.0f}};
	hue = std::isfinite(hue) ? std::clamp(hue, -180.0f, 180.0f) : 0.0f;
	const auto unit = [](float& value) { value = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f; };
	unit(saturation);
	unit(lightness);
	for(float& value : balance) unit(value);
	unit(brightness);
	unit(contrast);
	level = clamped(level);
	gradient.normalize();
}

AdjustmentShader adjustmentShader(const Adjustment& adjustment) noexcept {
	switch(adjustment.kind) {
	case AdjustmentKind::hueSaturation:
		return AdjustmentShader::hueSaturation;
	case AdjustmentKind::threshold:
	case AdjustmentKind::gradientMap:
		return AdjustmentShader::byLuminosity;
	default:
		return AdjustmentShader::perChannel;
	}
}

std::array<std::uint8_t, std::size_t{256} * 4> adjustmentTable(const Adjustment& adjustment) {
	std::array<std::uint8_t, std::size_t{256} * 4> table{};
	for(std::size_t index = 0; index < 256; ++index) {
		const float value = static_cast<float>(index) / 255.0f;
		const Pixel grey{value, value, value, 1.0f};
		const Pixel out =
		    adjustmentShader(adjustment) == AdjustmentShader::hueSaturation ? grey : adjustPixel(adjustment, grey);
		const std::array<float, 4> channels{
		    adjustmentShader(adjustment) == AdjustmentShader::perChannel ? adjustChannel(adjustment, 0, value)
			                                                             : out.red,
		    adjustmentShader(adjustment) == AdjustmentShader::perChannel ? adjustChannel(adjustment, 1, value)
			                                                             : out.green,
		    adjustmentShader(adjustment) == AdjustmentShader::perChannel ? adjustChannel(adjustment, 2, value)
			                                                             : out.blue,
		    adjustment.kind == AdjustmentKind::gradientMap ? adjustment.gradient.at(value).alpha : 1.0f};
		for(std::size_t channel = 0; channel < 4; ++channel)
			table[index * 4 + channel] = byteFromUnit(channels[channel]);
	}
	return table;
}

Pixel adjustPixel(const Adjustment& adjustment, const Pixel& pixel) noexcept {
	switch(adjustment.kind) {
	case AdjustmentKind::hueSaturation:
		return hueSaturation(adjustment, pixel);
	case AdjustmentKind::threshold: {
		const float value = detail::luminosityOf(pixel) >= adjustment.level ? 1.0f : 0.0f;
		return {value, value, value, pixel.alpha};
	}
	case AdjustmentKind::gradientMap: {
		const Pixel mapped = adjustment.gradient.at(clamped(detail::luminosityOf(pixel)));
		// The gradient's own alpha mixes it with the colour below
		const auto mix = [&](float below, float above) { return below + (above - below) * mapped.alpha; };
		return {mix(pixel.red, mapped.red), mix(pixel.green, mapped.green), mix(pixel.blue, mapped.blue), pixel.alpha};
	}
	default:
		return {adjustChannel(adjustment, 0, clamped(pixel.red)), adjustChannel(adjustment, 1, clamped(pixel.green)),
		        adjustChannel(adjustment, 2, clamped(pixel.blue)), pixel.alpha};
	}
}

std::string_view adjustmentShaderSource() noexcept {
	return R"(
vec3 adjustHueSaturation(vec3 color, vec3 hsl) {
	float angle = radians(hsl.x);
	float c = cos(angle), s = sin(angle), third = 1.0 / 3.0, root = sqrt(third);
	float a = c + (1.0 - c) * third, b = third * (1.0 - c) - root * s, d = third * (1.0 - c) + root * s;
	vec3 turned = vec3(dot(color, vec3(a, b, d)), dot(color, vec3(d, a, b)), dot(color, vec3(b, d, a)));
	float grey = dot(turned, vec3(0.3, 0.59, 0.11));
	turned = grey + (turned - grey) * (1.0 + hsl.y);
	turned = hsl.z >= 0.0 ? turned + (1.0 - turned) * hsl.z : turned * (1.0 + hsl.z);
	return clamp(turned, 0.0, 1.0);
}
)";
}

} // namespace affresco::imaging
