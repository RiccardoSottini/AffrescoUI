/// @file
/// @brief Tempera's session: tool names, colours, the sample document, the display conversion, the view, and the
/// snapshot the screen's state shows.

#include "tempera/Tempera.hpp"

#include <affresco/imaging/Composite.hpp>
#include <affresco/imaging/Project.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace tempera {

namespace {

/// @brief The tools' names, in Tool's order.
constexpr std::array<std::string_view, 15> toolNameList{"move",     "marquee", "ellipse",    "lasso",  "polygon",
                                                        "wand",     "crop",    "brush",      "eraser", "clone",
                                                        "gradient", "fill",    "eyedropper", "hand",   "zoom"};

/// @brief The selection modes' names, in SelectionMode's order.
constexpr std::array<std::string_view, 4> selectionModeNames{"replace", "add", "subtract", "intersect"};

/// @brief The view's size before the canvas has drawn once.
constexpr int defaultViewWidth = 900;
/// @brief The view's size before the canvas has drawn once.
constexpr int defaultViewHeight = 620;
/// @brief The side of a layer's thumbnail.
constexpr int layerThumbnailSide = 32;
/// @brief The side of the navigator's picture.
constexpr int navigatorSide = 160;

/// @brief A layer kind's name.
/// @param kind The kind.
[[nodiscard]] std::string_view kindName(imaging::LayerKind kind) noexcept {
	switch(kind) {
	case imaging::LayerKind::group:
		return "group";
	case imaging::LayerKind::adjustment:
		return "adjustment";
	case imaging::LayerKind::pixel:
		break;
	}
	return "pixel";
}

/// @brief One row of the Layers panel.
/// @param layer The layer.
/// @param depth Its depth.
[[nodiscard]] json layerJson(const imaging::Layer& layer, int depth) {
	json row = {{"id", std::to_string(layer.id)},
	            {"name", layer.name},
	            {"label", layer.name},
	            {"depth", depth},
	            {"kind", kindName(layer.kind)},
	            {"visible", layer.visible},
	            {"opacity", static_cast<int>(std::lround(layer.opacity * 100.0f))},
	            {"blend", imaging::blendModeName(layer.blend)},
	            {"lockPixels", layer.locks.pixels},
	            {"lockPosition", layer.locks.position},
	            {"lockAll", layer.locks.all},
	            {"clipped", layer.clipped},
	            {"mask", layer.mask != 0},
	            {"thumbnail", layer.kind == imaging::LayerKind::pixel ? "layer-" + std::to_string(layer.id) : ""}};
	row["adjustment"] =
	    layer.kind == imaging::LayerKind::adjustment ? imaging::adjustmentToJson(layer.adjustment) : json(nullptr);
	return row;
}

/// @brief The History panel's rows and place.
/// @param history The history.
[[nodiscard]] json historyJson(const imaging::History& history) {
	json rows = json::array({{{"id", "0"}, {"label", "Open"}}});
	const auto& steps = history.steps();
	for(std::size_t index = 0; index < steps.size(); ++index)
		rows.push_back({{"id", std::to_string(index + 1)}, {"label", steps[index].name}});
	return rows;
}

/// @brief The options bar's values, as the tools use them (a preset or a key may have changed them).
/// @param settings The tools' settings.
[[nodiscard]] json optionsJson(const ToolSettings& settings) {
	const auto percent = [](float share) { return static_cast<int>(std::lround(share * 100.0f)); };
	return {{"size", static_cast<int>(std::lround(settings.tip.size))},
	        {"hardness", percent(settings.tip.hardness)},
	        {"spacing", percent(settings.tip.spacing)},
	        {"opacity", percent(settings.tip.opacity)},
	        {"flow", percent(settings.tip.flow)},
	        {"mode", selectionModeNames.at(static_cast<std::size_t>(settings.mode))},
	        {"tolerance", settings.sample.tolerance},
	        {"contiguous", settings.sample.contiguous},
	        {"allLayers", settings.sample.allLayers},
	        {"fillOpacity", percent(settings.opacity)},
	        {"shape", imaging::gradientShapeNames()[static_cast<std::size_t>(settings.shape)]},
	        {"quickMask", settings.quickMask}};
}

/// @brief Sends the layers' thumbnails and the navigator's picture to the screen, when the document changed.
/// @param session The session.
/// @param screen The screen.
void updateThumbnails(Session& session, affresco::Screen& screen) {
	const imaging::Document& document = session.canvas->document();
	if(session.thumbnailRevision == document.revision() && session.thumbnailGeneration == session.canvas->generation())
		return;
	session.thumbnailRevision = document.revision();
	session.thumbnailGeneration = session.canvas->generation();
	for(const auto& [id, layer] : document.structure().layers) {
		if(layer.kind != imaging::LayerKind::pixel) continue;
		const auto pixels = imaging::thumbnail(document, layer.pixels, layerThumbnailSide);
		screen.setImage("layer-" + std::to_string(id), layerThumbnailSide, layerThumbnailSide, pixels);
	}
	const auto whole = imaging::thumbnail(document, 0, navigatorSide);
	screen.setImage("navigator", navigatorSide, navigatorSide, whole);
}

/// @brief A hex digit pair's value.
/// @param text Two hex digits.
/// @return The value 0 to 1, or std::nullopt.
[[nodiscard]] std::optional<float> channelOf(std::string_view text) noexcept {
	if(text.size() != 2) return std::nullopt;
	unsigned value = 0;
	const char* const last = text.data() + text.size();
	// NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): bounded by last, two characters past data()
	const auto [end, error] = std::from_chars(text.data(), last, value, 16);
	if(error != std::errc{} || end != last) return std::nullopt;
	return imaging::unitFromByte(static_cast<std::uint8_t>(value));
}

} // namespace

