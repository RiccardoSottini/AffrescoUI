/// @file
/// @brief The "editor" node type: a TextEditor, its font, line numbers and colours, and its text. An application's
/// TextEditor subclass has these properties too. Its "caret" is the runtime's (Events.cpp), as it writes the state.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/TextEditor.hpp"

#include <array>
#include <format>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets a text editor's look: the defaults, then what the node gives.
/// @param panel The panel, a TextEditor.
/// @param values Its font, textColor, lineNumbers and colours, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyEditor(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return;
	editor->setFont(nullptr);
	editor->setLineNumbers(false);
	editor->resetColors();

	using Setter = void (TextEditor::*)(Color);
	static constexpr std::array<std::pair<const char*, Setter>, 6> colorSetters = {
	    {{"activeLineNumberColor", &TextEditor::setActiveLineNumberColor},
		 {"caretColor", &TextEditor::setCaretColor},
		 {"lineColor", &TextEditor::setLineColor},
		 {"lineNumberColor", &TextEditor::setLineNumberColor},
		 {"selectionColor", &TextEditor::setSelectionColor},
		 {"textColor", &TextEditor::setTextColor}}};
	for(const auto& [key, value] : values.items()) {
		const std::string at = std::format("{}.{}", path, key);
		if(key == "font") editor->setFont(reader.font(value, at));
		else if(key == "lineNumbers") editor->setLineNumbers(boolean(value, at));
		for(const auto& [name, setter] : colorSetters)
			if(key == name) (editor->*setter)(reader.color(value, at));
	}
}

/// @brief Shows an editor's text: its bound value always, else "text" when it changed (so typing isn't overwritten
/// by every update).
/// @param panel The panel, a TextEditor.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText The unbound text shown last time.
void showText(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return;
	if(bound) {
		editor->setText(text(*bound));
		return;
	}
	const std::optional<std::string> newText =
	    properties.contains("text") ? std::optional<std::string>(text(properties["text"])) : std::nullopt;
	if(newText != lastText) editor->setText(newText.value_or(""));
	lastText = newText;
}

/// @brief Reports text the user changed.
/// @param panel The panel, a TextEditor.
/// @param changed Gets the text.
/// @return The connection.
Connection listenToEditor(Panel& panel, std::function<void(const json&)> changed) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return 0;
	return editor->onTextChanged.connect([changed = std::move(changed)](const std::string& value) { changed(value); });
}

/// @brief Disconnects listenToEditor().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToEditor(Panel& panel, Connection connection) {
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) editor->onTextChanged.disconnect(connection);
}

} // namespace

void registerEditorType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},        {"text", ValueKind::Text},
	                   {"caret", ValueKind::StatePath},       {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},       {"lineNumbers", ValueKind::Boolean},
	                   {"lineNumberColor", ValueKind::Color}, {"activeLineNumberColor", ValueKind::Color},
	                   {"lineColor", ValueKind::Color},       {"selectionColor", ValueKind::Color},
	                   {"caretColor", ValueKind::Color},      {"autofocus", ValueKind::Boolean}};
	set->apply = applyEditor;
	set->everyUpdate = true; // colours follow the theme
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TextEditor*>(&panel) != nullptr; };
	set->value = {"text", showText, listenToEditor, stopListeningToEditor};

	registry.add({"editor", "A multi-line text editor: selection, undo, clipboard, optional line numbers",
	              [] { return std::make_unique<TextEditor>(); }, set, false});
}

} // namespace affresco::format
