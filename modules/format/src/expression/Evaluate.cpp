/// @file
/// @brief format::evaluateExpression (Evaluate.hpp).

#include "affresco/format/Evaluate.hpp"

#include "Expression.hpp"

namespace affresco::format {

json evaluateExpression(const json& value, const json& variables, const std::string& where) {
	static const json empty = json::object();
	WorkBudget budget{stepsPerEvent, operationsPerEvent, bytesPerEvent};
	Scope locals;
	if(variables.is_object()) locals.vars = variables;
	const Context context{empty, empty, empty, empty, locals, empty, nullptr, nullptr, &budget};
	return evaluate(value, context, where);
}

} // namespace affresco::format
