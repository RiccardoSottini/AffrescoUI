/// @file
/// @brief Unit tests for rich text: TextLayout (runs broken into lines across styles), a label's "runs" with bold,
/// italic, size, colour, underline, strike, highlight, shadow, links and icons, "selectable" with Cmd/Ctrl+A and
/// Cmd/Ctrl+C, "onLink", and the limits.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/RichText.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a label of runs, a selectable label, and one that shows the last link pressed.
/// @return The document.
json richScreen() {
	const std::string roboto = (std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf").string();
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"state", {{"link", ""}}},
	        {"fonts", {{"default", {{"file", roboto}, {"size", 16}}}}},
	        {"root",
	         {{"id", "page"},
	          {"children",
	           {{{"type", "label"},
	             {"id", "rich"},
	             {"x", 0},
	             {"y", 0},
	             {"width", 300},
	             {"height", 40},
	             {"runs",
	              {"Plain ",
	               {{"text", "bold"}, {"bold", true}},
	               " and ",
	               {{"text", "big"}, {"size", 24}, {"color", "#ff0000"}},
	               " ",
	               {{"text", "link"}, {"link", "https://example.com"}}}},
	             {"onLink", {{"set", "state.link"}, {"to", "{{event.link}}"}}}},
	            {{"type", "label"},
	             {"id", "copy"},
	             {"x", 0},
	             {"y", 50},
	             {"width", 300},
	             {"height", 30},
	             {"text", "Select me"},
	             {"selectable", true}},
	            {{"type", "label"}, {"id", "shown"}, {"x", 0}, {"y", 90}, {"text", "{{state.link}}"}}}}}}};
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

/// @brief Tells whether a document with this "runs" is refused, with this text in the error.
/// @param runs The runs.
/// @param expected Part of the error.
void checkRefused(const json& runs, const std::string& expected) {
	json document = richScreen();
	document["root"]["children"][0]["runs"] = runs;
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	try {
		affresco::Screen screen = affresco::Screen::fromString(document.dump(), {});
		screen.build(*window);
		window->render();
		FAIL("expected an error mentioning " << expected);
	} catch(const std::exception& error) {
		CHECK(std::string(error.what()).find(expected) != std::string::npos);
	}
}

} // namespace

TEST_CASE("rich text: runs break into lines at spaces across styles, and hit testing finds them") {
	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	const auto big = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 32.0f);
	std::vector<affresco::TextRun> runs(2);
	runs[0].text = affresco::codepointsOf("one two ");
	runs[1].text = affresco::codepointsOf("three four");
	runs[1].font = big;
	affresco::TextLayout layout;
	layout.layOut(runs, *font, font->measure(U"one two "), 0.0f, affresco::Align::Start);
	CHECK(layout.text() == U"one two three four");
	REQUIRE(layout.lines().size() >= 2);
	CHECK(layout.lines()[0].end == 8); // "one two " fits, "three" doesn't
	CHECK(layout.lines()[1].height == doctest::Approx(big->lineHeight()));
	CHECK(layout.positionAt({0.0f, 0.0f}) == 0);
	CHECK(layout.positionAt({1000.0f, 0.0f}) == layout.lines()[0].end);

	layout.layOut(runs, *font, std::numeric_limits<float>::infinity(), 0.0f, affresco::Align::Start);
	CHECK(layout.lines().size() == 1);
	CHECK(layout.height() == doctest::Approx(big->lineHeight()));
	REQUIRE(layout.pieceAt({1.0f, 1.0f}));
	CHECK(layout.pieces()[*layout.pieceAt({1.0f, 1.0f})].run == 0);
}

TEST_CASE("rich text: a label's runs, with bold and larger variants of its font, and colours") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	affresco::Screen screen = affresco::Screen::fromString(richScreen().dump(), {});
	screen.build(*window);
	window->render();

	const auto& rich = panel<affresco::Label>(screen, "rich");
	REQUIRE(rich.runs().size() == 6);
	CHECK(rich.text() == "Plain bold and big link");
	REQUIRE(rich.runs()[1].font);
	CHECK(rich.runs()[1].font->style().bold);
	REQUIRE(rich.runs()[3].font);
	CHECK(rich.runs()[3].font->pixelHeight() == 24.0f);
	CHECK(rich.runs()[3].color == *affresco::Color::parseHex("#ff0000"));
	CHECK(!rich.runs()[0].font); // the label's own
	CHECK(rich.runs()[5].link == "https://example.com");
}

TEST_CASE("rich text: a press on a link runs onLink with the link") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	affresco::Screen screen = affresco::Screen::fromString(richScreen().dump(), {});
	screen.build(*window);
	window->render();

	const auto& rich = panel<affresco::Label>(screen, "rich");
	const affresco::TextLayout& layout = rich.textLayout();
	const affresco::TextLayout::Piece& link = layout.pieces().back();
	REQUIRE(link.run == 5);
	const affresco::Rect content = rich.frame();
	const float y = content.top + (content.height() - layout.height()) / 2.0f + layout.height() / 2.0f;
	const affresco::Vec2 at{content.left + link.x + link.width / 2.0f, y};
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
	window->render();
	CHECK(panel<affresco::Label>(screen, "shown").text() == "https://example.com");
}

