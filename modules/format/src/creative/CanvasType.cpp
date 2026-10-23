/// @file
/// @brief The "canvas" node type: a CanvasView, its items (a list the document gives, or a host's model), its levels
/// of detail, grid, snapping, rulers and colours, its view (camera, selection, guides) as its value, and onTransform.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/CanvasView.hpp"
#include "affresco/render/RichText.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxItems = 10'000;   ///< Items a document gives at most; a host's model takes more.
constexpr std::size_t maxLevels = 256;     ///< Levels of detail a canvas reads at most.
constexpr std::size_t maxTextBytes = 4096; ///< An item's text, in bytes, at most.
constexpr float maxCoordinate = 1e9f;      ///< World coordinates and sizes stay within this.
constexpr std::array<float, 6> textSizes{8.0f, 12.0f, 16.0f, 24.0f, 36.0f, 48.0f}; ///< The text's font sizes.

/// @brief The items a document gives: a model of its own, so that a host's model is told apart and left alone.
class DocumentItems final : public CanvasItemModel {
public:
	/// @brief Takes the items.
	/// @param items The items, in drawing order.
	explicit DocumentItems(std::vector<CanvasItem> items) noexcept : items_(std::move(items)) {}
	/// @copydoc CanvasItemModel::size
	[[nodiscard]] std::size_t size() const noexcept override { return items_.size(); }
	/// @copydoc CanvasItemModel::item
	[[nodiscard]] const CanvasItem& item(std::size_t index) const noexcept override { return items_[index]; }

private:
	std::vector<CanvasItem> items_; ///< The items.
};

/// @brief Reads a number within limits.
/// @param value The value.
/// @param at Its place, for errors.
/// @param low The least it may be.
/// @param high The most it may be.
/// @return The number.
/// @throws DocumentError If it isn't a number within them.
float within(const json& value, const std::string& at, float low, float high) {
	const float read = number(value, at);
	if(std::isnan(read) || read < low || read > high)
		fail(at, "expected a number from " + text(json(low)) + " to " + text(json(high)));
	return read;
}

/// @brief Reads one item: {id, x, y, width, height, rotate, color, borderColor, borderWidth, radius, text, textSize,
/// textColor}.
/// @param value The item.
/// @param reader Reads colours.
/// @param at Its place, for errors.
/// @return The item.
CanvasItem readItem(const json& value, const PropertyReader& reader, const std::string& at) {
	object(value, at);
	CanvasItem item;
	item.id = value.contains("id") ? text(value["id"]) : std::string();
	if(item.id.empty()) fail(at + ".id", "an item needs an id");
	const auto coordinate = [&](const char* key, float low) {
		return value.contains(key) ? within(value[key], at + "." + key, low, maxCoordinate) : 0.0f;
	};
	const float x = coordinate("x", -maxCoordinate), y = coordinate("y", -maxCoordinate);
	item.bounds = {x, y, x + coordinate("width", 0.0f), y + coordinate("height", 0.0f)};
	if(value.contains("rotate")) item.rotate = within(value["rotate"], at + ".rotate", -3600.0f, 3600.0f);
	if(value.contains("color")) item.color = reader.color(value["color"], at + ".color");
	if(value.contains("borderColor")) item.borderColor = reader.color(value["borderColor"], at + ".borderColor");
	if(value.contains("borderWidth")) item.borderWidth = within(value["borderWidth"], at + ".borderWidth", 0, 1e4f);
	if(value.contains("radius")) item.radius = within(value["radius"], at + ".radius", 0.0f, maxCoordinate);
	if(value.contains("text")) {
		const std::string words = text(value["text"]);
		if(words.size() > maxTextBytes) fail(at + ".text", "at most 4096 bytes");
		item.text = codepointsOf(words);
	}
	if(value.contains("textSize")) item.textSize = within(value["textSize"], at + ".textSize", 0.0f, 1e6f);
	if(value.contains("textColor")) item.textColor = reader.color(value["textColor"], at + ".textColor");
	return item;
}

