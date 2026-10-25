/// @file
/// @brief Values an expression builds nest no deeper than maxNesting (Value.hpp): a `reduce` that wraps its
/// accumulator at every step is refused, not left to overflow the stack when it is converted or destroyed.

#include "Expression.hpp"
#include "Value.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::json;

namespace {

/// @brief Evaluates a template with no state.
/// @param text The template.
/// @return The result.
/// @throws DocumentError If it is invalid.
json evaluate(const std::string& text) {
	static const json state = json::object(), window = json::object(), revision = 1, event = json::object();
	static const json globals = json::object();
	static const affresco::format::Scope locals{nullptr, nullptr, json::object(), {}, {}};
	const affresco::format::Context context{state, window, revision, event, locals, globals, nullptr};
	return affresco::format::evaluate(json(text), context, "test");
}

} // namespace

TEST_CASE("value nesting: a value an expression builds nests at most maxNesting levels") {
	CHECK(evaluate("{{reduce(range(200), [acc], [])}}").dump().size() == 402);
	CHECK_THROWS_WITH_AS((void)evaluate("{{reduce(range(10000), [acc], [])}}"),
	                     doctest::Contains("nested more than 256 levels"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate("{{reduce(range(10000), { a: acc }, {})}}"),
	                     doctest::Contains("nested more than 256 levels"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate("{{reduce(range(300), reduce(range(400), [acc], acc), [])}}"),
	                     doctest::Contains("nested more than 256 levels"), affresco::DocumentError);
}

TEST_CASE("value nesting: a list counts one level more than its deepest item") {
	using affresco::format::Value;
	Value deep = Value::fromList({});
	for(int i = 1; i < affresco::maxNesting; ++i) deep = Value::fromList({Value::fromInteger(1), deep});
	CHECK_NOTHROW((void)Value::fromObject({{"wide", Value::fromList({Value::fromInteger(2)})}}));
	CHECK_THROWS_AS((void)Value::fromList({deep}), affresco::DocumentError);
	CHECK_THROWS_AS((void)Value::fromObject({{"deep", deep}}), affresco::DocumentError);
}
