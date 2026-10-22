/// @file
/// @brief Contribution points across apps: the "points" section, typed exports, a feature's needs and an app's
/// contributions as tools print them.

#include "ContributionPoints.hpp"
#include "ExpressionParser.hpp"
#include "NodeKeys.hpp"

#include <algorithm>
#include <format>

namespace affresco::format {

namespace {

using expect::fail;

/// The keys of a point.
constexpr auto pointKeys = std::to_array<std::string_view>({"slot", "receives", "description", "note"});

/// The keys of a need.
constexpr auto needKeys = std::to_array<std::string_view>(
    {"kind", "type", "item", "arguments", "slot", "capability", "description", "note", "optional"});

/// The types a field or an argument can have: JSON Schema's, and "image" (the name of an image the host draws).
constexpr auto fieldTypes =
    std::to_array<std::string_view>({"string", "number", "integer", "boolean", "array", "object", "null", "image"});

/// Longest name a point, a need or a field may have.
constexpr std::size_t maxName = 64;

/// Most fields an item, or arguments an action, may have.
constexpr std::size_t maxFields = 32;

/// @brief Tells whether a list of names holds one.
/// @param names The names.
/// @param name The name.
/// @return true if it does.
template <std::size_t N> bool among(const std::array<std::string_view, N>& names, std::string_view name) {
	return std::ranges::find(names, name) != names.end();
}

/// @brief Tells whether a text is one identifier.
/// @param text The text.
/// @return true for a letter or '_', then letters, digits or '_', at most maxName long.
bool identifier(std::string_view text) {
	const auto start = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
	return !text.empty() && text.size() <= maxName && start(text.front()) &&
	       std::ranges::all_of(text.substr(1), [&](char c) { return start(c) || (c >= '0' && c <= '9'); });
}

/// @brief Tells whether a text is identifiers joined by dots ("panels.dock").
/// @param text The text.
/// @return true if it is.
bool dottedName(std::string_view text) {
	if(text.empty() || text.size() > maxName) return false;
	for(std::size_t start = 0;;) {
		const auto dot = text.find('.', start);
		if(!identifier(text.substr(start, dot - start))) return false;
		if(dot == std::string_view::npos) return true;
		start = dot + 1;
	}
}

/// @brief Refuses a key an object doesn't know.
/// @param value The object.
/// @param keys The keys it may have.
/// @param place Its place.
/// @throws DocumentError On an unknown key.
template <std::size_t N>
void knownKeys(const json& value, const std::array<std::string_view, N>& keys, const std::string& place) {
	for(const auto& [key, item] : value.items())
		if(!among(keys, key)) fail(concatenate(place, ".", key), "unknown key");
}

/// @brief Whether a value is the name of a field's type.
/// @param type The value.
/// @return Whether it is "string", "number", "integer", "boolean", "array", "object", "null" or "image".
bool fieldType(const json& type) { return type.is_string() && among(fieldTypes, type.get_ref<const std::string&>()); }

/// @brief The error for a value that isn't the name of a field's type.
constexpr const char* expectedFieldType =
    "expected a type: string, number, integer, boolean, array, object, null or image";

/// @brief Checks `{ "<name>": <type> }`: an item's fields or an action's arguments.
/// @param fields The object.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkFields(const json& fields, const std::string& place) {
	expect::object(fields, place);
	if(fields.size() > maxFields) fail(place, std::format("more than {} fields", maxFields));
	for(const auto& [field, type] : fields.items()) {
		if(!identifier(field)) fail(concatenate(place, ".", field), "must be an identifier");
		if(!fieldType(type)) fail(concatenate(place, ".", field), expectedFieldType);
	}
}

/// @brief Reads a text key if it is there.
/// @param object The object.
/// @param key The key.
/// @param place The object's place.
/// @throws DocumentError If it is there and isn't a text.
void optionalText(const json& object, const char* key, const std::string& place) {
	if(const auto value = object.find(key); value != object.end()) expect::string(*value, place + "." + key);
}

/// @brief Checks what a slot need or a capability need points at: its "slot" or its "capability".
/// @param need The need.
/// @param kind Its kind.
/// @param place Its place.
/// @throws DocumentError If one is missing, or there for another kind.
void checkNeedTarget(const json& need, const std::string& kind, const std::string& place) {
	if(kind == "slot") {
		if(!need.contains("slot") || !need["slot"].is_string() || !among(pointSlots, need["slot"].get<std::string>()))
			fail(place + ".slot", "expected a slot: dock, panel, menu or toolbar");
	} else if(need.contains("slot")) fail(place + ".slot", "only a slot need has one");
	if(kind == "capability") {
		const std::string capability = need.contains("capability") ? expect::string(need["capability"], place) : "";
		if(!capability.starts_with("services.") && !capability.starts_with("connections."))
			fail(place + ".capability", R"(expected "services.<name>" or "connections.<name>")");
	} else if(need.contains("capability")) fail(place + ".capability", "only a capability need has one");
}

/// @brief Checks the parts of a need its kind gives meaning to.
/// @param need The need.
/// @param kind Its kind.
/// @param place Its place.
/// @throws DocumentError If one is missing, or there for another kind.
void checkNeedParts(const json& need, const std::string& kind, const std::string& place) {
	checkNeedTarget(need, kind, place);
	if(const auto item = need.find("item"); item != need.end()) checkFields(*item, place + ".item");
	if(const auto arguments = need.find("arguments"); arguments != need.end()) {
		if(kind != "action") fail(place + ".arguments", "only an action need has arguments");
		checkFields(*arguments, place + ".arguments");
	}
	if(const auto type = need.find("type"); type != need.end() && !fieldType(*type))
		fail(place + ".type", expectedFieldType);
}

/// @brief An export's contract, compactly: kind, type, item and arguments.
/// @param entry The export.
/// @return `{ "kind", "type", ... }`.
json contractOf(const json& entry) {
	json contract = {{"kind", exportKind(entry)}, {"type", entry.value("type", json())}};
	for(const char* key : {"item", "arguments"})
		if(entry.contains(key)) contract[key] = entry[key];
	return contract;
}

/// @brief Prints a contract's item or arguments: "{id: string, image: image}".
/// @param fields The fields.
/// @return The text.
std::string fieldsText(const json& fields) {
	std::string text;
	for(const auto& [field, type] : fields.items())
		text +=
		    std::format("{}{}: {}", text.empty() ? "" : ", ", field, type.is_string() ? type.get<std::string>() : "?");
	return "{" + text + "}";
}

/// @brief Prints a contract or a need on one line: "list of {id: string}", "action (id: string)", "slot dock".
/// @param contract The contract or the need.
/// @return The text.
std::string contractText(const json& contract) {
	const std::string kind = contract.value("kind", std::string("value"));
	if(kind == "slot") return "slot " + contract.value("slot", std::string());
	if(kind == "capability") return "capability " + contract.value("capability", std::string());
	std::string text = kind;
	if(contract.contains("item")) text += " of " + fieldsText(contract["item"]);
	if(contract.contains("arguments")) text += " with " + fieldsText(contract["arguments"]);
	if(contract.contains("type") && contract["type"].is_string() && kind != "action")
		text += " (" + contract["type"].get<std::string>() + ")";
	return text;
}

} // namespace

