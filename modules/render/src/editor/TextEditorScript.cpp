/// @file
/// @brief TextEditor's edits as a script makes them: insert text, remove a range, find where a step lands.

#include "affresco/render/TextEditor.hpp"

#include "detail/Utf8.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace affresco {

bool TextEditor::insertText(std::string_view utf8) {
	const std::u32string text = detail::fromUtf8(utf8);
	if(!editable() || (text.empty() && !hasSelection())) return false;
	beginEdit(Edit::Other);
	replaceSelection(text);
	endEdit();
	return true;
}

std::string TextEditor::removeText(Position from, Position to) {
	from = clamped(from);
	to = clamped(to);
	if(to < from) std::swap(from, to);
	if(!editable() || from == to) return "";
	std::string removed = detail::toUtf8(textIn(from, to));
	beginEdit(Edit::Other);
	setSelection(from, to);
	replaceSelection({});
	endEdit();
	return removed;
}

std::string TextEditor::textBetween(Position from, Position to) const {
	from = clamped(from);
	to = clamped(to);
	if(to < from) std::swap(from, to);
	return detail::toUtf8(textIn(from, to));
}

TextEditor::Position TextEditor::nextPosition(Position from, bool forward, bool word) const {
	return step(clamped(from), forward, word);
}

} // namespace affresco
