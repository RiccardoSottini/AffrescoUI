/// @file
/// @brief Forge's VS Code colour theme importer (ThemeImport.hpp).

#include "features/themes/ThemeImport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace forge {

namespace {

/// @brief A Forge colour token and where a VS Code theme keeps it.
struct TokenSource {
	std::string_view token;                     ///< Forge's token, without "color.".
	std::initializer_list<std::string_view> keys; ///< VS Code's colour keys, the first one set winning.
	std::array<std::string_view, 3> defaults;   ///< For a dark, a light and a high contrast theme without any of them.
};

/// @brief The tokens an imported theme sets, from VS Code's names (code.visualstudio.com/api/references/theme-color).
const std::array<TokenSource, 21> tokenSources = {{
    {"titleBar", {"titleBar.activeBackground", "editorGroupHeader.tabsBackground"}, {"#3c3c3c", "#dddddd", "#000000"}},
    {"activityBar", {"activityBar.background", "sideBar.background"}, {"#333333", "#2c2c2c", "#000000"}},
    {"sideBar", {"sideBar.background", "editor.background"}, {"#252526", "#f3f3f3", "#000000"}},
    {"editor", {"editor.background"}, {"#1e1e1e", "#ffffff", "#000000"}},
    {"editorText", {"editor.foreground", "foreground"}, {"#d4d4d4", "#000000", "#ffffff"}},
    {"tabs", {"editorGroupHeader.tabsBackground", "tab.inactiveBackground"}, {"#252526", "#f3f3f3", "#000000"}},
    {"tabActive", {"tab.activeBackground", "editor.background"}, {"#1e1e1e", "#ffffff", "#000000"}},
    {"panel", {"panel.background", "editor.background"}, {"#1e1e1e", "#ffffff", "#000000"}},
    {"statusBar", {"statusBar.background"}, {"#007acc", "#007acc", "#000000"}},
    {"statusText", {"statusBar.foreground"}, {"#ffffff", "#ffffff", "#ffffff"}},
    {"accent", {"focusBorder", "activityBarBadge.background", "button.background"}, {"#007fd4", "#0090f1", "#f38518"}},
    {"selection", {"list.activeSelectionBackground", "editor.selectionBackground"}, {"#04395e", "#0060c0", "#000000"}},
    {"hover", {"list.hoverBackground"}, {"#2a2d2e", "#e8e8e8", "#000000"}},
    {"input", {"input.background", "dropdown.background"}, {"#3c3c3c", "#ffffff", "#000000"}},
    {"border", {"panel.border", "editorGroup.border", "contrastBorder"}, {"#454545", "#d4d4d4", "#6fc3df"}},
    {"muted", {"descriptionForeground", "tab.inactiveForeground"}, {"#8b8b8b", "#717171", "#ffffff"}},
    {"error", {"editorError.foreground", "errorForeground"}, {"#f14c4c", "#e51400", "#f48771"}},
    {"warning", {"editorWarning.foreground"}, {"#cca700", "#bf8803", "#cca700"}},
    {"info", {"editorInfo.foreground"}, {"#3794ff", "#1a85ff", "#3794ff"}},
    {"widget", {"editorWidget.background", "quickInput.background"}, {"#252526", "#f3f3f3", "#0c141f"}},
    {"text", {"foreground", "sideBar.foreground", "editor.foreground"}, {"#cccccc", "#616161", "#ffffff"}},
}};

/// @brief Removes the comments and trailing commas VS Code allows in its JSON files, leaving strings untouched.
/// @param text The file's text.
/// @return Plain JSON text (if the rest of it is JSON).
std::string plainJson(std::string_view text) {
	std::string out;
	out.reserve(text.size());
	std::size_t pendingComma = std::string::npos; // where a comma waits until the next token says if it trails
	for(std::size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		if(c == '"') {
			const std::size_t start = i;
			for(++i; i < text.size() && text[i] != '"'; ++i)
				if(text[i] == '\\') ++i;
			out.append(text.substr(start, i - start + 1));
			pendingComma = std::string::npos;
		} else if(c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
			i = std::min(text.find('\n', i), text.size()) - 1;
		} else if(c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
			const std::size_t end = text.find("*/", i + 2);
			i = end == std::string_view::npos ? text.size() : end + 1;
		} else if(c == ',') {
			pendingComma = out.size();
			out.push_back(c);
		} else {
			if((c == '}' || c == ']') && pendingComma != std::string::npos) out[pendingComma] = ' ';
			if(std::isspace(static_cast<unsigned char>(c)) == 0) pendingComma = std::string::npos;
			out.push_back(c);
		}
	}
	return out;
}

/// @brief A theme colour as Forge writes colours.
/// @param value The theme's value.
/// @return `#rrggbb` or `#rrggbbaa` in lower case, or "" if it isn't a colour (`#rgb` and `#rgba` are expanded).
std::string colour(const affresco::json& value) {
	if(!value.is_string()) return "";
	std::string text = value.get<std::string>();
	if(text.empty() || text.front() != '#') return "";
	for(std::size_t i = 1; i < text.size(); ++i) {
		if(std::isxdigit(static_cast<unsigned char>(text[i])) == 0) return "";
		text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
	}
	if(text.size() == 4 || text.size() == 5) {
		std::string full = "#";
		for(std::size_t i = 1; i < text.size(); ++i) full.append(2, text[i]);
		return full;
	}
	return text.size() == 7 || text.size() == 9 ? text : "";
}

/// @brief Which of a token's defaults a theme type uses.
/// @param type The theme's `type`.
/// @return 0 for dark, 1 for light, 2 for high contrast.
std::size_t typeIndex(const std::string& type) {
	if(type == "light" || type == "vs") return 1;
	if(type.starts_with("hc") || type == "high-contrast") return 2;
	return 0;
}

} // namespace

