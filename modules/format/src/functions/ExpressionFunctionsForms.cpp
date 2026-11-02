/// @file
/// @brief Expression functions on forms of Lisp in a text (RFC 0005): what a code editor, a spreadsheet or anything
/// that holds code needs to evaluate what is at a position.

#include "ExpressionFunctions.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <array>
#include <optional>
#include <string>

namespace affresco::format::detail {

namespace {

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
	return formValue(find(call[0].text(), static_cast<std::size_t>(line), static_cast<std::size_t>(column)));
}

/// @brief `formBefore(text, line, column)`. @param call The call. @return The form.
Value formBeforeOf(const Call& call) { return atPosition(call, lisp::formBefore); }

/// @brief `formAround(text, line, column)`. @param call The call. @return The form.
Value formAroundOf(const Call& call) { return atPosition(call, lisp::formAround); }

/// @brief `printForm(value)`: a value as Lisp. @param call The call. @return The text.
Value printFormOf(const Call& call) {
	lisp::Interpreter interpreter;
	const auto value = std::make_shared<const json>(call[0].toJson());
	return Value::ownJson(json(interpreter.print(interpreter.viewJson(value))));
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
});

} // namespace

std::span<const Function> formsFunctions() { return table; }

} // namespace affresco::format::detail
