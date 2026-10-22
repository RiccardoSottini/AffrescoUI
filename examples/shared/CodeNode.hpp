#pragma once

/// @file
/// @brief The "code" node type (registerCodeEditor(), CodeEditor.hpp): its editor, and how a node's properties apply
/// to it and its events run the actions they name. Only CodeEditor.cpp includes it: it is a header so that every
/// target that compiles CodeEditor.cpp by name builds it unchanged.

#include "CodeEditor.hpp"
#include "CodeSyntax.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/format/SyntaxHighlighter.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace examples::node {

using Position = CodeEditor::Position;
using syntax::utf8;

/// Names of the kinds of text in "syntax", indexed by CodeEditor::Token ("plain" is the editor's "textColor").
inline constexpr std::array<std::string_view, 7> tokenNames = {"plain",  "keyword", "type",   "string",
                                                               "number", "comment", "heading"};

/// @brief The "code" type's editor: a CodeEditor and the actions its properties name.
struct ScreenCodeEditor : CodeEditor {
	std::string hoverAction;                         ///< "onHover".
	std::string typeAction;                          ///< "onType".
	affresco::json tokens = affresco::json::array(); ///< The "tokens" last given.
};

/// @brief The event of a place in the text, for "onHover" and "onType": its line and column from 1, and its box.
/// @param editor The editor.
/// @param at The place.
inline affresco::json caretEvent(const CodeEditor& editor, Position at) {
	affresco::json event{{"line", at.line + 1}, {"column", at.column + 1}};
	if(const std::optional<affresco::Rect> box = editor.rectAt(at)) {
		event["x"] = box->left;
		event["y"] = box->bottom;
		event["top"] = box->top;
	}
	return event;
}

/// @brief Makes a "code" node's editor, whose hovers and typing run the actions its properties name.
/// @param screen The screen that runs them.
inline std::unique_ptr<ScreenCodeEditor> makeScreenCodeEditor(affresco::Screen& screen) {
	auto editor = std::make_unique<ScreenCodeEditor>();
	ScreenCodeEditor& self = *editor;
	self.onHoverPosition.connect([&screen, &self](std::optional<Position> at) {
		if(self.hoverAction.empty()) return;
		affresco::json event = at ? caretEvent(self, *at) : affresco::json::object();
		event["over"] = at.has_value();
		screen.runAction(self.hoverAction, self, event.dump());
	});
	self.onTyped.connect([&screen, &self](const std::string& text) {
		if(self.typeAction.empty()) return;
		affresco::json event = caretEvent(self, self.caret());
		event["text"] = text;
		event["word"] = utf8(self.wordBeforeCaret());
		screen.runAction(self.typeAction, self, event.dump());
	});
	return editor;
}

