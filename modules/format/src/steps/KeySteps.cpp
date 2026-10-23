/// @file
/// @brief The steps that type for the user: press (keys, as a keyboard sends them), insert (text at the caret) and
/// cut (a span of an editor, to the clipboard or a state path).

#include "Engine.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/Controls.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxTimes = 1000;    ///< The most times a press repeats.
constexpr std::size_t maxPresses = 10000; ///< The most presses one step sends, its chord times "times".

/// @brief The panel a key step acts on: "in" ("#id") or the focused one.
/// @param s The step.
/// @return It, or nullptr without a window or when there is none.
Panel* targetOf(const Running& s) {
	Engine& engine = s.steps.engine();
	if(!engine.window) return nullptr;
	if(!s.step.contains("in")) return engine.window->focusedPanel();
	std::string id = text(s.value("in"));
	if(id.starts_with('#')) id.erase(0, 1);
	return engine.window->root().findById(id);
}

/// @brief "press": a chord ("ctrl+x ctrl+s"), each press sent to the panel as the keyboard would, "times" over.
/// Bindings don't hear these presses, so a binding can press the keys it stands for without running itself.
/// @param s The step.
void press(const Running& s) {
	const std::string& where = s.where;
	const std::string chord = normalizeChord(string(s.value("press"), where + ".press"));
	std::vector<KeyEvent> presses;
	for(std::size_t at = 0; at < chord.size();) {
		const std::size_t end = std::min(chord.find(' ', at), chord.size());
		const std::optional<KeyEvent> event = keyPress(std::string_view(chord).substr(at, end - at));
		if(!event) fail(where + ".press", "no key '" + chord.substr(at, end - at) + "'");
		presses.push_back(*event);
		at = end + 1;
	}
	std::size_t times = 1;
	if(s.step.contains("times")) {
		const json count = s.value("times");
		if(!count.is_number_integer() || count.get<long long>() < 1 ||
		   count.get<long long>() > static_cast<long long>(maxTimes))
			fail(where + ".times", "must be a whole number from 1 to " + std::to_string(maxTimes));
		times = count.get<std::size_t>();
	}
	if(presses.size() * times > maxPresses)
		fail(where + ".times", "sends more than " + std::to_string(maxPresses) + " presses");
	Panel* panel = targetOf(s);
	if(!panel) return;
	Engine& engine = s.steps.engine();
	for(std::size_t n = 0; n < times; ++n)
		for(const KeyEvent& event : presses) engine.events.press(*panel, event, typedBy(event));
}

/// @brief "insert": text at the caret of an editor or a text field, over its selection.
/// @param s The step.
void insert(const Running& s) {
	const std::string utf8 = string(s.value("insert"), s.where + ".insert");
	Panel* panel = targetOf(s);
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
	fail(where + ".cut", "must be \"lineEnd\", \"word\", \"selection\" or \"line\"");
}

/// @brief "cut": a span of an editor (or the whole of a text field, for "line"), removed unless "keep", to "into"
/// (added to its text with "append") or else the clipboard.
/// @param s The step.
void cut(const Running& s) {
	const std::string& where = s.where;
	const std::string kind = string(s.value("cut"), where + ".cut");
	const bool keep = s.step.contains("keep") && boolean(s.value("keep"), where + ".keep");
	const bool append = s.step.contains("append") && boolean(s.value("append"), where + ".append");
	Panel* panel = targetOf(s);
	if(!panel) return;
	std::string taken;
	if(auto* editor = dynamic_cast<TextEditor*>(panel)) {
		const auto [from, to] = cutSpan(*editor, kind, where);
		taken = keep || editor->readOnly() ? editor->textBetween(from, to) : editor->removeText(from, to);
	} else if(auto* field = dynamic_cast<TextArea*>(panel)) {
		if(kind != "line" && kind != "lineEnd" && kind != "word" && kind != "selection")
			fail(where + ".cut", "must be \"lineEnd\", \"word\", \"selection\" or \"line\"");
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

/// @brief Runs a key step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runKeyStep(const Running& s) {
	if(s.step.contains("press")) press(s);
	else if(s.step.contains("insert")) insert(s);
	else if(s.step.contains("cut")) cut(s);
	else return std::nullopt;
	return true;
}

} // namespace

void registerKeySteps(StepRegistry& registry) {
	registry.add({"keys",
	              {{"press", {"in", "times"}}, {"insert", {"in"}}, {"cut", {"in", "into", "append", "keep"}}},
	              runKeyStep});
}

} // namespace affresco::format
