/// @file
/// @brief Picks.hpp: a request's picks kept to the contract's keys and sizes, and the first message's block about them.
/// The records come from the host but end in a model's prompt, so nothing in them is trusted: every key is checked for
/// its type and cut to its size, and the definition or node shown is read from the document the agent read only.

#include "Picks.hpp"

#include "affresco/intelligence/ScreenQueries.hpp"

#include <exception>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <vector>

namespace affresco::intelligence {

using affresco::json;

namespace {

/// The most bytes of a text in a record.
constexpr std::size_t maxText = 200;

/// The most bytes of an address or a JSON Pointer in a record.
constexpr std::size_t maxAddress = 512;

/// The most bindings, and handlers, a record keeps.
constexpr std::size_t maxEntries = 16;

/// The most repeats a record keeps.
constexpr std::size_t maxRepeats = 4;

/// The most bytes of a record's line in the block (a record within its bounds is far smaller; only one made of
/// characters JSON escapes comes near).
constexpr std::size_t maxRecordLine = 3000;

/// The most bytes of a definition's text in the block.
constexpr std::size_t maxDefinition = 8000;

/// The most bytes of a JSON document's node in the block.
constexpr std::size_t maxNode = 4000;

/// @brief A text cut to a size on a character boundary.
/// @param text The text.
/// @param size The most bytes.
/// @return It.
std::string clipped(std::string_view text, std::size_t size) {
	if(text.size() <= size) return std::string(text);
	while(size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80) --size;
	return std::string(text.substr(0, size));
}

/// @brief A text cut to a size on a character boundary, saying so when it is cut.
/// @param text The text.
/// @param size The most bytes.
/// @return It.
std::string shortened(std::string_view text, std::size_t size) {
	return text.size() <= size ? std::string(text) : clipped(text, size) + "\n(cut short)";
}

/// @brief An object's text member, cut to a size.
/// @param from The object.
/// @param key The member.
/// @param size The most bytes.
/// @return It, or nullopt if it isn't a text.
std::optional<std::string> textOf(const json& from, const char* key, std::size_t size) {
	const auto found = from.find(key);
	if(found == from.end() || !found->is_string()) return std::nullopt;
	return clipped(found->get_ref<const std::string&>(), size);
}

/// @brief Copies an object's members of one kind, each only if it is of that kind (texts are cut to a size).
/// @param from The object.
/// @param to Where they go.
/// @param keys The members.
/// @param kind "text", "integer" or "flag".
/// @param size The most bytes of a text.
void copyMembers(const json& from, json& to, std::initializer_list<const char*> keys, std::string_view kind,
                 std::size_t size = maxText) {
	for(const char* key : keys) {
		const auto found = from.find(key);
		if(found == from.end()) continue;
		if(kind == "text" && found->is_string()) to[key] = clipped(found->get_ref<const std::string&>(), size);
		else if((kind == "integer" && found->is_number_integer()) || (kind == "flag" && found->is_boolean()))
			to[key] = *found;
	}
}

/// @brief A record's bindings: at most maxEntries, each a property's text as written.
/// @param in Its "bindings".
/// @return Them (an empty object if none is kept).
json bindingsOf(const json& in) {
	json out = json::object();
	if(!in.is_object()) return out;
	for(const auto& [property, written] : in.items()) {
		if(out.size() >= maxEntries) break;
		if(written.is_string() && property.size() <= maxText)
			out[property] = clipped(written.get_ref<const std::string&>(), maxText);
	}
	return out;
}

/// @brief A record's handlers: at most maxEntries texts.
/// @param in Its "handlers".
/// @return Them.
json handlersOf(const json& in) {
	json out = json::array();
	if(!in.is_array()) return out;
	for(const json& handler : in) {
		if(out.size() >= maxEntries) break;
		if(handler.is_string()) out.push_back(clipped(handler.get_ref<const std::string&>(), maxText));
	}
	return out;
}

/// @brief A record's repeats, outermost first: at most maxRepeats, each { for, in, index }.
/// @param in Its "repeat".
/// @return Them.
json repeatsOf(const json& in) {
	json out = json::array();
	if(!in.is_array()) return out;
	for(const json& item : in) {
		if(out.size() >= maxRepeats) break;
		if(!item.is_object()) continue;
		json kept = json::object();
		copyMembers(item, kept, {"for", "in"}, "text");
		copyMembers(item, kept, {"index"}, "integer");
		out.push_back(std::move(kept));
	}
	return out;
}

/// @brief A record's bounds: four numbers, or nothing.
/// @param in Its "bounds".
/// @return Them, or nullopt.
std::optional<json> boundsOf(const json& in) {
	if(!in.is_array() || in.size() != 4) return std::nullopt;
	for(const json& number : in)
		if(!number.is_number()) return std::nullopt;
	return in;
}

/// @brief A record's object member, kept to its keys: the component it is written in, or its source.
/// @param in The member.
/// @param source Whether it is "source" (else "component").
/// @return It, or nullopt if it isn't an object.
std::optional<json> partOf(const json& in, bool source) {
	if(!in.is_object()) return std::nullopt;
	json kept = json::object();
	if(source) {
		copyMembers(in, kept, {"address"}, "text", maxAddress);
		copyMembers(in, kept, {"kind", "file"}, "text");
		copyMembers(in, kept, {"line", "column"}, "integer");
		copyMembers(in, kept, {"shared"}, "flag");
	} else {
		copyMembers(in, kept, {"name", "usedAt"}, "text");
		copyMembers(in, kept, {"kit"}, "flag");
	}
	return kept;
}

/// @brief One record, kept to the contract.
/// @param item The record as sent.
/// @return It, or nullopt if it has no node text.
std::optional<json> recordOf(const json& item) {
	if(!item.is_object()) return std::nullopt;
	const std::optional<std::string> node = textOf(item, "node", maxText);
	if(!node || node->empty()) return std::nullopt;
	json kept = json::object();
	copyMembers(item, kept, {"id"}, "text");
	copyMembers(item, kept, {"revision"}, "integer");
	kept["node"] = *node;
	copyMembers(item, kept, {"type", "hitType"}, "text");
	copyMembers(item, kept, {"pointer"}, "text", maxAddress);
	if(item.contains("bounds"))
		if(std::optional<json> bounds = boundsOf(item["bounds"])) kept["bounds"] = std::move(*bounds);
	if(json bindings = bindingsOf(item.value("bindings", json())); !bindings.empty())
		kept["bindings"] = std::move(bindings);
	if(json handlers = handlersOf(item.value("handlers", json())); !handlers.empty())
		kept["handlers"] = std::move(handlers);
	if(json repeats = repeatsOf(item.value("repeat", json())); !repeats.empty()) kept["repeat"] = std::move(repeats);
	if(std::optional<json> component = partOf(item.value("component", json()), false))
		kept["component"] = std::move(*component);
	copyMembers(item, kept, {"exact"}, "flag");
	if(std::optional<json> source = partOf(item.value("source", json()), true)) kept["source"] = std::move(*source);
	copyMembers(item, kept, {"proposal"}, "integer");
	return kept;
}

/// @brief The value at an address in the document, or nullopt if there is nothing there.
/// @param document The document.
/// @param address "#id" or a JSON Pointer.
/// @param depth Levels of children to keep whole (-1: all).
/// @return It.
std::optional<json> readAt(const json& document, const std::string& address, int depth = -1) {
	try {
		return nodeAt(document, address, depth);
	} catch(const std::exception&) {
		return std::nullopt;
	}
}

/// @brief What a pick is built by, in a program: the definition's text at its address.
/// @param address The definition's address (source.address).
/// @param document The document the agent read.
/// @param shown The addresses already shown, to which it is added.
/// @return The text.
std::string definitionPart(const std::string& address, const json& document, std::vector<std::string>& shown) {
	for(const std::string& earlier : shown)
		if(earlier == address) return "It is built by the same definition as above.";
	shown.push_back(address);
	const std::optional<json> text = readAt(document, address);
	if(!text || !text->is_string()) return "It is built by the definition at " + address + ", which could not be read.";
	return "It is built by this definition:\n" + shortened(text->get_ref<const std::string&>(), maxDefinition);
}

/// @brief A pick's node, in a JSON document: the node at its address, to depth 2.
/// @param node The node's address ("#id").
/// @param document The document the agent read.
/// @param shown The addresses already shown, to which it is added.
/// @return The text.
std::string nodePart(const std::string& node, const json& document, std::vector<std::string>& shown) {
	for(const std::string& earlier : shown)
		if(earlier == node) return "The node is the same as above.";
	shown.push_back(node);
	// only a node: an address that isn't "#id" could name a value of the state, which a pick never brings
	const std::optional<json> found = node.starts_with('#') ? readAt(document, node, 2) : std::nullopt;
	if(!found || !found->is_object()) return "The node " + node + " could not be read.";
	return "The node:\n" + shortened(compact(*found), maxNode);
}

/// @brief One pick's part of the block: its record on one line, then its definition or node.
/// @param pick The record.
/// @param document The document the agent read.
/// @param shown The addresses already shown.
/// @return It.
std::string pickPart(const json& pick, const json& document, std::vector<std::string>& shown) {
	const std::string line = "- " + shortened(compact(pick), maxRecordLine) + "\n";
	const auto source = pick.find("source");
	const std::optional<std::string> address =
	    source != pick.end() && source->is_object() ? textOf(*source, "address", maxAddress) : std::nullopt;
	if(address && address->starts_with("/program/")) return line + definitionPart(*address, document, shown);
	const std::string node = textOf(pick, "node", maxText).value_or("");
	return line + nodePart(node, document, shown);
}

/// @brief How many nodes a document's trees hold: root, screens, templates, overlays and components' nodes.
/// @param document The document.
/// @return The count.
std::size_t nodeCount(const json& document) {
	std::vector<const json*> waiting;
	if(const auto root = document.find("root"); root != document.end()) waiting.push_back(&*root);
	for(const char* section : {"screens", "templates", "overlays", "components"}) {
		const auto found = document.find(section);
		if(found == document.end() || !found->is_object()) continue;
		for(const json& entry : *found) {
			const bool component = std::string_view(section) == "components";
			if(!component) waiting.push_back(&entry);
			else if(entry.is_object() && entry.contains("node")) waiting.push_back(&entry["node"]);
		}
	}
	std::size_t count = 0;
	while(!waiting.empty()) { // a stack, not recursion: a document's depth is not this function's bound
		const json* node = waiting.back();
		waiting.pop_back();
		if(!node->is_object()) continue;
		++count;
		if(const auto children = node->find("children"); children != node->end() && children->is_array())
			for(const json& child : *children) waiting.push_back(&child);
	}
	return count;
}

/// @brief How many definitions a document's program holds.
/// @param document The document.
/// @return The count.
std::size_t definitionCount(const json& document) {
	std::size_t count = 0;
	const auto program = document.find("program");
	if(program == document.end() || !program->is_object()) return count;
	for(const json& file : *program)
		if(file.is_object())
			for(const json& text : file)
				if(text.is_string()) ++count;
	return count;
}

} // namespace

json picksOf(const json& in) {
	json out = json::array();
	if(!in.is_array()) return out;
	for(const json& item : in) {
		if(out.size() >= maxPicks) break;
		if(std::optional<json> record = recordOf(item)) out.push_back(std::move(*record));
	}
	return out;
}

std::string pickBlock(const json& picks, const json& document) {
	if(!picks.is_array() || picks.empty()) return "";
	std::string out = "The person pointed at this on the screen:";
	std::vector<std::string> shown;
	std::size_t taken = 0;
	for(const json& pick : picks) {
		if(taken >= maxPicks) break;
		if(!pick.is_object()) continue;
		++taken;
		out += "\n" + pickPart(pick, document, shown);
	}
	return out;
}

std::string screenSummary(const json& document, int revision, bool tools) {
	const std::size_t definitions = definitionCount(document);
	std::string line = "The rest of the screen (revision " + std::to_string(revision) + ", " +
	                   std::to_string(nodeCount(document)) + " nodes";
	if(definitions > 0) line += ", " + std::to_string(definitions) + " definitions";
	return line + ") isn't shown here: " +
	       (tools ? "read what the change needs with the tools (get_screen, find_nodes, get_format_guide)."
	              : "ask for what the change needs with read, find and guide.");
}

} // namespace affresco::intelligence
