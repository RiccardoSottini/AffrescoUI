/// @file
/// @brief The kit's design tokens (KitTokens.hpp): one table of names, dark and light values and what each is for.

#include "KitTokens.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace affresco::format {

namespace {

/// @brief One kit token.
struct KitToken {
	std::string_view name;  ///< "kit.surface".
	std::string_view dark;  ///< Its dark colour (the base), or empty for a size.
	std::string_view light; ///< Its light colour, or empty for a size.
	double size;            ///< A size's value, in pixels (0 for a colour).
	std::string_view about; ///< What it is for.
};

/// @brief Every kit token. The dark colours are those the built-in components had before the kit.
constexpr std::array kitTable{
    KitToken{"kit.background", "#1e1e1e", "#ffffff", 0, "Pages and the settings page."},
    KitToken{"kit.surface", "#252526", "#ffffff", 0, "Menus, dialogs, toasts, cards and popovers."},
    KitToken{"kit.bar", "#3c3c3c", "#f3f4f6", 0, "Menu bars, toolbars and status bars."},
    KitToken{"kit.barHover", "#505050", "#e5e7eb", 0, "A bar's item under the pointer, focused or open."},
    KitToken{"kit.field", "#3c3c3c", "#ffffff", 0, "Text fields."},
    KitToken{"kit.text", "#cccccc", "#1f2328", 0, "Text."},
    KitToken{"kit.dim", "#8b8b8b", "#6e7781", 0, "Secondary text: shortcuts, hints, disabled items."},
    KitToken{"kit.body", "#a0a0a0", "#57606a", 0, "A dialog's message and a card's text."},
    KitToken{"kit.selection", "#04395e", "#ddf4ff", 0, "The highlighted item of a menu or a list."},
    KitToken{"kit.border", "#454545", "#d0d7de", 0, "Borders and separators."},
    KitToken{"kit.primary", "#0e639c", "#0969da", 0, "The main button and progress."},
    KitToken{"kit.onPrimary", "#ffffff", "#ffffff", 0, "Text on `kit.primary`."},
    KitToken{"kit.secondary", "#3a3d41", "#eaeef2", 0, "Other buttons, tracks, a pressed or checked item."},
    KitToken{"kit.onSecondary", "#ffffff", "#1f2328", 0, "Text on `kit.secondary`."},
    KitToken{"kit.accent", "#2aaaff", "#0969da", 0, "Matched letters, links and the focused item's mark."},
    KitToken{"kit.focus", "#ffffff", "#0969da", 0, "The ring of the focused button."},
    KitToken{"kit.backdrop", "#00000088", "#00000040", 0, "Behind a modal dialog."},
    KitToken{"kit.info", "#3794ff", "#0969da", 0, "Information."},
    KitToken{"kit.warning", "#cca700", "#9a6700", 0, "Warnings."},
    KitToken{"kit.error", "#f14c4c", "#cf222e", 0, "Errors."},
    KitToken{"kit.success", "#89d185", "#1a7f37", 0, "Success."},
    KitToken{"kit.radius", "", "", 4, "The corners of menus, buttons and fields."},
    KitToken{"kit.cardRadius", "", "", 6, "The corners of dialogs, toasts, cards and popovers."},
    KitToken{"kit.borderWidth", "", "", 1, "Borders."},
};

/// @brief Makes the tokens section.
/// @param themed Whether colours get a light variant.
/// @return The tokens.
json makeTokens(bool themed) {
	json tokens = json::object();
	for(const KitToken& token : kitTable) {
		const std::string name(token.name);
		if(token.dark.empty()) tokens[name] = token.size;
		else if(!themed) tokens[name] = token.dark;
		else tokens[name] = {{"dark", token.dark}, {"light", token.light}};
	}
	return tokens;
}

/// @brief Tells whether a section has a "kit." name.
/// @param section The section (anything that isn't an object has none).
/// @return true if it does.
bool hasKitName(const json& section) {
	if(!section.is_object()) return false;
	for(auto entry = section.begin(); entry != section.end(); ++entry)
		if(entry.key().starts_with("kit.")) return true;
	return false;
}

} // namespace

const json& kitTokens(bool themed) {
	static const json plain = makeTokens(false);
	static const json both = makeTokens(true);
	return themed ? both : plain;
}

bool isKitToken(std::string_view name) noexcept {
	return std::ranges::any_of(kitTable, [name](const KitToken& token) { return token.name == name; });
}

bool followsKitThemes(const json& document) {
	if(!document.is_object()) return false;
	const auto tokens = document.find("tokens");
	const auto colors = document.find("colors");
	return (tokens != document.end() && hasKitName(*tokens)) || (colors != document.end() && hasKitName(*colors));
}

std::string kitTokenTable() {
	std::string text = "| Token | Dark | Light | For |\n|---|---|---|---|\n";
	for(const KitToken& token : kitTable) {
		const std::string size = std::to_string(static_cast<int>(token.size));
		const std::string dark = token.dark.empty() ? size : "`" + std::string(token.dark) + "`";
		const std::string light = token.light.empty() ? size : "`" + std::string(token.light) + "`";
		text += "| `" + std::string(token.name) + "` | " + dark + " | " + light + " | " + std::string(token.about) +
		        " |\n";
	}
	return text;
}

} // namespace affresco::format