std::span<const std::string_view> toolNames() noexcept { return toolNameList; }

std::optional<Tool> toolOf(std::string_view name) noexcept {
	const auto index = std::ranges::find(toolNameList, name) - toolNameList.begin();
	if(index == std::ssize(toolNameList)) return std::nullopt;
	return static_cast<Tool>(index);
}

std::optional<imaging::Pixel> colorOf(std::string_view text) noexcept {
	if(text.starts_with('#')) text.remove_prefix(1);
	if(text.size() != 6 && text.size() != 8) return std::nullopt;
	std::array<float, 4> channels{0.0f, 0.0f, 0.0f, 1.0f};
	for(std::size_t index = 0; index * 2 < text.size(); ++index) {
		// Not substr, which may throw: index * 2 + 2 is within the text.
		const auto channel = channelOf(std::string_view(text.data() + index * 2, 2));
		if(!channel) return std::nullopt;
		channels[index] = *channel;
	}
	return imaging::Pixel{channels[0], channels[1], channels[2], channels[3]};
}

std::string hexOf(const imaging::Pixel& pixel) {
	const imaging::Pixel colour = imaging::clamped(pixel);
	std::array<char, 10> text{};
	const auto byte = [](float value) { return static_cast<unsigned>(imaging::byteFromUnit(value)); };
	if(byte(colour.alpha) == 255)
		std::snprintf(text.data(), text.size(), "#%02x%02x%02x", byte(colour.red), byte(colour.green),
		              byte(colour.blue));
	else
		std::snprintf(text.data(), text.size(), "#%02x%02x%02x%02x", byte(colour.red), byte(colour.green),
		              byte(colour.blue), byte(colour.alpha));
	return text.data();
}

std::unique_ptr<imaging::Document> sampleDocument(int width, int height, const imaging::Limits& limits) {
	auto document = std::make_unique<imaging::Document>(width, height, limits);
	const auto across = static_cast<float>(width), down = static_cast<float>(height);
	const std::uint64_t sky = document->structure().active;
	document->changeLayer(sky, "Layer Properties", [](imaging::Layer& layer) { layer.name = "Sky"; });
	imaging::Gradient dawn;
	dawn.stops = {{0.0f, {0.33f, 0.52f, 0.82f, 1.0f}}, {1.0f, {0.97f, 0.84f, 0.66f, 1.0f}}};
	imaging::drawGradient(*document, document->layer(sky)->pixels, dawn, imaging::GradientShape::linear,
	                      {across / 2.0f, 0.0f}, {across / 2.0f, down * 0.7f}, 1.0f);
	const std::uint64_t sun = document->addPixelLayer("Sun");
	const imaging::Point sunFrom{across * 0.66f, down * 0.16f}, sunTo{across * 0.66f + down * 0.2f, down * 0.36f};
	imaging::fillCoverage(*document, document->layer(sun)->pixels,
	                      imaging::ellipseCoverage(width, height, sunFrom, sunTo), {1.0f, 0.86f, 0.36f, 1.0f}, 1.0f,
	                      "Fill");
	const std::uint64_t hills = document->addPixelLayer("Hills");
	std::vector<imaging::Point> outline;
	constexpr int segments = 48;
	for(int index = 0; index <= segments; ++index) {
		const float share = static_cast<float>(index) / segments;
		outline.push_back(
		    {across * share, down * (0.66f + 0.07f * std::sin(share * 9.0f) + 0.04f * std::sin(share * 23.0f))});
	}
	outline.push_back({across, down});
	outline.push_back({0.0f, down});
	imaging::fillCoverage(*document, document->layer(hills)->pixels,
	                      imaging::polygonCoverage(width, height, outline, limits), {0.23f, 0.47f, 0.31f, 1.0f}, 1.0f,
	                      "Fill");
	document->clearHistory();
	return document;
}

