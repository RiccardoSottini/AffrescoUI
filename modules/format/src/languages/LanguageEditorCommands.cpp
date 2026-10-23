/// @file
/// @brief LanguageEditor's code commands (comment, move, delete, copy, indent and open lines), its one-off requests
/// (go to, insert) and the word before the caret.

#include "LanguageEditor.hpp"

#include "affresco/render/RichText.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

constexpr std::size_t maxWordCharacters = 256; ///< The longest word before the caret reported.

/// @brief Moves a selection end on a line by the columns the text after a change moved, never to before the change.
/// @param position The selection end.
/// @param line The line that changed.
/// @param at Where the change happened.
/// @param change How many columns the text after it moved (negative to the left).
void shiftColumn(TextPosition& position, std::size_t line, std::size_t at, long change) {
	if(position.line != line || position.column < at) return;
	position.column =
	    static_cast<std::size_t>(std::max(static_cast<long>(at), static_cast<long>(position.column) + change));
}

} // namespace

const std::vector<std::string_view>& LanguageEditor::codeCommandNames() {
	static const std::vector<std::string_view> names = {
	    "toggleComment", "moveLinesUp", "moveLinesDown", "deleteLines",     "copyLine",
	    "cutLine",       "indentLines", "outdentLines",  "insertLineAfter", "insertLineBefore"};
	return names;
}

bool LanguageEditor::runCodeCommand(std::string_view name) {
	if(name == "copyLine") copyLine(false);
	else if(std::ranges::find(codeCommandNames(), name) == codeCommandNames().end()) return false;
	else if(readOnly()) return true;
	else if(name == "toggleComment") toggleComment();
	else if(name == "moveLinesUp" || name == "moveLinesDown") moveLines(name == "moveLinesDown");
	else if(name == "deleteLines") deleteLines();
	else if(name == "cutLine") copyLine(true);
	else if(name == "indentLines" || name == "outdentLines") indentLines(name == "outdentLines");
	else openLine(name == "insertLineAfter");
	return true;
}

// Comment out unless every non-blank line already is; the markers line up at the smallest indentation
void LanguageEditor::toggleComment() {
	if(!language_ || language_->lineComment.empty()) return;
	const std::u32string bare = codepointsOf(language_->lineComment);
	const std::u32string marker = bare + U" ";
	const auto [first, last] = selectedLines();
	std::optional<std::size_t> column;
	bool commented = true;
	for(std::size_t line = first; line <= last; ++line) {
		const std::size_t indent = indentOf(line);
		if(indent == lines()[line].size()) continue; // blank
		column = std::min(column.value_or(indent), indent);
		commented = commented && lines()[line].compare(indent, bare.size(), bare) == 0;
	}
	if(!column) return;

	beginEdit(Edit::Other);
	Position caret = this->caret(), anchor = this->anchor();
	for(std::size_t line = first; line <= last; ++line) {
		const std::size_t indent = indentOf(line);
		if(indent == lines()[line].size()) continue;
		const std::size_t at = commented ? indent : *column;
		long change = static_cast<long>(marker.size());
		if(commented) {
			// The marker's space goes too, if it has one
			const std::size_t length =
			    lines()[line].compare(at, marker.size(), marker) == 0 ? marker.size() : bare.size();
			erase({line, at}, {line, at + length});
			change = -static_cast<long>(length);
		} else insert({line, at}, marker);
		shiftColumn(caret, line, at, change);
		shiftColumn(anchor, line, at, change);
	}
	setSelection(anchor, caret);
	endEdit();
}

