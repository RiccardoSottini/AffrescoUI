/// @file
/// @brief The "editor" node type: a TextEditor, its font, line numbers and colours, its text, and its view: wrapping,
/// folds, decorations, the find highlight, brackets, indentation guides and read-only. An application's TextEditor
/// subclass has these properties too. Its "caret" is the runtime's (Events.cpp), as it writes the state.

#include "ExpressionFunctions.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/format/SyntaxHighlighter.hpp"
#include "affresco/render/TextEditor.hpp"

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxListed = 100'000; ///< The most decorations or folds read from a node (untrusted input).

/// @brief Decodes UTF-8 (an invalid byte stands for itself).
/// @param text The text.
std::u32string codepoints(std::string_view text) {
	std::u32string out;
	for(std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t size = std::min(detail::utf8CharacterSize(lead), text.size() - i);
		char32_t c = size == 1 ? lead : lead & (0xFFu >> (size + 1));
		for(std::size_t k = 1; k < size; ++k) c = (c << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
		out += c;
		i += size;
	}
	return out;
}

/// @brief Reads a line number (from 0), clamped to what a size holds.
/// @param value The number.
/// @param path Its place, for errors.
std::size_t lineNumber(const json& value, const std::string& path) {
	const float n = number(value, path);
	if(!(n >= 0.0f)) fail(path, "expected a line number, from 0");
	return n >= 1e15f ? SIZE_MAX : static_cast<std::size_t>(n);
}

/// @brief Reads one decoration: {line, endLine, column, endColumn, kind, color, icon, text}.
/// @param value The decoration.
/// @param reader Reads its colour.
/// @param path Its place, for errors.
EditorDecoration readDecoration(const json& value, const PropertyReader& reader, const std::string& path) {
	object(value, path);
	EditorDecoration d;
	d.line = value.contains("line") ? lineNumber(value["line"], path + ".line") : 0;
	d.endLine = value.contains("endLine") ? lineNumber(value["endLine"], path + ".endLine") : d.line;
	if(value.contains("column")) d.column = lineNumber(value["column"], path + ".column");
	if(value.contains("endColumn")) d.endColumn = lineNumber(value["endColumn"], path + ".endColumn");
	const std::string kind = value.contains("kind") ? string(value["kind"], path + ".kind") : "background";
	using Kind = EditorDecoration::Kind;
	static constexpr std::array<std::pair<const char*, Kind>, 4> kinds = {
	    {{"background", Kind::Background}, {"squiggle", Kind::Squiggle}, {"gutter", Kind::Gutter}, {"inline", Kind::Inline}}};
	const auto found = std::ranges::find_if(kinds, [&kind](const auto& k) { return kind == k.first; });
	if(found == kinds.end()) fail(path + ".kind", "expected background, squiggle, gutter or inline");
	d.kind = found->second;
	d.color = value.contains("color") ? reader.color(value["color"], path + ".color") : colors::accent.withAlpha(0.25f);
	if(value.contains("icon")) {
		const std::u32string icon = codepoints(string(value["icon"], path + ".icon"));
		d.icon = icon.empty() ? 0 : icon.front();
	}
	if(value.contains("text")) d.text = codepoints(text(value["text"]));
	return d;
}

/// @brief Reads a list of up to maxListed items.
/// @param values The node's values.
/// @param key The list's key.
/// @param path The node's place, for errors.
/// @param read Reads one item, given it and its place.
template <typename Item, typename Read>
std::vector<Item> readList(const json& values, const char* key, const std::string& path, Read read) {
	std::vector<Item> items;
	if(!values.contains(key)) return items;
	const json& given = values[key];
	const std::string at = path + "." + key;
	if(!given.is_array()) fail(at, "expected a list");
	items.reserve(std::min(given.size(), maxListed));
	for(std::size_t i = 0; i < given.size() && i < maxListed; ++i)
		items.push_back(read(given[i], concatenate(at, "[", std::to_string(i) + "]")));
	return items;
}

/// @brief Runs the find step a find object asks for, once per "request": "step" is first, next, previous, replace or
/// replaceAll, and "replace" the replacement.
/// @param editor The editor.
/// @param find The find object.
/// @param at Its place, for errors.
void applyFindStep(TextEditor& editor, const json& find, const std::string& at) {
	if(!find.contains("step") || !find.contains("request")) return;
	using Step = TextEditor::FindStep;
	static const std::array<std::pair<std::string_view, Step>, 5> steps = {
	    {{"first", Step::First}, {"next", Step::Next}, {"previous", Step::Previous}, {"replace", Step::Replace},
	     {"replaceAll", Step::ReplaceAll}}};
	const std::string name = string(find["step"], at + ".step");
	const auto found = std::ranges::find(steps, std::string_view(name), &std::pair<std::string_view, Step>::first);
	if(found == steps.end()) fail(at + ".step", "expected first, next, previous, replace or replaceAll");
	const float request = number(find["request"], at + ".request");
	if(request < 1) return; // none asked for yet
	const std::string replacement = find.contains("replace") ? text(find["replace"]) : std::string();
	static_cast<void>(editor.findStep(found->second, replacement, static_cast<std::uint64_t>(request)));
}

/// @brief Sets an editor's find highlight: {text, regex, wholeWord, caseSensitive, inSelection}, or none, and runs
/// its step (applyFindStep()).
/// @param editor The editor.
/// @param values The node's values.
/// @param path The node's place, for errors.
void applyFind(TextEditor& editor, const json& values, const std::string& path) {
	if(!values.contains("find") || values["find"].is_null()) { // null: none, as a binding may give
		static_cast<void>(editor.setFind({}, {}));
		return;
	}
	const json& find = values["find"];
	const std::string at = path + ".find";
	object(find, at);
	const auto flag = [&](const char* key) { return find.contains(key) && boolean(find[key], at + "." + key); };
	const PatternOptions options{.regex = flag("regex"), .wholeWord = flag("wholeWord"),
	                             .caseSensitive = flag("caseSensitive")};
	// A pattern that doesn't compile highlights nothing: the user may be typing it
	static_cast<void>(editor.setFind(find.contains("text") ? text(find["text"]) : std::string(), options));
	editor.setFindInSelection(flag("inSelection"));
	applyFindStep(editor, find, at);
}

/// @brief Sets an editor's grammar: a highlighter for the language "grammar" names (or a file name's), its styles in
/// the colours "@syntax.<style>" gives (or its parent style's), or none. Only a highlighter this key installed is
/// replaced or removed: a subclass may install its own.
/// @param editor The editor.
/// @param values The node's values.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applyGrammar(TextEditor& editor, const json& values, const PropertyReader& reader, const std::string& path) {
	static const char tag = 0; // what marks the highlighters this key installs
	auto* current = dynamic_cast<SyntaxHighlighter*>(editor.highlighter());
	const bool ours = current && current->owner() == &tag;
	std::string language;
	if(values.contains("grammar")) {
		const std::string given = string(values["grammar"], path + ".grammar");
		language = SyntaxHighlighter::canonical(given);
		if(language.empty()) language = SyntaxHighlighter::languageFor(given);
	}
	if(language.empty()) {
		if(ours) editor.setHighlighter(nullptr);
		return;
	}
	if(editor.highlighter() && !ours) return;
	if(!ours || current->language() != language) {
		auto made = SyntaxHighlighter::create(language);
		made->setOwner(&tag);
		current = made.get();
		editor.setHighlighter(std::move(made));
	}
	// Resolved now, in the current theme: the reader lives only through this update
	std::map<std::string, Color, std::less<>> palette;
	for(const std::string& style : current->styles())
		for(std::string_view name = style; !name.empty();) {
			if(palette.contains(name)) break;
			try {
				palette.emplace(name, reader.color("@syntax." + std::string(name), path + ".grammar"));
				break;
			} catch(const DocumentError&) {
				// no such token: the parent style's, if any
			}
			const std::size_t dot = name.rfind('.');
			name = dot == std::string_view::npos ? std::string_view() : name.substr(0, dot);
		}
	current->setPalette([palette = std::move(palette)](std::string_view style) -> std::optional<Color> {
		const auto found = palette.find(style);
		return found == palette.end() ? std::nullopt : std::optional(found->second);
	});
}