/// @brief Sets the items the document gives, and the fonts their text needs; without "items", takes away only
/// items the document gave (a host's model stays).
/// @param canvas The canvas.
/// @param values The node's values.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyItems(CanvasView& canvas, const json& values, const PropertyReader& reader, const std::string& path) {
	if(!values.contains("items")) {
		if(dynamic_cast<const DocumentItems*>(canvas.model().get())) canvas.setModel(nullptr);
		return;
	}
	const json& given = values["items"];
	if(!given.is_array()) fail(path + ".items", "expected a list of items");
	std::vector<CanvasItem> items;
	items.reserve(std::min(given.size(), maxItems));
	bool anyText = false;
	for(std::size_t i = 0; i < given.size() && i < maxItems; ++i) {
		items.push_back(readItem(given[i], reader, concatenate(path, ".items[", std::to_string(i) + "]")));
		anyText = anyText || !items.back().text.empty();
	}
	std::vector<std::shared_ptr<const Font>> fonts;
	if(anyText) { // only then: a document has a few font sizes to spend
		const std::shared_ptr<const Font> base =
		    values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr;
		for(const float size : textSizes)
			if(const auto font = reader.fontVariant(base.get(), size, FontStyle{}, path + ".font"))
				fonts.push_back(font);
	}
	canvas.setTextFonts(std::move(fonts));
	canvas.setModel(std::make_shared<DocumentItems>(std::move(items)));
}

/// @brief Sets the levels of detail: "lod", a list of {for, below, use}: below the zoom "below", the child "use"
/// stands in for the child "for". Levels no longer listed are taken away.
/// @param canvas The canvas.
/// @param values The node's values.
/// @param path The node's place, for errors.
void applyLevels(CanvasView& canvas, const json& values, const std::string& path) {
	std::vector<std::pair<std::string, std::vector<CanvasLevel>>> wanted;
	if(values.contains("lod")) {
		const json& given = values["lod"];
		if(!given.is_array()) fail(path + ".lod", "expected a list of levels");
		for(std::size_t i = 0; i < given.size() && i < maxLevels; ++i) {
			const std::string at = concatenate(path, ".lod[", std::to_string(i) + "]");
			const json& level = object(given[i], at);
			const std::string child = level.contains("for") ? text(level["for"]) : std::string();
			const std::string use = level.contains("use") ? text(level["use"]) : std::string();
			if(child.empty() || use.empty()) fail(at, R"(a level needs "for" and "use": the ids of two children)");
			const float below = within(level.value("below", json(0)), at + ".below", 0.0f, Camera2D::maximumZoom);
			const auto found =
			    std::ranges::find(wanted, child, &std::pair<std::string, std::vector<CanvasLevel>>::first);
			(found != wanted.end() ? found->second : wanted.emplace_back(child, std::vector<CanvasLevel>{}).second)
			    .push_back({below, use});
		}
	}
	std::vector<std::string> gone;
	for(const auto& [child, levels] : canvas.levels())
		if(std::ranges::find(wanted, child, &std::pair<std::string, std::vector<CanvasLevel>>::first) == wanted.end())
			gone.push_back(child);
	for(const std::string& child : gone) canvas.setLevels(child, {});
	for(auto& [child, levels] : wanted) canvas.setLevels(child, std::move(levels));
}

/// @brief Sets a canvas from its node: items, levels of detail, grid, snapping, rulers and accent colour.
/// @param panel The panel, a CanvasView.
/// @param values Its values, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyCanvas(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* canvas = dynamic_cast<CanvasView*>(&panel);
	if(!canvas) return;
	applyItems(*canvas, values, reader, path);
	applyLevels(*canvas, values, path);
	canvas->setGrid(values.contains("grid") ? within(values["grid"], path + ".grid", 0.0f, 1e6f) : 0.0f);
	canvas->setSnap(!values.contains("snap") || boolean(values["snap"], path + ".snap"));
	canvas->setRulers(!values.contains("rulers") || boolean(values["rulers"], path + ".rulers"));
	canvas->setAccentColor(values.contains("accentColor") ? reader.color(values["accentColor"], path + ".accentColor")
	                                                      : colors::accent);
}

/// @brief Reads up to a limit of finite numbers from a list; anything else is skipped (the state is untrusted).
/// @param value The list.
/// @param limit The most to read.
/// @return The numbers.
std::vector<float> numbersOf(const json& value, std::size_t limit) {
	std::vector<float> numbers;
	if(!value.is_array()) return numbers;
	for(std::size_t i = 0; i < value.size() && numbers.size() < limit; ++i)
		if(value[i].is_number() && std::isfinite(value[i].get<double>())) numbers.push_back(value[i].get<float>());
	return numbers;
}

/// @brief Reads a view's camera over the current one: each of "x", "y" and "zoom" only if it is a number.
/// @param camera The view's "camera".
/// @param next The current camera.
/// @return The camera.
Camera2D cameraOf(const json& camera, Camera2D next) {
	const auto read = [&](const char* key, float& into) {
		if(const auto found = camera.find(key); found != camera.end() && found->is_number()) into = found->get<float>();
	};
	read("x", next.x);
	read("y", next.y);
	read("zoom", next.zoom);
	return next;
}

