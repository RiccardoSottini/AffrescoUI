/// @file
/// @brief The project manifest's JSON for layers' settings and adjustments: numbers are brought into range, names
/// and kinds checked, lists bounded before anything is kept.

#include "ProjectJson.hpp"

#include "affresco/imaging/Project.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace affresco::imaging {

namespace {

/// @brief The names of the layer kinds, by LayerKind.
constexpr std::array<std::string_view, 3> kindNames{"pixel", "group", "adjustment"};

/// @brief A number, held to a range a float keeps (a JSON number may be far larger).
/// @param value The JSON.
/// @param what Its name, for the error.
[[nodiscard]] float numberOf(const json& value, const std::string& what) {
	if(!value.is_number()) project::malformed("has a " + what + " that is not a number");
	return static_cast<float>(std::clamp(value.get<double>(), -1.0e6, 1.0e6));
}

/// @brief A number from an object, or a default when absent.
/// @param object The object.
/// @param key Its key.
/// @param otherwise The default.
[[nodiscard]] float numberIn(const json& object, const char* key, float otherwise) {
	const auto found = object.find(key);
	return found == object.end() ? otherwise : numberOf(*found, key);
}

/// @brief A boolean from an object, or a default when absent.
/// @param object The object.
/// @param key Its key.
/// @param otherwise The default.
[[nodiscard]] bool booleanIn(const json& object, const char* key, bool otherwise) {
	const auto found = object.find(key);
	if(found == object.end()) return otherwise;
	if(!found->is_boolean()) project::malformed(std::string("has a ") + key + " that is not true or false");
	return found->get<bool>();
}

/// @brief A list from an object, at most a length; empty when absent.
/// @param object The object.
/// @param key Its key.
/// @param most The longest allowed.
[[nodiscard]] const json& listIn(const json& object, const char* key, std::size_t most) {
	static const json none = json::array();
	const auto found = object.find(key);
	if(found == object.end()) return none;
	if(!found->is_array() || found->size() > most)
		project::malformed(std::string("has a ") + key + " that is not a list of at most " + std::to_string(most));
	return *found;
}

/// @brief Reads a curve and a gradient map's stops.
/// @param value The adjustment's JSON.
/// @param adjustment Its curve and gradient are set.
void readLists(const json& value, Adjustment& adjustment) {
	if(value.contains("curve")) adjustment.curve.clear();
	for(const json& point : listIn(value, "curve", Adjustment::maxCurvePoints)) {
		if(!point.is_array() || point.size() != 2) project::malformed("has a curve point that is not [in, out]");
		adjustment.curve.emplace_back(numberOf(point[0], "curve"), numberOf(point[1], "curve"));
	}
	if(value.contains("gradient")) adjustment.gradient.stops.clear();
	for(const json& stop : listIn(value, "gradient", Gradient::maxStops)) {
		if(!stop.is_array() || stop.size() != 5) project::malformed("has a stop that is not [at, r, g, b, a]");
		adjustment.gradient.stops.push_back({numberOf(stop[0], "stop"),
		                                     {numberOf(stop[1], "stop"), numberOf(stop[2], "stop"),
		                                      numberOf(stop[3], "stop"), numberOf(stop[4], "stop")}});
	}
}

} // namespace

json adjustmentToJson(const Adjustment& adjustment) {
	json curve = json::array(), stops = json::array();
	for(const auto& [input, output] : adjustment.curve) curve.push_back(json::array({input, output}));
	for(const GradientStop& stop : adjustment.gradient.stops)
		stops.push_back(
		    json::array({stop.position, stop.color.red, stop.color.green, stop.color.blue, stop.color.alpha}));
	return json{{"kind", adjustmentKindNames()[static_cast<std::size_t>(adjustment.kind)]},
	            {"inputBlack", adjustment.inputBlack},
	            {"inputWhite", adjustment.inputWhite},
	            {"gamma", adjustment.gamma},
	            {"outputBlack", adjustment.outputBlack},
	            {"outputWhite", adjustment.outputWhite},
	            {"curve", std::move(curve)},
	            {"hue", adjustment.hue},
	            {"saturation", adjustment.saturation},
	            {"lightness", adjustment.lightness},
	            {"balance", adjustment.balance},
	            {"brightness", adjustment.brightness},
	            {"contrast", adjustment.contrast},
	            {"level", adjustment.level},
	            {"gradient", std::move(stops)}};
}

