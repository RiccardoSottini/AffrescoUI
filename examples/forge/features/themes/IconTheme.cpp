/// @file
/// @brief Forge's file icon theme (IconTheme.hpp).

#include "features/themes/IconTheme.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace forge {

namespace {

/// @brief Whether a text is a hex colour: #rgb, #rrggbb or #rrggbbaa.
/// @param text The text.
/// @return Whether it is.
bool isHexColor(const std::string& text) {
	if(text.empty() || text.front() != '#') return false;
	const std::size_t digits = text.size() - 1;
	if(digits != 3 && digits != 6 && digits != 8) return false;
	return std::all_of(text.begin() + 1, text.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
}

/// @brief A look of the theme: `theme[section][key]`, or `theme[section]` itself without a key.
/// @param theme The theme.
/// @param section The part of the theme: "file", "folder", "extensions" or "names".
/// @param key The key to look up in it, or empty for the part itself.
/// @return The look, or null if there is none of the right shape.
const affresco::json* lookAt(const affresco::json& theme, const char* section, const std::string& key) {
	const auto part = theme.find(section);
	if(part == theme.end() || !part->is_object()) return nullptr;
	if(key.empty()) return &*part;
	const auto found = part->find(key);
	return found != part->end() && found->is_object() ? &*found : nullptr;
}

/// @brief Gives one item a look's icon and colour.
/// @param item The item.
/// @param look The look, or null for none.
void dress(affresco::json& item, const affresco::json* look) {
	if(!look) return;
	if(const auto icon = look->find("icon"); icon != look->end() && icon->is_string()) item["icon"] = *icon;
	if(const auto color = look->find("color"); color != look->end() && color->is_string() &&
	                                           isHexColor(color->get<std::string>()))
		item["iconColor"] = *color;
}

/// @brief The look of a file: by name, else by extension (the text after its last dot), else `file`.
/// @param theme The theme.
/// @param name The file name.
/// @return The look, or null.
const affresco::json* fileLook(const affresco::json& theme, const std::string& name) {
	if(const affresco::json* byName = lookAt(theme, "names", name)) return byName;
	const std::size_t dot = name.rfind('.');
	if(dot != std::string::npos && dot + 1 < name.size()) {
		std::string extension = name.substr(dot + 1);
		std::ranges::transform(extension, extension.begin(),
		                       [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
		if(const affresco::json* byExtension = lookAt(theme, "extensions", extension))
			return byExtension;
	}
	return lookAt(theme, "file", "");
}

} // namespace

void applyIconTheme(affresco::json& items, const affresco::json& theme) {
	if(!items.is_array() || !theme.is_object()) return;
	for(affresco::json& item : items) {
		if(!item.is_object()) continue;
		const auto children = item.find("children");
		if(children != item.end()) {
			applyIconTheme(*children, theme); // before dress(), whose new keys may move the children
			dress(item, lookAt(theme, "folder", ""));
		} else dress(item, fileLook(theme, item.value("label", std::string())));
	}
}

} // namespace forge
