/// @file
/// @brief Blend modes: the W3C formulas on the CPU, and the same in GLSL.

#include "affresco/imaging/Blend.hpp"

#include "Luminosity.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::imaging {

namespace {

/// @brief The separable blend function of a mode, for one channel.
/// @param mode A separable mode (normal to difference).
/// @param backdrop The channel below.
/// @param source The layer's channel.
[[nodiscard]] float separable(BlendMode mode, float backdrop, float source) noexcept {
	switch(mode) {
	case BlendMode::multiply:
		return backdrop * source;
	case BlendMode::screen:
		return backdrop + source - backdrop * source;
	case BlendMode::overlay:
		return backdrop <= 0.5f ? 2.0f * backdrop * source : 1.0f - 2.0f * (1.0f - backdrop) * (1.0f - source);
	case BlendMode::softLight: {
		if(source <= 0.5f) return backdrop - (1.0f - 2.0f * source) * backdrop * (1.0f - backdrop);
		const float curve =
		    backdrop <= 0.25f ? ((16.0f * backdrop - 12.0f) * backdrop + 4.0f) * backdrop : std::sqrt(backdrop);
		return backdrop + (2.0f * source - 1.0f) * (curve - backdrop);
	}
	case BlendMode::darken:
		return std::min(backdrop, source);
	case BlendMode::lighten:
		return std::max(backdrop, source);
	case BlendMode::difference:
		return std::abs(backdrop - source);
	default:
		return source;
	}
}

/// @brief Brings a colour into 0 to 1 keeping its luminosity.
/// @param color The colour.
[[nodiscard]] Pixel clipColor(Pixel color) noexcept {
	const float lum = detail::luminosityOf(color);
	const float low = std::min({color.red, color.green, color.blue});
	const float high = std::max({color.red, color.green, color.blue});
	const auto scale = [&](float factor, float limit) {
		for(float* channel : {&color.red, &color.green, &color.blue})
			*channel = lum + (*channel - lum) * factor / std::max(limit, 1e-6f);
	};
	if(low < 0.0f) scale(lum, lum - low);
	if(high > 1.0f) scale(1.0f - lum, high - lum);
	return color;
}

/// @brief A colour with another luminosity.
/// @param color The colour.
/// @param lum The luminosity.
[[nodiscard]] Pixel setLuminosity(const Pixel& color, float lum) noexcept {
	const float shift = lum - detail::luminosityOf(color);
	return clipColor({color.red + shift, color.green + shift, color.blue + shift, color.alpha});
}

/// @brief The saturation of a colour: its highest channel less its lowest.
/// @param color The colour.
[[nodiscard]] float saturationOf(const Pixel& color) noexcept {
	return std::max({color.red, color.green, color.blue}) - std::min({color.red, color.green, color.blue});
}

/// @brief A colour with another saturation, keeping the order of its channels.
/// @param color The colour.
/// @param saturation The saturation.
[[nodiscard]] Pixel setSaturation(Pixel color, float saturation) noexcept {
	std::array<float*, 3> order{&color.red, &color.green, &color.blue};
	std::ranges::sort(order, [](const float* left, const float* right) { return *left < *right; });
	float& low = *order[0];
	float& middle = *order[1];
	float& high = *order[2];
	if(high > low) {
		middle = (middle - low) * saturation / (high - low);
		high = saturation;
	} else middle = high = 0.0f;
	low = 0.0f;
	return color;
}

} // namespace

const std::array<std::string_view, blendModeCount>& blendModeNames() noexcept {
	static constexpr std::array<std::string_view, blendModeCount> names{
	    "normal",  "multiply",   "screen", "overlay",    "soft-light", "darken",
	    "lighten", "difference", "hue",    "saturation", "color",      "luminosity"};
	return names;
}

std::string_view blendModeName(BlendMode mode) noexcept { return blendModeNames()[static_cast<std::size_t>(mode)]; }

std::optional<BlendMode> blendModeOf(std::string_view name) noexcept {
	const auto& names = blendModeNames();
	const auto found = std::ranges::find(names, name);
	if(found == names.end()) return std::nullopt;
	return static_cast<BlendMode>(found - names.begin());
}

