/// @file
/// @brief format::editing::ImageDocuments: the images by name, colours, a blank document, the display conversion, the
/// view, the snapshot a screen's state shows, the thumbnails, and the operations' argument readers.

#include "ImageDocuments.hpp"
#include "TypeRegistry.hpp"

#include "affresco/imaging/Composite.hpp"
#include "affresco/imaging/Project.hpp"
#include "affresco/render/Bitmap.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace affresco::format::editing {

namespace {

/// @brief The selection modes' names, in SelectionMode's order.
constexpr std::array<std::string_view, 4> selectionModeNames{"replace", "add", "subtract", "intersect"};

/// @brief The view's size before a canvas has drawn once.
constexpr int defaultViewWidth = 900;
/// @brief The view's size before a canvas has drawn once.
constexpr int defaultViewHeight = 620;
/// @brief The side of a layer's thumbnail.
constexpr int layerThumbnailSide = 32;
/// @brief The side of the navigator's picture.
constexpr int navigatorSide = 160;

/// @brief A layer kind's name.
/// @param kind The kind.
/// @return It.
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

/// @brief The name of a layer's thumbnail among the named images.
/// @param image The image's name.
/// @param id The layer's id.
/// @return "<image>/layer/<id>".
[[nodiscard]] std::string thumbnailName(const std::string& image, std::uint64_t id) {
	return image + "/layer/" + std::to_string(id);
}

/// @brief One row of a layers list.
/// @param image The image's name, for the thumbnail's.
/// @param layer The layer.
/// @param depth Its depth.
/// @return The row.
[[nodiscard]] json layerJson(const std::string& image, const imaging::Layer& layer, int depth) {
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
	            {"thumbnail", layer.kind == imaging::LayerKind::pixel ? thumbnailName(image, layer.id) : ""}};
	row["adjustment"] =
	    layer.kind == imaging::LayerKind::adjustment ? imaging::adjustmentToJson(layer.adjustment) : json(nullptr);
	return row;
}

/// @brief A history list's rows: the opening, then each step.
/// @param history The history.
/// @return The rows.
[[nodiscard]] json historyJson(const imaging::History& history) {
	json rows = json::array({{{"id", "0"}, {"label", "Open"}}});
	const auto& steps = history.steps();
	for(std::size_t index = 0; index < steps.size(); ++index)
		rows.push_back({{"id", std::to_string(index + 1)}, {"label", steps[index].name}});
	return rows;
}

/// @brief The tool options, as the gestures use them (a preset or a key may have changed them).
/// @param settings The tools' settings.
/// @return The options.
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

/// @brief A thumbnail as a bitmap of the named images.
/// @param pixels Its RGBA bytes.
/// @param side Its side.
/// @return The bitmap.
[[nodiscard]] std::shared_ptr<const Bitmap> bitmapOf(const std::vector<std::uint8_t>& pixels, int side) {
	return Bitmap::fromPixels(side, side, pixels, Bitmap::Limits{});
}

} // namespace

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

std::unique_ptr<imaging::Document> blankDocument(int width, int height, imaging::PixelFormat format,
                                                 const imaging::Pixel& background, const imaging::Limits& limits) {
	auto document = std::make_unique<imaging::Document>(width, height, limits, format);
	const std::uint64_t layer = document->structure().active;
	document->changeLayer(layer, "Layer Properties", [](imaging::Layer& changed) { changed.name = "Background"; });
	if(background.alpha > 0.0f) imaging::fillSelection(*document, document->layer(layer)->pixels, background, 1.0f);
	document->clearHistory();
	return document;
}

ImageDocument::ImageDocument(std::string imageName, ImageDefinition read, json definitionSource)
    : name(std::move(imageName)), definition(std::move(read)), source(std::move(definitionSource)),
      limits(definition.limits),
      canvas(std::make_shared<imaging::CanvasModel>(blankDocument(
          definition.width, definition.height, definition.format, definition.background, definition.limits))) {
	chooseTool(*this, definition.tool);
	fitView(*this);
}

ImageDocument::~ImageDocument() {
	stroke.reset();
	preview.reset();
	canvas->onPointer = nullptr;
	canvas->setDisplay(nullptr);
}

ImageDocument& ImageDocuments::image(const std::string& name, const json& definition, const std::string& at) {
	const auto found = images_.find(name);
	if(found != images_.end() && found->second->source == definition) return *found->second;
	ImageDefinition read = readImageDefinition(definition, at);
	auto made = std::make_unique<ImageDocument>(name, std::move(read), definition);
	ImageDocument& image = *made;
	images_.insert_or_assign(name, std::move(made));
	return image;
}

ImageDocument* ImageDocuments::find(std::string_view name) noexcept {
	const auto found = images_.find(name);
	return found == images_.end() ? nullptr : found->second.get();
}

void replaceDocument(ImageDocument& image, std::unique_ptr<imaging::Document> document) {
	image.stroke.reset();
	image.preview.reset();
	image.path.clear();
	image.press = {};
	image.cloneOffset.reset();
	image.canvas->setDocument(std::move(document));
	fitView(image);
}

