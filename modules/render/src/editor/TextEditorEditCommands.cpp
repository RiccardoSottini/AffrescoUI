/// @file
/// @brief TextEditor's edit commands by name: undo and redo, the clipboard, select all, indent and outdent, unfold all,
/// so that menus, toolbars and context menus reach what the keys do.

#include "affresco/render/TextEditor.hpp"

#include <set>

namespace affresco {

bool TextEditor::editCommand(std::string_view name) {
	static const std::set<std::string_view> names = {"undo",      "redo",        "cut",          "copy",     "paste",
	                                                 "selectAll", "indentLines", "outdentLines", "unfoldAll"};
	if(!names.contains(name)) return false;
	if(name == "copy") copy(false);
	else if(name == "selectAll") {
		clearCursors();
		anchor_ = {0, 0};
		caret_ = {lines_.size() - 1, lines_.back().size()};
		invalidatePaint();
		onCaretMoved(caret_);
	} else if(name == "unfoldAll") unfoldAll();
	else if(!editable()) return true; // the rest change the text
	else if(name == "undo" || name == "redo") undo(name == "redo");
	else if(name == "cut") copy(true);
	else if(name == "paste") paste();
	else indentLines(name == "outdentLines");
	return true;
}

} // namespace affresco
