#pragma once

/// @file
/// @brief format::evaluateExpression: a value of the screen format (`"{{value * 1000}}"`, a template, an operator)
/// evaluated with some variables and nothing else: no state, no window. Data migrations, imports and CSV
/// conversions use it (data::Evaluator).

#include "affresco/core/Json.hpp"

#include <string>

namespace affresco::format {

/// @brief Evaluates a value with only the given variables, within one event's work budget.
/// @param value The value: an expression in `{{ }}`, a template, an operator or plain JSON.
/// @param variables The variables, by name (`{"value": 3, "record": {...}}`).
/// @param where Its place, for errors.
/// @return The value.
/// @throws DocumentError If it is invalid, reads what isn't there, or runs out of budget.
[[nodiscard]] json evaluateExpression(const json& value, const json& variables,
                                      const std::string& where = "expression");

} // namespace affresco::format