std::string themeId(std::string_view name) {
	std::string id;
	for(const char c : name) {
		if(id.size() == 40) break;
		const auto u = static_cast<unsigned char>(c);
		if(std::isalnum(u) != 0 && u < 128) id.push_back(static_cast<char>(std::tolower(u)));
		else if(!id.empty() && id.back() != '-') id.push_back('-');
	}
	while(!id.empty() && id.back() == '-') id.pop_back();
	return id.empty() ? "imported" : id;
}

affresco::json importVsCodeTheme(std::string_view text, std::string_view fallbackName) {
	affresco::json theme;
	try {
		theme = affresco::parseUntrusted(plainJson(text));
	} catch(const std::exception&) {
		throw std::runtime_error("not a VS Code colour theme: the file isn't JSON");
	}
	if(!theme.is_object()) throw std::runtime_error("not a VS Code colour theme: the file isn't a JSON object");
	const auto field = [&theme](const char* key, std::string_view otherwise) {
		const auto it = theme.find(key);
		return it != theme.end() && it->is_string() ? it->get<std::string>() : std::string(otherwise);
	};
	const std::string name = field("name", fallbackName);
	const std::string type = field("type", "dark");
	const affresco::json colors = theme.value("colors", affresco::json::object());
	const std::string id = themeId(name);

	affresco::json patch = affresco::json::array();
	for(const TokenSource& source : tokenSources) {
		std::string value;
		for(const std::string_view key : source.keys) {
			if(colors.is_object() && colors.contains(key)) value = colour(colors[std::string(key)]);
			if(!value.empty()) break;
		}
		if(value.empty()) value = source.defaults[typeIndex(type)];
		patch.push_back({{"op", "add"}, {"path", "/tokens/color." + std::string(source.token) + "/" + id}, {"value", value}});
	}
	return {{"id", id}, {"name", name}, {"type", typeIndex(type) == 1 ? "light" : "dark"}, {"patch", std::move(patch)}};
}

} // namespace forge