Session::Session(const imaging::Limits& limits)
    : limits(limits), canvas(std::make_shared<imaging::CanvasModel>(sampleDocument(1200, 800, limits))) {
	fitView(*this);
}

Session::~Session() {
	stroke.reset();
	preview.reset();
	canvas->onPointer = nullptr;
	canvas->setDisplay(nullptr);
}

void replaceDocument(Session& session, std::unique_ptr<imaging::Document> document) {
	session.stroke.reset();
	session.preview.reset();
	session.path.clear();
	session.press = {};
	session.cloneOffset.reset();
	session.canvas->setDocument(std::move(document));
	fitView(session);
}

void updateDisplay(Session& session) {
	session.canvas->setDisplay(nullptr);
	session.displayTransform.reset();
	if(session.working == session.display && !session.proof) return;
	session.displayTransform.emplace(session.working, session.display, imaging::RenderingIntent::relativeColorimetric,
	                                 session.proof);
	session.canvas->setDisplay(&*session.displayTransform);
}

void fitView(Session& session) {
	const imaging::CanvasView& drawn = session.canvas->view;
	const int width = drawn.width > 0 ? drawn.width : defaultViewWidth;
	const int height = drawn.height > 0 ? drawn.height : defaultViewHeight;
	const imaging::Document& document = session.canvas->document();
	const double fit =
	    0.9 * std::min(static_cast<double>(width) / document.width(), static_cast<double>(height) / document.height());
	session.view.zoom = std::clamp(fit, 1.0 / 64.0, 64.0);
	session.view.x = document.width() / 2.0;
	session.view.y = document.height() / 2.0;
	session.view.width = width;
	session.view.height = height;
}

json snapshot(Session& session, affresco::Screen& screen) {
	updateThumbnails(session, screen);
	const imaging::Document& document = session.canvas->document();
	json layers = json::array();
	for(const auto& [id, depth] : document.listed())
		if(const imaging::Layer* layer = document.layer(id)) layers.push_back(layerJson(*layer, depth));
	const imaging::History& history = document.history();
	const auto bounds = imaging::selectionBounds(document);
	const ToolSettings& settings = session.settings;
	return {{"width", document.width()},
	        {"height", document.height()},
	        {"format", document.format() == imaging::PixelFormat::rgba8 ? "rgba8" : "rgba16f"},
	        {"profile", session.working.name()},
	        {"display", session.display.name()},
	        {"proof", session.proof ? session.proof->name() : ""},
	        {"revision", document.revision()},
	        {"active", std::to_string(document.structure().active)},
	        {"layers", std::move(layers)},
	        {"history", historyJson(history)},
	        {"position", history.position()},
	        {"dropped", history.dropped()},
	        {"canUndo", history.canUndo()},
	        {"canRedo", history.canRedo()},
	        {"memory", history.memoryBytes()},
	        {"selection", document.hasSelection()},
	        {"bounds", bounds},
	        {"foreground", hexOf(settings.foreground)},
	        {"background", hexOf(settings.background)},
	        {"tool", toolNameList[static_cast<std::size_t>(settings.tool)]},
	        {"options", optionsJson(settings)},
	        {"view", {{"zoom", session.view.zoom}, {"x", session.view.x}, {"y", session.view.y}}},
	        {"message", session.message},
	        {"previewing", session.preview != nullptr}};
}

} // namespace tempera
