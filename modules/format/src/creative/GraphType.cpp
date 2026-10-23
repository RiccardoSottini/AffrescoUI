/// @file
/// @brief The "graph" node type: a GraphView of nodes and links from state, its camera and selection, and its edits
/// (connect, disconnect, a link dropped on empty space, move, select, delete) as the node's handlers.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/GraphView.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr double maxCoordinate = 1'000'000.0; ///< The farthest a node or the pan is from the origin, in graph units.
constexpr double maxNodeSize = 4'000.0;       ///< The widest a node, and the tallest its body, in graph units.
constexpr double maxPortLinks = 1'000.0;      ///< The most links a port can be said to take.

/// @brief The place of an item of a list, for errors: "path[3]".
/// @param path The list's place.
/// @param index The item's position.
/// @return The place.
std::string itemPlace(const std::string& path, std::size_t index) { return path + "[" + std::to_string(index) + "]"; }

/// @brief Reads a number within a range.
/// @param value The value.
/// @param at Its place, for errors.
/// @param low The smallest it may be.
/// @param high The largest it may be.
/// @return The number.
/// @throws DocumentError If it isn't a number within the range.
float numberWithin(const json& value, const std::string& at, double low, double high) {
	const double read = number(value, at);
	if(std::isnan(read) || read < low || read > high)
		fail(at, "expected a number from " + text(json(low)) + " to " + text(json(high)));
	return static_cast<float>(read);
}

/// @brief Reads a text key of an object, or a default when it is missing.
/// @param object The object.
/// @param key The key.
/// @param path The object's place, for errors.
/// @param otherwise The default.
/// @return The text.
std::string textOr(const json& object, const char* key, const std::string& path, std::string otherwise = {}) {
	return object.contains(key) ? string(object[key], path + "." + key) : std::move(otherwise);
}

/// @brief Reads a node's "inputs" or "outputs": texts (the id), or { id, label, kind, links }.
/// @param value The list.
/// @param path Its place, for errors.
/// @return The ports.
/// @throws DocumentError If it isn't such a list, has more than 64 or two of the same id.
std::vector<GraphPort> portsOf(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of ports");
	if(value.size() > GraphView::maximumPorts) fail(path, "too many ports (at most 64)");
	std::vector<GraphPort> ports;
	ports.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		GraphPort port;
		if(entry.is_string()) port.id = string(entry, place);
		else {
			if(!entry.is_object() || !entry.contains("id"))
				fail(place, "expected an id, or { id, label, kind, links }");
			port.id = string(entry["id"], place + ".id");
			port.label = textOr(entry, "label", place);
			port.kind = textOr(entry, "kind", place, "any");
			if(entry.contains("links"))
				port.links =
				    static_cast<std::size_t>(numberWithin(entry["links"], place + ".links", 0.0, maxPortLinks));
		}
		for(const GraphPort& other : ports)
			if(other.id == port.id) fail(place + ".id", "another port has the id \"" + port.id + "\"");
		ports.push_back(std::move(port));
	}
	return ports;
}

/// @brief Reads "nodes": { id, title, x, y, width, bodyHeight, inputs, outputs }.
/// @param value The list.
/// @param path Its place, for errors.
/// @return The nodes.
/// @throws DocumentError If it isn't such a list, has more than 10,000 or two of the same id.
std::vector<GraphNode> nodesOf(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of nodes");
	if(value.size() > GraphView::maximumNodes) fail(path, "too many nodes (at most 10000)");
	std::vector<GraphNode> nodes;
	nodes.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(!entry.is_object() || !entry.contains("id")) fail(place, "expected { id, title, x, y, inputs, outputs }");
		GraphNode node;
		node.id = text(entry["id"]);
		node.title = textOr(entry, "title", place);
		if(entry.contains("x")) node.position.x = numberWithin(entry["x"], place + ".x", -maxCoordinate, maxCoordinate);
		if(entry.contains("y")) node.position.y = numberWithin(entry["y"], place + ".y", -maxCoordinate, maxCoordinate);
		if(entry.contains("width")) node.width = numberWithin(entry["width"], place + ".width", 16.0, maxNodeSize);
		if(entry.contains("bodyHeight"))
			node.bodyHeight = numberWithin(entry["bodyHeight"], place + ".bodyHeight", 0.0, maxNodeSize);
		if(entry.contains("inputs")) node.inputs = portsOf(entry["inputs"], place + ".inputs");
		if(entry.contains("outputs")) node.outputs = portsOf(entry["outputs"], place + ".outputs");
		nodes.push_back(std::move(node));
	}
	// Ids are checked once all are read: sorting copies is O(n log n) where comparing pairs would be O(n²)
	std::vector<std::pair<std::string_view, std::size_t>> ids;
	ids.reserve(nodes.size());
	for(std::size_t i = 0; i < nodes.size(); ++i) ids.emplace_back(nodes[i].id, i);
	std::ranges::sort(ids);
	for(std::size_t i = 1; i < ids.size(); ++i)
		if(ids[i].first == ids[i - 1].first)
			fail(itemPlace(path, ids[i].second) + ".id",
			     "another node has the id \"" + std::string(ids[i].first) + "\"");
	return nodes;
}

