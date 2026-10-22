/// @file
/// @brief "rotate", "scale", "skew", "translate" and "transformOrigin", properties of every panel: how it is drawn
/// turned, scaled, skewed or moved after layout (Panel::setTransform). Layout doesn't see them.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Panel.hpp"
#include "affresco/render/TimelineView.hpp"
#include "affresco/render/Transform.hpp"

#include <cmath>

namespace affresco::format {

namespace {

/// @brief Reads a pair of numbers: one number for both, or [x, y].
/// @param value The value.
/// @param single Whether one number may stand for both ("scale").
/// @param path Its place, for errors.
/// @return The pair.
/// @throws DocumentError If it is not one.
Vec2 pair(const json& value, bool single, const std::string& path) {
	if(single && value.is_number()) {
		const float both = number(value, path);
		return {both, both};
	}
	if(!value.is_array() || value.size() != 2) fail(path, single ? "expected a number or [x, y]" : "expected [x, y]");
	return {number(value[0], path + "[0]"), number(value[1], path + "[1]")};
}

/// @brief Reads a transform origin: "center", an edge or a corner ("top", "bottomRight"), or [x%, y%] of the size.
/// @param value The value.
/// @param path Its place, for errors.
/// @return The origin, as fractions of the panel's size.
/// @throws DocumentError If it is not one.
Vec2 origin(const json& value, const std::string& path) {
	if(value.is_array()) {
		const Vec2 percent = pair(value, false, path);
		return {percent.x * 0.01f, percent.y * 0.01f};
	}
	const std::string name = string(value, path);
	static constexpr std::pair<std::string_view, Vec2> named[] = {
	    {"center", {0.5f, 0.5f}},  {"top", {0.5f, 0.0f}},    {"bottom", {0.5f, 1.0f}},     {"left", {0.0f, 0.5f}},
	    {"right", {1.0f, 0.5f}},   {"topLeft", {0.0f, 0.0f}}, {"topRight", {1.0f, 0.0f}}, {"bottomLeft", {0.0f, 1.0f}},
	    {"bottomRight", {1.0f, 1.0f}}};
	for(const auto& [key, fraction] : named)
		if(name == key) return fraction;
	fail(path, R"(expected "center", an edge ("top", "bottomRight"...) or [x%, y%])");
}

/// @brief Sets a panel's transform from its keys; the identity when it has none.
/// @param panel The panel.
/// @param values Its "rotate", "scale", "skew", "translate" and "transformOrigin", if given.
/// @param reader Not used.
/// @param path The node's place, for errors.
void applyTransform(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static_cast<void>(reader);
	const auto given = [&values](const char* key) { return values.contains(key) && !values[key].is_null(); };
	const float degrees = given("rotate") ? number(values["rotate"], path + ".rotate") : 0.0f;
	const Vec2 scale = given("scale") ? pair(values["scale"], true, path + ".scale") : Vec2{1.0f, 1.0f};
	const Vec2 skew = given("skew") ? pair(values["skew"], false, path + ".skew") : Vec2{};
	const Vec2 translate = given("translate") ? pair(values["translate"], false, path + ".translate") : Vec2{};
	const Vec2 at = given("transformOrigin") ? origin(values["transformOrigin"], path + ".transformOrigin")
	                                         : Vec2{0.5f, 0.5f};
	if(!std::isfinite(degrees) || !std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(skew.x) ||
	   !std::isfinite(skew.y) || !std::isfinite(translate.x) || !std::isfinite(translate.y))
		fail(path, "a transform needs finite numbers");
	panel.setTransform(Affine2::compose(translate, degrees, scale, skew), at);
}

} // namespace

void registerTransformProperties(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"rotate", ValueKind::Number},
	                   {"scale", ValueKind::Any},
	                   {"skew", ValueKind::Any},
	                   {"translate", ValueKind::Any},
	                   {"transformOrigin", ValueKind::Any}};
	set->apply = applyTransform;
	set->everyUpdate = true; // setTransform() does nothing when it is the same; it also clears keys removed
	// A timeline's own "scale" is its zoom (TimelineType.cpp), not a transform
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TimelineView*>(&panel) == nullptr; };
	registry.addProperties(std::move(set));
}

} // namespace affresco::format
