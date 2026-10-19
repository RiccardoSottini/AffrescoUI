/// @file
/// @brief Fuzzes the expression parser and evaluator (format/src/Expression.hpp): a revision can put any template,
/// expression or operator into a screen, and the screen evaluates it on every update.
///
/// The input is a JSON value to evaluate (a `{{...}}` template, a `$` operator, or anything holding them) or, when it
/// isn't JSON, the inside of a `{{ }}`. Evaluating either gives a value or throws DocumentError, as the runtime expects.

#include "Expression.hpp"
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using affresco::json;

namespace {

/// @brief Evaluates a value as a screen would, with the Kanban template's state.
/// @param value The value.
/// @throws DocumentError If the value is invalid.
void evaluate(const json& value) {
	static const json state = affresco::fuzz::kanbanTemplate().at("state");
	static const json window = {{"width", 1280}, {"height", 800}};
	static const json revision = 3, event = {{"x", 10}, {"y", 20}, {"key", "Enter"}, {"text", "a"}};
	static const json globals = {{"mode", "default"}, {"proposals", json::array()}, {"history", json::array()},
	                             {"theme", {{"name", "light"}}}, {"screen", "main"}, {"preview", nullptr}};
	static const affresco::format::Scope locals{nullptr, nullptr, {{"card", {{"title", "x"}, {"points", 3}}}}};
	static const affresco::format::NodeLookup nodes = [](std::string_view id) {
		return id == "board" ? json{{"x", 0}, {"y", 0}, {"width", 800}, {"height", 600}, {"visible", true}} : json();
	};
	const affresco::format::Context context{state, window, revision, event, locals, globals, &nodes};
	static_cast<void>(affresco::format::evaluate(value, context, "fuzz"));
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	json value;
	try {
		value = affresco::parseUntrusted(text);
	} catch(const json::parse_error&) {
		value = "{{" + std::string(text) + "}}";
		if(!affresco::fuzz::isUtf8(value)) return 0; // documents are parsed JSON: their strings are always UTF-8
	} catch(const affresco::DocumentError&) {
		return 0; // too deep: never reaches the evaluator
	}
	try {
		evaluate(value);
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
