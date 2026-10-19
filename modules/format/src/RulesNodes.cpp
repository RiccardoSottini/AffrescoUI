/// @file
/// @brief format::Rules for revisions' nodes and paths: ids for the nodes a revision adds, and hints for paths that
/// find nothing.

#include "affresco/format/NodeIds.hpp"
#include "affresco/format/Rules.hpp"

#include "CheckerImpl.hpp"

#include <charconv>

namespace affresco::format {

namespace {

/// @brief Splits a JSON Pointer into its unescaped keys.
/// @param pointer The pointer ("" is the whole document).
/// @return Its keys.
std::vector<std::string> keysOfPointer(std::string_view pointer) {
	std::vector<std::string> keys;
	std::size_t start = pointer.find('/');
	while(start != std::string_view::npos) {
		const std::size_t end = pointer.find('/', start + 1);
		std::string key(pointer.substr(start + 1, end == std::string_view::npos ? std::string_view::npos : end - start - 1));
		for(std::size_t at = 0; (at = key.find('~', at)) != std::string::npos; ++at)
			key.replace(at, 2, at + 1 < key.size() && key[at + 1] == '1' ? "/" : "~");
		keys.push_back(std::move(key));
		start = end;
	}
	return keys;
}

/// @brief Finds what a value holds under a key, without throwing.
/// @param value An object or a list.
/// @param key A key, or a list index in decimal.
/// @return What it holds, or nullptr if it holds nothing there.
const json* memberOf(const json& value, const std::string& key) {
	if(value.is_object()) {
		const auto it = value.find(key);
		return it != value.end() ? &*it : nullptr;
	}
	std::size_t index = 0;
	const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index);
	if(!value.is_array() || error != std::errc() || end != key.data() + key.size() || index >= value.size())
		return nullptr;
	return &value[index];
}

/// @brief Finds the value at a JSON Pointer, without throwing.
/// @param document The document.
/// @param pointer The pointer.
/// @return The value, or nullptr if there is none (or the pointer is malformed).
const json* valueAt(const json& document, std::string_view pointer) {
	const json* value = &document;
	for(const std::string& key : keysOfPointer(pointer))
		if(value = memberOf(*value, key); !value) return nullptr;
	return value;
}

/// @brief The id of the node a value at a node place goes into, for the words of children that show nothing.
/// @param document The document.
/// @param pointer The node place ("/root/children/2", "/screens/home").
/// @return The parent's id, the screen's, template's or component's name, or "".
std::string parentIdOf(const json& document, std::string_view pointer) {
	if(const std::size_t children = pointer.rfind("/children/"); children != std::string_view::npos) {
		const json* parent = valueAt(document, pointer.substr(0, children));
		return parent && parent->is_object() ? parent->value("id", std::string()) : std::string();
	}
	const std::vector<std::string> keys = keysOfPointer(pointer);
	return keys.size() >= 2 ? keys[1] : std::string();
}

/// @brief The hint for the first key of a pointer that isn't there.
/// @param holder What should hold it.
/// @param where The pointer to @p holder, or the same from its node's "#id".
/// @param key The key.
/// @return The hint.
std::string missingKeyHint(const json& holder, const std::string& where, const std::string& key) {
	if(holder.is_object()) {
		const std::string near = checker::hintFor(key, checker::keysOf(holder), "", "it has:");
		return "'" + where + "' has no '" + key + "' (" + near + ")";
	}
	if(holder.is_array()) {
		if(holder.empty()) return "'" + where + "' is an empty list: use '-' to append";
		return "'" + where + "' is a list of " + std::to_string(holder.size()) + ": use 0 to " +
		       std::to_string(holder.size() - 1) + ", or '-' to append";
	}
	return "'" + where + "' is a " + std::string(holder.type_name()) + ": nothing is inside it";
}

} // namespace

void Rules::nameNodes(const json& document, std::string_view pointer, bool replaces, json& value) const {
	const std::string place(pointer);
	constexpr std::string_view children = "/children";
	if(isNodePlace(place)) {
		if(!value.is_object()) return;
		const json* old = replaces ? valueAt(document, place) : nullptr;
		if(old && old->is_object() && old->contains("id") && !value.contains("id")) value["id"] = (*old)["id"];
		NodeNamer(document).name(value, place, parentIdOf(document, place));
	} else if(place.ends_with(children) && isNodePlace(place.substr(0, place.size() - children.size())) &&
	          value.is_array()) {
		NodeNamer namer(document);
		const json* parent = valueAt(document, place.substr(0, place.size() - children.size()));
		const std::string id = parent && parent->is_object() ? parent->value("id", std::string()) : std::string();
		for(std::size_t i = 0; i < value.size(); ++i) namer.name(value[i], place + "/" + std::to_string(i), id);
	} else if((place == "/screens" || place == "/templates") && value.is_object()) {
		NodeNamer namer(document);
		for(auto& [name, node] : value.items()) namer.name(node, place + "/" + name, name);
	} else if(const std::vector<std::string> keys = keysOfPointer(place);
	          keys.size() == 2 && keys[0] == "components" && value.is_object() && value.contains("node")) {
		NodeNamer(document).name(value["node"], place + "/node", keys[1]);
	}
}

std::string Rules::pathHint(const json& document, std::string_view path) const {
	std::string pointer(path), base, name; // a "#id" path: its node's pointer and "#id", which name what is inside
	if(path.starts_with('#')) {
		const std::size_t slash = path.find('/');
		const std::string id(path.substr(1, slash == std::string_view::npos ? std::string_view::npos : slash - 1));
		const std::optional<std::string> found = findNode(document, id);
		if(!found) return checker::hintFor(id, nodeIds(document), "#", "the ids are:");
		base = *found;
		name = "#" + id;
		pointer = base + (slash == std::string_view::npos ? "" : std::string(path.substr(slash)));
	}

	const std::vector<std::string> keys = keysOfPointer(pointer);
	const json* value = &document;
	std::string at;
	for(std::size_t i = 0; i < keys.size(); ++i) {
		const json* next = memberOf(*value, keys[i]);
		if(!next) {
			if(i + 1 == keys.size() && keys[i] == "-" && value->is_array()) return ""; // appending: it is all there
			const bool inside = !name.empty() && at.size() >= base.size();
			return missingKeyHint(*value, inside ? name + at.substr(base.size()) : at.empty() ? "/" : at, keys[i]);
		}
		at += "/" + keys[i];
		value = next;
	}
	return "";
}

} // namespace affresco::format
