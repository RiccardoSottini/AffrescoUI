/// @file
/// @brief The "codeInput" node type: a CodeInput, a one-time-code input of a fixed number of cells, with its colours.
/// Its text, "bind", "onChange" and "onSubmit" are a text area's (the runtime's), as CodeInput is one.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/CodeInput.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace affresco::format {

namespace {

/// @brief Sets a code input from its node: its length and colours, the defaults for those it doesn't give.
/// @param panel The panel, a CodeInput.
/// @param values Its keys.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applyCodeInput(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* input = dynamic_cast<CodeInput*>(&panel);
	if(!input) return;
	int length = 6;
	if(values.contains("length")) {
		length = integer(values["length"], path + ".length");
		if(length < 1 || length > static_cast<int>(CodeInput::mostCells)) fail(path + ".length", "expected 1 to 12");
	}
	input->setLength(static_cast<std::size_t>(length));
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	input->setCellColor(colorOr("cellColor", colors::white));
	input->setCellBorderColor(colorOr("cellBorderColor", colors::idle));
	input->setActiveColor(colorOr("activeColor", colors::accent));
	input->setGap(values.contains("cellGap") ? number(values["cellGap"], path + ".cellGap") : 8.0f);
	input->setCellRadius(values.contains("cellRadius") ? number(values["cellRadius"], path + ".cellRadius") : 6.0f);
}

} // namespace

void registerCodeInputType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	// A text area's keys (the runtime applies them), then its own
	set->properties = {{"text", ValueKind::Text},         {"textColor", ValueKind::Color, true, true},
	                   {"font", ValueKind::Font},         {"padding", ValueKind::Number},
	                   {"bind", ValueKind::StatePath},    {"check"},
	                   {"autofocus", ValueKind::Boolean}, {"length", ValueKind::Number},
	                   {"cellColor", ValueKind::Color},   {"cellBorderColor", ValueKind::Color},
	                   {"activeColor", ValueKind::Color}, {"cellGap", ValueKind::Number},
	                   {"cellRadius", ValueKind::Number}};
	set->apply = applyCodeInput;
	set->everyUpdate = true; // colours follow the theme
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const CodeInput*>(&panel) != nullptr; };
	registry.add({"codeInput", "A one-time-code input: one character per cell, digits unless \"accept\" says",
	              [] { return std::make_unique<CodeInput>(); }, set, false});
}

} // namespace affresco::format
