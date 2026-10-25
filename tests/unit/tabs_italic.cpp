/// @file
/// @brief Unit tests for italic tabs: a tab marked "italic" (a preview tab, say) gets the strip's font slanted, at its
/// size, while a strip with none keeps no second font; a mark that isn't a boolean is refused.
///
/// The fonts are the examples' (Roboto, Lucide).

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/Tabs.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with two strips: "strip", its first tab italic, and "plain", none.
/// @return The document.
json italicScreen() {
	const std::filesystem::path fonts = AFFRESCOUI_FONTS_DIR;
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"fonts", {{"default", {{"file", (fonts / "Roboto-Regular.ttf").string()}, {"size", 15}}}}},
	        {"root",
	         {{"children",
	           {{{"type", "tabs"},
	             {"id", "strip"},
	             {"width", 300},
	             {"height", 32},
	             {"value", "b"},
	             {"tabs", {{{"id", "a"}, {"italic", true}}, {{"id", "b"}, {"italic", false}}}}},
	            {{"type", "tabs"}, {"id", "plain"}, {"width", 300}, {"height", 32}, {"tabs", {"a", "b"}}}}}}}};
}

} // namespace

TEST_CASE("tabs node: an italic tab gets the strip's font slanted; a strip without one keeps no second font") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(italicScreen().dump());
	screen.build(*window);
	window->render();
	const auto* strip = dynamic_cast<affresco::Tabs*>(screen.find("strip"));
	const auto* plain = dynamic_cast<affresco::Tabs*>(screen.find("plain"));
	REQUIRE(strip != nullptr);
	REQUIRE(plain != nullptr);
	CHECK(strip->tabs()[0].italic);
	CHECK_FALSE(strip->tabs()[1].italic);
	REQUIRE(strip->italicFont() != nullptr);
	CHECK(strip->italicFont()->style().italic);
	CHECK_FALSE(strip->italicFont()->style().bold);
	CHECK(strip->italicFont()->pixelHeight() == window->defaultFont()->pixelHeight());
	CHECK(plain->italicFont() == nullptr);

	// The preview tab kept: no italic tab left, no second font
	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"#strip/tabs": ["a", "b"]}})", &error));
	window->render();
	CHECK(strip->italicFont() == nullptr);
}

TEST_CASE("tabs node: an italic mark that isn't a boolean is refused") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(italicScreen().dump());
	screen.build(*window);
	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#strip/tabs": [{"id": "a", "italic": "yes"}]}})", &error));
	CHECK(error.find("italic") != std::string::npos);
	REQUIRE(dynamic_cast<affresco::Tabs*>(screen.find("strip")) != nullptr);
	CHECK(dynamic_cast<affresco::Tabs*>(screen.find("strip"))->tabs()[0].italic);
}
