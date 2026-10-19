/// @file
/// @brief Unit tests for the screen module that need no window: loading a screen from its text.

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <string>

namespace {

/// @brief A screen whose state holds a value nested in @p depth lists.
/// @param depth How many lists.
/// @return The screen's text.
std::string screenNestedIn(std::size_t depth) {
	return R"({"format": "affrescoui-screen", "version": 1, "root": {"type": "panel"}, "state": {"x": )" +
	       std::string(depth, '[') + std::string(depth, ']') + "}}";
}

} // namespace

// A screen file is untrusted input: before, one nested 200,000 deep overflowed the stack (AddressSanitizer: SEGV)
TEST_CASE("screen: a screen nested deeper than the limit is refused, not parsed") {
	CHECK_NOTHROW((void)affresco::Screen::fromString(screenNestedIn(100)));
	CHECK_THROWS_WITH_AS((void)affresco::Screen::fromString(screenNestedIn(200000)),
	                     doctest::Contains("nested more than 256 levels deep"), affresco::ScreenError);
}