Adjustment adjustmentFromJson(const json& value) {
	if(!value.is_object()) project::malformed("has an adjustment that is not an object");
	Adjustment adjustment;
	const auto kind = value.find("kind");
	const auto known =
	    kind != value.end() && kind->is_string() ? adjustmentKindOf(kind->get<std::string>()) : std::nullopt;
	if(!known) project::malformed("has an adjustment of no known kind");
	adjustment.kind = *known;
	adjustment.inputBlack = numberIn(value, "inputBlack", adjustment.inputBlack);
	adjustment.inputWhite = numberIn(value, "inputWhite", adjustment.inputWhite);
	adjustment.gamma = numberIn(value, "gamma", adjustment.gamma);
	adjustment.outputBlack = numberIn(value, "outputBlack", adjustment.outputBlack);
	adjustment.outputWhite = numberIn(value, "outputWhite", adjustment.outputWhite);
	adjustment.hue = numberIn(value, "hue", adjustment.hue);
	adjustment.saturation = numberIn(value, "saturation", adjustment.saturation);
	adjustment.lightness = numberIn(value, "lightness", adjustment.lightness);
	const json& balance = listIn(value, "balance", 3);
	for(std::size_t index = 0; index < balance.size(); ++index)
		adjustment.balance.at(index) = numberOf(balance[index], "balance");
	adjustment.brightness = numberIn(value, "brightness", adjustment.brightness);
	adjustment.contrast = numberIn(value, "contrast", adjustment.contrast);
	adjustment.level = numberIn(value, "level", adjustment.level);
	readLists(value, adjustment);
	adjustment.normalize();
	return adjustment;
}

namespace project {

void malformed(const std::string& what) { throw DocumentError("imaging: the project file " + what); }

std::uint64_t whole(const json& value, std::uint64_t most, const std::string& what) {
	if(!value.is_number_unsigned() || value.get<std::uint64_t>() > most)
		malformed("has a " + what + " that is not a whole number up to " + std::to_string(most));
	return value.get<std::uint64_t>();
}

json layerSettingsToJson(const Layer& layer) {
	json value{
	    {"kind", kindNames.at(static_cast<std::size_t>(layer.kind))},
	    {"name", layer.name},
	    {"visible", layer.visible},
	    {"opacity", layer.opacity},
	    {"blend", blendModeName(layer.blend)},
	    {"locks", {{"pixels", layer.locks.pixels}, {"position", layer.locks.position}, {"all", layer.locks.all}}},
	    {"clipped", layer.clipped},
	    {"maskEnabled", layer.maskEnabled}};
	if(layer.kind == LayerKind::adjustment) value["adjustment"] = adjustmentToJson(layer.adjustment);
	return value;
}

void readLayerSettings(const json& value, Layer& layer) {
	if(!value.is_object()) malformed("has a layer that is not an object");
	const auto kind = value.find("kind");
	const auto named = kind != value.end() && kind->is_string()
	                       ? std::find(kindNames.begin(), kindNames.end(), kind->get<std::string>())
	                       : kindNames.end();
	if(named == kindNames.end()) malformed("has a layer of no known kind");
	layer.kind = static_cast<LayerKind>(named - kindNames.begin());
	const auto name = value.find("name");
	if(name == value.end() || !name->is_string() || name->get<std::string>().size() > 255)
		malformed("has a layer without a name of at most 255 bytes");
	layer.name = name->get<std::string>();
	layer.visible = booleanIn(value, "visible", true);
	layer.opacity = std::clamp(numberIn(value, "opacity", 1.0f), 0.0f, 1.0f);
	const auto blend = value.find("blend");
	const auto mode = blend == value.end() ? std::optional<BlendMode>(BlendMode::normal)
	                  : blend->is_string() ? blendModeOf(blend->get<std::string>())
	                                       : std::nullopt;
	if(!mode) malformed("has a layer of no known blend mode");
	layer.blend = *mode;
	if(const auto locks = value.find("locks"); locks != value.end()) {
		if(!locks->is_object()) malformed("has locks that are not an object");
		layer.locks = {booleanIn(*locks, "pixels", false), booleanIn(*locks, "position", false),
		               booleanIn(*locks, "all", false)};
	}
	layer.clipped = booleanIn(value, "clipped", false);
	layer.maskEnabled = booleanIn(value, "maskEnabled", true);
	if(layer.kind == LayerKind::adjustment) layer.adjustment = adjustmentFromJson(value.value("adjustment", json()));
}

} // namespace project

} // namespace affresco::imaging
