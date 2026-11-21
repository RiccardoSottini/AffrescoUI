/// @file
/// @brief format::editing: the gestures' names, choosing a document's tool, and the tool options (the brush, the
/// sampling, the gradient, the colours).

#include "ImageDocuments.hpp"

#include "affresco/imaging/Shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <utility>

namespace affresco::format::editing {

namespace {

/// @brief The gestures' names, in Gesture's order.
constexpr std::array<std::string_view, 23> gestureNameList{
    "move", "select-rectangle", "select-ellipse", "lasso",    "polygon-lasso", "magic-wand", "crop", "paint",
    "erase", "clone",           "gradient",       "fill",     "sample",        "pan",        "zoom", "text",
    "shape", "pen",             "path-select",    "quick-select", "selection-box", "layer-box",
    "retouch"};

/// @brief The keys of the tool options, as setOptions() reads them.
constexpr std::array<std::string_view, 29> optionKeyList{
    "preset",     "size",       "sizeStep",  "hardness",    "spacing",     "opacity",        "flow",     "mode",
    "tolerance",  "contiguous", "allLayers", "fillOpacity", "shape",       "gradientPreset", "gradient", "foreground",
    "background", "swap",       "reset",     "quickMask",   "cloneSource", "autoSelect",     "vector",   "cropRatio",
    "retouch",    "reverse",    "dither",    "transparency", "healing"};

/// @brief A gradient argument: {stops: [{at, color}]} as a gradient editor gives it (or "position" for "at").
/// @param value The value.
/// @return The gradient.
/// @throws DocumentError If it isn't one.
[[nodiscard]] imaging::Gradient gradientOf(const json& value) {
	const auto stops = value.is_object() ? value.find("stops") : value.end();
	if(!value.is_object() || stops == value.end() || !stops->is_array() || stops->size() < 2 ||
	   stops->size() > imaging::Gradient::maxStops)
		throw DocumentError("A gradient has 2 to 32 stops");
	imaging::Gradient gradient;
	gradient.stops.clear();
	for(const json& stop : *stops) {
		const auto colour = colorOf(textOf(stop, "color"));
		if(!colour) throw DocumentError("A stop's colour is #rrggbb");
		gradient.stops.push_back(
		    {std::clamp(static_cast<float>(numberOf(stop, "at", numberOf(stop, "position", 0.0))), 0.0f, 1.0f),
		     *colour});
	}
	std::ranges::stable_sort(gradient.stops, {}, &imaging::GradientStop::position);
	return gradient;
}

/// @brief Sets the brush options: preset, size, hardness, spacing, opacity, flow, sizeStep (a step bigger or smaller).
/// @param tip The tip.
/// @param limits The bounds of the brush.
/// @param arguments The options set.
/// @throws DocumentError If the preset is unknown.
void setBrush(imaging::BrushTip& tip, const imaging::Limits& limits, const json& arguments) {
	if(const std::string preset = textOf(arguments, "preset"); !preset.empty()) {
		const auto presets = imaging::brushPresets();
		const auto found = std::ranges::find(presets, preset, &imaging::BrushPreset::name);
		if(found == presets.end()) throw DocumentError("Unknown brush " + preset);
		tip = found->tip;
	}
	tip.size = static_cast<float>(numberOf(arguments, "size", tip.size));
	if(const double step = numberOf(arguments, "sizeStep", 0.0); step != 0.0)
		tip.size = std::round(tip.size * static_cast<float>(step > 0.0 ? 1.25 : 0.8)) + (step > 0.0 ? 1.0f : 0.0f);
	tip.hardness = static_cast<float>(numberOf(arguments, "hardness", tip.hardness * 100.0) / 100.0);
	tip.spacing = static_cast<float>(numberOf(arguments, "spacing", tip.spacing * 100.0) / 100.0);
	tip.opacity = static_cast<float>(numberOf(arguments, "opacity", tip.opacity * 100.0) / 100.0);
	tip.flow = static_cast<float>(numberOf(arguments, "flow", tip.flow * 100.0) / 100.0);
	tip.normalize(limits);
}

/// @brief Sets the gradient's shape, stops and options: shape, gradientPreset (an unknown one means foreground to
/// background), gradient, reverse, dither, transparency.
/// @param settings The settings.
/// @param arguments The options set.
/// @throws DocumentError If the shape or the stops are malformed.
void setGradient(ToolSettings& settings, const json& arguments) {
	if(const std::string shape = textOf(arguments, "shape"); !shape.empty()) {
		const auto names = imaging::gradientShapeNames();
		const auto found = std::ranges::find(names, shape);
		if(found == names.end()) throw DocumentError("Unknown gradient shape " + shape);
		settings.shape = static_cast<imaging::GradientShape>(found - names.begin());
	}
	if(const std::string preset = textOf(arguments, "gradientPreset"); !preset.empty()) {
		const auto presets = imaging::gradientPresets();
		const auto found = std::ranges::find(presets, preset, &imaging::GradientPreset::name);
		if(found == presets.end()) settings.gradient.reset();
		else settings.gradient = found->gradient;
	}
	if(arguments.contains("gradient")) settings.gradient = gradientOf(arguments["gradient"]);
	imaging::GradientOptions& options = settings.gradientOptions;
	options.reverse = flagOf(arguments, "reverse", options.reverse);
	options.dither = flagOf(arguments, "dither", options.dither);
	options.transparency = flagOf(arguments, "transparency", options.transparency);
}

/// @brief Sets the selection, fill, move, crop, retouch and healing options: mode, tolerance, contiguous, allLayers,
/// fillOpacity, autoSelect, cropRatio, retouch, healing.
/// @param settings The settings.
/// @param arguments The options set.
void setSampling(ToolSettings& settings, const json& arguments) {
	settings.mode = selectionModeOf(textOf(arguments, "mode"), settings.mode);
	settings.sample.tolerance = std::clamp(wholeOf(arguments, "tolerance", settings.sample.tolerance), 0, 255);
	settings.sample.contiguous = flagOf(arguments, "contiguous", settings.sample.contiguous);
	settings.sample.allLayers = flagOf(arguments, "allLayers", settings.sample.allLayers);
	settings.autoSelect = flagOf(arguments, "autoSelect", settings.autoSelect);
	settings.healing = flagOf(arguments, "healing", settings.healing);
	if(const std::string retouch = textOf(arguments, "retouch"); !retouch.empty()) {
		const auto names = imaging::retouchNames();
		const auto found = std::ranges::find(names, retouch);
		if(found == names.end()) throw DocumentError("Unknown retouch " + retouch);
		settings.retouch = static_cast<imaging::StrokeMode>(static_cast<std::size_t>(imaging::StrokeMode::dodge) +
		                                                    static_cast<std::size_t>(found - names.begin()));
	}
	settings.cropRatio =
	    std::clamp(static_cast<float>(numberOf(arguments, "cropRatio", settings.cropRatio)), 0.0f, 100.0f);
	settings.opacity = std::clamp(
	    static_cast<float>(numberOf(arguments, "fillOpacity", settings.opacity * 100.0) / 100.0), 0.0f, 1.0f);
}

/// @brief Sets the colours and modes: foreground, background, swap, reset (black on white), quickMask, cloneSource.
/// @param settings The settings.
/// @param cloneOffset Forgotten when the clone source is set.
/// @param arguments The options set.
/// @throws DocumentError If a colour or the clone source is malformed.
void setColours(ToolSettings& settings, std::optional<imaging::Point>& cloneOffset, const json& arguments) {
	for(const auto& [key, colour] :
	    {std::pair{"foreground", &settings.foreground}, {"background", &settings.background}})
		if(arguments.contains(key)) {
			const auto parsed = colorOf(textOf(arguments, key));
			if(!parsed) throw DocumentError("Not a colour");
			*colour = *parsed;
		}
	if(flagOf(arguments, "swap", false)) std::swap(settings.foreground, settings.background);
	if(flagOf(arguments, "reset", false)) {
		settings.foreground = {0.0f, 0.0f, 0.0f, 1.0f};
		settings.background = {1.0f, 1.0f, 1.0f, 1.0f};
	}
	settings.quickMask = flagOf(arguments, "quickMask", settings.quickMask);
	if(arguments.contains("cloneSource")) {
		settings.cloneSource = pointOf(arguments, "cloneSource");
		cloneOffset.reset();
	}
}

} // namespace

std::span<const std::string_view> gestureNames() noexcept { return gestureNameList; }

std::optional<Gesture> gestureOf(std::string_view name) noexcept {
	const auto index = std::ranges::find(gestureNameList, name) - gestureNameList.begin();
	if(index == std::ssize(gestureNameList)) return std::nullopt;
	return static_cast<Gesture>(index);
}

std::span<const std::string_view> optionKeys() noexcept { return optionKeyList; }

imaging::SelectionMode selectionModeOf(std::string_view name, imaging::SelectionMode fallback) noexcept {
	constexpr std::array<std::string_view, 4> names{"replace", "add", "subtract", "intersect"};
	const auto index = std::ranges::find(names, name) - names.begin();
	return index == std::ssize(names) ? fallback : static_cast<imaging::SelectionMode>(index);
}

imaging::Point pointOf(const json& arguments, std::string_view key) {
	const auto found = arguments.is_object() ? arguments.find(key) : arguments.end();
	if(!arguments.is_object() || found == arguments.end() || !found->is_object())
		throw DocumentError("Missing point " + std::string(key));
	return {static_cast<float>(withinReach(numberOf(*found, "x", 0.0))),
	        static_cast<float>(withinReach(numberOf(*found, "y", 0.0)))};
}

void chooseTool(ImageDocument& image, const std::string& name) {
	const auto& tools = image.definition.tools;
	const auto found = std::ranges::find(tools, name, &ImageTool::name);
	if(found == tools.end()) throw DocumentError("Unknown tool " + name);
	settle(image);
	image.typing = false;
	image.settings.tool = found->name;
	image.settings.gesture = found->gesture;
	setOptions(image.settings, image.cloneOffset, image.limits, found->settings);
}

void setOptions(ToolSettings& settings, std::optional<imaging::Point>& cloneOffset, const imaging::Limits& limits,
                const json& arguments) {
	setBrush(settings.tip, limits, arguments);
	setSampling(settings, arguments);
	setGradient(settings, arguments);
	setColours(settings, cloneOffset, arguments);
	if(arguments.is_object() && arguments.contains("vector")) imaging::readContent(arguments["vector"], settings.vector);
}

} // namespace affresco::format::editing