/// @brief The action a property names.
/// @param properties The node's properties.
/// @param key The property.
/// @return Its text, or "" if it isn't a string.
inline std::string actionOf(const affresco::json& properties, const char* key) {
	const auto found = properties.find(key);
	return found != properties.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// @brief Applies "popup": while it is true, the popup's keys go to "onPopupKey".
/// @param screen The screen that runs the action.
/// @param editor The editor.
/// @param properties The node's properties.
inline void applyPopup(affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties) {
	if(const std::string action = actionOf(properties, "onPopupKey");
	   properties.value("popup", false) && !action.empty())
		editor.setPopupKeys([&screen, &editor, action](std::string_view key) {
			screen.runAction(action, editor, affresco::json{{"key", key}}.dump());
			return true;
		});
	else editor.setPopupKeys({});
}

/// @brief Applies "insert": `{ "text", "before", "request" }` replaces the selection and the code points before the
/// caret with the text, once per request (a completion).
/// @param editor The editor.
/// @param properties The node's properties.
inline void applyInsert(ScreenCodeEditor& editor, const affresco::json& properties) {
	const auto insert = properties.find("insert");
	if(insert == properties.end() || !insert->is_object()) return;
	const auto text = insert->find("text"); // not operator[]: on a const object, a missing key is undefined behaviour
	if(text == insert->end() || !text->is_string()) return;
	editor.insertAtCaret(affresco::codepointsOf(text->get<std::string>()),
	                     static_cast<std::size_t>(std::max(0.0, insert->value("before", 0.0))),
	                     static_cast<long long>(insert->value("request", 0.0)));
}

/// @brief Applies "buffer": the application's text for that id, shared with every editor showing it.
/// @param editor The editor.
/// @param properties The node's properties.
/// @param buffers The application's texts by id, or an empty function.
inline void applyBuffer(ScreenCodeEditor& editor, const affresco::json& properties, const BufferLookup& buffers) {
	const auto buffer = properties.find("buffer");
	if(!buffers || buffer == properties.end() || !buffer->is_string()) return;
	if(auto text = buffers(buffer->get<std::string>()); text && text != editor.sharedModel())
		editor.setModel(std::move(text));
}

/// @brief The grammar overlay runs of a "tokens" value: `{ "line", "column", "length", "style" }` each; runs that
/// aren't objects with a style are skipped.
/// @param tokens The value.
inline std::vector<affresco::format::SyntaxHighlighter::Overlay> overlayRuns(const affresco::json& tokens) {
	std::vector<affresco::format::SyntaxHighlighter::Overlay> runs;
	for(const affresco::json& run : tokens.is_array() ? tokens : affresco::json::array()) {
		if(!run.is_object() || !run.value("style", affresco::json()).is_string()) continue;
		const auto whole = [&run](const char* key) {
			return static_cast<std::size_t>(std::max(0.0, run.value(key, 0.0)));
		};
		runs.push_back({whole("line"), whole("column"), whole("length"), run["style"].get<std::string>()});
	}
	return runs;
}

/// @brief Applies "tokens": runs over the grammar's colours, given again only when they change (edits move them
/// meanwhile).
/// @param editor The editor.
/// @param properties The node's properties.
inline void applyTokens(ScreenCodeEditor& editor, const affresco::json& properties) {
	affresco::json tokens = properties.value("tokens", affresco::json::array());
	if(tokens == editor.tokens) return;
	std::vector<affresco::format::SyntaxHighlighter::Overlay> runs = overlayRuns(tokens);
	editor.tokens = std::move(tokens);
	editor.setOverlay(std::move(runs));
}

/// @brief Applies "goTo": `{ "line": 12, "request": 3 }` moves the caret, once per request (Forge's go to line).
/// @param editor The editor.
/// @param properties The node's properties.
inline void applyGoTo(ScreenCodeEditor& editor, const affresco::json& properties) {
	const auto goTo = properties.find("goTo");
	if(goTo == properties.end() || !goTo->is_object()) return;
	editor.goToLine(static_cast<std::size_t>(std::max(0.0, goTo->value("line", 1.0))),
	                static_cast<long long>(goTo->value("request", 0.0)),
	                static_cast<std::size_t>(std::max(0.0, goTo->value("column", 1.0))));
}

/// @brief Applies "syntax": a colour for each kind of text.
/// @param screen The screen, which resolves the colours.
/// @param editor The editor.
/// @param properties The node's properties.
/// @throws std::invalid_argument For a kind it doesn't know, or a value that isn't a colour.
inline void applySyntax(const affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties) {
	if(!properties.contains("syntax")) return;
	for(const auto& [name, value] : properties["syntax"].items()) {
		const auto token = static_cast<std::size_t>(std::ranges::find(tokenNames, name) - tokenNames.begin());
		if(token == 0 || token == tokenNames.size() || !value.is_string()) // "plain" is "textColor"
			throw std::invalid_argument("syntax." + name +
			                            ": expected keyword, type, string, number, comment or heading, with a colour");
		editor.setTokenColor(static_cast<CodeEditor::Token>(token), screen.resolveColor(value.get<std::string>()));
	}
}

/// @brief Applies a "code" node's properties to its editor, in an order that keeps the language before its colours.
/// @param screen The screen.
/// @param editor The editor.
/// @param properties The node's properties.
/// @param buffers The application's texts by id, or an empty function.
inline void applyProperties(affresco::Screen& screen, ScreenCodeEditor& editor, const affresco::json& properties,
                            const BufferLookup& buffers) {
	editor.hoverAction = actionOf(properties, "onHover");
	editor.typeAction = actionOf(properties, "onType");
	applyPopup(screen, editor, properties);
	applyInsert(editor, properties);
	applyBuffer(editor, properties, buffers);
	editor.setLanguage(properties.contains("language") ? properties["language"].get<std::string>() : "");
	editor.resetTokenColors();
	applyTokens(editor, properties);
	applyGoTo(editor, properties);
	applySyntax(screen, editor, properties);
}

} // namespace examples::node
