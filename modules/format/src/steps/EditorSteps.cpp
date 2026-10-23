/// @file
/// @brief The step that runs a code editor's command, types text or cuts a span: edit.

#include "Engine.hpp"

#include "affresco/render/Controls.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// @brief A place in the text given by the step, counted from 1 as "caret" shows it.
/// @param s The step.
/// @param line The key of the line.
/// @param column The key of the column.
/// @param otherwise The place when the step has no line.
/// @return The place, counted from 0.
TextPosition placeOf(const Running& s, const char* line, const char* column, TextPosition otherwise) {
	if(!s.step.contains(line)) return otherwise;
	// A number of the step, counted from 1, as a count from 0
	const auto fromOne = [&s](const char* key) {
		return static_cast<std::size_t>(std::max(static_cast<double>(number(s.value(key), s.where + "." + key)), 1.0) -
		                                1.0);
	};
	return {fromOne(line), s.step.contains(column) ? fromOne(column) : 0};
}

/// @brief "type": "text" at the caret of an editor or a text field, over its selection, at once.
/// @param s The step.
void typeText(const Running& s) {
	const std::string utf8 = string(s.value("text"), s.where + ".text");
	Panel* panel = stepTarget(s);
	if(!panel) return;
	if(auto* editor = dynamic_cast<TextEditor*>(panel)) editor->insertText(utf8);
	else if(auto* field = dynamic_cast<TextArea*>(panel)) field->insertText(utf8);
	else fail(s.where + ".in", "is not an editor or a text field");
}

/// @brief The span a "cut" takes from an editor.
/// @param editor The editor.
/// @param kind "lineEnd", "word", "selection" or "line".
/// @param where The step's place, for errors.
/// @return Its start and end, start first.
std::pair<TextEditor::Position, TextEditor::Position> cutSpan(const TextEditor& editor, const std::string& kind,
                                                              const std::string& where) {
	using Position = TextEditor::Position;
	const Position caret = editor.caret();
	const std::size_t lines = editor.model().lineCount();
	const std::size_t length = editor.model().lineLength(caret.line);
	if(kind == "lineEnd") { // to the end of the line, or its break when the caret is there already
		const Position end{caret.line, length};
		return {caret, caret == end ? editor.nextPosition(caret, true, false) : end};
	}
	if(kind == "word") return {caret, editor.nextPosition(caret, true, true)};
	if(kind == "selection") {
		const Position anchor = editor.anchor();
		return anchor < caret ? std::pair{anchor, caret} : std::pair{caret, anchor};
	}
	if(kind == "line") { // the whole line and its break; the last line takes the break before it
		if(caret.line + 1 < lines) return {Position{caret.line, 0}, Position{caret.line + 1, 0}};
		if(caret.line == 0) return {Position{0, 0}, Position{0, length}};
		return {Position{caret.line - 1, editor.model().lineLength(caret.line - 1)}, Position{caret.line, length}};
	}
	fail(where + ".span", "must be \"lineEnd\", \"word\", \"selection\" or \"line\"");
}

/// @brief "cut": a "span" of an editor (or the whole of a text field, for "line"), removed unless "keep", to "into"
/// (added to its text with "append") or else the clipboard, at once.
/// @param s The step.
void cut(const Running& s) {
	const std::string& where = s.where;
	const std::string kind = s.step.contains("span") ? string(s.value("span"), where + ".span") : "selection";
	const bool keep = s.step.contains("keep") && boolean(s.value("keep"), where + ".keep");
	const bool append = s.step.contains("append") && boolean(s.value("append"), where + ".append");
	Panel* panel = stepTarget(s);
	if(!panel) return;
	std::string taken;
	if(auto* editor = dynamic_cast<TextEditor*>(panel)) {
		const auto [from, to] = cutSpan(*editor, kind, where);
		taken = keep || editor->readOnly() ? editor->textBetween(from, to) : editor->removeText(from, to);
	} else if(auto* field = dynamic_cast<TextArea*>(panel)) {
		if(kind != "line" && kind != "lineEnd" && kind != "word" && kind != "selection")
			fail(where + ".span", "must be \"lineEnd\", \"word\", \"selection\" or \"line\"");
		if(kind == "line") taken = keep ? field->text() : field->removeLast(field->text().size());
	} else {
		fail(where + ".in", "is not an editor or a text field");
	}
	Engine& engine = s.steps.engine();
	if(!s.step.contains("into")) {
		if(!taken.empty()) engine.window->setClipboardText(taken);
		return;
	}
	const json::json_pointer pointer = s.target("into");
	json& place = statePlace(engine.versions.state(), pointer, where + ".into");
	json result = append && place.is_string() ? json(place.get<std::string>() + taken) : json(taken);
	engine.document.spendBytes(result, where + ".into");
	expect::shallowEnough(result, pointerDepth(pointer), where + ".into");
	place = std::move(result);
	engine.document.markStateChanged(pointer);
}

/// @brief "edit": a command of a code editor by id ("in", else the focused one), once the event is handled, so the
/// editor has its text; "type" and "cut" act at once, so the steps after them see what they did.
/// @param s The step.
void edit(const Running& s) {
	const std::string command = text(s.value("edit"));
	if(command == "type") return typeText(s);
	if(command == "cut") return cut(s);
	const auto& names = TextEditor::commandNames();
	if(command != "goTo" && command != "select" && std::ranges::find(names, command) == names.end())
		fail(s.where + ".edit", "unknown command '" + command + "'");
	std::string id;
	if(s.step.contains("in")) {
		id = text(s.value("in"));
		if(id.starts_with('#')) id.erase(0, 1);
	}
	const TextPosition at = placeOf(s, "line", "column", {});
	const TextPosition to = placeOf(s, "toLine", "toColumn", at);
	const std::string prefix = s.step.contains("prefix") ? text(s.value("prefix")) : "//";
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	engine.scheduler.defer([&engine, id, command, at, to, prefix] {
		Panel* panel = id.empty() ? engine.window->focusedPanel() : engine.window->root().findById(id);
		auto* editor = dynamic_cast<TextEditor*>(panel);
		if(!editor) return;
		if(command == "goTo") editor->selectRange(at, at);
		else if(command == "select") editor->selectRange(at, to);
		else if(command == "toggleComment") editor->toggleComment(prefix);
		else editor->runCommand(command);
	});
}

/// @brief Runs the editor step, if the step is one.
/// @param s The step.
/// @return Whether to go on, or nothing if not an editor step.
std::optional<bool> runEditorStep(const Running& s) {
	if(!s.step.contains("edit")) return std::nullopt;
	edit(s);
	return true;
}

} // namespace

void registerEditorSteps(StepRegistry& registry) {
	registry.add({"editor",
	              {{"edit",
	                {"in", "line", "column", "toLine", "toColumn", "prefix", "text", "span", "into", "append", "keep"}}},
	              runEditorStep});
}

} // namespace affresco::format
