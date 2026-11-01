/// @file
/// @brief format::literalTexts(): the text a person reads that a document writes in place.

#include "affresco/format/Literals.hpp"

#include "NodeKeys.hpp"
#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

namespace affresco::format {

namespace {

/// The keys that hold what a person reads in the items of a list property, such as a tab's "label": items have no type
/// to carry the catalog's marks.
constexpr auto itemTextKeys =
    std::to_array<std::string_view>({"label", "title", "text", "tooltip", "tip", "description", "placeholder"});

/// How deep a component's text is followed through the components it uses.
constexpr int mostComponentDepth = 16;

/// @brief Whether a character is part of a word, as far as telling text from symbols goes: letters of any script, but
/// not punctuation, symbols, icons in private areas or emoji.
/// @param c The code point.
/// @return Whether it is.
[[nodiscard]] bool isLetter(char32_t c) {
	if(c < 0x80) return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
	if(c < 0xC0 || c == 0xD7 || c == 0xF7) return false;
	if(c >= 0x2000 && c <= 0x2BFF) return false; // punctuation, arrows, maths, shapes
	if(c >= 0xE000 && c <= 0xF8FF) return false; // private use: icon fonts
	return c < 0x1F000 || c >= 0x20000;          // emoji and pictographs
}

/// @brief Whether a text has a word in it.
/// @param text The text, as UTF-8.
/// @return Whether it does.
[[nodiscard]] bool hasWords(std::string_view text) {
	for(std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
		char32_t c = length == 1 ? lead : lead & (0xFFU >> (length + 1));
		for(std::size_t j = 1; j < length && i + j < text.size(); ++j)
			c = (c << 6U) | (static_cast<unsigned char>(text[i + j]) & 0x3FU);
		if(isLetter(c)) return true;
		i += length;
	}
	return false;
}

/// @brief A member's name as a JSON Pointer writes it.
/// @param name The name.
/// @return It, escaped as RFC 6901 says.
[[nodiscard]] std::string token(std::string_view name) {
	std::string escaped;
	for(const char c : name) {
		if(c == '~') escaped += "~0";
		else if(c == '/') escaped += "~1";
		else escaped += c;
	}
	return escaped;
}

/// @brief The text outside a value's expressions.
/// @param value The value.
/// @return It.
[[nodiscard]] std::string outsideExpressions(std::string_view value) {
	std::string outside;
	while(!value.empty()) {
		const std::size_t open = value.find("{{");
		outside += value.substr(0, open);
		if(open == std::string_view::npos) break;
		const std::size_t close = value.find("}}", open);
		if(close == std::string_view::npos) break;
		value.remove_prefix(close + 2);
	}
	return outside;
}

/// @brief A part of a key: anything a key can't hold in a `t('...')` becomes '-'.
/// @param word The word.
/// @return It.
[[nodiscard]] std::string keyPart(std::string_view word) {
	std::string part(word);
	std::ranges::replace_if(
	    part, [](char c) { return c == '\'' || c == '"' || c == '\\' || c == '{' || c == '}'; }, '-');
	return part;
}

/// @brief What a search through one file adds to.
struct Search {
	const TextVocabulary& vocabulary; ///< What says which values a person reads.
	const json& keep;                 ///< Texts to leave.
	std::vector<LiteralText>& found;  ///< What it found.

	/// @brief Looks at one value that a person reads.
	/// @param value The value.
	/// @param pointer Where it is.
	/// @param key The key it would get.
	void look(const json& value, const std::string& pointer, const std::string& key) const {
		if(!value.is_string()) return;
		const std::string& text = value.get_ref<const std::string&>();
		if(text.starts_with('@') || !hasWords(outsideExpressions(text))) return;
		if(std::ranges::find(keep, value) != keep.end()) return;
		found.push_back({pointer, keyPart(key), text, text.contains("{{")});
	}

	/// @brief Looks through the items of a list property.
	/// @param list The value.
	/// @param pointer Where it is.
	/// @param key The start of the keys.
	void lookInItems(const json& list, const std::string& pointer, const std::string& key) const {
		if(!list.is_array()) return;
		for(std::size_t i = 0; i < list.size(); ++i) {
			if(!list[i].is_object()) continue;
			const std::string at = std::format("{}/{}", pointer, i), itemKey = std::format("{}.{}", key, i);
			for(const auto& [name, member] : list[i].items()) {
				if(std::ranges::find(itemTextKeys, name) != itemTextKeys.end())
					look(member, std::format("{}/{}", at, token(name)), std::format("{}.{}", itemKey, name));
				else lookInItems(member, std::format("{}/{}", at, token(name)), std::format("{}.{}", itemKey, name));
			}
		}
	}