/// @brief Reads a view's selection: its string and number ids, up to the most a canvas selects.
/// @param selection The view's "selection", a list.
/// @return The ids.
std::vector<std::string> selectionOf(const json& selection) {
	std::vector<std::string> ids;
	for(std::size_t i = 0; i < selection.size() && ids.size() < CanvasView::maximumSelection; ++i)
		if(selection[i].is_string() || selection[i].is_number()) ids.push_back(text(selection[i]));
	return ids;
}

/// @brief Shows the view: the bound value, else "view"; {camera: {x, y, zoom}, selection: [ids], guides: {vertical,
/// horizontal}}, each part only if given.
/// Its last text is not used.
/// @param panel The panel, a CanvasView.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showView(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* canvas = dynamic_cast<CanvasView*>(&panel);
	const json& view = bound ? *bound : properties.contains("view") ? properties["view"] : emptyObject();
	if(!canvas || !view.is_object()) return;
	if(const auto camera = view.find("camera"); camera != view.end() && camera->is_object())
		canvas->setCamera(cameraOf(*camera, canvas->camera()));
	if(const auto selection = view.find("selection"); selection != view.end() && selection->is_array())
		canvas->setSelection(selectionOf(*selection));
	if(const auto guides = view.find("guides"); guides != view.end() && guides->is_object())
		canvas->setGuides({numbersOf(guides->value("vertical", json()), CanvasView::maximumGuides),
		                   numbersOf(guides->value("horizontal", json()), CanvasView::maximumGuides)});
}

/// @brief The view of a canvas, as showView() reads it.
/// @param canvas The canvas.
/// @return {camera, selection, guides}.
json viewOf(const CanvasView& canvas) {
	const Camera2D& camera = canvas.camera();
	return {{"camera", {{"x", camera.x}, {"y", camera.y}, {"zoom", camera.zoom}}},
	        {"selection", canvas.selection()},
	        {"guides", {{"vertical", canvas.guides().vertical}, {"horizontal", canvas.guides().horizontal}}}};
}

/// @brief Reports the view when a person changes it.
/// @param panel The panel, a CanvasView.
/// @param changed Gets the view.
/// @return The connection.
Connection listenToView(Panel& panel, std::function<void(const json&)> changed) {
	auto* canvas = dynamic_cast<CanvasView*>(&panel);
	if(!canvas) return 0;
	return canvas->onViewChanged.connect([canvas, changed = std::move(changed)] { changed(viewOf(*canvas)); });
}

/// @brief Disconnects listenToView().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToView(Panel& panel, Connection connection) {
	if(auto* canvas = dynamic_cast<CanvasView*>(&panel)) canvas->onViewChanged.disconnect(connection);
}

/// @brief Runs "onTransform" when a person moves, resizes, rotates or nudges items.
/// @param panel The panel, a CanvasView.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToCanvasEvents(Panel& panel, EventSink sink) {
	auto* canvas = dynamic_cast<CanvasView*>(&panel);
	if(!canvas) return {};
	const Connection transform = canvas->onTransform.connect([sink = std::move(sink)](
	                                                             const std::vector<CanvasTransform>& moved) {
		json items = json::array();
		for(const CanvasTransform& item : moved)
			items.push_back({{"id", item.id},
			                 {"x", item.bounds.left},
			                 {"y", item.bounds.top},
			                 {"width", item.bounds.width()},
			                 {"height", item.bounds.height()},
			                 {"rotate", item.rotate}});
		sink("onTransform", json{{"type", "transform"}, {"items", std::move(items)}});
	});
	return {transform, 0, 0, 0};
}

/// @brief Disconnects listenToCanvasEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToCanvasEvents(Panel& panel, const EventConnections& connections) {
	if(auto* canvas = dynamic_cast<CanvasView*>(&panel)) canvas->onTransform.disconnect(connections[0]);
}

} // namespace

void registerCanvasType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath}, {"view", ValueKind::Any},    {"items", ValueKind::Any},
	                   {"lod", ValueKind::Any},        {"grid", ValueKind::Number}, {"snap", ValueKind::Boolean},
	                   {"rulers", ValueKind::Boolean}, {"font", ValueKind::Font},   {"accentColor", ValueKind::Color}};
	set->apply = applyCanvas;
	set->everyUpdate = false; // up to 10,000 items: read again only when the values change
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const CanvasView*>(&panel) != nullptr; };
	set->value = {"view", showView, listenToView, stopListeningToView};
	set->events = {listenToCanvasEvents, stopListeningToCanvasEvents};

	registry.add({"canvas", "An infinite canvas: children and items in world units, a camera, selection and guides",
	              [] { return std::make_unique<CanvasView>(); }, set, false});
}

} // namespace affresco::format
