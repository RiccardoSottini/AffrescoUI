/// @file
/// @brief format::Picks, the records the person made by pointing, and describePick(), which makes one from the panels
/// and the document as written. Nothing here evaluates a binding, reads the state or a record, or copies what a panel
/// shows: a record says where something is written, never what the app's data is.

#include "Picks.hpp"

#include "Engine.hpp"
#include "Library.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace affresco::format {

/* The table */

const json& Picks::add(json record) {
	if(records_.size() >= maxKept) records_.pop_front();
	record["id"] = "pick-" + std::to_string(++made_);
	records_.push_back(std::move(record));
	return records_.back();
}

void Picks::remove(std::string_view id) {
	std::erase_if(records_, [id](const json& record) { return record["id"].get_ref<const std::string&>() == id; });
}

bool Picks::holds(std::string_view id) const {
	return std::ranges::any_of(records_,
	                           [id](const json& record) { return record["id"].get_ref<const std::string&>() == id; });
}

json Picks::records(std::span<const std::string> ids) const {
	json found = json::array();
	for(const std::string& id : ids) {
		const auto record = std::ranges::find_if(
		    records_, [&id](const json& kept) { return kept["id"].get_ref<const std::string&>() == id; });
		if(record != records_.end()) found.push_back(*record);
	}
	return found;
}

json Picks::current() const {
	json all = json::array();
	for(const json& record : records_) all.push_back(record);
	return all;
}

void Picks::cancelRunning(Window* window) noexcept {
	if(window && running_) window->cancelPick(running_);
	running_ = 0;
}

/* The record */

