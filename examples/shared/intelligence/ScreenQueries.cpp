/// @file
/// @brief Reading a screen document the way an agent needs it (ScreenQueries.hpp).

#include "intelligence/ScreenQueries.hpp"

#include "Assets.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace examples::intelligence {

using affresco::json;

namespace {

/// The most characters a topic of the guide gives (about 1,800 tokens), as the MCP server's GUIDE_TOPIC.
constexpr std::size_t guideTopic = 6000;

/// @brief One node of a walk.
struct Visit {
	const json* node;    ///< The node.
	std::string address; ///< "#id", or a path from the nearest node with one.
	int depth;           ///< Levels below the start.
	std::string parent;  ///< The parent's address, or "".
};

/// @brief The trees of a document: root, screens, templates, overlays, then components' nodes.
/// @param document The document.
/// @return (JSON Pointer, node) pairs.
std::vector<std::pair<std::string, const json*>> trees(const json& document) {
	std::vector<std::pair<std::string, const json*>> found;
	if(document.contains("root")) found.emplace_back("/root", &document["root"]);
	for(const char* section : {"screens", "templates", "overlays"})
		if(document.contains(section) && document[section].is_object())
			for(const auto& [name, node] : document[section].items())
				found.emplace_back("/" + std::string(section) + "/" + name, &node);
	if(document.contains("components") && document["components"].is_object())
		for(const auto& [name, component] : document["components"].items())
			if(component.is_object() && component.contains("node"))
				found.emplace_back("/components/" + name + "/node", &component["node"]);
	return found;
}

/// @brief Every node of a tree, parents first.
/// @param node The tree.
/// @param address Its address.
/// @param depth Its depth.
/// @param parent Its parent's address.
/// @param visit Called for each.
void walk(const json& node, std::string address, int depth, const std::string& parent,
          const std::function<void(const Visit&)>& visit) {
	if(!node.is_object() || node.value("slot", false)) return;
	if(node.contains("id") && node["id"].is_string() && !node["id"].get<std::string>().empty())
		address = "#" + node["id"].get<std::string>();
	visit({&node, address, depth, parent});
	if(!node.contains("children") || !node["children"].is_array()) return;
	for(std::size_t i = 0; i < node["children"].size(); ++i)
		walk(node["children"][i], address + "/children/" + std::to_string(i), depth + 1, address, visit);
}

/// @brief A node's type as the outline writes it: its type, else its style's, else "panel"; "use <name>" for a use.
/// @param node The node.
/// @param styles The document's styles.
/// @return The type.
std::string typeOf(const json& node, const json& styles) {
	if(node.contains("use")) return "use " + (node["use"].is_string() ? node["use"].get<std::string>() : compact(node["use"]));
	const json* at = &node;
	for(int i = 0; i < 16; ++i) {
		if(at->contains("type") && (*at)["type"].is_string()) return (*at)["type"].get<std::string>();
		if(!at->contains("style") || !(*at)["style"].is_string() || !styles.is_object()) break;
		const auto style = styles.find((*at)["style"].get<std::string>());
		if(style == styles.end() || !style->is_object()) break;
		at = &*style;
	}
	return "panel";
}

/// @brief A value in a few characters: text quoted, cut with "...".
/// @param value The value.
/// @return It.
std::string shortly(const json& value) {
	std::string text = value.is_string() ? value.get<std::string>() : compact(value);
	if(text.size() > 24) {
		std::size_t cut = 24;
		while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut; // keep characters whole
		text = text.substr(0, cut) + "...";
	}
	return compact(json(text));
}

/// @brief What an agent needs of a node to find and change it, without its styling.
/// @param node The node.
/// @param address Its address.
/// @param styles The document's styles.
/// @return Its summary.
std::string summary(const json& node, const std::string& address, const json& styles) {
	const std::string kind = typeOf(node, styles);
	std::string out = address.starts_with("#" + kind + "-") ? "" : kind;
	const auto add = [&out](const std::string& bit) { out += (out.empty() ? "" : " ") + bit; };
	for(const char* key : {"text", "placeholder", "tooltip"})
		if(node.contains(key) && node[key].is_string())
			add(std::string_view(key) == "text" ? shortly(node[key]) : std::string(key) + "=" + shortly(node[key]));
	for(const char* key : {"bind", "onClick"})
		if(node.contains(key) && node[key].is_string()) add(std::string(key) + "=" + node[key].get<std::string>());
	if(node.contains("repeat") && node["repeat"].is_object())
		add("repeat " + node["repeat"].value("for", std::string("item")) + " in " +
		    (node["repeat"].contains("in") ? (node["repeat"]["in"].is_string() ? node["repeat"]["in"].get<std::string>()
		                                                                       : compact(node["repeat"]["in"]))
		                                   : ""));
	if(node.contains("when")) add("when");
	return out;
}

/// @brief The JSON Pointer of the node with an id, like the host's #id paths.
/// @param document The document.
/// @param id The id.
/// @return It, or nullopt.
std::optional<std::string> pointerOf(const json& document, const std::string& id) {
	std::optional<std::string> found;
	const std::function<void(const json&, const std::string&)> search = [&](const json& node, const std::string& at) {
		if(found || !node.is_object()) return;
		if(node.contains("id") && node["id"] == id) {
			found = at;
			return;
		}
		if(node.contains("children") && node["children"].is_array())
			for(std::size_t i = 0; i < node["children"].size(); ++i)
				search(node["children"][i], at + "/children/" + std::to_string(i));
	};
	for(const auto& [pointer, node] : trees(document)) search(*node, pointer);
	return found;
}

/// @brief The JSON Pointer an address names.
/// @param document The document.
/// @param at "#id", "#id/rest" or a JSON Pointer.
/// @return The pointer.
/// @throws std::runtime_error If no node has that id.
std::string resolve(const json& document, const std::string& at) {
	if(!at.starts_with("#")) return at;
	const std::size_t slash = at.find('/');
	const std::string id = at.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
	const std::optional<std::string> pointer = pointerOf(document, id);
	if(!pointer) throw std::runtime_error("no node with id '" + id + "'");
	return *pointer + (slash == std::string::npos ? "" : at.substr(slash));
}

/// @brief The value at a JSON Pointer.
/// @param document The document.
/// @param pointer The pointer.
/// @param said The address as given, for the error.
/// @return It.
/// @throws std::runtime_error If there is nothing there.
const json& valueAt(const json& document, const std::string& pointer, const std::string& said) {
	try {
		return document.at(json::json_pointer(pointer));
	} catch(const json::exception&) {
		throw std::runtime_error("nothing at " + said);
	}
}

/// @brief A node with what is deeper than @p depth levels replaced by its children's addresses.
/// @param node The node.
/// @param address Its address.
/// @param depth Levels to keep.
/// @return It.
json pruned(const json& node, const std::string& address, int depth) {
	if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return node;
	json copy = node;
	json children = json::array();
	for(std::size_t i = 0; i < node["children"].size(); ++i) {
		const json& child = node["children"][i];
		const bool named = child.is_object() && child.contains("id") && child["id"].is_string();
		const std::string at = named ? "#" + child["id"].get<std::string>() : address + "/children/" + std::to_string(i);
		children.push_back(depth <= 0 ? json(at) : pruned(child, at, depth - 1));
	}
	copy["children"] = children;
	return copy;
}

/// @brief Text in lower case (ASCII letters).
/// @param text The text.
/// @return It.
std::string lower(std::string text) {
	std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

/// @brief One heading of the guide.
struct Heading {
	std::size_t line;  ///< Where it is.
	int level;         ///< 1 for the introduction, 2 for "##", 3 for "###".
	std::string title; ///< Its text, without backticks.
};

/// @brief Where the guide's topics start: the introduction at line 0, then each "##" and "###" outside code.
/// @param lines The guide's lines.
/// @return The headings.
std::vector<Heading> headings(const std::vector<std::string>& lines) {
	std::vector<Heading> starts{{0, 1, "introduction"}};
	bool fence = false;
	for(std::size_t i = 0; i < lines.size(); ++i) {
		if(lines[i].starts_with("```")) fence = !fence;
		std::string title;
		if(!fence && lines[i].starts_with("## ")) starts.push_back({i, 2, lines[i].substr(3)});
		else if(!fence && lines[i].starts_with("### ")) starts.push_back({i, 3, lines[i].substr(4)});
		else continue;
		std::erase(starts.back().title, '`');
		while(!starts.back().title.empty() && std::isspace(static_cast<unsigned char>(starts.back().title.back())))
			starts.back().title.pop_back();
	}
	return starts;
}

/// @brief Lines joined, from one to another.
/// @param lines The lines.
/// @param from The first.
/// @param to One past the last.
/// @return The text.
std::string joined(const std::vector<std::string>& lines, std::size_t from, std::size_t to) {
	std::string text;
	for(std::size_t i = from; i < to && i < lines.size(); ++i) text += lines[i] + "\n";
	return text;
}

/// @brief The text of a topic: down to the next heading of its level or above; a long "##" one names its "###"
/// topics instead of holding them.
/// @param lines The guide's lines.
/// @param starts The headings.
/// @param n The topic.
/// @return The text.
std::string section(const std::vector<std::string>& lines, const std::vector<Heading>& starts, std::size_t n) {
	const Heading& start = starts[n];
	std::size_t end = lines.size();
	for(std::size_t i = n + 1; i < starts.size(); ++i)
		if(starts[i].level <= start.level) {
			end = starts[i].line;
			break;
		}
	if(start.level == 1) end = starts.size() > 1 ? starts[1].line : lines.size();
	std::string part = joined(lines, start.line, end);
	if(start.level == 2 && part.size() > guideTopic) {
		std::vector<const Heading*> inner;
		for(std::size_t i = n + 1; i < starts.size() && starts[i].line < end; ++i)
			if(starts[i].level == 3) inner.push_back(&starts[i]);
		if(!inner.empty()) {
			part = joined(lines, start.line, inner.front()->line) + "\nMore in the topics: ";
			for(std::size_t i = 0; i < inner.size(); ++i) part += (i ? ", " : "") + inner[i]->title;
			part += ".";
		}
	}
	if(part.size() > guideTopic * 2) part = part.substr(0, guideTopic * 2) + "\n(cut short)";
	return part;
}

} // namespace

std::string compact(const json& value) { return value.dump(-1, ' ', false, json::error_handler_t::replace); }

std::string outline(const json& document, const std::string& at, int depth) {
	const json styles = document.value("styles", json::object());
	std::vector<std::pair<std::string, const json*>> starts;
	std::string pointer;
	if(!at.empty()) {
		pointer = resolve(document, at);
		starts.emplace_back(pointer, &valueAt(document, pointer, at));
	} else {
		starts = trees(document);
	}
	std::vector<std::pair<std::string, int>> lines; // text, nodes not shown
	for(const auto& [start, node] : starts) {
		std::size_t cut = std::string::npos;
		walk(*node, start, 0, "", [&](const Visit& visit) {
			if(depth >= 0 && visit.depth > depth) {
				if(cut != std::string::npos) ++lines[cut].second;
				return;
			}
			std::string line = std::string(static_cast<std::size_t>(visit.depth), ' ') + visit.address;
			const std::string said = summary(*visit.node, visit.address, styles);
			if(!said.empty()) line += " " + said;
			lines.emplace_back(line, 0);
			if(depth >= 0 && visit.depth == depth) cut = lines.size() - 1;
		});
	}
	std::string out;
	for(const auto& [text, hidden] : lines) out += text + (hidden ? " +" + std::to_string(hidden) : "") + "\n";
	return out;
}

json nodeAt(const json& document, const std::string& at, int depth) {
	const std::string pointer = resolve(document, at);
	const json& value = valueAt(document, pointer, at);
	return depth < 0 ? value : pruned(value, at, depth);
}

std::string findNodes(const json& document, const std::string& type, const std::string& text,
                      const std::string& property, const json& value, int limit) {
	const json styles = document.value("styles", json::object());
	const std::string needle = lower(text);
	std::vector<std::string> found;
	for(const auto& [pointer, start] : trees(document))
		walk(*start, pointer, 0, "", [&](const Visit& visit) {
			const json& node = *visit.node;
			if(!type.empty() && typeOf(node, styles) != type && !(node.contains("use") && node["use"] == type)) return;
			if(!needle.empty() && std::ranges::none_of(std::array<const char*, 3>{"text", "placeholder", "tooltip"},
			                                           [&](const char* key) {
				                                           return node.contains(key) && node[key].is_string() &&
				                                                  lower(node[key].get<std::string>()).find(needle) !=
				                                                      std::string::npos;
			                                           }))
				return;
			if(!property.empty() &&
			   (!node.contains(property) ||
			    (!value.is_null() && node[property] != value &&
			     !(value.is_string() && compact(node[property]) == value.get<std::string>()))))
				return;
			std::string line = visit.address;
			const std::string said = summary(node, visit.address, styles);
			if(!said.empty()) line += " " + said;
			if(!visit.parent.empty()) line += " in " + visit.parent;
			found.push_back(line);
		});
	const std::size_t shown = std::min(found.size(), static_cast<std::size_t>(std::max(1, limit)));
	std::string out = std::to_string(found.size()) + " found";
	for(std::size_t i = 0; i < shown; ++i) out += "\n" + found[i];
	if(found.size() > shown)
		out += "\n... and " + std::to_string(found.size() - shown) + " more: narrow the search or raise limit";
	return out;
}

std::optional<std::string> formatGuide(const std::string& guide, const std::string& topic) {
	std::vector<std::string> lines;
	std::istringstream in(guide);
	for(std::string line; std::getline(in, line);) lines.push_back(line);
	const std::vector<Heading> starts = headings(lines);
	if(topic.empty()) {
		std::string out = section(lines, starts, 0) + "\nTopics (pass one as `topic`): ";
		for(std::size_t i = 1; i < starts.size(); ++i) out += (i > 1 ? ", " : "") + starts[i].title;
		return out + ".";
	}
	std::string wanted = lower(topic);
	std::erase(wanted, '`');
	std::vector<std::size_t> matches;
	for(std::size_t i = 0; i < starts.size(); ++i)
		if(lower(starts[i].title) == wanted) matches.push_back(i);
	if(matches.empty())
		for(std::size_t i = 0; i < starts.size(); ++i)
			if(lower(starts[i].title).find(wanted) != std::string::npos) matches.push_back(i);
	if(matches.empty()) return std::nullopt;
	std::string out = section(lines, starts, matches.front());
	if(matches.size() > 1) {
		out += "\n(Also matching: ";
		for(std::size_t i = 1; i < matches.size(); ++i) out += (i > 1 ? ", " : "") + starts[matches[i]].title;
		out += ".)";
	}
	return out;
}

std::string readFormatGuide() {
	const std::filesystem::path assets = assetsDirectory();
	for(const std::filesystem::path& file : {assets / "SCREEN_FORMAT.md", assets.parent_path() / "SCREEN_FORMAT.md"}) {
		std::ifstream in(file, std::ios::binary);
		if(!in) continue;
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
	return "";
}

} // namespace examples::intelligence