TEST_CASE("rich text: a selectable label selects with the mouse and Cmd/Ctrl+A, and copies with Cmd/Ctrl+C") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	affresco::Screen screen = affresco::Screen::fromString(richScreen().dump(), {});
	screen.build(*window);
	window->render();

	auto& copy = panel<affresco::Label>(screen, "copy");
	CHECK(copy.selectable());
	const affresco::Rect content = copy.frame();
	const float y = (content.top + content.bottom) / 2.0f;
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {content.left, y}});
	window->routeMouseMove({content.left + copy.font()->measure(U"Select"), y});
	window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {content.left + 60.0f, y}});
	CHECK(copy.selectedText() == "Select");

	const affresco::Mods command{0x0008 | 0x0002}; // Cmd or Ctrl, whichever the platform uses
	window->setFocus(&copy);
	window->routeKey({affresco::Key::A, 0, affresco::Action::Press, command});
	CHECK(copy.selectedText() == "Select me");
	window->routeKey({affresco::Key::C, 0, affresco::Action::Press, command});
	CHECK(window->clipboardText() == "Select me");

	copy.select(0, 0);
	CHECK(copy.selectedText().empty());
}

TEST_CASE("rich text: malformed runs and runs over the limits are refused") {
	checkRefused("text", "expected a list of runs");
	checkRefused(json::array({json::object({{"txt", "a"}})}), "unknown run property");
	checkRefused(json::array({json::object({{"text", "a"}, {"size", "big"}})}), "expected a number of pixels");
	checkRefused(json::array({json::array({"nested"})}), "expected text or a run object");
	checkRefused(json(std::vector<std::string>(257, "x")), "at most 256 runs");
	checkRefused(json::array({std::string(70'000, 'x')}), "characters in all");
	checkRefused(json::array({json::object({{"text", "a"}, {"link", std::string(3000, 'l')}})}), "at most 2048 bytes");
}

TEST_CASE("rich text: a bold or italic font entry styles its text, and its runs keep that style unless they say") {
	json document = richScreen();
	const json regular = document["fonts"]["default"];
	document["fonts"]["heading"] = {{"file", regular["file"]}, {"size", 16}, {"bold", true}};
	document["fonts"]["aside"] = {{"file", regular["file"]}, {"size", 16}, {"italic", true}};
	json& rich = document["root"]["children"][0];
	rich["font"] = "heading";
	rich["runs"] = {"Bold ", {{"text", "regular"}, {"bold", false}}, {{"text", " slanted"}, {"italic", true}}};
	document["root"]["children"][1]["font"] = "aside";
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), {});
	screen.build(*window);
	window->render();

	const auto& label = panel<affresco::Label>(screen, "rich");
	REQUIRE(label.font());
	CHECK(label.font()->style() == affresco::FontStyle{true, false});
	REQUIRE(label.runs().size() == 3);
	CHECK(!label.runs()[0].font); // the label's own, bold
	REQUIRE(label.runs()[1].font);
	CHECK(label.runs()[1].font->style() == affresco::FontStyle{});
	REQUIRE(label.runs()[2].font);
	CHECK(label.runs()[2].font->style() == affresco::FontStyle{true, true});
	const auto& aside = panel<affresco::Label>(screen, "copy");
	REQUIRE(aside.font());
	CHECK(aside.font()->style() == affresco::FontStyle{false, true});

	document["fonts"]["heading"]["bold"] = "yes";
	CHECK_THROWS_WITH_AS(affresco::Screen::fromString(document.dump(), {}).build(*window),
	                     doctest::Contains("fonts.heading.bold"), affresco::DocumentError);
}

TEST_CASE("rich text: a run's underline, strike, background highlight and shadow, which null removes") {
	json document = richScreen();
	document["root"]["children"][0]["runs"] = {{{"text", "under"}, {"underline", true}},
	                                           {{"text", " struck"}, {"strike", true}},
	                                           {{"text", " marked"}, {"background", "#ffff00"}},
	                                           {{"text", " plain"}, {"background", nullptr}, {"underline", false}},
	                                           {{"text", " raised"}, {"shadow", "#00000066"}}};
	const ManualClock clock;
	const auto window = createHeadlessWindow(320, 140, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), {});
	screen.build(*window);
	window->render();

	const auto& rich = panel<affresco::Label>(screen, "rich");
	REQUIRE(rich.runs().size() == 5);
	CHECK(rich.runs()[0].underline);
	CHECK(!rich.runs()[0].strike);
	CHECK(rich.runs()[1].strike);
	CHECK(rich.runs()[2].background == *affresco::Color::parseHex("#ffff00"));
	CHECK(!rich.runs()[3].background);
	CHECK(!rich.runs()[3].underline);
	CHECK(!rich.runs()[3].shadow);
	CHECK(rich.runs()[4].shadow == *affresco::Color::parseHex("#00000066"));
	checkRefused(json::array({json::object({{"text", "a"}, {"background", "yellowish"}})}), "background");
	checkRefused(json::array({json::object({{"text", "a"}, {"strike", "yes"}})}), "strike");
	checkRefused(json::array({json::object({{"text", "a"}, {"shadow", 3}})}), "shadow");
}