/// @brief Sets an editor's view: wrap, folds, decorations, find, brackets, indentGuides, minimap, composition and
/// readOnly.
/// @param editor The editor.
/// @param values The node's values.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applyView(TextEditor& editor, const json& values, const PropertyReader& reader, const std::string& path) {
	const auto flag = [&](const char* key) { return values.contains(key) && boolean(values[key], path + "." + key); };
	editor.setWrap(flag("wrap"));
	editor.setBracketMatching(flag("brackets"));
	editor.setIndentGuides(flag("indentGuides"));
	editor.setMinimap(flag("minimap"));
	editor.setComposition(std::string_view(values.contains("composition") ? text(values["composition"]) : std::string()));
	editor.setReadOnly(flag("readOnly"));
	// Folds the user made stay until the document gives others
	if(values.contains("folds"))
		editor.offerFolds(readList<FoldRange>(values, "folds", path, [](const json& value, const std::string& at) {
			object(value, at);
			const std::size_t line = value.contains("line") ? lineNumber(value["line"], at + ".line") : 0;
			return FoldRange{line, value.contains("endLine") ? lineNumber(value["endLine"], at + ".endLine") : line};
		}));
	editor.setDecorations(readList<EditorDecoration>(values, "decorations", path,
	                                                 [&reader](const json& value, const std::string& at) {
		                                                 return readDecoration(value, reader, at);
	                                                 }));
	applyFind(editor, values, path);
}

