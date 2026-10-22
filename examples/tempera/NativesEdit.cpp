/// @file
/// @brief Tempera's editing natives: selections, the tool and its options, pointer events played by scenarios and
/// agents, crop, filters with a live preview, the colour profiles and fill.

#include "tempera/Tempera.hpp"

#include <affresco/imaging/Project.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace tempera {

namespace {

/// @brief The most tiles a filter preview computes per call: what is visible first, the rest on Apply.
constexpr std::size_t previewTiles = 256;
/// @brief The most pointer events one call plays.
constexpr std::size_t mostPoints = 4096;

/// @brief A selection mode's name.
/// @param name "replace", "add", "subtract" or "intersect".
/// @param fallback When it is none of them.
[[nodiscard]] imaging::SelectionMode modeOf(std::string_view name, imaging::SelectionMode fallback) noexcept {
	constexpr std::array<std::string_view, 4> names{"replace", "add", "subtract", "intersect"};
	const auto* const found = std::ranges::find(names, name);
	return found == names.end() ? fallback : static_cast<imaging::SelectionMode>(found - names.begin());
}

/// @brief A point argument: an object with x and y.
/// @param arguments The arguments.
/// @param key Its key.
[[nodiscard]] imaging::Point pointOf(const json& arguments, std::string_view key) {
	const auto found = arguments.find(key);
	if(found == arguments.end() || !found->is_object())
		throw affresco::DocumentError("Missing point " + std::string(key));
	return {static_cast<float>(withinReach(numberOf(*found, "x", 0.0))),
	        static_cast<float>(withinReach(numberOf(*found, "y", 0.0)))};
}

/// @brief The coverage of a polygon argument, its points at most the limits' polygonPoints.
/// @param session The session.
/// @param arguments points: [{x, y}].
/// @throws affresco::DocumentError If there is no list of points, or too long a one.
[[nodiscard]] imaging::Coverage polygonOf(const Session& session, const json& arguments) {
	const auto found = arguments.find("points");
	if(found == arguments.end() || !found->is_array() || found->size() > session.limits.polygonPoints)
		throw affresco::DocumentError("A polygon needs a list of points");
	std::vector<imaging::Point> points;
	points.reserve(found->size());
	for(const json& point : *found)
		points.push_back({static_cast<float>(withinReach(numberOf(point, "x", 0.0))),
		                  static_cast<float>(withinReach(numberOf(point, "y", 0.0)))});
	const imaging::Document& document = session.canvas->document();
	return imaging::polygonCoverage(document.width(), document.height(), points, session.limits);
}

/// @brief The coverage of a colour range argument.
/// @param session The session; its foreground is the colour when none is given.
/// @param arguments color, fuzziness (0 to 255), allLayers.
/// @throws affresco::DocumentError If the colour isn't one.
[[nodiscard]] imaging::Coverage colourRangeOf(const Session& session, const json& arguments) {
	const auto colour = colorOf(textOf(arguments, "color", hexOf(session.settings.foreground)));
	if(!colour) throw affresco::DocumentError("Not a colour");
	const int fuzziness = std::clamp(wholeOf(arguments, "fuzziness", 40), 0, 255);
	return imaging::colorRangeCoverage(session.canvas->document(), *colour, fuzziness,
	                                   flagOf(arguments, "allLayers", false));
}

/// @brief Selects by a shape, a wand click or a colour range; or all, none, the inverse.
/// @param document The session's document, whose selection changes.
/// @param session The session, read for its settings and limits.
/// @param arguments kind, mode, and the shape's from and to, points, x and y, color and fuzziness.
void select(imaging::Document& document, const Session& session, const json& arguments) {
	const std::string kind = textOf(arguments, "kind");
	const auto mode = modeOf(textOf(arguments, "mode"), session.settings.mode);
	const int width = document.width(), height = document.height();
	if(kind == "all") return imaging::selectAll(document);
	if(kind == "none") return imaging::deselect(document);
	if(kind == "invert") return imaging::invertSelection(document);
	if(kind == "rectangle" || kind == "ellipse") {
		const imaging::Point from = pointOf(arguments, "from"), to = pointOf(arguments, "to");
		const bool box = kind == "rectangle";
		return imaging::select(document, box ? "Rectangular Marquee" : "Elliptical Marquee",
		                       box ? imaging::rectangleCoverage(width, height, from, to)
		                           : imaging::ellipseCoverage(width, height, from, to),
		                       mode);
	}
	if(kind == "polygon") return imaging::select(document, "Polygonal Lasso", polygonOf(session, arguments), mode);
	if(kind == "wand")
		return imaging::select(document, "Magic Wand",
		                       imaging::wandCoverage(document, wholeOf(arguments, "x", 0), wholeOf(arguments, "y", 0),
		                                             session.settings.sample),
		                       mode);
	if(kind == "colorRange") return imaging::select(document, "Color Range", colourRangeOf(session, arguments), mode);
	throw affresco::DocumentError("Unknown selection " + kind);
}

/// @brief A gradient argument: {stops: [{at, color}]} as the gradient editor gives it (or "position" for "at").
/// @param value The value.
/// @throws affresco::DocumentError If it isn't one.
[[nodiscard]] imaging::Gradient gradientOf(const json& value) {
	const auto stops = value.is_object() ? value.find("stops") : value.end();
	if(!value.is_object() || stops == value.end() || !stops->is_array() || stops->size() < 2 ||
	   stops->size() > imaging::Gradient::maxStops)
		throw affresco::DocumentError("A gradient has 2 to 32 stops");
	imaging::Gradient gradient;
	gradient.stops.clear();
	for(const json& stop : *stops) {
		const auto colour = colorOf(textOf(stop, "color"));
		if(!colour) throw affresco::DocumentError("A stop's colour is #rrggbb");
		gradient.stops.push_back(
		    {std::clamp(static_cast<float>(numberOf(stop, "at", numberOf(stop, "position", 0.0))), 0.0f, 1.0f),
			 *colour});
	}
	std::ranges::stable_sort(gradient.stops, {}, &imaging::GradientStop::position);
	return gradient;
}

/// @brief Sets the brush options: preset, size, hardness, spacing, opacity, flow, sizeStep (the [ and ] keys).
/// @param session The session.
/// @param arguments The options set.
void setBrush(Session& session, const json& arguments) {
	imaging::BrushTip& tip = session.settings.tip;
	if(const std::string preset = textOf(arguments, "preset"); !preset.empty()) {
		const auto presets = imaging::brushPresets();
		const auto found = std::ranges::find(presets, preset, &imaging::BrushPreset::name);
		if(found == presets.end()) throw affresco::DocumentError("Unknown brush " + preset);
		tip = found->tip;
	}
	tip.size = static_cast<float>(numberOf(arguments, "size", tip.size));
	if(const double step = numberOf(arguments, "sizeStep", 0.0); step != 0.0)
		tip.size = std::round(tip.size * static_cast<float>(step > 0.0 ? 1.25 : 0.8)) + (step > 0.0 ? 1.0f : 0.0f);
	tip.hardness = static_cast<float>(numberOf(arguments, "hardness", tip.hardness * 100.0) / 100.0);
	tip.spacing = static_cast<float>(numberOf(arguments, "spacing", tip.spacing * 100.0) / 100.0);
	tip.opacity = static_cast<float>(numberOf(arguments, "opacity", tip.opacity * 100.0) / 100.0);
	tip.flow = static_cast<float>(numberOf(arguments, "flow", tip.flow * 100.0) / 100.0);
	tip.normalize(session.limits);
}

/// @brief Sets the selection, fill and gradient options: mode, tolerance, contiguous, allLayers, fillOpacity, shape,
/// gradientPreset, gradient.
/// @param session The session.
/// @param arguments The options set.
void setSampling(Session& session, const json& arguments) {
	ToolSettings& settings = session.settings;
	settings.mode = modeOf(textOf(arguments, "mode"), settings.mode);
	settings.sample.tolerance = std::clamp(wholeOf(arguments, "tolerance", settings.sample.tolerance), 0, 255);
	settings.sample.contiguous = flagOf(arguments, "contiguous", settings.sample.contiguous);
	settings.sample.allLayers = flagOf(arguments, "allLayers", settings.sample.allLayers);
	settings.opacity = std::clamp(
	    static_cast<float>(numberOf(arguments, "fillOpacity", settings.opacity * 100.0) / 100.0), 0.0f, 1.0f);
	if(const std::string shape = textOf(arguments, "shape"); !shape.empty()) {
		const auto names = imaging::gradientShapeNames();
		const auto found = std::ranges::find(names, shape);
		if(found == names.end()) throw affresco::DocumentError("Unknown gradient shape " + shape);
		settings.shape = static_cast<imaging::GradientShape>(found - names.begin());
	}
	if(const std::string preset = textOf(arguments, "gradientPreset"); !preset.empty()) {
		const auto presets = imaging::gradientPresets();
		const auto found = std::ranges::find(presets, preset, &imaging::GradientPreset::name);
		if(found == presets.end()) settings.gradient.reset();
		else settings.gradient = found->gradient;
	}
	if(arguments.contains("gradient")) settings.gradient = gradientOf(arguments["gradient"]);
}

/// @brief Sets the colours and modes: foreground, background, swap (X), reset (D), quickMask (Q).
/// @param session The session.
/// @param arguments The options set.
void setColours(Session& session, const json& arguments) {
	ToolSettings& settings = session.settings;
	for(const auto& [key, colour] :
	    {std::pair{"foreground", &settings.foreground}, {"background", &settings.background}})
		if(arguments.contains(key)) {
			const auto parsed = colorOf(textOf(arguments, key));
			if(!parsed) throw affresco::DocumentError("Not a colour");
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
		session.cloneOffset.reset();
	}
}

/// @brief Plays pointer events on the canvas as if at the view of the call's start, as scenarios and agents do.
/// @param session The session.
/// @param arguments points: [{kind ("press", "move", "release", "scroll"), x, y (document pixels), pressure, button
/// ("left" or "right"), scroll}].
void playPointer(Session& session, const json& arguments) {
	const auto points = arguments.find("points");
	if(points == arguments.end() || !points->is_array() || points->size() > mostPoints)
		throw affresco::DocumentError("pointer takes up to 4096 points");
	const imaging::CanvasView view = session.view;
	for(const json& point : *points) {
		const std::string kind = textOf(point, "kind", "move");
		imaging::CanvasPointer event;
		event.x = withinReach(numberOf(point, "x", 0.0));
		event.y = withinReach(numberOf(point, "y", 0.0));
		event.pointer.kind = kind == "press"     ? affresco::PointerKind::press
		                     : kind == "release" ? affresco::PointerKind::release
		                     : kind == "scroll"  ? affresco::PointerKind::scroll
		                                         : affresco::PointerKind::move;
		event.pointer.position = {static_cast<float>((event.x - view.x) * view.zoom + view.width / 2.0),
		                          static_cast<float>((event.y - view.y) * view.zoom + view.height / 2.0)};
		event.pointer.button =
		    textOf(point, "button") == "right" ? affresco::MouseButton::Right : affresco::MouseButton::Left;
		event.pointer.scroll = {0.0f, static_cast<float>(numberOf(point, "scroll", 0.0))};
		session.pressure = static_cast<float>(std::clamp(numberOf(point, "pressure", 1.0), 0.0, 1.0));
		applyPointer(session, event);
	}
	session.pressure = 1.0f;
}

/// @brief The image filters change: the active pixel layer's, or its mask's when "mask" is true.
/// @param session The session.
/// @param arguments The arguments.
/// @throws affresco::DocumentError If it can't be changed.
[[nodiscard]] std::uint64_t filterTarget(const Session& session, const json& arguments) {
	const imaging::Document& document = session.canvas->document();
	const imaging::Layer* layer = document.activeLayer();
	if(layer == nullptr || layer->kind != imaging::LayerKind::pixel)
		throw affresco::DocumentError("Filters change a pixel layer");
	const std::uint64_t target = flagOf(arguments, "mask", false) && layer->mask != 0 ? layer->mask : layer->pixels;
	if(const auto why = document.paintable(target)) throw affresco::DocumentError(*why);
	return target;
}

/// @brief A filter argument.
/// @param session The session.
/// @param arguments kind, radius, amount (percent), threshold, angle, distance, monochrome, seed.
[[nodiscard]] imaging::Filter filterOf(const Session& session, const json& arguments) {
	imaging::Filter filter;
	const std::string kind = textOf(arguments, "kind", "gaussian-blur");
	const auto parsed = imaging::filterKindOf(kind);
	if(!parsed) throw affresco::DocumentError("Unknown filter " + kind);
	filter.kind = *parsed;
	filter.radius = static_cast<float>(numberOf(arguments, "radius", filter.radius));
	filter.amount = static_cast<float>(numberOf(arguments, "amount", filter.amount * 100.0) / 100.0);
	filter.threshold = wholeOf(arguments, "threshold", filter.threshold);
	filter.angle = static_cast<float>(numberOf(arguments, "angle", filter.angle));
	filter.distance = static_cast<float>(numberOf(arguments, "distance", filter.distance));
	filter.monochrome = flagOf(arguments, "monochrome", filter.monochrome);
	filter.seed = static_cast<std::uint32_t>(std::clamp(numberOf(arguments, "seed", filter.seed), 0.0, 4294967295.0));
	filter.normalize(session.limits);
	return filter;
}

/// @brief Previews a filter over what the view shows; the preview is a step in progress until applied or cancelled.
/// @param session The session.
/// @param arguments The filter.
void previewFilter(Session& session, const json& arguments) {
	if(session.stroke) settle(session);
	session.preview.reset();
	session.previewed = filterOf(session, arguments);
	auto& document = session.canvas->document();
	session.preview = std::make_unique<imaging::FilterRun>(document, filterTarget(session, arguments),
	                                                       session.previewed, textOf(arguments, "name", "Filter"));
	const imaging::CanvasView& view = session.view;
	const double halfWidth = view.width / 2.0 / view.zoom, halfHeight = view.height / 2.0 / view.zoom;
	const int left = std::max(0, static_cast<int>(view.x - halfWidth));
	const int top = std::max(0, static_cast<int>(view.y - halfHeight));
	const int right = std::min(document.width(), static_cast<int>(std::ceil(view.x + halfWidth)));
	const int bottom = std::min(document.height(), static_cast<int>(std::ceil(view.y + halfHeight)));
	if(right > left && bottom > top) session.preview->runRegion(left, top, right - left, bottom - top, previewTiles);
}

/// @brief A profile by its key.
/// @param key "srgb", "displayP3", "wideRgb" or "gray".
[[nodiscard]] imaging::ColorProfile profileOf(std::string_view key) {
	if(key == "srgb") return imaging::ColorProfile::srgb();
	if(key == "displayP3") return imaging::ColorProfile::displayP3();
	if(key == "wideRgb") return imaging::ColorProfile::wideRgb();
	if(key == "gray") return imaging::ColorProfile::gray();
	throw affresco::DocumentError("Unknown profile " + std::string(key));
}

/// @brief Sets the working space (assigned, the numbers kept), the display's or the proof's ("" for none).
/// @param session The session.
/// @param arguments working, display, proof.
void setProfile(Session& session, const json& arguments) {
	if(arguments.contains("working")) session.working = profileOf(textOf(arguments, "working"));
	if(arguments.contains("display")) session.display = profileOf(textOf(arguments, "display"));
	if(arguments.contains("proof")) {
		const std::string proof = textOf(arguments, "proof");
		session.proof = proof.empty() ? std::nullopt : std::optional(profileOf(proof));
	}
	updateDisplay(session);
}

/// @brief Registers the filters' natives.
/// @param screen The screen.
/// @param session The session.
void registerFilters(affresco::Screen& screen, const std::weak_ptr<Session>& session) {
	addNative(screen, "tempera.previewFilter", session,
	          [](Session& self, const json& arguments) { previewFilter(self, arguments); });
	addNative(screen, "tempera.applyFilter", session, [](Session& self, const json& arguments) {
		if(!self.preview || arguments.contains("kind")) previewFilter(self, arguments);
		self.preview->finish();
		self.preview.reset();
	});
	addNative(screen, "tempera.cancelFilter", session, [](Session& self, const json&) {
		if(self.preview) self.preview->cancel();
		self.preview.reset();
	});
	addNative(screen, "tempera.feather", session, [](Session& self, const json& arguments) {
		settle(self);
		imaging::featherSelection(self.canvas->document(), static_cast<float>(numberOf(arguments, "radius", 4.0)));
	});
}

} // namespace