void checkPointsSection(const json& section, const std::string& name) {
	expect::object(section, name);
	for(const auto& [point, entry] : section.items()) {
		const std::string at = concatenate(name, ".", point);
		if(!dottedName(point)) fail(at, "must be a name like panels.dock");
		expect::object(entry, at);
		knownKeys(entry, pointKeys, at);
		if(!entry.contains("slot") || !entry["slot"].is_string() ||
		   !among(pointSlots, entry["slot"].get<std::string>()))
			fail(at + ".slot", "expected a slot: dock, panel, menu or toolbar");
		if(const auto receives = entry.find("receives"); receives != entry.end()) {
			if(!receives->is_array() || receives->size() > maxFields)
				fail(at + ".receives", "expected a list of exports");
			for(const json& exported : *receives)
				if(!exported.is_string() || !dottedName(exported.get_ref<const std::string&>()))
					fail(at + ".receives", "expected the names of exports");
		}
		optionalText(entry, "description", at);
		optionalText(entry, "note", at);
	}
}

void checkExportContract(const json& entry, const std::string& place) {
	const std::string kind = entry.contains("kind") ? expect::string(entry["kind"], place + ".kind") : "value";
	if(!among(exportKinds, kind))
		fail(place + ".kind", "expected a kind: value, state, list, action, model, surface or image");
	if(const auto item = entry.find("item"); item != entry.end()) checkFields(*item, place + ".item");
	if(const auto arguments = entry.find("arguments"); arguments != entry.end()) {
		if(kind != "action") fail(place + ".arguments", "only an action export has arguments");
		checkFields(*arguments, place + ".arguments");
	}
}

