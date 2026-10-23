/// @file
/// @brief The step that runs a code editor's command: edit.

#include "Engine.hpp"

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <string>

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

/// @brief "edit": a command of a code editor by id ("in"), once the event is handled, so the editor has its text.
/// @param s The step.
void edit(const Running& s) {
	const std::string command = text(s.value("edit"));
	const auto& names = TextEditor::commandNames();
	if(command != "goTo" && command != "select" && std::ranges::find(names, command) == names.end())
		fail(s.where + ".edit", "unknown command '" + command + "'");
	if(!s.step.contains("in")) fail(s.where, "an edit step needs \"in\": the editor's id");
	std::string id = text(s.value("in"));
	if(id.starts_with('#')) id.erase(0, 1);
	const TextPosition at = placeOf(s, "line", "column", {});
	const TextPosition to = placeOf(s, "toLine", "toColumn", at);
	const std::string prefix = s.step.contains("prefix") ? text(s.value("prefix")) : "//";
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	engine.scheduler.defer([&engine, id, command, at, to, prefix] {
		auto* editor = dynamic_cast<TextEditor*>(engine.window->root().findById(id));
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
	registry.add({"editor", {{"edit", {"in", "line", "column", "toLine", "toColumn", "prefix"}}}, runEditorStep});
}

} // namespace affresco::format