/// @brief Reads "links": { from, output, to, input }.
/// @param value The list.
/// @param path Its place, for errors.
/// @return The links (whether they name real ports isn't checked: the view skips those that don't).
/// @throws DocumentError If it isn't such a list or has more than 50,000.
std::vector<GraphLink> linksOf(const json& value, const std::string& path) {
	if(!value.is_array()) fail(path, "expected a list of links");
	if(value.size() > GraphView::maximumLinks) fail(path, "too many links (at most 50000)");
	std::vector<GraphLink> links;
	links.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const json& entry = value[i];
		const std::string place = itemPlace(path, i);
		if(!entry.is_object() || !entry.contains("from") || !entry.contains("output") || !entry.contains("to") ||
		   !entry.contains("input"))
			fail(place, "expected { from, output, to, input }");
		links.push_back({text(entry["from"]), text(entry["output"]), text(entry["to"]), text(entry["input"])});
	}
	return links;
}

/// @brief Reads "pan": [x, y], where the graph's origin is in the view.
/// @param value The value.
/// @param at Its place, for errors.
/// @return The pan.
/// @throws DocumentError If it isn't two numbers within a million.
Vec2 panOf(const json& value, const std::string& at) {
	if(!value.is_array() || value.size() != 2) fail(at, "expected [x, y]");
	return {numberWithin(value[0], at + "[0]", -maxCoordinate, maxCoordinate),
	        numberWithin(value[1], at + "[1]", -maxCoordinate, maxCoordinate)};
}

/// @brief Reads "selectedIds": an id, or a list of ids.
/// @param value The value.
/// @return The ids (at most as many as the nodes a graph keeps).
std::vector<std::string> idsOf(const json& value) {
	std::vector<std::string> ids;
	if(value.is_string() || value.is_number()) ids.push_back(text(value));
	else if(value.is_array())
		for(std::size_t i = 0; i < value.size() && i < GraphView::maximumNodes; ++i)
			if(value[i].is_string() || value[i].is_number()) ids.push_back(text(value[i]));
	return ids;
}

/// @brief Gives a graph the colours its node gives each kind of node, in "kindColors".
/// @param view The graph.
/// @param values The node's values.
/// @param reader Reads colours, through the theme.
/// @param path The node's place, for errors.
/// @throws DocumentError If it isn't an object of at most 64 colours.
void applyKindColors(GraphView& view, const json& values, const PropertyReader& reader, const std::string& path) {
	if(!values.contains("kindColors")) return;
	const json& kinds = values["kindColors"];
	if(!kinds.is_object()) fail(path + ".kindColors", R"(expected { "kind": colour })");
	if(kinds.size() > GraphView::maximumKinds) fail(path + ".kindColors", "too many kinds (at most 64)");
	for(const auto& [kind, color] : kinds.items())
		view.setKindColor(kind, reader.color(color, concatenate(path, ".kindColors.", kind)));
}

/// @brief Gives a graph the camera its node asks for, if it asks for one.
/// @param view The graph.
/// @param values The node's values: "zoom" and "pan".
/// @param path The node's place, for errors.
/// @throws DocumentError If a value can't be read.
void applyGraphCamera(GraphView& view, const json& values, const std::string& path) {
	if(!values.contains("zoom") && !values.contains("pan")) return;
	GraphCamera camera;
	if(values.contains("zoom"))
		camera.zoom = numberWithin(values["zoom"], path + ".zoom", GraphCamera::minimumZoom, GraphCamera::maximumZoom);
	if(values.contains("pan")) camera.pan = panOf(values["pan"], path + ".pan");
	view.setGivenCamera(camera);
}

/// @brief Shows a graph node: its nodes and links, rules, colours, font, minimap, camera and selection.
/// @param panel The panel, a GraphView.
/// @param values Its values, if given.
/// @param reader Reads colours and fonts, through the theme.
/// @param path The node's place, for errors.
void applyGraph(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<GraphView*>(&panel);
	if(!view) return;
	GraphModel model;
	if(values.contains("nodes")) model.nodes = nodesOf(values["nodes"], path + ".nodes");
	if(values.contains("links")) model.links = linksOf(values["links"], path + ".links");
	view->setModel(std::move(model));
	view->setCyclesAllowed(values.contains("cycles") && values["cycles"].is_boolean() && values["cycles"].get<bool>());
	applyKindColors(*view, values, reader, path);
	const auto colorOr = [&](const char* key, Color otherwise) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : otherwise;
	};
	view->setColors(colorOr("nodeColor", Color::hex(0x2b2e36)), colorOr("headerColor", Color::hex(0x3d424e)),
	                colorOr("textColor", Color::hex(0xeceef2)), colorOr("accentColor", colors::accent));
	view->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	view->setMinimap(!values.contains("minimap") || !values["minimap"].is_boolean() || values["minimap"].get<bool>());
	applyGraphCamera(*view, values, path);
	// "selectedIds", not "selected": every node's "selected" says whether it is selected itself
	if(values.contains("selectedIds")) view->setGivenSelection(idsOf(values["selectedIds"]));
}