	/// @brief Looks through a node's own values.
	/// @param node The node, or a state block of it.
	/// @param type Its type.
	/// @param pointer Where it is.
	/// @param key The start of the keys.
	void lookInNode(const json& node, std::string_view type, const std::string& pointer, const std::string& key) const {
		for(const auto& [name, value] : node.items()) {
			const std::string at = std::format("{}/{}", pointer, token(name));
			const std::string inner = std::format("{}.{}", key, name);
			if(vocabulary.shows(type, name)) look(value, at, inner);
			else if(name != "children" && name != "repeat") lookInItems(value, at, inner);
			if(value.is_object() && std::ranges::find(stateBlocks, name) != stateBlocks.end())
				lookInNode(value, type, at, inner);
		}
	}

	/// @brief Looks through what a node gives the component it uses.
	/// @param node The node.
	/// @param component The component.
	/// @param pointer Where the node is.
	/// @param key The start of the keys.
	void lookInGiven(const json& node, const std::string& component, const std::string& pointer,
	                 const std::string& key) const {
		const auto given = node.find("with");
		if(given == node.end() || !given->is_object()) return;
		const auto text = vocabulary.parameters.find(component);
		for(const auto& [parameter, value] : given->items()) {
			const std::string at = std::format("{}/with/{}", pointer, token(parameter));
			const std::string inner = std::format("{}.{}", key, parameter);
			if(text != vocabulary.parameters.end() && text->second.contains(parameter)) look(value, at, inner);
			else lookInItems(value, at, inner);
		}
	}

	/// @brief Looks through one node: what it says, and what it gives the component it uses.
	/// @param node The node.
	/// @param pointer Where it is.
	void lookAtNode(const json& node, const std::string& pointer) const {
		const std::string name = node.value("id", placeOf(pointer));
		std::string key = name;
		if(pointer.starts_with("/components/")) {
			constexpr std::size_t nameAt = std::string_view("/components/").size();
			key = std::format("component.{}.{}", pointer.substr(nameAt, pointer.find('/', nameAt) - nameAt), name);
		}
		std::string type = node.value("type", std::string("panel"));
		if(const auto use = node.find("use"); use != node.end() && use->is_string()) {
			const std::string& component = use->get_ref<const std::string&>();
			if(const auto known = vocabulary.componentTypes.find(component); known != vocabulary.componentTypes.end())
				type = known->second;
			lookInGiven(node, component, pointer, key);
		}
		lookInNode(node, type, pointer, key);
	}

	/// @brief Looks at some members of an object.
	/// @param object The object.
	/// @param names The members.
	/// @param pointer Where it is.
	/// @param key The start of the keys.
	void lookAtMembers(const json& object, std::initializer_list<const char*> names, const std::string& pointer,
	                   const std::string& key) const {
		if(!object.is_object()) return;
		for(const char* name : names)
			if(const auto member = object.find(name); member != object.end())
				look(*member, std::format("{}/{}", pointer, name), std::format("{}.{}", key, name));
	}