void updateDisplay(ImageDocument& image) {
	image.canvas->setDisplay(nullptr);
	image.displayTransform.reset();
	if(image.working == image.display && !image.proof) return;
	image.displayTransform.emplace(image.working, image.display, imaging::RenderingIntent::relativeColorimetric,
	                               image.proof);
	image.canvas->setDisplay(&*image.displayTransform);
}

void fitView(ImageDocument& image) {
	const imaging::CanvasView& drawn = image.canvas->view;
	const int width = drawn.width > 0 ? drawn.width : defaultViewWidth;
	const int height = drawn.height > 0 ? drawn.height : defaultViewHeight;
	const imaging::Document& document = image.canvas->document();
	const double fit =
	    0.9 * std::min(static_cast<double>(width) / document.width(), static_cast<double>(height) / document.height());
	image.view.zoom = std::clamp(fit, 1.0 / 64.0, 64.0);
	image.view.x = document.width() / 2.0;
	image.view.y = document.height() / 2.0;
	image.view.width = width;
	image.view.height = height;
}

void updateThumbnails(ImageDocument& image, ImageCache& images) {
	const imaging::Document& document = image.canvas->document();
	if(image.thumbnailRevision == document.revision() && image.thumbnailGeneration == image.canvas->generation())
		return;
	image.thumbnailRevision = document.revision();
	image.thumbnailGeneration = image.canvas->generation();
	for(const auto& [id, layer] : document.structure().layers) {
		if(layer.kind != imaging::LayerKind::pixel) continue;
		const auto pixels = imaging::thumbnail(document, layer.pixels, layerThumbnailSide);
		images.setNamed(thumbnailName(image.name, id), bitmapOf(pixels, layerThumbnailSide));
	}
	images.setNamed(image.name + "/navigator", bitmapOf(imaging::thumbnail(document, 0, navigatorSide), navigatorSide));
}

json snapshot(const ImageDocument& image) {
	const imaging::Document& document = image.canvas->document();
	json layers = json::array();
	for(const auto& [id, depth] : document.listed())
		if(const imaging::Layer* layer = document.layer(id)) layers.push_back(layerJson(image.name, *layer, depth));
	const imaging::History& history = document.history();
	const auto bounds = imaging::selectionBounds(document);
	const ToolSettings& settings = image.settings;
	return {{"width", document.width()},
	        {"height", document.height()},
	        {"format", document.format() == imaging::PixelFormat::rgba8 ? "rgba8" : "rgba16f"},
	        {"profile", image.working.name()},
	        {"display", image.display.name()},
	        {"proof", image.proof ? image.proof->name() : ""},
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
	        {"tool", settings.tool},
	        {"gesture", gestureNames()[static_cast<std::size_t>(settings.gesture)]},
	        {"options", optionsJson(settings)},
	        {"view", {{"zoom", image.view.zoom}, {"x", image.view.x}, {"y", image.view.y}}},
	        {"message", image.message},
	        {"previewing", image.preview != nullptr}};
}

// NOLINTNEXTLINE(bugprone-exception-escape): get() is only reached for a number, which it always reads
double numberOf(const json& arguments, std::string_view key, double fallback) noexcept {
	if(!arguments.is_object()) return fallback;
	const auto found = arguments.find(key);
	if(found == arguments.end() || !found->is_number()) return fallback;
	const double value = found->get<double>();
	// A document may send any number: narrowing one past a float's range to a float would be undefined
	constexpr double most = std::numeric_limits<float>::max();
	return std::isfinite(value) ? std::clamp(value, -most, most) : fallback;
}

int wholeOf(const json& arguments, std::string_view key, int fallback) noexcept {
	// Clamped before the conversion, which is undefined for a number past an int's range
	constexpr double least = std::numeric_limits<int>::min(), most = std::numeric_limits<int>::max();
	return static_cast<int>(std::clamp(numberOf(arguments, key, fallback), least, most));
}

std::string textOf(const json& arguments, std::string_view key, std::string_view fallback) {
	if(!arguments.is_object()) return std::string(fallback);
	const auto found = arguments.find(key);
	return found != arguments.end() && found->is_string() ? found->get<std::string>() : std::string(fallback);
}

// NOLINTNEXTLINE(bugprone-exception-escape): get() is only reached for a boolean, which it always reads
bool flagOf(const json& arguments, std::string_view key, bool fallback) noexcept {
	if(!arguments.is_object()) return fallback;
	const auto found = arguments.find(key);
	return found != arguments.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

std::uint64_t layerOf(const ImageDocument& image, const json& arguments) {
	const imaging::Document& document = image.canvas->document();
	std::uint64_t id = document.structure().active;
	if(const std::string text = textOf(arguments, "id"); !text.empty()) {
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
		if(error != std::errc{} || end != text.data() + text.size()) id = 0;
	}
	if(document.layer(id) == nullptr) throw DocumentError("No such layer");
	return id;
}

void settle(ImageDocument& image) {
	if(image.stroke) image.stroke->finish();
	image.stroke.reset();
	image.preview.reset();
	image.path.clear();
}

std::shared_ptr<ImageDocuments> makeImageDocuments() { return std::make_shared<ImageDocuments>(); }

} // namespace affresco::format::editing
