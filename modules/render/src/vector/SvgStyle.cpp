/// @file
/// @brief vector::SvgStyle: reading SVG paint, stroke, opacity and transform values.

#include "SvgStyle.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace affresco::vector {

namespace {

constexpr float magnitudeLimit = 1.0e6f; ///< The largest number read, either sign.
constexpr std::size_t listLimit = 64;    ///< The most numbers in a list.

/// @brief Removes spaces from both ends.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept {
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) text.remove_prefix(1);
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) text.remove_suffix(1);
	return text;
}

/// @brief Reads a paint: a hex colour, a few names, currentColor (white, for tinting), or none.
/// @param text The value.
/// @return The colour, or nullopt for none and for what isn't read (gradients, for one).
[[nodiscard]] std::optional<Color> paint(std::string_view text) {
	static constexpr std::array<std::pair<std::string_view, std::uint32_t>, 9> names{{{"black", 0x000000},
	                                                                                  {"white", 0xffffff},
	                                                                                  {"currentColor", 0xffffff},
	                                                                                  {"red", 0xff0000},
	                                                                                  {"green", 0x008000},
	                                                                                  {"blue", 0x0000ff},
	                                                                                  {"gray", 0x808080},
	                                                                                  {"grey", 0x808080},
	                                                                                  {"yellow", 0xffff00}}};
	text = trim(text);
	if(text.starts_with('#')) return Color::parseHex(text);
	for(const auto& [name, rgb] : names)
		if(text == name) return Color::hex(rgb);
	return std::nullopt;
}

/// @brief Reads an opacity, clamped to [0, 1].
/// @param text The value.
/// @param fallback What it is when the value isn't a number.
[[nodiscard]] float opacityOf(std::string_view text, float fallback) {
	return std::clamp(svgNumber(text).value_or(fallback), 0.0f, 1.0f);
}

/// @brief Sets a stroke property (stroke-width, -linejoin, -linecap, -miterlimit, -dasharray).
/// @param line The stroke.
/// @param key The property, without "stroke-".
/// @param value Its value.
void setLine(StrokeStyle& line, std::string_view key, std::string_view value) {
	value = trim(value);
	if(key == "width") line.width = std::max(svgNumber(value).value_or(line.width), 0.0f);
	else if(key == "miterlimit") line.miterLimit = std::max(svgNumber(value).value_or(line.miterLimit), 1.0f);
	else if(key == "linejoin")
		line.join = value == "round" ? LineJoin::round : value == "bevel" ? LineJoin::bevel : LineJoin::miter;
	else if(key == "linecap")
		line.cap = value == "round" ? LineCap::round : value == "square" ? LineCap::square : LineCap::butt;
	else if(key == "dasharray") {
		line.dash = value == "none" ? std::vector<float>{} : svgNumbers(value);
		if(std::ranges::any_of(line.dash, [](float length) { return length < 0.0f; })) line.dash.clear();
	}
}

/// @brief Reads one transform function's arguments and composes it into a transform.
/// @param transform The transform so far.
/// @param name The function's name.
/// @param arguments Its arguments.
void composeFunction(PathTransform& transform, std::string_view name, const std::vector<float>& arguments) {
	PathTransform local;
	if(name == "translate" && !arguments.empty())
		local.offset = {arguments[0], arguments.size() > 1 ? arguments[1] : 0.0f};
	else if(name == "scale" && !arguments.empty())
		local.scale = {arguments[0], arguments.size() > 1 ? arguments[1] : arguments[0]};
	else if(name == "matrix" && arguments.size() == 6 && arguments[1] == 0.0f && arguments[2] == 0.0f)
		local = {{arguments[0], arguments[3]}, {arguments[4], arguments[5]}};
	else return; // rotations and skews aren't drawn
	transform = compose(transform, local);
}

} // namespace

std::optional<float> svgNumber(std::string_view text) {
	text = trim(text);
	if(text.ends_with("px")) text.remove_suffix(2);
	if(text.empty() || text.size() > 64) return std::nullopt;
	const std::string copy(text);
	char* end = nullptr;
	const float value = std::strtof(copy.c_str(), &end);
	if(end != copy.c_str() + copy.size() || !std::isfinite(value) || std::abs(value) > magnitudeLimit)
		return std::nullopt;
	return value;
}

std::vector<float> svgNumbers(std::string_view text) {
	std::vector<float> result;
	std::size_t at = 0;
	while(result.size() < listLimit) {
		while(at < text.size() && (std::isspace(static_cast<unsigned char>(text[at])) != 0 || text[at] == ',')) ++at;
		const std::size_t start = at;
		while(at < text.size() && std::isspace(static_cast<unsigned char>(text[at])) == 0 && text[at] != ',') ++at;
		if(start == at) break;
		const auto number = svgNumber(text.substr(start, at - start));
		if(!number) break;
		result.push_back(*number);
	}
	return result;
}

PathTransform svgTransform(std::string_view text) {
	PathTransform transform;
	for(std::size_t functions = 0; functions < listLimit; ++functions) {
		const std::size_t open = text.find('('), close = text.find(')');
		if(open == std::string_view::npos || close == std::string_view::npos || close < open) break;
		std::string_view name = trim(text.substr(0, open));
		while(name.starts_with(',')) name = trim(name.substr(1)); // functions may be separated by commas
		composeFunction(transform, name, svgNumbers(text.substr(open + 1, close - open - 1)));
		text.remove_prefix(close + 1);
	}
	return transform;
}

void SvgStyle::set(std::string_view key, std::string_view value) {
	value = trim(value);
	if(key == "fill") fill = paint(value);
	else if(key == "stroke") stroke = paint(value);
	else if(key == "fill-rule") fillRule = value == "evenodd" ? FillRule::evenOdd : FillRule::nonZero;
	else if(key == "fill-opacity") fillOpacity = opacityOf(value, fillOpacity);
	else if(key == "stroke-opacity") strokeOpacity = opacityOf(value, strokeOpacity);
	else if(key == "opacity") opacity *= opacityOf(value, 1.0f);
	else if(key.starts_with("stroke-")) setLine(line, key.substr(7), value);
}

void SvgStyle::apply(const XmlTag& tag) {
	for(const auto& [key, value] : tag.attributes)
		if(key != "style" && key != "transform") set(key, value);
	if(const auto style = tag.attribute("style")) {
		std::string_view rest = *style;
		while(!rest.empty()) {
			const std::size_t end = std::min(rest.find(';'), rest.size());
			const std::string_view declaration = rest.substr(0, end);
			if(const std::size_t colon = declaration.find(':'); colon != std::string_view::npos)
				set(trim(declaration.substr(0, colon)), declaration.substr(colon + 1));
			rest.remove_prefix(std::min(end + 1, rest.size()));
		}
	}
	if(const auto local = tag.attribute("transform")) transform = compose(transform, svgTransform(*local));
}

} // namespace affresco::vector
