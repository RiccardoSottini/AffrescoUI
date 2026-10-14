#pragma once

// Values in screen documents: data lookups, templates and operators.
//
//   "{{state.threads[state.current].title}}"   the value at a path (any JSON type)
//   "Clicked {{state.clicks}} times"          a string with values interpolated
//   { "$clamp": ["{{event.x}}", 180, 520] }    an operator: one key starting with '$'
//
// Paths start with a variable: state, event, window, revision, proposals, history, preview, or a name given by
// "repeat" / "forEach".

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace sgl::screen {

using json = nlohmann::json;

// What a path can see while a value is evaluated
struct Context {
	const json& state;
	const json& window;
	const json& revision;
	const json& event;
	const json& locals;  // object of repeat/forEach variables
	const json& globals; // other screen-wide variables (proposals, history, preview)
};

// Evaluates templates and operators, recursing into objects and arrays. `where` names the place in the document
// for error messages. Throws ScreenError.
[[nodiscard]] json evaluate(const json& value, const Context& context, const std::string& where);

// The value at `path` ("state.threads[0].title"), or null if it doesn't exist
[[nodiscard]] json lookup(std::string_view path, const Context& context, const std::string& where);

// A writable location: `path` must start with "state". Returns the JSON pointer inside the state.
[[nodiscard]] json::json_pointer statePointer(std::string_view path, const Context& context, const std::string& where);

[[nodiscard]] bool truthy(const json& value) noexcept; // false for false, null, 0, "", [], {}
[[nodiscard]] std::string toText(const json& value);   // how a value appears inside a string

} // namespace sgl::screen