void LanguageEditor::moveLines(bool down) {
	const auto [first, last] = selectedLines();
	if(down ? last + 1 >= lines().size() : first == 0) return;
	beginEdit(Edit::Other);
	if(down) {
		const std::u32string below = lines()[last + 1];
		erase({last, lines()[last].size()}, {last + 1, below.size()});
		insert({first, 0}, below + U"\n");
	} else {
		const std::u32string above = lines()[first - 1];
		erase({first - 1, 0}, {first, 0});
		insert({last - 1, lines()[last - 1].size()}, U"\n" + above);
	}
	Position caret = this->caret(), anchor = this->anchor();
	for(Position* p : {&caret, &anchor}) p->line = down ? p->line + 1 : p->line - 1;
	setSelection(anchor, caret);
	endEdit();
}

void LanguageEditor::deleteLines() {
	const auto [first, last] = selectedLines();
	const std::size_t column = caret().column;
	beginEdit(Edit::Other);
	if(last + 1 < lines().size()) erase({first, 0}, {last + 1, 0});
	else if(first > 0) erase({first - 1, lines()[first - 1].size()}, {last, lines()[last].size()});
	else erase({first, 0}, {last, lines()[last].size()});
	const std::size_t line = std::min(first, lines().size() - 1);
	const Position caret{line, std::min(column, lines()[line].size())};
	setSelection(caret, caret);
	endEdit();
}

void LanguageEditor::copyLine(bool cut) {
	if(hasSelection() || !window()) return;
	const std::size_t line = caret().line;
	window()->setClipboardText(model().textIn({line, 0}, {line, lines()[line].size()}) + "\n");
	if(cut) deleteLines();
}

void LanguageEditor::openLine(bool below) {
	const std::size_t line = caret().line;
	const std::size_t indent = indentOf(line);
	beginEdit(Edit::Other);
	const std::u32string blank(indent, U' ');
	Position at;
	if(below) at = insert({line, lines()[line].size()}, U"\n" + blank);
	else {
		insert({line, 0}, blank + U"\n");
		at = {line, indent};
	}
	setSelection(at, at);
	endEdit();
}

/* Requests */

void LanguageEditor::goTo(std::size_t line, std::size_t column, double request) {
	if(goToRequest_ == request) return;
	goToRequest_ = request;
	const std::size_t at = std::min(line, lines().size() - 1);
	moveCaret({at, std::min(column, lines()[at].size())}, false);
}

void LanguageEditor::insertText(std::string_view text, std::size_t before, double request) {
	if(insertRequest_ == request || readOnly()) return;
	insertRequest_ = request;
	beginEdit(Edit::Other);
	const auto [start, end] = selection();
	const Position from{start.line, start.column - std::min(before, start.column)};
	erase(from, end);
	const Position after = insert(from, codepointsOf(text));
	setSelection(after, after);
	endEdit();
}

std::string LanguageEditor::wordBeforeCaret() {
	const Position at = caret();
	const std::u32string& text = lines()[at.line];
	const std::size_t column = std::min(at.column, text.size());
	if(language_ && language_->wordPattern) {
		// The last match that ends at the caret, in the line's last bytes up to it (a word is shorter)
		const std::size_t first = column > maxWordCharacters ? column - maxWordCharacters : 0;
		const std::string head = model().textIn({at.line, first}, {at.line, column});
		for(std::size_t from = 0; from <= head.size();) {
			const std::optional<PatternMatch> match = language_->wordPattern->find(head, from, groups_).match;
			if(!match) break;
			if(match->offset + match->length == head.size() && match->length > 0) return head.substr(match->offset);
			from = match->offset + std::max<std::size_t>(match->length, 1);
		}
		return {};
	}
	const auto isWord = [](char32_t c) {
		return c == U'_' || (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
		       c > 0x7F;
	};
	std::size_t start = column;
	while(start > 0 && column - start < maxWordCharacters && isWord(text[start - 1])) --start;
	return model().textIn({at.line, start}, {at.line, column});
}

json LanguageEditor::placeEvent(Position at) const {
	json event{{"line", at.line}, {"column", at.column}};
	if(const std::optional<Rect> box = rectAt(at)) {
		event["x"] = box->left;
		event["y"] = box->bottom;
		event["top"] = box->top;
	}
	return event;
}

} // namespace affresco::format
