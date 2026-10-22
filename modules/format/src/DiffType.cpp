/// @file
/// @brief The "diff" node type: a DiffView comparing two texts, side by side or inline, in a language's colours (an
/// application's editor factory gives them), with its font and its change colours.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/DiffView.hpp"

#include <algorithm>
#include <memory>
#include <string>

namespace affresco::format {

namespace {

/// @brief Sets a diff view from its node: texts, mode, language, font and colours.
/// @param panel The panel, a DiffView.
/// @param values Its keys.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyDiff(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<DiffView*>(&panel);
	if(!view) return;
	const auto textOf = [&values](const char* key) { return values.contains(key) ? text(values[key]) : std::string(); };
	const std::string mode = values.contains("mode") ? string(values["mode"], path + ".mode") : "split";
	if(mode != "split" && mode != "inline") fail(path + ".mode", "expected split or inline");
	view->setMode(mode == "inline" ? DiffView::Mode::Inline : DiffView::Mode::Split);
	view->setLanguage(values.contains("language") ? string(values["language"], path + ".language") : "");
	view->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	view->setTextColor(values.contains("textColor") ? reader.color(values["textColor"], path + ".textColor")
	                                                : Color{0.0f, 0.0f, 0.0f, 0.0f});
	// A changed line's characters are marked in its colour, twice as strong
	DiffColors colors;
	if(values.contains("deletedColor")) {
		colors.deleted = reader.color(values["deletedColor"], path + ".deletedColor");
		colors.deletedText = colors.deleted.withAlpha(std::min(colors.deleted.a * 2.0f, 1.0f));
	}
	if(values.contains("insertedColor")) {
		colors.inserted = reader.color(values["insertedColor"], path + ".insertedColor");
		colors.insertedText = colors.inserted.withAlpha(std::min(colors.inserted.a * 2.0f, 1.0f));
	}
	view->setColors(colors);
	view->setTexts(textOf("original"), textOf("modified"));
}

} // namespace

void registerDiffType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"original", ValueKind::Text},       {"modified", ValueKind::Text},
	                   {"mode", ValueKind::Text},           {"language", ValueKind::Text},
	                   {"font", ValueKind::Font},           {"textColor", ValueKind::Color},
	                   {"insertedColor", ValueKind::Color}, {"deletedColor", ValueKind::Color}};
	set->apply = applyDiff;
	set->everyUpdate = true; // colours follow the theme
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const DiffView*>(&panel) != nullptr; };

	registry.add({"diff", "Two texts compared line by line and character by character, side by side or inline",
	              [] { return std::make_unique<DiffView>(); }, set, false});
}

} // namespace affresco::format
