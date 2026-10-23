/// @file
/// @brief Fuzzes Forge's colour theme importer (examples/forge/features/themes/ThemeImport.hpp): a theme file
/// someone downloaded is untrusted, and what it gives becomes a patch to the screen's design tokens.
///
/// A theme gives an id of lower-case letters, digits and dashes (at most 40), and a patch whose every op adds a colour
/// variant under /tokens/ as `#rrggbb` or `#rrggbbaa`; anything else is a std::runtime_error.

#include "Fuzz.hpp"

#include "features/themes/ThemeImport.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

/// @brief Tells whether a text is a colour the importer keeps.
/// @param text The text.
/// @return Whether it is `#rrggbb` or `#rrggbbaa`.
bool isColour(const std::string& text) {
	if((text.size() != 7 && text.size() != 9) || text[0] != '#') return false;
	for(std::size_t i = 1; i < text.size(); ++i) {
		const char c = text[i];
		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
	}
	return true;
}

/// @brief Tells whether a text is a theme id.
/// @param id The text.
/// @return Whether it is 1 to 40 of lower-case letters, digits and dashes.
bool isThemeId(const std::string& id) {
	if(id.empty() || id.size() > 40) return false;
	for(const char c : id)
		if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
	return true;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const auto text = affresco::fuzz::text(data, size);
	if(!isThemeId(forge::themeId(text))) affresco::fuzz::broken("a name becomes a theme id");
	try {
		const affresco::json theme = forge::importColorTheme(text, "fuzzed");
		if(!isThemeId(theme.at("id").get<std::string>())) affresco::fuzz::broken("a theme's id is a theme id");
		for(const affresco::json& op : theme.at("patch")) {
			if(op.at("op") != "add") affresco::fuzz::broken("a theme's ops add");
			if(!op.at("path").get<std::string>().starts_with("/tokens/")) affresco::fuzz::broken("ops stay in /tokens/");
			if(!isColour(op.at("value").get<std::string>())) affresco::fuzz::broken("ops add colours only");
		}
	} catch(const std::runtime_error&) {
		return 0; // not a theme: refused, as it should be
	}
	return 0;
}
