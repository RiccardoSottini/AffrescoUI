/// @file
/// @brief The "editor" keys of code editing: the keys bound to code commands, the popup, the one-off requests and
/// the runs painted over the colours, each read from untrusted input with its limits.

#include "LanguageKeys.hpp"

#include "LanguageHighlighter.hpp"
#include "Shortcuts.hpp"

#include "affresco/format/SyntaxHighlighter.hpp"

#include <algorithm>
#include <format>

namespace affresco::format {

namespace {

using expect::fail;
using expect::object;

constexpr double maxPlace = 1e9; ///< Lines, columns and counts read are kept below this.

/// @brief A whole number of a request, from 0, kept below maxPlace.
/// @param value The request.
/// @param key The key.
/// @param at The request's place.
/// @return It; 0 without the key.
std::size_t place(const json& value, const char* key, const std::string& at) {
	const auto found = value.find(key);
	if(found == value.end()) return 0;
	const float n = expect::number(*found, at + "." + key);
	if(!(n >= 0.0f)) fail(at + "." + key, "expected a number from 0");
	return static_cast<std::size_t>(std::min(static_cast<double>(n), maxPlace));
}

/// @brief A request's number: the request applies once per number.
/// @param value The request.
/// @param at Its place.
/// @return It; 0 without one.
double requestOf(const json& value, const std::string& at) {
	const auto found = value.find("request");
	if(found == value.end()) return 0.0;
	if(!found->is_number()) fail(at + ".request", "expected a number");
	return found->get<double>();
}

/// @brief Reads "commands": `{ "mod+/": "toggleComment", ... }`.
/// @param value The value.
/// @param at Its place.
/// @return The normalised shortcuts and their commands.
std::map<std::string, std::string, std::less<>> commandsOf(const json& value, const std::string& at) {
	object(value, at);
	if(value.size() > LanguageEditor::maxCommands)
		fail(at, std::format("expected at most {} keys", LanguageEditor::maxCommands));
	std::map<std::string, std::string, std::less<>> out;
	const auto& names = LanguageEditor::codeCommandNames();
	for(const auto& [key, item] : value.items()) {
		const std::string name = expect::string(item, at + "." + key);
		if(std::ranges::find(names, name) == names.end())
			fail(at + "." + key, "expected toggleComment, moveLinesUp, moveLinesDown, deleteLines, copyLine, "
			                     "cutLine, indentLines, outdentLines, insertLineAfter or insertLineBefore");
		const std::string shortcut = normalizeShortcut(key);
		if(shortcut.empty()) fail(at + "." + key, "expected a key, e.g. \"mod+/\"");
		out[shortcut] = name;
	}
	return out;
}

/// @brief Reads "tokens": `[{ "line", "column", "length", "style" }]` (from 0), at most
/// SyntaxHighlighter::maxOverlays.
/// @param value The value.
/// @param at Its place.
/// @return The runs.
std::vector<SyntaxHighlighter::Overlay> runsOf(const json& value, const std::string& at) {
	if(!value.is_array()) fail(at, "expected a list of runs");
	if(value.size() > SyntaxHighlighter::maxOverlays)
		fail(at, std::format("expected at most {} runs", SyntaxHighlighter::maxOverlays));
	std::vector<SyntaxHighlighter::Overlay> runs;
	runs.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string where = std::format("{}[{}]", at, i);
		object(value[i], where);
		if(!value[i].contains("style")) fail(where + ".style", "expected a style, e.g. \"variable\"");
		const std::string style = expect::string(value[i]["style"], where + ".style");
		runs.push_back({place(value[i], "line", where), place(value[i], "column", where),
		                place(value[i], "length", where), style});
	}
	return runs;
}

/// @brief Applies "tokens" to the editor's highlighter, when they changed since the last time.
/// @param editor The editor.
/// @param values The node's values.
/// @param path The node's place.
void applyTokens(TextEditor& editor, const json& values, const std::string& path) {
	auto* code = dynamic_cast<LanguageEditor*>(&editor);
	const json& tokens = values.contains("tokens") ? values["tokens"] : json::array();
	if(code == nullptr || !code->tokensChanged(tokens)) return;
	std::vector<SyntaxHighlighter::Overlay> runs = runsOf(tokens, path + ".tokens");
	if(auto* language = dynamic_cast<LanguageHighlighter*>(editor.highlighter())) language->setOverlay(std::move(runs));
	else if(auto* grammar = dynamic_cast<SyntaxHighlighter*>(editor.highlighter()))
		grammar->setOverlay(std::move(runs));
}

} // namespace

void applyCodeKeys(TextEditor& editor, const json& values, const std::string& path) {
	applyTokens(editor, values, path);
	auto* code = dynamic_cast<LanguageEditor*>(&editor);
	if(code == nullptr) return;
	code->setCommands(values.contains("commands") ? commandsOf(values["commands"], path + ".commands")
	                                              : std::map<std::string, std::string, std::less<>>());
	code->setPopup(values.contains("popup") && expect::boolean(values["popup"], path + ".popup"));
	// null: none, as a binding may give
	if(const auto goTo = values.find("goTo"); goTo != values.end() && !goTo->is_null()) {
		const std::string at = path + ".goTo";
		object(*goTo, at);
		code->goTo(place(*goTo, "line", at), place(*goTo, "column", at), requestOf(*goTo, at));
	}
	if(const auto insert = values.find("insert"); insert != values.end() && !insert->is_null()) {
		const std::string at = path + ".insert";
		object(*insert, at);
		const std::string text = insert->contains("text") ? expect::string((*insert)["text"], at + ".text") : "";
		code->insertText(text, place(*insert, "before", at), requestOf(*insert, at));
	}
}

Connection listenToCodeEvents(TextEditor& editor, const EventSink& sink) {
	auto* code = dynamic_cast<LanguageEditor*>(&editor);
	if(code == nullptr) return 0;
	return code->onCode.connect([sink](std::string_view handler, const json& event) { sink(handler, event); });
}

void stopListeningToCodeEvents(TextEditor& editor, Connection connection) {
	if(auto* code = dynamic_cast<LanguageEditor*>(&editor)) code->onCode.disconnect(connection);
}

} // namespace affresco::format