Pixel blendColors(BlendMode mode, const Pixel& backdrop, const Pixel& source) noexcept {
	switch(mode) {
	case BlendMode::hue:
		return setLuminosity(setSaturation(source, saturationOf(backdrop)), detail::luminosityOf(backdrop));
	case BlendMode::saturation:
		return setLuminosity(setSaturation(backdrop, saturationOf(source)), detail::luminosityOf(backdrop));
	case BlendMode::color:
		return setLuminosity(source, detail::luminosityOf(backdrop));
	case BlendMode::luminosity:
		return setLuminosity(backdrop, detail::luminosityOf(source));
	default:
		return {separable(mode, backdrop.red, source.red), separable(mode, backdrop.green, source.green),
		        separable(mode, backdrop.blue, source.blue), 1.0f};
	}
}

Pixel blendPixels(const Pixel& backdrop, const Pixel& source, BlendMode mode, float opacity) noexcept {
	const float sourceAlpha = clamped(source.alpha * opacity);
	const float backdropAlpha = clamped(backdrop.alpha);
	const float alpha = sourceAlpha + backdropAlpha * (1.0f - sourceAlpha);
	if(alpha <= 0.0f) return {};
	const Pixel mixed = mode == BlendMode::normal ? source : blendColors(mode, backdrop, source);
	const auto channel = [&](float below, float above, float blended) {
		const float colour = (1.0f - backdropAlpha) * above + backdropAlpha * blended;
		return (sourceAlpha * colour + (1.0f - sourceAlpha) * backdropAlpha * below) / alpha;
	};
	return {channel(backdrop.red, source.red, mixed.red), channel(backdrop.green, source.green, mixed.green),
	        channel(backdrop.blue, source.blue, mixed.blue), alpha};
}

std::string_view blendShaderSource() noexcept {
	return R"(
float blendLum(vec3 c) { return dot(c, vec3(0.3, 0.59, 0.11)); }
vec3 blendClip(vec3 c) {
	float l = blendLum(c);
	float n = min(min(c.r, c.g), c.b);
	float x = max(max(c.r, c.g), c.b);
	if(n < 0.0) c = l + (c - l) * l / max(l - n, 1e-6);
	if(x > 1.0) c = l + (c - l) * (1.0 - l) / max(x - l, 1e-6);
	return c;
}
vec3 blendSetLum(vec3 c, float l) { return blendClip(c + (l - blendLum(c))); }
float blendSat(vec3 c) { return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b); }
vec3 blendSetSat(vec3 c, float s) {
	float n = min(min(c.r, c.g), c.b);
	float x = max(max(c.r, c.g), c.b);
	if(x <= n) return vec3(0.0);
	return (c - n) * s / (x - n);
}
float blendSoft(float b, float s) {
	if(s <= 0.5) return b - (1.0 - 2.0 * s) * b * (1.0 - b);
	float d = b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(b);
	return b + (2.0 * s - 1.0) * (d - b);
}
vec3 blendColors(int mode, vec3 b, vec3 s) {
	if(mode == 1) return b * s;
	if(mode == 2) return b + s - b * s;
	if(mode == 3) return mix(2.0 * b * s, 1.0 - 2.0 * (1.0 - b) * (1.0 - s), step(vec3(0.5), b));
	if(mode == 4) return vec3(blendSoft(b.r, s.r), blendSoft(b.g, s.g), blendSoft(b.b, s.b));
	if(mode == 5) return min(b, s);
	if(mode == 6) return max(b, s);
	if(mode == 7) return abs(b - s);
	if(mode == 8) return blendSetLum(blendSetSat(s, blendSat(b)), blendLum(b));
	if(mode == 9) return blendSetLum(blendSetSat(b, blendSat(s)), blendLum(b));
	if(mode == 10) return blendSetLum(s, blendLum(b));
	if(mode == 11) return blendSetLum(b, blendLum(s));
	return s;
}
vec4 blendPixels(vec4 backdrop, vec4 source, int mode, float opacity) {
	float sa = clamp(source.a * opacity, 0.0, 1.0);
	float ba = clamp(backdrop.a, 0.0, 1.0);
	float a = sa + ba * (1.0 - sa);
	if(a <= 0.0) return vec4(0.0);
	vec3 blended = blendColors(mode, backdrop.rgb, source.rgb);
	vec3 colour = (1.0 - ba) * source.rgb + ba * blended;
	return vec4((sa * colour + (1.0 - sa) * ba * backdrop.rgb) / a, a);
}
)";
}

} // namespace affresco::imaging