	/// @brief Looks through what a document declares outside its nodes.
	/// @param document The document.
	void lookOutsideNodes(const json& document) const {
		if(const auto window = document.find("window"); window != document.end() && window->is_object()) {
			lookAtMembers(*window, {"title"}, "/window", "window");
			lookInItems(window->value("menuBar", json()), "/window/menuBar", "window.menuBar");
		}
		if(const auto actions = document.find("actions"); actions != document.end() && actions->is_object())
			for(const auto& [name, action] : actions->items())
				lookAtMembers(action, {"title", "category", "description"}, "/actions/" + token(name),
				              "action." + name);
		if(const auto menus = document.find("menus"); menus != document.end() && menus->is_object())
			for(const auto& [menu, items] : menus->items())
				if(items.is_object())
					for(const auto& [name, item] : items.items())
						lookAtMembers(item, {"title"}, std::format("/menus/{}/{}", token(menu), token(name)),
						              std::format("menu.{}.{}", menu, name));
		if(const auto settings = document.find("settings"); settings != document.end() && settings->is_object())
			for(const auto& [name, setting] : settings->items())
				lookAtMembers(setting, {"title", "description"}, "/settings/" + token(name), "setting." + name);
	}
};

/// @brief Adds the names a value shows as they are ("{{label}}", "Hi {{name}}"): a name inside a larger expression may
/// be anything, such as the key in `t(key)`.
/// @param value The value.
/// @param names Where they go.
void addNamesRead(std::string_view value, std::set<std::string, std::less<>>& names) {
	for(std::size_t open = value.find("{{"); open != std::string_view::npos; open = value.find("{{", open)) {
		const std::size_t close = value.find("}}", open);
		if(close == std::string_view::npos) return;
		std::string_view inside = value.substr(open + 2, close - open - 2);
		inside.remove_prefix(std::min(inside.find_first_not_of(' '), inside.size()));
		inside.remove_suffix(inside.size() - (inside.find_last_not_of(' ') + 1));
		const bool isName = !inside.empty() && std::ranges::all_of(inside, [](char c) {
			return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
		});
		if(isName) names.emplace(inside);
		open = close;
	}
}

/// @brief Adds the names a node passes on to the text parameters of the component it uses.
/// @param node The node.
/// @param use The component.
/// @param vocabulary What is known so far.
/// @param names The names read.
void addNamesGiven(const json& node, const std::string& use, const TextVocabulary& vocabulary,
                   std::set<std::string, std::less<>>& names) {
	const auto text = vocabulary.parameters.find(use);
	const auto given = node.find("with");
	if(text == vocabulary.parameters.end() || given == node.end() || !given->is_object()) return;
	for(const auto& [parameter, value] : given->items())
		if(value.is_string() && text->second.contains(parameter))
			addNamesRead(value.get_ref<const std::string&>(), names);
}

/// @brief Adds what a component's node shows, through its own properties and the components it uses, to a set of the
/// names read there.
/// @param node The component's node, or one inside it.
/// @param vocabulary What is known so far.
/// @param names The names read.
/// @param depth How deep it is.
void addNamesShown(const json& node, const TextVocabulary& vocabulary, std::set<std::string, std::less<>>& names,
                   int depth) {
	if(!node.is_object() || depth > mostComponentDepth) return;
	const std::string use = node.value("use", std::string());
	const auto known = vocabulary.componentTypes.find(use);
	const std::string type =
	    known != vocabulary.componentTypes.end() ? known->second : node.value("type", std::string("panel"));
	for(const auto& [name, value] : node.items())
		if(value.is_string() && vocabulary.shows(type, name)) addNamesRead(value.get_ref<const std::string&>(), names);
	addNamesGiven(node, use, vocabulary, names);
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(const json& child : *children) addNamesShown(child, vocabulary, names, depth + 1);
}

/// @brief Reads the marks the catalog's types carry.
/// @param catalog The catalog.
/// @param vocabulary Where they go.
void readTypeMarks(const json& catalog, TextVocabulary& vocabulary) {
	const auto types = catalog.find("types");
	if(types == catalog.end() || !types->is_object()) return;
	for(const auto& [name, type] : types->items())
		if(const auto text = type.find("text"); text != type.end() && text->is_array())
			for(const json& property : *text) vocabulary.types[name].insert(property.get<std::string>());
}

/// @brief Adds the parameters that reach what a person reads, one step further through the components.
/// @param components The document's components.
/// @param vocabulary What is known so far, which receives them.
/// @return Whether it found any new one.
bool growParameters(const json& components, TextVocabulary& vocabulary) {
	bool grew = false;
	for(const auto& [name, component] : components.items()) {
		if(!component.is_object() || !component.contains("params") || !component["params"].is_object()) continue;
		std::set<std::string, std::less<>> read;
		addNamesShown(component.value("node", json()), vocabulary, read, 0);
		for(const auto& [parameter, fallback] : component["params"].items())
			if(read.contains(parameter)) grew = vocabulary.parameters[name].insert(parameter).second || grew;
	}
	return grew;
}

} // namespace

bool TextVocabulary::shows(std::string_view type, std::string_view property) const {
	const auto own = types.find(type);
	const auto common = types.find("panel");
	return (own != types.end() && own->second.contains(property)) ||
	       (common != types.end() && common->second.contains(property));
}

TextVocabulary textVocabulary(const json& catalog, const json& document) {
	TextVocabulary vocabulary;
	readTypeMarks(catalog, vocabulary);
	const auto components = document.find("components");
	if(components == document.end() || !components->is_object()) return vocabulary;
	for(const auto& [name, component] : components->items())
		if(component.is_object() && component.contains("node") && component["node"].is_object()) {
			const json& node = component["node"];
			if(const auto type = node.find("type"); type != node.end() && type->is_string())
				vocabulary.componentTypes[name] = type->get<std::string>();
		}
	// As often as it takes for text to pass through components that use each other
	for(int round = 0; round < mostComponentDepth && growParameters(*components, vocabulary); ++round) {
	}
	return vocabulary;
}

std::vector<LiteralText> literalTexts(const json& document, const TextVocabulary& vocabulary, const json& keep) {
	std::vector<LiteralText> found;
	const Search search{vocabulary, keep, found};
	search.lookOutsideNodes(document);
	forEachNode(document,
	            [&search](const json& node, const std::string& pointer) { search.lookAtNode(node, pointer); });
	return found;
}

std::string messageOfText(std::string_view text) {
	std::string message;
	for(const char c : text) {
		if(c == '\'') message += "''";
		else if(c == '{') message += "'{'";
		else if(c == '}') message += "'}'";
		else message += c;
	}
	return message;
}

} // namespace affresco::format
