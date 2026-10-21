/// @file
/// @brief Unit tests for design tokens ("tokens"): numbers and colours used as "@name", aliases, per-theme values, a
/// font size from a token, one edit restyling every node that uses a token, and the errors (unknown, circular, mixed,
/// a name in both "colors" and "tokens").

#include "Checker.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Stack.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen whose spacing, colours and type size come from tokens, with a light and a dark theme.
/// @return The document.
json tokenScreen() {
	const std::string roboto = (std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf").string();
	return {{"format", "affrescoui-screen"},
	        {"version", 1},
	        {"state", {{"theme", "light"}}},
	        {"theme", "{{state.theme}}"},
	        {"colors", {{"brand", "#6366f1"}}},
	        {"tokens",
	         {{"space.s", 4},
	          {"space.m", "@space.s"},
	          {"type.body", 20},
	          {"color.surface", {{"light", "#ffffff"}, {"dark", "#111827"}}},
	          {"color.accent", "@brand"}}},
	        {"fonts", {{"default", {{"file", roboto}, {"size", "@type.body"}}}}},
	        {"root",
	         {{"id", "page"}, {"color", "@color.surface"},
	          {"children",
	           {{{"type", "column"},
	             {"id", "list"},
	             {"gap", "@space.m"},
	             {"children",
	              {{{"type", "label"}, {"id", "a"}, {"text", "A"}, {"padding", "@space.m"}, {"color", "@color.accent"}},
	               {{"type", "label"}, {"id", "b"}, {"text", "B"}, {"padding", "@space.s"}}}}}}}}}};
}

/// @brief A panel of the screen, as a type.
/// @tparam T The panel's type.
/// @param screen The screen.
/// @param id Its id.
/// @return It.
template <typename T> T& panel(affresco::Screen& screen, std::string_view id) {
	auto* found = dynamic_cast<T*>(screen.find(id));
	REQUIRE(found);
	return *found;
}

/// @brief Tells whether a document with these tokens is refused, with this text in the error.
/// @param tokens The "tokens" section.
/// @param expected Part of the error.
void checkRefused(const json& tokens, const std::string& expected) {
	json document = tokenScreen();
	document["tokens"].update(tokens);
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	try {
		affresco::Screen screen = affresco::Screen::fromString(document.dump(), {});
		screen.build(*window);
		FAIL("expected an error mentioning " << expected);
	} catch(const std::exception& error) {
		CHECK(std::string(error.what()).find(expected) != std::string::npos);
	}
}

} // namespace

TEST_CASE("tokens: numbers, colours, aliases and type sizes are read as @name") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(tokenScreen().dump(), {});
	screen.build(*window);
	window->render();

	CHECK(panel<affresco::Stack>(screen, "list").gap() == 4.0f);
	CHECK(panel<affresco::Label>(screen, "a").padding() == 4.0f); // through the alias
	CHECK(panel<affresco::Label>(screen, "a").color() == *affresco::Color::parseHex("#6366f1"));
	CHECK(panel<affresco::Panel>(screen, "page").color() == *affresco::Color::parseHex("#ffffff"));
	REQUIRE(panel<affresco::Label>(screen, "a").font());
	// A size spans the ascender to the descender, and Roboto has no line gap: the line is the token's 20
	CHECK(panel<affresco::Label>(screen, "a").font()->lineHeight() == doctest::Approx(20.0f));
}

TEST_CASE("tokens: anchors, radius and shadow read number tokens in their members, as the checker allows") {
	json document = tokenScreen();
	document["root"]["children"].push_back({{"id", "box"},
	                                        {"width", 10},
	                                        {"height", 10},
	                                        {"anchors", {{"right", "@space.m"}, {"bottom", "@space.s"}}},
	                                        {"radius", {{"topLeft", "@space.m"}}},
	                                        {"shadow", {{"x", "@space.s"}, {"blur", "@space.m"}}}});
	CHECK(affresco::format::check(document, document["state"], {})->empty());
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), {});
	screen.build(*window);
	window->render();

	const affresco::Panel& box = panel<affresco::Panel>(screen, "box");
	CHECK(box.frame().right == 296.0f);
	CHECK(box.frame().bottom == 196.0f);
	CHECK(box.cornerRadii().topLeft == 4.0f);
	CHECK(box.shadow().offset.x == 4.0f);
	CHECK(box.shadow().blur == 4.0f);
}

TEST_CASE("tokens: the theme switches them, and one edit restyles every node that uses one") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(tokenScreen().dump(), {});
	screen.build(*window);
	window->render();

	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"/state/theme": "dark"}})", &error));
	window->render();
	CHECK(panel<affresco::Panel>(screen, "page").color() == *affresco::Color::parseHex("#111827"));

	REQUIRE(screen.applyRevision(R"({"set": {"/tokens/space.s": 10}})", &error));
	window->render();
	CHECK(panel<affresco::Stack>(screen, "list").gap() == 10.0f);
	CHECK(panel<affresco::Label>(screen, "a").padding() == 10.0f);
	CHECK(panel<affresco::Label>(screen, "b").padding() == 10.0f);
}

TEST_CASE("tokens: unknown, circular and mixed tokens, and names in both sections, are refused") {
	checkRefused({{"space.m", "@nothing"}}, "unknown token");
	checkRefused({{"a", "@b"}, {"b", "@a"}}, "deeper than 16");
	checkRefused({{"mixed", {{"light", 4}, {"dark", "#000000"}}}}, "either a number or a color");
	checkRefused({{"brand", 4}}, "\"colors\" has a color of this name too");
	checkRefused({{"bad", true}}, "tokens.bad");
}
