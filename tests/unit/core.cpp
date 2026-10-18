/// @file
/// @brief Unit tests for the core module: parsing JSON from outside the app.

#include <affresco/core/Json.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace {

/// @brief JSON values nested inside each other.
/// @param depth How many levels.
/// @param open What opens a level, e.g. "[" or "{\"a\":".
/// @param close What closes a level, e.g. ']'.
/// @param inside What the innermost level holds.
/// @return E.g. "[[[0]]]" for 3, "[", ']', "0".
std::string nested(int depth, std::string_view open, char close, std::string_view inside = "") {
	std::string text;
	for(int i = 0; i < depth; ++i) text += open;
	return text + std::string(inside) + std::string(static_cast<std::size_t>(depth), close);
}

} // namespace

TEST_CASE("core: parseUntrusted reads JSON nested up to maxNesting levels") {
	CHECK(affresco::parseUntrusted(R"({"a": [1, {"b": null}]})") ==
	      affresco::json::parse(R"({"a": [1, {"b": null}]})"));
	CHECK_NOTHROW((void)affresco::parseUntrusted(nested(affresco::maxNesting, "[", ']')));

	const affresco::json value = affresco::parseUntrusted(nested(affresco::maxNesting, "[", ']', "0"));
	CHECK(value.dump().size() == 2 * static_cast<std::size_t>(affresco::maxNesting) + 1);
}

TEST_CASE("core: parseUntrusted refuses JSON nested deeper than maxNesting") {
	const int deeper = affresco::maxNesting + 1;
	CHECK_THROWS_WITH_AS((void)affresco::parseUntrusted(nested(deeper, "[", ']', "0")), // [[...[0]...]]
	                     doctest::Contains("nested more than 256 levels deep"), affresco::DocumentError);
	CHECK_THROWS_AS((void)affresco::parseUntrusted(nested(deeper, R"({"a":)", '}', "0")), // {"a":{"a":...0...}}
	                affresco::DocumentError);

	CHECK_THROWS_AS((void)affresco::parseUntrusted(nested(affresco::maxNesting + 2, "[", ']')),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)affresco::parseUntrusted(nested(100000, "[", ']')), affresco::DocumentError);
}

// nlohmann::json reports array_start and object_start at the depth outside the new level: an empty innermost level
// is still counted
TEST_CASE("core: parseUntrusted refuses an empty array or object nested maxNesting + 1 deep") {
	CHECK_THROWS_AS((void)affresco::parseUntrusted(nested(affresco::maxNesting + 1, "[", ']')),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)affresco::parseUntrusted(nested(affresco::maxNesting, R"({"a":)", '}', "{}")),
	                affresco::DocumentError);
}

// Built with warnings as errors, this also checks that GCC's -Wdangling-reference accepts a temporary path
TEST_CASE("core: expect::object returns the object itself, or names the place that isn't one") {
	const affresco::json value = affresco::json::parse(R"({"a": {"b": 1}})");
	const std::string at = "root";
	const affresco::json& inner = affresco::expect::object(value["a"], at + ".a");
	CHECK(&inner == &value["a"]);
	CHECK_THROWS_WITH_AS((void)affresco::expect::object(inner["b"], at + ".a.b"),
	                     "AffrescoUI screen: root.a.b: expected an object", affresco::DocumentError);
}

TEST_CASE("core: parseUntrusted refuses text that is not JSON, and skips comments only when asked") {
	CHECK_THROWS_AS((void)affresco::parseUntrusted("{\"a\": }"), affresco::json::parse_error);
	CHECK_THROWS_AS((void)affresco::parseUntrusted("// note\n{}"), affresco::json::parse_error);
	CHECK(affresco::parseUntrusted("// note\n{}", true) == affresco::json::object());
}
