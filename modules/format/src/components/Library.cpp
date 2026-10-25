/// @file
/// @brief The installed library's components and tokens (Library.hpp).

#include "Library.hpp"

#include <affresco/core/ComponentLibrary.hpp>

namespace affresco::format {

namespace {

/// @brief The namespace of a token's name: up to and including its first dot ("kit." for "kit.surface").
/// @param name The name.
/// @return It, or "" for a name without a dot.
std::string_view namespaceOf(std::string_view name) {
	const std::size_t dot = name.find('.');
	return dot == std::string_view::npos ? std::string_view() : name.substr(0, dot + 1);
}

/// @brief Tells whether a section names something in one of the library's token namespaces.
/// @param section The section (anything that isn't an object names nothing).
/// @param tokens The library's tokens.
/// @return true if it does.
bool namesLibraryNamespace(const json& section, const json& tokens) {
	if(!section.is_object()) return false;
	for(auto token = tokens.begin(); token != tokens.end(); ++token) {
		const std::string_view prefix = namespaceOf(token.key());
		if(prefix.empty()) continue;
		for(auto entry = section.begin(); entry != section.end(); ++entry)
			if(entry.key().starts_with(prefix)) return true;
	}
	return false;
}

/// @brief A token's value for the table: its colour in backquotes, or its size.
/// @param token The token's description.
/// @param variant "dark" or "light".
/// @return The text.
std::string tableValue(const json& token, const char* variant) {
	if(const auto colour = token.find(variant); colour != token.end() && colour->is_string())
		return "`" + colour->get<std::string>() + "`";
	if(const auto dark = token.find("dark"); dark != token.end() && dark->is_string()) // no light variant: the base
		return "`" + dark->get<std::string>() + "`";
	return std::to_string(token.value("value", 0));
}

} // namespace

const json* libraryComponent(std::string_view name) {
	const ComponentLibrary* library = componentLibrary();
	return library ? library->component(name) : nullptr;
}

const json& libraryTokens(bool themed) {
	static const json none = json::object();
	const ComponentLibrary* library = componentLibrary();
	return library ? library->tokenDefaults(themed) : none;
}

bool isLibraryToken(std::string_view name) {
	const ComponentLibrary* library = componentLibrary();
	return library && library->tokens().contains(name);
}

bool followsLibraryThemes(const json& document) {
	const ComponentLibrary* library = componentLibrary();
	if(!library || !document.is_object()) return false;
	const auto tokens = document.find("tokens");
	const auto colors = document.find("colors");
	return (tokens != document.end() && namesLibraryNamespace(*tokens, library->tokens())) ||
	       (colors != document.end() && namesLibraryNamespace(*colors, library->tokens()));
}

std::string libraryTokenTable() {
	std::string text = "| Token | Dark | Light | For |\n|---|---|---|---|\n";
	const ComponentLibrary* library = componentLibrary();
	if(!library) return text;
	for(const auto& [name, token] : library->tokens().items())
		text += "| `" + name + "` | " + tableValue(token, "dark") + " | " + tableValue(token, "light") + " | " +
		        token.value("about", std::string()) + " |\n";
	return text;
}

std::set<std::string> actionRunningLibraryComponents() {
	std::set<std::string> names;
	if(const ComponentLibrary* library = componentLibrary())
		for(const auto& [name, definition] : library->components().items())
			if(definition.contains("runsActions")) names.emplace(name);
	return names;
}

bool declaredToRunActions(const json& definition, const json& with, bool (*computes)(const json&)) {
	const auto runs = definition.find("runsActions");
	if(runs == definition.end()) return false;
	if(runs->is_boolean()) return runs->get<bool>();
	if(!runs->is_string() || !with.is_object()) return false;
	const auto value = with.find(runs->get_ref<const std::string&>());
	return value != with.end() && computes(*value);
}

} // namespace affresco::format