/// @brief A link as an event's fields.
/// @param link The link.
/// @param type The event's type.
/// @return {"type", "from", "output", "to", "input"}.
json linkEvent(const GraphLink& link, const char* type) {
	return {{"type", type}, {"from", link.from}, {"output", link.output}, {"to", link.to}, {"input", link.input}};
}

/// @brief The ids of nodes given by their indices.
/// @param model The model.
/// @param indices The indices.
/// @return The ids.
json idsAt(const GraphModel& model, const std::vector<std::size_t>& indices) {
	json ids = json::array();
	for(const std::size_t index : indices)
		if(index < model.nodes.size()) ids.push_back(model.nodes[index].id);
	return ids;
}

/// @brief Runs "onConnect", "onDisconnect", "onLinkDrop", "onMove", "onSelect" and "onDelete" for the user's edits.
/// @param panel The panel, a GraphView.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToGraphEvents(Panel& panel, EventSink sink) {
	auto* view = dynamic_cast<GraphView*>(&panel);
	if(!view) return {};
	// The view outlives the connection: stopListeningToGraphEvents() disconnects it before the panel goes
	const Connection edit = view->onEdit.connect([view, sink = std::move(sink)](const GraphEvent& event) {
		const GraphModel& model = view->model();
		switch(event.kind) {
		case GraphEvent::Kind::Connect:
			sink("onConnect", linkEvent(event.link, "connect"));
			break;
		case GraphEvent::Kind::Disconnect: {
			json fields = linkEvent(event.link, "disconnect");
			fields["index"] = static_cast<std::int64_t>(event.linkIndex);
			sink("onDisconnect", std::move(fields));
			break;
		}
		case GraphEvent::Kind::LinkDrop:
			sink("onLinkDrop", json{{"type", "linkDrop"},
			                        {"node", event.node},
			                        {"port", event.port},
			                        {"output", event.output},
			                        {"x", event.at.x},
			                        {"y", event.at.y}});
			break;
		case GraphEvent::Kind::Move: {
			json moved = json::array();
			for(std::size_t i = 0; i < event.nodes.size() && i < event.positions.size(); ++i)
				if(event.nodes[i] < model.nodes.size())
					moved.push_back({{"id", model.nodes[event.nodes[i]].id},
					                 {"index", static_cast<std::int64_t>(event.nodes[i])},
					                 {"x", std::round(event.positions[i].x)},
					                 {"y", std::round(event.positions[i].y)}});
			sink("onMove", json{{"type", "move"},
			                    {"ids", idsAt(model, event.nodes)},
			                    {"dx", std::round(event.delta.x)},
			                    {"dy", std::round(event.delta.y)},
			                    {"nodes", std::move(moved)}});
			break;
		}
		case GraphEvent::Kind::Select:
			sink("onSelect", json{{"type", "select"}, {"ids", idsAt(model, event.nodes)}});
			break;
		case GraphEvent::Kind::Delete:
			sink("onDelete", json{{"type", "delete"}, {"ids", idsAt(model, event.nodes)}});
			break;
		}
	});
	return {edit, 0, 0, 0};
}

/// @brief Disconnects listenToGraphEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToGraphEvents(Panel& panel, const EventConnections& connections) {
	if(auto* view = dynamic_cast<GraphView*>(&panel)) view->onEdit.disconnect(connections[0]);
}

} // namespace

void registerGraphType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {
	    {"nodes", ValueKind::Any},       {"links", ValueKind::Any},         {"cycles", ValueKind::Boolean},
	    {"kindColors", ValueKind::Any},  {"nodeColor", ValueKind::Color},   {"headerColor", ValueKind::Color},
	    {"textColor", ValueKind::Color}, {"accentColor", ValueKind::Color}, {"font", ValueKind::Font},
	    {"minimap", ValueKind::Boolean}, {"zoom", ValueKind::Number},       {"pan", ValueKind::Any},
	    {"selectedIds", ValueKind::Any}};
	set->apply = applyGraph;
	set->everyUpdate = true; // colours follow the theme; an unchanged model, camera or selection is skipped
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const GraphView*>(&panel) != nullptr; };
	set->events = {listenToGraphEvents, stopListeningToGraphEvents};

	registry.add({"graph", "A node editor: nodes with ports, links between them, a minimap; edits run its handlers",
	              [] { return std::make_unique<GraphView>(); }, set, false});
}

} // namespace affresco::format
