/// @file
/// @brief Expression functions on forms of Lisp in a text (RFC 0005): what a code editor, a spreadsheet or anything
/// that holds code needs to evaluate what is at a position.

#include "Expression.hpp"
#include "ExpressionFunctions.hpp"

#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Notation.hpp>

#include <array>
#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format::detail {

namespace {

constexpr std::size_t maxKeywords = 64; ///< The keywords formKeyword() and setFormKeyword() follow, at most.

/// @brief A form as a value.
/// @param form The form.
/// @return `{text, line, column, endLine, endColumn, kind, name}`, or null.
Value formValue(const std::optional<lisp::FormAt>& form) {
	if(!form) return Value::ownJson(json());
	return Value::ownJson(json{{"text", form->text},
	                           {"line", form->line},
	                           {"column", form->column},
	                           {"endLine", form->endLine},
	                           {"endColumn", form->endColumn},
	                           {"kind", form->kind},
	                           {"name", form->name}});
}

/// @brief Reads the arguments of a function on a position.
/// @param call The call: a text, a line and a column.
/// @param find What finds the form.
/// @return The form.
Value atPosition(const Call& call, std::optional<lisp::FormAt> (*find)(std::string_view, std::size_t, std::size_t)) {
	if(!call[0].isText()) call.fail("expected a text, a line and a column");
	const double line = call.number(1);
	const double column = call.number(2);
	if(line < 1 || column < 1 || line > 1e9 || column > 1e9) call.fail("a line and a column count from 1");
	call.spend(call[0].text().size() / 8); // the text is read, however much of it the position asks for
	return formValue(find(call[0].text(), static_cast<std::size_t>(line), static_cast<std::size_t>(column)));
}

/// @brief `formBefore(text, line, column)`. @param call The call. @return The form.
Value formBeforeOf(const Call& call) { return atPosition(call, lisp::formBefore); }

/// @brief `formAround(text, line, column)`. @param call The call. @return The form.
Value formAroundOf(const Call& call) { return atPosition(call, lisp::formAround); }

/// @brief `printForm(value)`: a value as Lisp. @param call The call. @return The text.
Value printFormOf(const Call& call) {
	thread_local lisp::Interpreter interpreter; // making one costs far more than printing with it
	const lisp::Interpreter::Transient region(interpreter);
	interpreter.setBudget({operationsPerEvent, bytesPerEvent});
	const auto value = std::make_shared<const json>(call[0].toJson());
	return Value::ownJson(json(interpreter.print(interpreter.viewJson(value))));
}

/// @brief The keywords a function on a keyword's value is given: a text, or a list of texts.
/// @param call The call: the text, then the keywords.
/// @return The keywords, without their ':'.
std::vector<std::string> keywordsOf(const Call& call) {
	if(!call[0].isText()) call.fail("expected a text of Lisp, then its keywords");
	call.spend(call[0].text().size() / 8); // the text is read up to the keyword, as far as its end
	const Value& given = call[1];
	std::vector<std::string> keywords;
	const auto add = [&](const Value& keyword) {
		if(!keyword.isText() || keyword.text().empty()) call.fail("a keyword is a text, such as 'window'");
		const std::string_view text = keyword.text();
		keywords.emplace_back(text.starts_with(':') ? text.substr(1) : text);
	};
	if(given.isList()) {
		if(given.size() > maxKeywords) call.fail("expected at most 64 keywords");
		for(std::size_t i = 0; i < given.size(); ++i) add(given.item(i));
	} else {
		add(given);
	}
	if(keywords.empty()) call.fail("expected a keyword");
	return keywords;
}

/// @brief `formKeyword(text, keywords)`: a keyword's value in a text of Lisp, read as data (lisp::findKeyword()).
/// @param call The call.
/// @return The value, or null if no form has it, or it or the text isn't plain data.
Value formKeywordOf(const Call& call) {
	const std::vector<std::string> keywords = keywordsOf(call);
	try {
		const std::string_view text = call[0].text();
		const auto span = lisp::findKeyword(text, keywords);
		if(!span) return Value::ownJson(json());
		return Value::ownJson(
		    notation::read(text.substr(span->first, span->second - span->first), notation::Notation::Lisp));
	} catch(const std::exception&) {
		return Value::ownJson(json()); // not forms, or not data
	}
}

/// @brief `setFormKeyword(text, keywords, value)`: a text of Lisp with a keyword's value replaced, every other byte
/// kept (lisp::withKeyword()).
/// @param call The call.
/// @return The new text, or null if no form has the keyword, or the text doesn't read as forms.
Value setFormKeywordOf(const Call& call) {
	const std::vector<std::string> keywords = keywordsOf(call);
	try {
		std::optional<std::string> text =
		    lisp::withKeyword(call[0].text(), keywords, lisp::keywordValueText(call[2].toJson()));
		return text ? Value::fromText(std::move(*text)) : Value::ownJson(json());
	} catch(const std::exception&) {
		return Value::ownJson(json()); // not forms, or a value Lisp can't write
	}
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "formBefore",
	 .arguments = "text, line, column",
	 .about =
	     "The form of Lisp that ends at or before a position, however deep: {text, line, column, endLine, endColumn, "
	     "kind, name}, or null",
	 .fewest = 3,
	 .most = 3,
	 .apply = formBeforeOf},
    {.name = "formAround",
	 .arguments = "text, line, column",
	 .about =
	     "The top-level form of Lisp around a position, or else the last before it, or else the next: as formBefore",
	 .fewest = 3,
	 .most = 3,
	 .apply = formAroundOf},
    {.name = "printForm",
	 .arguments = "value",
	 .about = "A value as Lisp text",
	 .fewest = 1,
	 .most = 1,
	 .apply = printFormOf},
    {.name = "formKeyword",
	 .arguments = "text, keywords",
	 .about = "A keyword's value in a text of Lisp, read as data and never run: in the first top-level form that has "
	          "it, a map or the options of defcommand, defscreen or defcustom; a list of keywords looks inside maps "
	          "(['window', 'title']). Null if no form has it, or it isn't data",
	 .fewest = 2,
	 .most = 2,
	 .apply = formKeywordOf},
    {.name = "setFormKeyword",
	 .arguments = "text, keywords, value",
	 .about = "The text of Lisp with that keyword's value (as formKeyword finds it) replaced by the value, written "
	          "as Lisp, and every other character as it was; null if no form has it",
	 .fewest = 3,
	 .most = 3,
	 .apply = setFormKeywordOf},
});

} // namespace

std::span<const Function> formsFunctions() { return table; }

} // namespace affresco::format::detail
