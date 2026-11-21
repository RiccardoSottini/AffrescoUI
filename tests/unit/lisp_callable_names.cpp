/// @file
/// @brief Unit tests for Interpreter::callableNames(): every name code can call or read now, which an editor's
/// completion offers.

#include <affresco/lisp/Interpreter.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace lisp = affresco::lisp;

namespace {

/// @brief Tells whether a sorted list of names holds one.
/// @param names The names, sorted.
/// @param name The name.
/// @return true if it is there.
bool has(const std::vector<std::string>& names, const std::string& name) {
	return std::ranges::binary_search(names, name);
}

} // namespace

TEST_CASE("lisp: callableNames has the language's own functions, its special forms and macros") {
	const lisp::Interpreter interpreter;
	const std::vector<std::string> names = interpreter.callableNames();
	CHECK(has(names, "map"));
	CHECK(has(names, "defn"));
	CHECK(has(names, "if"));
	CHECK(has(names, "let"));
}

TEST_CASE("lisp: callableNames has a host's primitive and a program's definition") {
	lisp::Interpreter interpreter;
	interpreter.definePrimitive("host-greeting",
	                            {.pure = true, .call = [](lisp::Interpreter&, std::span<const lisp::Value>) {
		                             return lisp::Value::integer(1);
	                             }});
	static_cast<void>(interpreter.eval("(defn shout \"Says it louder.\" [text] text)", "test", "tester"));
	const std::vector<std::string> names = interpreter.callableNames();
	CHECK(has(names, "host-greeting"));
	CHECK(has(names, "shout"));
	CHECK_FALSE(has(names, "never-defined-anywhere"));
}

TEST_CASE("lisp: callableNames is sorted, each name once") {
	lisp::Interpreter interpreter;
	static_cast<void>(interpreter.eval("(defn map-twice [f xs] (map f (map f xs)))", "test", "tester"));
	const std::vector<std::string> names = interpreter.callableNames();
	REQUIRE_FALSE(names.empty());
	CHECK(std::ranges::is_sorted(names));
	CHECK(std::ranges::adjacent_find(names) == names.end());
	CHECK(has(names, "map-twice"));
}
