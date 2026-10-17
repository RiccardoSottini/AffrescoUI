#pragma once

/// @file
/// @brief Values in screen documents: data lookups, templates and operators.
///
/// - `"{{state.threads[state.current].title}}"`: the value at a path (any JSON type)
/// - `"Clicked {{state.clicks}} times"`: a string with values interpolated
/// - `{ "$clamp": ["{{event.x}}", 180, 520] }`: an operator (one key starting with '$')
///
/// Paths start with a variable: state, event, window, revision, proposals, history, preview, or a name given by
/// "repeat" / "forEach".

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>

namespace affresco::format {

/// @brief What a path can see while a value is evaluated.
struct Context {
	const json& state;    ///< The live state.
	const json& window;   ///< Window size.
	const json& revision; ///< The current revision.
	const json& event;    ///< The event being handled.
	const json& locals;   ///< "repeat" / "forEach" variables.
	const json& globals;  ///< Other screen-wide variables (proposals, history, preview).
};

/// @brief Evaluates templates and operators, recursing into objects and arrays.
/// @param value The value.
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The evaluated value.
/// @throws DocumentError If a path or operator is invalid.
[[nodiscard]] json evaluate(const json& value, const Context& context, const std::string& where);

/// @brief Reads the value at a path.
/// @param path E.g. "state.threads[0].title".
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The value, or null if it doesn't exist.
/// @throws DocumentError If the path is malformed.
[[nodiscard]] json lookup(std::string_view path, const Context& context, const std::string& where);

/// @brief Resolves a writable location.
/// @param path A path starting with "state".
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The JSON Pointer inside the state.
/// @throws DocumentError If the path doesn't start with "state" or is malformed.
[[nodiscard]] json::json_pointer statePointer(std::string_view path, const Context& context, const std::string& where);

/// @brief Tells whether a value counts as true.
/// @param value The value.
/// @return false for false, null, 0, "", [] and {}.
[[nodiscard]] bool truthy(const json& value) noexcept;

} // namespace affresco::format