/// @brief Sets a text editor's look: the defaults, then what the node gives.
/// @param panel The panel, a TextEditor.
/// @param values Its font, textColor, lineNumbers, colours and view keys, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyEditor(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return;
	editor->setFont(nullptr);
	editor->setLineNumbers(false);
	editor->resetColors();

	using Setter = void (TextEditor::*)(Color);
	static constexpr std::array<std::pair<const char*, Setter>, 9> colorSetters = {
	    {{"activeLineNumberColor", &TextEditor::setActiveLineNumberColor},
		 {"bracketColor", &TextEditor::setBracketColor},
		 {"caretColor", &TextEditor::setCaretColor},
		 {"guideColor", &TextEditor::setGuideColor},
		 {"matchColor", &TextEditor::setMatchColor},
		 {"lineColor", &TextEditor::setLineColor},
		 {"lineNumberColor", &TextEditor::setLineNumberColor},
		 {"selectionColor", &TextEditor::setSelectionColor},
		 {"textColor", &TextEditor::setTextColor}}};
	for(const auto& [key, value] : values.items()) {
		const std::string at = concatenate(path, ".", key);
		if(key == "font") editor->setFont(reader.font(value, at));
		else if(key == "lineNumbers") editor->setLineNumbers(boolean(value, at));
		for(const auto& [name, setter] : colorSetters)
			if(key == name) (editor->*setter)(reader.color(value, at));
	}
	applyView(*editor, values, reader, path);
	applyGrammar(*editor, values, reader, path);
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

/// @brief Runs "onGutterClick" on a press in the gutter and "onWordHover" when the pointer moves onto a word or off
/// one.
/// @param panel The panel, a TextEditor.
/// @param sink Runs a handler.
/// @return The connections.
EventConnections listenToEditorEvents(Panel& panel, EventSink sink) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return {};
	const Connection gutter = editor->onGutterClicked.connect([sink](std::size_t line, Mods mods) {
		sink("onGutterClick", json{{"type", "gutterClick"},
		                           {"line", line},
		                           {"shift", mods.shift()},
		                           {"alt", mods.alt()},
		                           {"control", mods.control()},
		                           {"super", mods.super()}});
	});
	const Connection hover = editor->onWordHovered.connect([sink](TextEditor::Position at, const std::string& word) {
		const bool onWord = at.line != SIZE_MAX;
		sink("onWordHover", json{{"type", "wordHover"},
		                         {"line", onWord ? json(at.line) : json()},
		                         {"column", onWord ? json(at.column) : json()},
		                         {"word", word}});
	});
	return {gutter, hover, 0, 0};
}

/// @brief Disconnects listenToEditorEvents().
/// @param panel The panel.
/// @param connections The connections.
void stopListeningToEditorEvents(Panel& panel, const EventConnections& connections) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return;
	editor->onGutterClicked.disconnect(connections[0]);
	editor->onWordHovered.disconnect(connections[1]);
}

} // namespace

void registerEditorType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},        {"text", ValueKind::Text},
	                   {"caret", ValueKind::StatePath},       {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},       {"lineNumbers", ValueKind::Boolean},
	                   {"lineNumberColor", ValueKind::Color}, {"activeLineNumberColor", ValueKind::Color},
	                   {"lineColor", ValueKind::Color},       {"selectionColor", ValueKind::Color},
	                   {"caretColor", ValueKind::Color},      {"autofocus", ValueKind::Boolean},
	                   {"wrap", ValueKind::Boolean},          {"folds", ValueKind::Any},
	                   {"decorations", ValueKind::Any},       {"find", ValueKind::Any},
	                   {"brackets", ValueKind::Boolean},      {"indentGuides", ValueKind::Boolean},
	                   {"readOnly", ValueKind::Boolean},      {"guideColor", ValueKind::Color},
	                   {"matchColor", ValueKind::Color},      {"bracketColor", ValueKind::Color},
	                   {"grammar", ValueKind::Text},          {"minimap", ValueKind::Boolean},
	                   {"composition", ValueKind::Text}};
	set->apply = applyEditor;
	set->everyUpdate = true; // colours follow the theme
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TextEditor*>(&panel) != nullptr; };
	set->value = {"text", showText, listenToEditor, stopListeningToEditor};
	set->events = {listenToEditorEvents, stopListeningToEditorEvents};

	registry.add({"editor", "A multi-line text editor: selection, undo, clipboard, line numbers, wrap, folds, decorations, find, cursors",
	              [] { return std::make_unique<TextEditor>(); }, set, false});
}

} // namespace affresco::format
