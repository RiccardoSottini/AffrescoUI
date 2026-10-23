/// @file
/// @brief Readers for property values, and the layout properties.

#include "PropertyValues.hpp"
#include "LayoutProperties.hpp"
#include "NodeKeys.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <stdexcept>
#include <vector>

namespace affresco::format {

using expect::fail;
using expect::number;
using expect::object;
using expect::string;

Length length(const json& value, const std::string& path) {
	if(value.is_number()) return px(value.get<float>());

	if(value.is_string()) {
		std::string t = value.get<std::string>();
		Unit unit = Unit::Pixel;
		if(t.ends_with('%')) {
			unit = Unit::Percent;
			t.pop_back();
		} else if(t.ends_with("px")) {
			t.resize(t.size() - 2);
		}

		try {
			std::size_t used = 0;
			const float amount = std::stof(t, &used);
			if(used == t.size() && std::isfinite(amount)) return {amount, unit};
			// NOLINTNEXTLINE(bugprone-empty-catch): not a number, reported below like any other malformed length
		} catch(const std::exception&) {
		}
	}

	fail(path, R"(expected a length: a number of pixels, "12px" or "50%")");
}

namespace {

/// @brief Splits a text at its spaces.
/// @param text The text, e.g. "left right".
/// @return Its words, without empty ones.
std::vector<std::string> words(const std::string& text) {
	std::vector<std::string> result;
	std::size_t start = 0;
	while(start < text.size()) {
		const std::size_t end = std::min(text.find(' ', start), text.size());
		if(end > start) result.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	return result;
}

/// @brief Reads an edge name: "top", "bottom", "left" or "right".
/// @param name The name.
/// @param path Its place, for errors.
/// @return The edge.
/// @throws DocumentError If the name is unknown.
Edge edge(const std::string& name, const std::string& path) {
	if(name == "top") return Edge::Top;
	if(name == "bottom") return Edge::Bottom;
	if(name == "left") return Edge::Left;
	if(name == "right") return Edge::Right;
	fail(path, "unknown edge '" + name + "' (expected top, bottom, left or right)");
}

/// @brief Anchors an edge of a panel: a length from the parent's edge, or another panel's edge ("#header.bottom + 8").
/// @param panel The panel.
/// @param name The edge, e.g. "top".
/// @param offset Its value.
/// @param where Its place, for errors.
/// @throws DocumentError If the value is malformed.
void anchor(Panel& panel, const std::string& name, const json& offset, const std::string& where) {
	if(!offset.is_string() || !offset.get<std::string>().starts_with('#')) {
		panel.setAnchor(edge(name, where), length(offset, where));
		return;
	}

	const std::string spec = offset.get<std::string>();
	const std::size_t dot = spec.find('.');
	if(dot == std::string::npos) fail(where, R"(expected "#id.edge", e.g. "#header.bottom + 8")");
	std::size_t end = dot + 1;
	while(end < spec.size() && std::isalpha(static_cast<unsigned char>(spec[end]))) ++end;

	std::string rest = spec.substr(end); // "+ 8", "-8" or nothing
	std::erase(rest, ' ');
	float delta = 0.0f;
	if(!rest.empty()) {
		try {
			std::size_t used = 0;
			delta = std::stof(rest, &used);
			if(used != rest.size() || (rest[0] != '+' && rest[0] != '-')) throw std::invalid_argument(rest);
		} catch(const std::exception&) {
			fail(where, R"(expected "#id.edge", "#id.edge + 8" or "#id.edge - 8")");
		}
	}
	panel.setAnchorTo(edge(name, where), spec.substr(1, dot - 1), edge(spec.substr(dot + 1, end - dot - 1), where),
	                  delta);
}

} // namespace

Align align(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "start") return Align::Start;
	if(name == "center") return Align::Center;
	if(name == "end") return Align::End;
	fail(path, R"(expected "start", "center" or "end")");
}

std::pair<Align, Align> alignments(const json& value, const std::string& path) {
	if(value.is_string()) {
		const std::vector<std::string> both = words(value.get<std::string>());
		if(both.size() == 2) return {align(json(both[0]), path), align(json(both[1]), path)};
		return {align(value, path), align(value, path)};
	}
	object(value, path);
	return {align(value.value("horizontal", json("start")), path + ".horizontal"),
	        align(value.value("vertical", json("start")), path + ".vertical")};
}

Cursor cursor(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "arrow") return Cursor::Arrow;
	if(name == "ibeam") return Cursor::IBeam;
	if(name == "hand") return Cursor::Hand;
	if(name == "resize-horizontal") return Cursor::ResizeHorizontal;
	if(name == "resize-vertical") return Cursor::ResizeVertical;
	fail(path, R"(expected "arrow", "ibeam", "hand", "resize-horizontal" or "resize-vertical")");
}

Placement placement(const json& value, const std::string& path) {
	const std::string name = string(value, path);
	if(name == "below") return Placement::Below;
	if(name == "below-end") return Placement::BelowEnd;
	if(name == "above") return Placement::Above;
	if(name == "above-end") return Placement::AboveEnd;
	if(name == "right") return Placement::Right;
	if(name == "left") return Placement::Left;
	if(name == "center") return Placement::Center;
	if(name == "window") return Placement::Window;
	fail(path, "expected below, below-end, above, above-end, right, left, center or window");
}

float finiteNumber(const json& value, const std::string& path) {
	const float amount = number(value, path);
	if(!std::isfinite(amount)) fail(path, "number too large");
	return amount;
}

float pixels(const json& value, const std::string& path) {
	const float amount = finiteNumber(value, path);
	if(amount < 0.0f) fail(path, "expected a number of pixels, 0 or more");
	return amount;
}

CornerRadii radii(const json& value, const std::string& path) {
	if(!value.is_object()) return CornerRadii::all(pixels(value, path));

	CornerRadii result;
	for(const auto& [corner, radius] : value.items()) {
		const std::string at = std::format("{}.{}", path, corner);
		if(corner == "topLeft") result.topLeft = pixels(radius, at);
		else if(corner == "topRight") result.topRight = pixels(radius, at);
		else if(corner == "bottomRight") result.bottomRight = pixels(radius, at);
		else if(corner == "bottomLeft") result.bottomLeft = pixels(radius, at);
		else fail(at, "unknown corner (expected topLeft, topRight, bottomRight or bottomLeft)");
	}
	return result;
}

bool layoutProperty(Panel& panel, std::string_view key, const json& value, const std::string& at) {
	if(key == "x") panel.setX(length(value, at));
	else if(key == "y") panel.setY(length(value, at));
	else if(key == "width") panel.setWidth(length(value, at));
	else if(key == "height") panel.setHeight(length(value, at));
	else if(key == "minWidth") panel.setMinWidth(length(value, at));
	else if(key == "minHeight") panel.setMinHeight(length(value, at));
	else if(key == "grow") panel.setGrow(number(value, at));
	else if(key == "fill") {
		const Length offset = value.is_boolean() && value.get<bool>() ? px(0.0f) : length(value, at);
		for(const Edge e : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) panel.setAnchor(e, offset);
	} else if(key == "anchors" && value.is_string()) {
		const std::vector<std::string> edges = words(value.get<std::string>());
		if(edges.empty()) fail(at, R"(expected edges, e.g. "left right:12", or an object)");
		for(const std::string& word : edges) {
			const std::size_t colon = word.find(':');
			if(colon == std::string::npos) {
				panel.setAnchor(edge(word, at), px(0.0f));
				continue;
			}
			const std::string offset = word.substr(colon + 1);
			char* end = nullptr;
			const double pixels = std::strtod(offset.c_str(), &end);
			const json written = !offset.empty() && *end == '\0' && std::isfinite(pixels) ? json(pixels) : json(offset);
			panel.setAnchor(edge(word.substr(0, colon), at), length(written, concatenate(at, ".", word)));
		}
	} else if(key == "anchors") {
		for(const auto& [name, offset] : object(value, at).items())
			anchor(panel, name, offset, concatenate(at, ".", name));
	} else {
		return layoutItemProperty(panel, key, value, at);
	}
	return true;
}

} // namespace affresco::format
