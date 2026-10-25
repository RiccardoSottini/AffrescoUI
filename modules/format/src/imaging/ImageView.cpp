/// @file
/// @brief format::editing: the "image" step's view and tool operations: snapshot, histogram, fit, zoom, setTool,
/// setOptions, setProfile, and pointer, which plays pointer events through the current tool as a view's canvas would.

#include "ImageOperations.hpp"

#include "affresco/imaging/Composite.hpp"

#include <algorithm>
#include <array>

namespace affresco::format::editing {

namespace {

/// @brief The most pointer events one call plays.
constexpr std::size_t mostPoints = 4096;
/// @brief The smallest and largest zoom.
constexpr double leastZoom = 1.0 / 64.0, mostZoom = 64.0;

/// @brief snapshot: changes nothing; the step publishes the snapshot after every operation.
void publishOnly(ImageDocument&, const json&, Window*) {}

/// @brief The most pixels histogram measures: it reads the smallest mip level with at most this many.
constexpr long long histogramPixels = 512LL * 512LL;

/// @brief histogram: counts the image's pixels as shown by brightness, for the snapshot's histogram. Transparent
/// pixels aren't counted; a large image is measured at a mip level.
/// @param image The image.
/// @param with bins (2 to 256, default 64).
void histogram(ImageDocument& image, const json& with, Window*) {
	const imaging::Document& document = image.canvas->document();
	const int bins = std::clamp(wholeOf(with, "bins", 64), 2, 256);
	int level = 0;
	const int levels = document.selection().levels();
	while(level + 1 < levels &&
	      static_cast<long long>(document.width() >> level) * (document.height() >> level) > histogramPixels)
		++level;
	const int width = std::max(1, document.width() >> level), height = std::max(1, document.height() >> level);
	const std::vector<std::uint8_t> pixels = imaging::flatten(document, level, 0, 0, width, height);
	std::vector<int> counts(static_cast<std::size_t>(bins), 0);
	for(std::size_t at = 0; at + 3 < pixels.size(); at += 4) {
		if(pixels[at + 3] == 0) continue;
		const double brightness = 0.2126 * pixels[at] + 0.7152 * pixels[at + 1] + 0.0722 * pixels[at + 2];
		++counts[std::min(static_cast<std::size_t>(brightness * bins / 256.0), counts.size() - 1)];
	}
	image.histogram = std::move(counts);
}

/// @brief fit: zooms and centres the view on the whole image.
/// @param image The image.
void fit(ImageDocument& image, const json&, Window*) { fitView(image); }

/// @brief zoom: multiplies or sets the zoom, and centres the view on a point.
/// @param image The image.
/// @param with by (a factor), to (a zoom; wins over by), x, y (the document point at the centre).
void zoom(ImageDocument& image, const json& with, Window*) {
	imaging::CanvasView& view = image.view;
	view.zoom = std::clamp(view.zoom * numberOf(with, "by", 1.0), leastZoom, mostZoom);
	if(const double to = numberOf(with, "to", 0.0); to > 0.0) view.zoom = std::clamp(to, leastZoom, mostZoom);
	view.x = withinReach(numberOf(with, "x", view.x));
	view.y = withinReach(numberOf(with, "y", view.y));
}

/// @brief setTool: chooses one of the document's tools, which sets its options.
/// @param image The image.
/// @param with tool (its name in the "images" section).
/// @throws DocumentError If there is no such tool.
void setTool(ImageDocument& image, const json& with, Window*) { chooseTool(image, nameOf(with, "tool")); }

/// @brief setOptions: sets the tools' options (optionKeys()).
/// @param image The image.
/// @param with The options.
/// @throws DocumentError If one is malformed.
void setToolOptions(ImageDocument& image, const json& with, Window*) {
	setOptions(image.settings, image.cloneOffset, image.limits, with);
}

/// @brief A profile by its key.
/// @param key "srgb", "displayP3", "wideRgb" or "gray".
/// @return The profile.
/// @throws DocumentError If it is none of them.
[[nodiscard]] imaging::ColorProfile profileOf(std::string_view key) {
	if(key == "srgb") return imaging::ColorProfile::srgb();
	if(key == "displayP3") return imaging::ColorProfile::displayP3();
	if(key == "wideRgb") return imaging::ColorProfile::wideRgb();
	if(key == "gray") return imaging::ColorProfile::gray();
	throw DocumentError("Unknown profile " + std::string(key));
}

/// @brief setProfile: sets the working space (assigned, the numbers kept), the display's or the proof's.
/// @param image The image.
/// @param with working, display, proof ("" for none).
/// @throws DocumentError If a profile is unknown.
void setProfile(ImageDocument& image, const json& with, Window*) {
	if(with.contains("working")) image.working = profileOf(textOf(with, "working"));
	if(with.contains("display")) image.display = profileOf(textOf(with, "display"));
	if(with.contains("proof")) {
		const std::string proof = textOf(with, "proof");
		image.proof = proof.empty() ? std::nullopt : std::optional(profileOf(proof));
	}
	updateDisplay(image);
}

/// @brief Whether a pointer event holds the space bar, which pans whatever the tool.
/// @param point The event: modifiers, a list that may name "space".
/// @return Whether it does.
[[nodiscard]] bool holdsSpace(const json& point) {
	const auto found = point.find("modifiers");
	return found != point.end() && found->is_array() && std::ranges::find(*found, json("space")) != found->end();
}

/// @brief The modifier keys of a pointer event.
/// @param point The event: modifiers, a list of "shift", "control", "alt" and "super" ("space" is holdsSpace()'s).
/// @return The bits.
[[nodiscard]] Mods modifiersOf(const json& point) {
	constexpr std::array<std::string_view, 4> names{"shift", "control", "alt", "super"};
	Mods mods;
	const auto found = point.find("modifiers");
	if(found == point.end() || !found->is_array()) return mods;
	for(const json& name : *found)
		if(name.is_string())
			if(const auto index = std::ranges::find(names, name.get<std::string>()) - names.begin();
			   index < std::ssize(names))
				mods.bits |= 1 << index;
	return mods;
}

/// @brief Plays one pointer event through the current tool, at the view's surface point of its document point.
/// @param image The image.
/// @param point phase ("press", "move", "release" or "scroll"; "kind" is read too), x, y (document pixels),
/// button ("left" or "right"), pressure (0 to 1), scroll, modifiers ("space" pans whatever the tool).
/// @param view The view at the call's start: a pan or zoom on the way doesn't move the points played after it.
void playPoint(ImageDocument& image, const json& point, const imaging::CanvasView& view) {
	const std::string phase = textOf(point, "phase", textOf(point, "kind", "move"));
	imaging::CanvasPointer event;
	event.x = withinReach(numberOf(point, "x", 0.0));
	event.y = withinReach(numberOf(point, "y", 0.0));
	event.pointer.kind = phase == "press"     ? PointerKind::press
	                     : phase == "release" ? PointerKind::release
	                     : phase == "scroll"  ? PointerKind::scroll
	                                          : PointerKind::move;
	event.pointer.position = {static_cast<float>((event.x - view.x) * view.zoom + view.width / 2.0),
	                          static_cast<float>((event.y - view.y) * view.zoom + view.height / 2.0)};
	event.pointer.button = textOf(point, "button") == "right" ? MouseButton::Right : MouseButton::Left;
	event.pointer.scroll = {0.0f, static_cast<float>(std::clamp(numberOf(point, "scroll", 0.0), -1e3, 1e3))};
	event.pointer.mods = modifiersOf(point);
	image.pressure = static_cast<float>(std::clamp(numberOf(point, "pressure", 1.0), 0.0, 1.0));
	const bool held = image.spaceHeld; // the view's space bar
	image.spaceHeld = held || holdsSpace(point);
	applyPointer(image, event);
	image.spaceHeld = held;
}

/// @brief pointer: plays pointer events through the current tool, as a view's canvas does, so scenarios, agents and
/// views share one code path.
/// @param image The image.
/// @param with One event (phase, x, y, button, pressure, scroll, modifiers), or points: a list of up to 4096.
/// @throws DocumentError If the list is too long.
void pointer(ImageDocument& image, const json& with, Window*) {
	const imaging::CanvasView view = image.view;
	const auto points = with.find("points");
	if(points == with.end()) playPoint(image, with, view);
	else if(!points->is_array() || points->size() > mostPoints)
		throw DocumentError("pointer takes up to " + std::to_string(mostPoints) + " points");
	else
		for(const json& point : *points) playPoint(image, point, view);
	image.pressure = 1.0f;
}

} // namespace

std::vector<ImageOperation> viewOperations() {
	return {
	    {"snapshot", {}, "Changes nothing; publishes the image's snapshot.", publishOnly},
	    {"histogram", {"bins"}, "Counts the image's pixels as shown by brightness, for the snapshot's histogram.",
	     histogram},
	    {"fit", {}, "Zooms and centres the view on the whole image.", fit},
	    {"zoom", {"by", "to", "x", "y"}, "Multiplies or sets the zoom, and centres the view on a point.", zoom},
	    {"setTool", {"tool"}, "Chooses one of the document's tools, with its options.", setTool},
	    {"setOptions",
	     {"preset", "size", "sizeStep", "hardness", "spacing", "opacity", "flow", "mode", "tolerance", "contiguous",
	      "allLayers", "fillOpacity", "shape", "gradientPreset", "gradient", "foreground", "background", "swap", "reset",
	      "quickMask", "cloneSource", "autoSelect"},
	     "Sets the tools' options and the colours.",
	     setToolOptions},
	    {"setProfile", {"working", "display", "proof"}, "Sets the working, display or proof colour profile.",
	     setProfile},
	    {"pointer",
	     {"phase", "x", "y", "button", "pressure", "scroll", "modifiers", "points"},
	     "Plays pointer events (document pixels) through the current tool.",
	     pointer},
	};
}

} // namespace affresco::format::editing
