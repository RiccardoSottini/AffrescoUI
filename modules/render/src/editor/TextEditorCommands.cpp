/// @file
/// @brief TextEditor's named commands, for key bindings and screens: selections, line operations, case conversion,
/// transpose and comment toggling.

#include "affresco/render/TextEditor.hpp"

#include "detail/Utf8.hpp"
#include "editor/TextEditorShared.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace affresco {

using editor::isBlank;
using editor::wordAround;

namespace {

/// @brief A letter's capital, for ASCII and Latin-1 (others are kept).
/// @param c The code point.
/// @return Its capital, or @p c.
char32_t upper(char32_t c) noexcept {
	if((c >= U'a' && c <= U'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 0x20;
	return c;
}

/// @brief A letter's small form, for ASCII and Latin-1 (others are kept).
/// @param c The code point.
/// @return Its small form, or @p c.
char32_t lower(char32_t c) noexcept {
	if((c >= U'A' && c <= U'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 0x20;
	return c;
}

/// @brief Converts the case of a text.
/// @param text The text.
/// @param how "upperCase", "lowerCase", "titleCase" (each word's first letter capital) or "swapCase".
/// @return The text converted.
std::u32string convertCase(std::u32string text, std::string_view how) {
	bool wordStart = true;
	for(char32_t& c : text) {
		const bool letter = upper(c) != lower(c);
		if(how == "upperCase") c = upper(c);
		else if(how == "lowerCase") c = lower(c);
		else if(how == "swapCase") c = c == upper(c) ? lower(c) : upper(c);
		else c = wordStart ? upper(c) : lower(c);
		wordStart = !letter && !editor::isWord(c);
	}
	return text;
}

/// @brief Moves a position on a line by an insertion (or a removal) at a column.
/// @param at The position.
/// @param line The line edited.
/// @param column Where.
/// @param change Code points inserted (negative: removed).
void shiftColumn(TextPosition& at, std::size_t line, std::size_t column, long change) noexcept {
	if(at.line != line || at.column < column) return;
	at.column = static_cast<std::size_t>(std::max(static_cast<long>(column), static_cast<long>(at.column) + change));
}

} // namespace

const std::vector<std::string_view>& TextEditor::commandNames() {
	static const std::vector<std::string_view> names = {"expandToLine",   "expandToWord",     "selectAllOccurrences",
	                                                    "splitIntoLines", "insertLineBefore", "insertLineAfter",
	                                                    "duplicateLines", "deleteLines",      "moveLinesUp",
	                                                    "moveLinesDown",  "joinLines",        "sortLines",
	                                                    "reverseLines",   "uniqueLines",      "upperCase",
	                                                    "lowerCase",      "titleCase",        "swapCase",
	                                                    "transpose",      "toggleComment"};
	return names;
}

bool TextEditor::runCommand(std::string_view name) {
	if(name == "expandToLine") {
		const auto [from, to] = selection();
		std::size_t end = selectedLines().second + 1;
		if(hasSelection() && from.column == 0 && to.column == 0) end = to.line + 1; // whole lines: one more
		clearCursors();
		anchor_ = {from.line, 0};
		moveCaret(end < lines_.size() ? Position{end, 0} : Position{lines_.size() - 1, lines_.back().size()}, true);
	} else if(name == "expandToWord") {
		if(!hasSelection()) selectWord(caret_);
	} else if(name == "selectAllOccurrences") {
		if(!hasSelection()) selectWord(caret_);
		for(std::size_t before = SIZE_MAX; before != cursors_.size();) {
			before = cursors_.size();
			selectNextOccurrence();
		}
		onCaretMoved(caret_);
	} else if(name == "splitIntoLines") {
		const auto [from, to] = selection();
		if(from.line == to.line) return true;
		clearCursors();
		setSelection(from, {from.line, lines_[from.line].size()});
		for(std::size_t line = from.line + 1; line <= to.line; ++line)
			if(line < to.line || to.column > 0)
				addCursor({{line, 0}, {line, line == to.line ? to.column : lines_[line].size()}});
		revealCaret();
		onCaretMoved(caret_);
	} else if(name == "insertLineBefore" || name == "insertLineAfter") {
		if(!editable()) return true;
		clearCursors();
		const std::size_t line = caret_.line, indent = indentOf(line);
		beginEdit(Edit::Other);
		if(name == "insertLineAfter") {
			insert({line, lines_[line].size()}, U"\n" + std::u32string(indent, U' '));
			caret_ = anchor_ = {line + 1, indent};
		} else {
			insert({line, 0}, std::u32string(indent, U' ') + U"\n");
			caret_ = anchor_ = {line, indent};
		}
		endEdit();
	} else if(name == "toggleComment") toggleComment("//");
	else return lineCommand(name) || textCommand(name);
	return true;
}

void TextEditor::selectRange(Position anchor, Position caret) {
	clearCursors();
	anchor_ = clamped(anchor);
	moveCaret(clamped(caret), true);
	onCaretMoved(caret_); // even if it was there: a screen asking for it may need the answer
}

void TextEditor::toggleComment(std::string_view prefix) {
	if(!editable() || prefix.empty()) return;
	clearCursors();
	const std::u32string mark = detail::fromUtf8(prefix);
	const auto [first, last] = selectedLines();
	// Comment out unless every non-blank line already starts with the prefix, at the smallest indentation
	bool commented = true;
	std::size_t indent = SIZE_MAX;
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines_[line];
		if(std::ranges::all_of(text, isBlank)) continue;
		const std::size_t at = indentOf(line);
		indent = std::min(indent, at);
		commented = commented && text.compare(at, mark.size(), mark) == 0;
	}
	if(indent == SIZE_MAX) return; // only blank lines
	beginEdit(Edit::Other);
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines_[line];
		if(std::ranges::all_of(text, isBlank)) continue;
		if(commented) {
			const std::size_t at = indentOf(line);
			const std::size_t removed =
			    mark.size() + (text.size() > at + mark.size() && text[at + mark.size()] == U' ');
			erase({line, at}, {line, at + removed});
			for(Position* p : {&caret_, &anchor_}) shiftColumn(*p, line, at, -static_cast<long>(removed));
		} else {
			insert({line, indent}, mark + U" ");
			for(Position* p : {&caret_, &anchor_}) shiftColumn(*p, line, indent, static_cast<long>(mark.size() + 1));
		}
	}
	endEdit();
}

void TextEditor::replaceLines(std::size_t first, std::size_t last, const std::vector<std::u32string>& with,
                              Position anchor, Position caret) {
	std::u32string text;
	for(const std::u32string& line : with) text += (text.empty() && &line == &with.front() ? U"" : U"\n") + line;
	beginEdit(Edit::Other);
	erase({first, 0}, {last, lines_[last].size()});
	insert({first, 0}, text);
	setSelection(clamped(anchor), clamped(caret));
	endEdit();
}

bool TextEditor::lineCommand(std::string_view name) {
	static const std::set<std::string_view> names = {"duplicateLines", "deleteLines", "moveLinesUp",  "moveLinesDown",
	                                                 "joinLines",      "sortLines",   "reverseLines", "uniqueLines"};
	if(!names.contains(name)) return false;
	if(!editable()) return true;
	clearCursors();
	auto [first, last] = selectedLines();
	const auto range = [this](std::size_t from, std::size_t to) {
		std::vector<std::u32string> block;
		for(std::size_t line = from; line <= to; ++line) block.push_back(lines_[line]);
		return block;
	};
	const auto moved = [](Position p, long by) {
		return Position{static_cast<std::size_t>(static_cast<long>(p.line) + by), p.column};
	};
	if(name == "duplicateLines") {
		std::vector<std::u32string> block = range(first, last);
		const long size = static_cast<long>(block.size());
		block.insert(block.end(), block.begin(), block.end());
		replaceLines(first, last, block, moved(anchor_, size), moved(caret_, size));
	} else if(name == "deleteLines") {
		beginEdit(Edit::Other);
		const std::size_t column = caret_.column;
		if(last + 1 < lines_.size()) erase({first, 0}, {last + 1, 0});
		else if(first > 0) erase({first - 1, lines_[first - 1].size()}, {last, lines_[last].size()});
		else erase({0, 0}, {last, lines_[last].size()});
		const std::size_t line = std::min(first, lines_.size() - 1);
		caret_ = anchor_ = {line, std::min(column, lines_[line].size())};
		endEdit();
	} else if(name == "moveLinesUp" || name == "moveLinesDown") {
		const bool down = name == "moveLinesDown";
		if(down ? last + 1 >= lines_.size() : first == 0) return true;
		std::vector<std::u32string> block = range(first, last);
		if(down) block.insert(block.begin(), lines_[last + 1]);
		else block.push_back(lines_[first - 1]);
		replaceLines(down ? first : first - 1, down ? last + 1 : last, block, moved(anchor_, down ? 1 : -1),
		             moved(caret_, down ? 1 : -1));
	} else if(name == "joinLines") {
		if(first == last) ++last;
		if(last >= lines_.size()) return true;
		std::u32string joined = lines_[first];
		std::size_t at = joined.size();
		for(std::size_t line = first + 1; line <= last; ++line) {
			const std::u32string& next = lines_[line];
			const std::size_t start = std::min(indentOf(line), next.size());
			while(!joined.empty() && isBlank(joined.back())) joined.pop_back();
			if(!joined.empty() && start < next.size()) joined += U' ';
			at = joined.size();
			joined += next.substr(start);
		}
		replaceLines(first, last, {joined}, {first, at}, {first, at});
	} else {
		if(first == last) first = 0, last = lines_.size() - 1; // no lines selected: the whole text
		std::vector<std::u32string> block = range(first, last);
		if(name == "sortLines") std::ranges::sort(block);
		else if(name == "reverseLines") std::ranges::reverse(block);
		else {
			std::set<std::u32string> seen;
			std::erase_if(block, [&seen](const std::u32string& line) { return !seen.insert(line).second; });
		}
		replaceLines(first, last, block, {first, 0}, {first + block.size() - 1, block.back().size()});
	}
	return true;
}

bool TextEditor::textCommand(std::string_view name) {
	static const std::set<std::string_view> names = {"upperCase", "lowerCase", "titleCase", "swapCase", "transpose"};
	if(!names.contains(name)) return false;
	if(!editable()) return true;
	forEachCursor(Edit::Other, [this, name] {
		if(name == "transpose") {
			// The characters around the caret (the two before it at the line's end); the caret moves past them
			const std::u32string& text = lines_[caret_.line];
			if(hasSelection() || text.size() < 2 || caret_.column == 0) return;
			const std::size_t at = std::min(caret_.column, text.size() - 1) - 1;
			const std::u32string swapped{text[at + 1], text[at]};
			erase({caret_.line, at}, {caret_.line, at + 2});
			caret_ = anchor_ = insert({caret_.line, at}, swapped);
			return;
		}
		if(!hasSelection()) { // the word at the caret
			const auto [start, end] = wordAround(lines_[caret_.line], caret_.column);
			anchor_ = {caret_.line, start};
			caret_ = {caret_.line, end};
		}
		const bool forward = anchor_ < caret_;
		const auto [from, to] = selection();
		const std::u32string text = convertCase(textIn(from, to), name);
		erase(from, to);
		const Position end = insert(from, text);
		anchor_ = forward ? from : end;
		caret_ = forward ? end : from;
	});
	return true;
}

} // namespace affresco