namespace {

constexpr std::size_t maxEach = 16;     ///< Bindings and handlers a record lists, each.
constexpr std::size_t maxWritten = 200; ///< A binding as written, a list as written, a name: cut to this many bytes.
constexpr std::size_t maxRepeats = 4;   ///< Enclosing repeats a record lists, the innermost ones.
constexpr std::size_t maxRecord = 4096; ///< The largest record, as JSON.

/// @brief A panel a node built, and its record.
struct Built {
	const Panel* panel = nullptr;   ///< The panel, or nullptr if none.
	const Record* record = nullptr; ///< Its record.
};

/// @brief The nearest panel a node built: a panel itself, or the ancestor whose type created it.
/// @param engine The runtime.
/// @param panel The panel, or nullptr.
/// @return It, or {} if none.
Built builtAt(Engine& engine, const Panel* panel) {
	for(; panel; panel = panel->parent())
		if(const Record* record = engine.reconciler.recordOf(panel)) return {panel, record};
	return {};
}

/// @brief A built panel with an address of its own: the node a revision would name.
struct Addressed {
	Built built;         ///< The panel.
	std::string node;    ///< "#id", or "/root" for a root without one.
	std::string pointer; ///< Where it is written in the document.
};

/// @brief The nearest built panel, from one up, whose node is written in the document: its id is found there, or it
/// is the top panel built from a root without an id.
/// @param engine The runtime.
/// @param document The document as built.
/// @param panel Where to start, or nullptr.
/// @return It, or std::nullopt.
std::optional<Addressed> addressedFrom(Engine& engine, const json& document, const Panel* panel) {
	for(Built built = builtAt(engine, panel); built.panel; built = builtAt(engine, built.panel->parent())) {
		const std::string& id = built.panel->id();
		if(!id.empty()) {
			if(std::optional<std::string> pointer = nodePointer(document, id))
				return Addressed{built, "#" + id, std::move(*pointer)};
		} else if(!builtAt(engine, built.panel->parent()).panel && document.contains("root")) {
			return Addressed{built, "/root", "/root"};
		}
	}
	return std::nullopt;
}

/// @brief A value as written, as a text cut to maxWritten bytes.
/// @param written The value.
/// @return A text as itself, anything else as JSON.
std::string writtenText(const json& written) {
	return cutToCharacter(written.is_string() ? written.get<std::string>() : written.dump(), maxWritten);
}

/// @brief A node's bindings and handlers as written, as Runtime::inspect() gives them, at most maxEach each.
/// @param record The node's record.
/// @param into The record being made: gets "bindings" and "handlers".
void addWritten(const Record& record, json& into) {
	json bindings = json::object();
	for(const CompiledProperty& property : record.node->properties)
		if(property.value.dynamic() && bindings.size() < maxEach)
			bindings[property.key] = writtenText(property.value.written());
	json handlers = json::array();
	for(const auto& [key, handler] : record.handlers.items())
		if(handlers.size() < maxEach) handlers.push_back(cutToCharacter(key, maxWritten));
	into["bindings"] = std::move(bindings);
	into["handlers"] = std::move(handlers);
}

/// @brief The position of a repeat's item: the "[k]" its place ends with, or else its index variable.
/// @param record The item's record.
/// @param repeat The repeat.
/// @return The index.
json indexOf(const Record& record, const CompiledRepeat& repeat) {
	if(const std::size_t open = record.path.rfind('['); open != std::string::npos && record.path.ends_with(']')) {
		const std::string digits = record.path.substr(open + 1, record.path.size() - open - 2);
		if(!digits.empty() && std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
			return std::stoull(digits);
	}
	const json* index = record.locals ? record.locals->find(repeat.index) : nullptr;
	return index && index->is_number_integer() ? *index : json();
}

/// @brief The repeats a panel is an item of, from it up: `[{for, in, index}]`, outermost first, the innermost
/// maxRepeats only. Neither the item nor its key: the list is never evaluated.
/// @param engine The runtime.
/// @param panel The panel picked.
/// @return The list; empty outside any repeat.
json repeatsOf(Engine& engine, const Panel* panel) {
	std::vector<json> found; // innermost first
	for(; panel && found.size() < maxRepeats; panel = panel->parent()) {
		const Record* record = engine.reconciler.recordOf(panel);
		if(!record || !record->node || !record->node->repeat) continue;
		const json& written = record->node->own["repeat"];
		const json in = written.is_object() ? written.value("in", json()) : json();
		found.push_back({{"for", record->node->repeat->item},
		                 {"in", writtenText(in)},
		                 {"index", indexOf(*record, *record->node->repeat)}});
	}
	std::ranges::reverse(found);
	return json(found);
}

/// @brief The component a pointer lies in: `/components/<name>/node...`.
/// @param pointer The pointer.
/// @return The component's name and the prefix of its node, or std::nullopt.
std::optional<std::pair<std::string, std::string>> componentOfPointer(const std::string& pointer) {
	constexpr std::string_view prefix = "/components/";
	if(!pointer.starts_with(prefix)) return std::nullopt;
	const std::size_t end = pointer.find('/', prefix.size());
	if(end == std::string::npos || pointer.compare(end, 5, "/node") != 0) return std::nullopt;
	const std::string token = pointer.substr(prefix.size(), end - prefix.size());
	try {
		return std::pair{json::json_pointer("/" + token).back(), pointer.substr(0, end + 5)};
	} catch(const json::exception&) {
		return std::nullopt;
	}
}

/// @brief Whether a pointer is a place or lies under it.
/// @param pointer The pointer.
/// @param place The place.
/// @return Whether it does.
bool under(std::string_view pointer, std::string_view place) {
	return pointer.starts_with(place) && (pointer.size() == place.size() || pointer[place.size()] == '/');
}

/// @brief The kit's component a node is a use of: its "use" names no component of the document, and the library has
/// one by that name.
/// @param document The document.
/// @param pointer Where the node is written.
/// @return The component's name, or std::nullopt.
std::optional<std::string> kitComponentAt(const json& document, const std::string& pointer) {
	const json& node = document.at(json::json_pointer(pointer));
	const auto use = node.is_object() ? node.find("use") : node.end();
	if(use == node.end() || !use->is_string()) return std::nullopt;
	const std::string& name = use->get_ref<const std::string&>();
	const auto components = document.find("components");
	if(components != document.end() && components->is_object() && components->contains(name)) return std::nullopt;
	return libraryComponent(name) ? std::optional<std::string>(name) : std::nullopt;
}

/// @brief Adds "component" to a record: the document's component its pointer lies in, with the nearest ancestor
/// written outside it; or the kit's component its node uses, the click having been inside it.
/// @param engine The runtime.
/// @param document The document.
/// @param addressed The node described.
/// @param record The record being made.
void addComponent(Engine& engine, const json& document, const Addressed& addressed, json& record) {
	if(const auto component = componentOfPointer(addressed.pointer)) {
		json described = {{"name", cutToCharacter(component->first, maxWritten)}, {"kit", false}};
		for(std::optional<Addressed> up = addressedFrom(engine, document, addressed.built.panel->parent()); up;
		    up = addressedFrom(engine, document, up->built.panel->parent()))
			if(!under(up->pointer, component->second)) {
				described["usedAt"] = up->node;
				break;
			}
		record["component"] = std::move(described);
	} else if(const std::optional<std::string> kit = kitComponentAt(document, addressed.pointer)) {
		record["component"] = {{"name", cutToCharacter(*kit, maxWritten)}, {"kit", true}, {"usedAt", addressed.node}};
	}
}

/// @brief Adds "source" to a record: for a program, the definition that built the node or else its nearest ancestor
/// with one (the record is then not exact); for a JSON document, the node itself.
/// @param engine The runtime.
/// @param addressed The node described.
/// @param record The record being made.
void addSource(Engine& engine, const Addressed& addressed, json& record) {
	if(!engine.versions.sourceDocument().contains("program")) {
		record["source"] = documentSource(engine, addressed.node, addressed.pointer);
		return;
	}
	for(const Panel* panel = addressed.built.panel; panel; panel = panel->parent()) {
		if(panel->id().empty()) continue;
		if(std::optional<json> source = programSource(engine, panel->id())) {
			if(panel != addressed.built.panel) record["exact"] = false;
			record["source"] = std::move(*source);
			return;
		}
	}
}

/// @brief Keeps a record within maxRecord bytes: drops bindings, then handlers, from the last. Everything else is
/// short already: ids, types and names are a document's, and a pointer is as deep as the document's nesting.
/// @param record The record.
void fit(json& record) {
	json& bindings = record["bindings"];
	while(record.dump().size() > maxRecord && !bindings.empty()) bindings.erase(std::prev(bindings.end()));
	json& handlers = record["handlers"];
	while(record.dump().size() > maxRecord && !handlers.empty()) handlers.erase(handlers.size() - 1);
}

} // namespace

std::string cutToCharacter(std::string text, std::size_t most) {
	if(text.size() <= most) return text;
	std::size_t end = most;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end; // not inside a character
	text.resize(end);
	return text;
}

json describePick(Engine& engine, const Panel& panel) {
	const Built hit = builtAt(engine, &panel);
	if(!hit.panel) return nullptr;
	const json& document = engine.versions.document();
	const std::optional<Addressed> addressed = addressedFrom(engine, document, hit.panel);
	if(!addressed) return nullptr;
	const Record& record = *addressed->built.record;

	json described = {{"revision", engine.versions.head().number}};
	if(const std::optional<int> proposal = engine.versions.previewing()) described["proposal"] = *proposal;
	described["node"] = addressed->node;
	described["type"] = record.type;
	described["pointer"] = addressed->pointer;
	const Rect& frame = addressed->built.panel->frame();
	described["bounds"] = {std::lround(frame.left), std::lround(frame.top), std::lround(frame.right - frame.left),
	                       std::lround(frame.bottom - frame.top)};
	addWritten(record, described);
	if(json repeats = repeatsOf(engine, hit.panel); !repeats.empty()) described["repeat"] = std::move(repeats);
	addComponent(engine, document, *addressed, described);
	described["exact"] = addressed->built.panel == hit.panel;
	if(addressed->built.panel != hit.panel) described["hitType"] = hit.record->type;
	addSource(engine, *addressed, described);
	fit(described);
	return described;
}

Panel* pickCandidate(Engine& engine, Panel* panel) {
	for(; panel; panel = panel->parent())
		if(engine.reconciler.recordOf(panel)) return panel;
	return nullptr;
}

std::string pickCaption(Engine& engine, const Panel& panel) {
	const Record* record = engine.reconciler.recordOf(&panel);
	std::string caption = record ? record->type : std::string();
	if(panel.id().empty()) return caption;
	caption += " #" + panel.id();
	if(const std::optional<std::string> definition = definitionName(engine, panel.id())) caption += " · " + *definition;
	return caption;
}

} // namespace affresco::format
