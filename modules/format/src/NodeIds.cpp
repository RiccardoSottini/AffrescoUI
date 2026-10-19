/// @file
/// @brief Node ids: walking a document's nodes, duplicate ids, and readable ids for nodes without one (NodeIds.hpp).

#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>

namespace affresco::format {

namespace {

/// The most words an id takes from what its node shows.
constexpr std::size_t maxWords = 3;

/// The longest a word can be: longer ones (a hash, a URL) say little and make long ids.
constexpr std::size_t maxWordLength = 16;

/// Words that say nothing about a node: the roots of expressions and the commonest functions.
constexpr std::array<std::string_view, 9> noiseWords = {"state", "computed", "item",  "index", "len",
                                                        "trim",  "upper",    "lower", "event"};

/// Styles naming styles that the namer follows to find a node's type, as the runtime allows.
constexpr std::size_t maxStyleDepth = 16;

/// The node types the format builds in (SCREEN_FORMAT.md, "Every type"): the first word of the ids given here.
constexpr std::array<std::string_view, 13> builtInTypes = {"panel",  "label",  "textarea", "editor", "stack",
                                                           "row",    "column", "grid",     "toggle", "checkbox",
                                                           "slider", "outlet", "use"};

/// @brief Walks a node and its children.
/// @param node The node.
/// @param pointer Its JSON Pointer.
/// @param visit Gets each node.
void walk(const json& node, const std::string& pointer,
          const std::function<void(const json& node, const std::string& pointer)>& visit) {
	if(!node.is_object() || node.value("slot", json(false)) == true) return;
	visit(node, pointer);
	const auto children = node.find("children");
	if(children == node.end() || !children->is_array()) return;
	for(std::size_t i = 0; i < children->size(); ++i)
		walk((*children)[i], pointer + "/children/" + std::to_string(i), visit);
}

/// @brief Finds the first node with an id in a node and its children, parents first, stopping there.
/// @param node The node.
/// @param id The id.
/// @return The path from @p node to the one found ("" for @p node itself, "/children/2/children/0"), or
/// std::nullopt. It is written only on the way back from the node found.
std::optional<std::string> pathTo(const json& node, std::string_view id) {
	if(!node.is_object()) return std::nullopt;
	if(const auto slot = node.find("slot"); slot != node.end() && *slot == true) return std::nullopt;
	if(const auto own = node.find("id"); own != node.end() && own->is_string() && own->get_ref<const std::string&>() == id)
		return std::string();
	const auto children = node.find("children");
	if(children == node.end() || !children->is_array()) return std::nullopt;
	for(std::size_t i = 0; i < children->size(); ++i)
		if(std::optional<std::string> rest = pathTo((*children)[i], id))
			return "/children/" + std::to_string(i) + *rest;
	return std::nullopt;
}

/// @brief Collects the ids of a node and of the nodes inside it, as views into it.
/// @param node The node.
/// @param ids Receives them; valid while the node is.
void collectIds(const json& node, std::vector<std::string_view>& ids) {
	if(!node.is_object() || node.value("slot", json(false)) == true) return;
	if(const auto id = node.find("id"); id != node.end() && id->is_string())
		ids.emplace_back(id->get_ref<const std::string&>());
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(const json& child : *children) collectIds(child, ids);
}

/// @brief Collects the ids of every node of a document (forEachNode's order), as views into it.
/// @param document The document.
/// @param ids Receives them; valid while the document is.
void idsOf(const json& document, std::vector<std::string_view>& ids) {
	if(!document.is_object()) return;
	if(const auto root = document.find("root"); root != document.end()) collectIds(*root, ids);
	for(const char* name : {"screens", "templates"}) {
		const auto section = document.find(name);
		if(section == document.end() || !section->is_object()) continue;
		for(const json& node : *section) collectIds(node, ids);
	}
	const auto components = document.find("components");
	if(components == document.end() || !components->is_object()) return;
	for(const json& component : *components)
		if(const auto node = component.is_object() ? component.find("node") : component.end(); node != component.end())
			collectIds(*node, ids);
}

/// @brief Escapes a key for a JSON Pointer (RFC 6901): '~' as "~0", '/' as "~1".
/// @param key The key.
/// @return The escaped key.
std::string escaped(std::string_view key) {
	std::string text;
	for(const char c : key) text += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
	return text;
}

/// @brief Tells whether a character is a small ASCII letter.
/// @param c The character.
/// @return true if it is.
bool small(char c) { return c >= 'a' && c <= 'z'; }

/// @brief Tells whether a character is a capital ASCII letter.
/// @param c The character.
/// @return true if it is.
bool capital(char c) { return c >= 'A' && c <= 'Z'; }

/// @brief Tells whether a word says something about a node: not empty, not too long, not a noise word.
/// @param word The word, lower case.
/// @return true if it does.
bool telling(const std::string& word) {
	return !word.empty() && word.size() <= maxWordLength && std::ranges::find(noiseWords, word) == noiseWords.end();
}

/// @brief Splits text into lower-case ASCII words: at anything that isn't a letter or a digit, and where a capital
/// follows a small letter ("topBar" is "top", "bar").
/// @param text The text.
/// @return The words, at most maxWords, without noise words or words longer than maxWordLength.
std::vector<std::string> wordsOf(std::string_view text) {
	std::vector<std::string> words;
	std::string word;
	const auto finish = [&] {
		if(telling(word) && words.size() < maxWords) words.push_back(word);
		word.clear();
	};
	for(std::size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		if(!small(c) && !capital(c) && (c < '0' || c > '9')) {
			finish();
		} else {
			if(capital(c) && i > 0 && small(text[i - 1])) finish(); // "topBar": a new word
			word += capital(c) ? static_cast<char>(c - 'A' + 'a') : c;
		}
	}
	finish();
	return words;
}

/// @brief The node a pointer to a child is in: "/root/children/2" is in "/root".
/// @param pointer The pointer.
/// @return The parent's pointer; an empty view without data if the key after "children" isn't an index or '-';
/// std::nullopt if the pointer doesn't end with "/children/<key>".
std::optional<std::string_view> parentNodeOf(std::string_view pointer) {
	constexpr std::string_view children = "/children/";
	const std::size_t last = pointer.rfind('/');
	if(last == std::string_view::npos || last + 1 < children.size()) return std::nullopt;
	const std::size_t start = last + 1 - children.size();
	if(pointer.substr(start, children.size()) != children) return std::nullopt;
	const std::string_view index = pointer.substr(last + 1);
	const bool number = !index.empty() && std::ranges::all_of(index, [](char c) { return c >= '0' && c <= '9'; });
	return number || index == "-" ? pointer.substr(0, start) : std::string_view();
}

/// @brief Joins words with '-'.
/// @param words The words.
/// @return The text.
std::string joined(const std::vector<std::string>& words) {
	std::string text;
	for(const std::string& word : words) text += (text.empty() ? "" : "-") + word;
	return text;
}

/// @brief The words that describe what a node shows or does, from the first of its properties that has some.
/// @param node The node.
/// @return The words; empty if none of them says anything.
std::vector<std::string> describingWords(const json& node) {
	static constexpr std::array<std::string_view, 7> sources = {"text",    "placeholder", "bind",   "repeat",
	                                                            "tooltip", "style",       "onClick"};
	for(const std::string_view key : sources) {
		const auto it = node.find(key);
		if(it == node.end()) continue;
		const json& value = key == "repeat" && it->is_object() ? it->value("for", json("item")) : *it;
		if(!value.is_string()) continue;
		if(std::vector<std::string> words = wordsOf(value.get_ref<const std::string&>()); !words.empty()) return words;
	}
	return {};
}

} // namespace

std::vector<std::pair<std::string, const json*>> nodeTrees(const json& document) {
	std::vector<std::pair<std::string, const json*>> trees;
	if(!document.is_object()) return trees;
	if(const auto root = document.find("root"); root != document.end()) trees.emplace_back("/root", &*root);
	for(const char* section : {"screens", "templates"}) {
		const auto nodes = document.find(section);
		if(nodes == document.end() || !nodes->is_object()) continue;
		for(const auto& [name, node] : nodes->items())
			trees.emplace_back("/" + std::string(section) + "/" + escaped(name), &node);
	}
	const auto components = document.find("components");
	if(components == document.end() || !components->is_object()) return trees;
	for(const auto& [name, component] : components->items())
		if(const auto node = component.is_object() ? component.find("node") : component.end(); node != component.end())
			trees.emplace_back("/components/" + escaped(name) + "/node", &*node);
	return trees;
}

void forEachNode(const json& document, const std::function<void(const json& node, const std::string& pointer)>& visit) {
	for(const auto& [pointer, tree] : nodeTrees(document)) walk(*tree, pointer, visit);
}

// Called for every "#id" path and on every step of a blame: no pointer is built until the node is found
std::optional<std::string> findNodeWithId(const json& document, std::string_view id) {
	if(!document.is_object()) return std::nullopt;
	if(const auto root = document.find("root"); root != document.end())
		if(std::optional<std::string> path = pathTo(*root, id)) return "/root" + *path;
	for(const char* section : {"screens", "templates"}) {
		const auto nodes = document.find(section);
		if(nodes == document.end() || !nodes->is_object()) continue;
		for(const auto& [name, node] : nodes->items())
			if(std::optional<std::string> path = pathTo(node, id))
				return "/" + std::string(section) + "/" + escaped(name) + *path;
	}
	const auto components = document.find("components");
	if(components == document.end() || !components->is_object()) return std::nullopt;
	for(const auto& [name, component] : components->items())
		if(const auto node = component.is_object() ? component.find("node") : component.end(); node != component.end())
			if(std::optional<std::string> path = pathTo(*node, id)) return "/components/" + escaped(name) + "/node" + *path;
	return std::nullopt;
}

std::vector<DuplicateId> duplicateIds(const json& document) {
	// Every revision is checked: first only the ids, as views into the document (no copies, no pointers), and the
	// pointers only when an id comes twice
	std::vector<std::string_view> ids;
	idsOf(document, ids);
	std::ranges::sort(ids);
	if(std::ranges::adjacent_find(ids) == ids.end()) return {};

	std::map<std::string, std::vector<std::string>, std::less<>> seen;
	std::vector<std::string> order; // ids in the order their second node comes
	forEachNode(document, [&](const json& node, const std::string& pointer) {
		const auto id = node.find("id");
		if(id == node.end() || !id->is_string()) return;
		std::vector<std::string>& pointers = seen[id->get<std::string>()];
		pointers.push_back(pointer);
		if(pointers.size() == 2) order.push_back(id->get<std::string>());
	});
	std::vector<DuplicateId> result;
	for(std::string& id : order) {
		std::vector<std::string> pointers = std::move(seen[id]);
		result.push_back({std::move(id), std::move(pointers)});
	}
	return result;
}

std::vector<std::string> nodeIds(const json& document) {
	std::vector<std::string_view> ids;
	idsOf(document, ids);
	std::ranges::sort(ids);
	const auto [first, last] = std::ranges::unique(ids);
	ids.erase(first, last);
	return {ids.begin(), ids.end()};
}

std::string placeOf(std::string_view pointer) {
	std::string place;
	bool index = false; // the key after "children" is an index
	for(std::size_t start = pointer.find('/'); start != std::string_view::npos;) {
		const std::size_t end = pointer.find('/', start + 1);
		const std::string_view key =
		    pointer.substr(start + 1, end == std::string_view::npos ? std::string_view::npos : end - start - 1);
		place += index ? "[" + std::string(key) + "]" : (place.empty() ? "" : ".") + std::string(key);
		index = key == "children";
		start = end;
	}
	return place;
}

bool isNodePlace(std::string_view pointer) {
	// Peel "/children/<index>" off the end, then the rest must be where a tree starts
	for(std::optional<std::string_view> parent; (parent = parentNodeOf(pointer));) {
		if(!parent->data()) return false; // "/children/<not an index>"
		pointer = *parent;
	}
	if(pointer == "/root") return true;
	const auto slashes = static_cast<std::size_t>(std::ranges::count(pointer, '/'));
	if(slashes == 2 && (pointer.starts_with("/screens/") || pointer.starts_with("/templates/"))) return true;
	return slashes == 3 && pointer.starts_with("/components/") && pointer.ends_with("/node");
}

NodeNamer::NodeNamer(const json& document) : styles_(document.value("styles", json::object())) {
	for(std::string& id : nodeIds(document)) taken_.insert(std::move(id));
}

std::string NodeNamer::typeOf(const json& node) const {
	if(const auto use = node.find("use"); use != node.end() && use->is_string())
		return joined(wordsOf(use->get<std::string>()));
	const json* styled = &node;
	for(std::size_t depth = 0; depth < maxStyleDepth; ++depth) { // a style may give the type, or its style may
		if(const auto type = styled->find("type"); type != styled->end() && type->is_string())
			return joined(wordsOf(type->get<std::string>()));
		const auto style = styled->find("style");
		if(style == styled->end() || !style->is_string() || !styles_.is_object() || !styles_.contains(*style)) break;
		styled = &styles_[style->get<std::string>()];
		if(!styled->is_object()) break;
	}
	return "panel";
}

std::string NodeNamer::newId(const json& node, std::string_view parentId) {
	std::string type = typeOf(node);
	if(type.empty()) type = "panel";

	std::vector<std::string> words = describingWords(node);
	if(words.empty()) { // the parent's words, without the type and number an id given here has ("row-card-2": "card")
		words = wordsOf(parentId);
		std::erase_if(
		    words, [](const std::string& word) { return word.find_first_not_of("0123456789") == std::string::npos; });
		if(words.size() > 1 && std::ranges::find(builtInTypes, words.front()) != builtInTypes.end())
			words.erase(words.begin());
	}
	std::string id = words.empty() ? type : type + "-" + joined(words);
	if(taken_.insert(id).second) return id;
	for(int n = 2;; ++n)
		if(std::string numbered = id + "-" + std::to_string(n); taken_.insert(numbered).second) return numbered;
}

void NodeNamer::name(json& node, const std::string& pointer, std::string_view parentId, const Visit& visit) {
	if(!node.is_object() || node.value("slot", json(false)) == true) return;
	if(!node.contains("id")) {
		json named = {{"id", newId(node, parentId)}}; // first, as files write it
		for(auto& [key, value] : node.items()) named[key] = std::move(value);
		node = std::move(named);
		if(visit) visit(pointer, node["id"].get<std::string>());
	}
	const auto children = node.find("children");
	if(children == node.end() || !children->is_array()) return;
	const std::string id = node["id"].is_string() ? node["id"].get<std::string>() : std::string(parentId);
	for(std::size_t i = 0; i < children->size(); ++i)
		name((*children)[i], pointer + "/children/" + std::to_string(i), id, visit);
}

std::vector<std::pair<std::string, std::string>> NodeNamer::nameAll(json& document) {
	std::vector<std::pair<std::string, std::string>> named;
	if(!document.is_object()) return named;
	NodeNamer namer(document);
	const Visit note = [&](const std::string& pointer, const std::string& id) { named.emplace_back(pointer, id); };

	if(document.contains("root")) namer.name(document["root"], "/root", "root", note); // "panel-root"
	for(const char* section : {"screens", "templates"}) {
		if(!document.contains(section) || !document[section].is_object()) continue;
		for(auto& [key, node] : document[section].items())
			namer.name(node, "/" + std::string(section) + "/" + escaped(key), key, note);
	}
	if(document.contains("components") && document["components"].is_object())
		for(auto& [key, component] : document["components"].items())
			if(component.is_object() && component.contains("node"))
				namer.name(component["node"], "/components/" + escaped(key) + "/node", key, note);
	return named;
}

} // namespace affresco::format