void registerEditing(affresco::Screen& screen, const std::shared_ptr<Session>& session) {
	const std::weak_ptr<Session> weak = session;
	addNative(screen, "tempera.select", weak, [](Session& self, const json& arguments) {
		settle(self);
		select(self.canvas->document(), self, arguments);
	});
	addNative(screen, "tempera.setTool", weak, [](Session& self, const json& arguments) {
		const auto tool = toolOf(textOf(arguments, "tool"));
		if(!tool) throw affresco::DocumentError("Unknown tool " + textOf(arguments, "tool"));
		settle(self);
		self.settings.tool = *tool;
	});
	addNative(screen, "tempera.setOptions", weak, [](Session& self, const json& arguments) {
		setBrush(self, arguments);
		setSampling(self, arguments);
		setColours(self, arguments);
	});
	addNative(screen, "tempera.pointer", weak,
	          [](Session& self, const json& arguments) { playPointer(self, arguments); });
	addNative(screen, "tempera.crop", weak, [](Session& self, const json& arguments) {
		settle(self);
		crop(self, wholeOf(arguments, "x", 0), wholeOf(arguments, "y", 0), wholeOf(arguments, "width", 0),
		     wholeOf(arguments, "height", 0));
	});
	addNative(screen, "tempera.setProfile", weak,
	          [](Session& self, const json& arguments) { setProfile(self, arguments); });
	addNative(screen, "tempera.fillSelection", weak, [](Session& self, const json& arguments) {
		settle(self);
		const auto colour = colorOf(textOf(arguments, "color", hexOf(self.settings.foreground)));
		if(!colour) throw affresco::DocumentError("Not a colour");
		imaging::fillSelection(self.canvas->document(), filterTarget(self, arguments), *colour, self.settings.opacity);
	});
	addNative(screen, "tempera.zoom", weak, [](Session& self, const json& arguments) {
		self.view.zoom = std::clamp(self.view.zoom * numberOf(arguments, "by", 1.0), 1.0 / 64.0, 64.0);
		if(const double to = numberOf(arguments, "to", 0.0); to > 0.0)
			self.view.zoom = std::clamp(to, 1.0 / 64.0, 64.0);
		self.view.x = withinReach(numberOf(arguments, "x", self.view.x));
		self.view.y = withinReach(numberOf(arguments, "y", self.view.y));
	});
	registerFilters(screen, weak);
}

} // namespace tempera