void checkNeeds(const json& needs, const std::string& place) {
	expect::object(needs, place);
	if(needs.size() > maxNeeds) fail(place, std::format("more than {} needs", maxNeeds));
	for(const auto& [name, need] : needs.items()) {
		const std::string at = concatenate(place, ".", name);
		if(!identifier(name)) fail(at, "must be an identifier");
		expect::object(need, at);
		knownKeys(need, needKeys, at);
		const std::string kind = need.contains("kind") ? expect::string(need["kind"], at + ".kind") : "";
		if(!among(needKinds, kind))
			fail(at + ".kind",
			     "expected a kind: value, state, list, action, model, surface, image, slot or capability");
		checkNeedParts(need, kind, at);
		optionalText(need, "description", at);
		optionalText(need, "note", at);
		if(const auto optional = need.find("optional"); optional != need.end())
			expect::boolean(*optional, at + ".optional");
	}
}

std::string exportKind(const json& entry) {
	const auto kind = entry.find("kind");
	return kind != entry.end() && kind->is_string() ? kind->get<std::string>() : "value";
}

std::string actionTarget(const json& entry) {
	const std::string value = entry.value("value", std::string());
	if(value.size() < 5 || !value.starts_with("{{") || !value.ends_with("}}")) return {};
	try {
		const json tree = detail::parse(std::string_view(value).substr(2, value.size() - 4), "exports");
		if(tree.contains("$value") && tree["$value"].is_string()) return tree["$value"].get<std::string>();
	} catch(const DocumentError&) {
		return {}; // not an expression: no target
	}
	return {};
}

std::set<std::string, std::less<>> grantedActions(const json& document) {
	std::set<std::string, std::less<>> granted;
	const auto points = document.find("points");
	const auto exports = document.find("exports");
	if(points == document.end() || !points->is_object() || exports == document.end() || !exports->is_object())
		return granted;
	for(const auto& [point, entry] : points->items())
		if(entry.is_object() && entry.contains("receives") && entry["receives"].is_array())
			for(const json& name : entry["receives"])
				if(name.is_string() && exports->contains(name.get<std::string>())) {
					const json& exported = (*exports)[name.get<std::string>()];
					if(exported.is_object() && exportKind(exported) == "action")
						if(std::string action = actionTarget(exported); !action.empty())
							granted.insert(std::move(action));
				}
	return granted;
}

json partOf(const json& manifest) {
	expect::object(manifest, "manifest");
	if(const auto needs = manifest.find("needs"); needs != manifest.end()) checkNeeds(*needs, "manifest.needs");
	for(const char* key : {"name", "release", "title"}) optionalText(manifest, key, "manifest");
	json part = {{"name", manifest.value("name", "")},
	             {"release", manifest.value("release", "")},
	             {"title", manifest.value("title", "")},
	             {"needs", manifest.value("needs", json::object())}};
	json views = json::array();
	const json provides = manifest.value("provides", json());
	if(provides.contains("views") && provides["views"].is_object())
		for(const auto& [point, nodes] : provides["views"].items()) views.push_back(point);
	part["views"] = std::move(views);
	return part;
}

json contributionsOf(const json& document, const json& parts) {
	json result = {{"points", json::object()}, {"exports", json::object()}, {"parts", parts}};
	if(const auto exports = document.find("exports"); exports != document.end() && exports->is_object())
		for(const auto& [name, entry] : exports->items())
			if(entry.is_object()) result["exports"][name] = contractOf(entry);
	if(const auto points = document.find("points"); points != document.end() && points->is_object())
		for(const auto& [name, entry] : points->items()) {
			if(!entry.is_object()) continue;
			json point = {{"slot", entry.value("slot", "")}, {"receives", entry.value("receives", json::array())}};
			if(entry.contains("description")) point["description"] = entry["description"];
			result["points"][name] = std::move(point);
		}
	return result;
}

std::string contributionsText(const json& contributions) {
	std::string text;
	for(const json entries = contributions.value("points", json::object());
	    const auto& [name, point] : entries.items()) {
		std::string receives;
		for(const json& exported : point.value("receives", json::array()))
			receives += (receives.empty() ? "" : ", ") + exported.get<std::string>();
		text += std::format("point {} ({}): receives {}\n", name, point.value("slot", ""),
		                    receives.empty() ? "nothing" : receives);
	}
	for(const json entries = contributions.value("exports", json::object());
	    const auto& [name, contract] : entries.items())
		text += std::format("export {}: {}\n", name, contractText(contract));
	for(const json& part : contributions.value("parts", json::array())) {
		text +=
		    std::format("part {} {}: {}\n", part.value("name", ""), part.value("release", ""), part.value("title", ""));
		for(const json entries = part.value("needs", json::object()); const auto& [need, wanted] : entries.items())
			text += std::format("  needs {}: {}{}\n", need, contractText(wanted),
			                    wanted.value("optional", false) ? " (optional)" : "");
	}
	return text.empty() ? "no contributions\n" : text;
}

} // namespace affresco::format
