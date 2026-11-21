/// @file
/// @brief General capabilities apps waited for: `padding` on rows, columns and grids (Panel::setContentInset()),
/// subtree `opacity` (Panel::setOpacity()), letter spacing in wrapped text (Font::wrap()), an element per tab for
/// assistive tools, the `openUrl` step (asked of the person, http and https only), `closeWindow: "main"` (only
/// from the person's own click or key) and clipboard images.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/net/SignIn.hpp>
#include <affresco/render/Accessibility.hpp>
#include <affresco/render/Bitmap.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/RichText.hpp>
#include <affresco/render/Tabs.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Builds a screen from a document in a headless window and lays it out.
struct Built {
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< The window.
	affresco::Screen screen;                                                          ///< The screen.

	/// @brief Builds it.
	/// @param document The screen document.
	explicit Built(const json& document) : screen(affresco::Screen::fromString(document.dump())) {
		screen.build(*window);
		window->render();
	}

	/// @brief Clicks a point as the person does.
	/// @param at The point.
	void click(affresco::Vec2 at) const {
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		window->render();
	}
};

/// @brief A screen document around a root.
/// @param root The root node.
/// @param more Other sections.
/// @return The document.
json screenOf(json root, const json& more = json::object()) {
	json document = {{"format", "affrescoui-screen"}, {"version", 2}, {"root", std::move(root)}};
	document.update(more);
	return document;
}

} // namespace

TEST_CASE("padding: a column places its children inside it, and fits its content with it") {
	Built built(screenOf(
	    {{"id", "root"},
		 {"children",
		  {{{"type", "column"},
		    {"id", "box"},
		    {"padding", {10, 20}},
		    {"alignItems", "stretch"},
		    {"fitContent", "height"},
		    {"width", 200},
		    {"gap", 4},
		    {"children", {{{"id", "a"}, {"height", 30}}, {{"id", "b"}, {"height", 30}, {"anchors", "left right"}}}}},
		   {{"type", "grid"},
		    {"id", "grid"},
		    {"padding", 8},
		    {"columns", 2},
		    {"y", 150},
		    {"width", 216},
		    {"height", 60},
		    {"children", {{{"id", "cell"}, {"height", 20}}}}}}}}));
	const affresco::Rect box = built.screen.find("box")->frame();
	const affresco::Rect a = built.screen.find("a")->frame();
	const affresco::Rect b = built.screen.find("b")->frame();
	CHECK(a.left == doctest::Approx(box.left + 20.0f));
	CHECK(a.top == doctest::Approx(box.top + 10.0f));
	CHECK(a.width() == doctest::Approx(160.0f)); // stretched within the padding
	CHECK(b.top == doctest::Approx(a.bottom + 4.0f));
	CHECK(b.width() == doctest::Approx(160.0f)); // its own anchors, within the padding too
	CHECK(box.height() == doctest::Approx(30.0f + 4.0f + 30.0f + 20.0f));

	const affresco::Rect grid = built.screen.find("grid")->frame();
	const affresco::Rect cell = built.screen.find("cell")->frame();
	CHECK(cell.left == doctest::Approx(grid.left + 8.0f));
	CHECK(cell.top == doctest::Approx(grid.top + 8.0f));
	CHECK(cell.width() == doctest::Approx(100.0f)); // (216 - 16) / 2

	ManualClock clock;
	const auto window = createHeadlessWindow(100, 100, clock);
	affresco::Screen bad = affresco::Screen::fromString(
	    screenOf({{"id", "root"}, {"children", {{{"type", "row"}, {"id", "r"}, {"padding", {1, 2, 3}}}}}}).dump());
	CHECK_THROWS(bad.build(*window));
}

TEST_CASE("opacity: the subtree is drawn as one translucent group, and back to opaque when the key goes") {
	Built built(screenOf(
	    {{"id", "root"},
		 {"onClick", {{{"toggle", "state.faded"}}}},
		 {"children", {{{"id", "card"}, {"opacity", "{{state.faded ? 0.4 : 1}}"}, {"width", 50}, {"x", 200}}}}},
	    {{"state", {{"faded", true}}}}));
	const affresco::Panel* card = built.screen.find("card");
	CHECK(card->opacity() == doctest::Approx(0.4f));
	CHECK(card->isComposited());
	built.click({10.0f, 10.0f});
	CHECK(card->opacity() == doctest::Approx(1.0f));
	CHECK_FALSE(card->isComposited());

	affresco::Panel panel;
	panel.setOpacity(3.0f);
	CHECK(panel.opacity() == 1.0f);
	panel.setOpacity(-1.0f);
	CHECK(panel.opacity() == 0.0f);
}

TEST_CASE("letter spacing: wrapped text breaks with the spacing counted") {
	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	REQUIRE(font != nullptr);
	const std::u32string text = U"one two three four five six seven eight";
	const float width = font->measure(U"one two three four");
	const std::size_t plain = font->wrap(text, width).size();
	const std::size_t spaced = font->wrap(text, width, 4.0f).size();
	CHECK(spaced > plain);
	CHECK(font->wrap(text, width, 0.0f).size() == plain);
}

TEST_CASE("tabs: each tab is an element of its own, selected when active, and pressing it picks it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 100, clock);
	auto strip = std::make_unique<affresco::Tabs>();
	affresco::Tabs& tabs = *strip;
	tabs.setSize(affresco::px(400.0f), affresco::px(32.0f));
	// No close buttons: without a font a tab is little more than its button, and its centre would be on it
	tabs.setTabs({{.id = "a", .label = "Alpha", .closable = false},
	              {.id = "b", .label = "Beta", .closable = false},
	              {.id = "c", .label = "Gamma", .closable = false}});
	tabs.setActive("a");
	std::string picked;
	tabs.onActivated.connect([&](const std::string& id) { picked = id; });
	window->root().addChild(std::move(strip));
	window->render();

	const std::vector<affresco::Panel*> found = affresco::findByRole(window->root(), "tab");
	REQUIRE(found.size() == 3);
	CHECK(affresco::accessibleName(*found[1]) == "Beta");
	CHECK(found[0]->accessibleState().selected == true);
	CHECK(found[1]->accessibleState().selected == false);
	CHECK(found[1]->frame().left > found[0]->frame().left);

	CHECK(affresco::performAccessibleAction(*window, *found[1], affresco::AccessibleAction::Press));
	CHECK(picked == "b");
	CHECK(found[1]->accessibleState().selected == true);
	CHECK(found[0]->accessibleState().selected == false);

	tabs.setTabs({{.id = "a", .label = "Alpha"}});
	window->render();
	CHECK(affresco::findByRole(window->root(), "tab").size() == 1);
}

TEST_CASE("openUrl: asks the person with the whole address, opens nothing on a no, and takes only http and https") {
	const json root = {{"id", "root"}, {"onClick", "open"}};
	Built built(screenOf(root, {{"actions",
	                             {{"open", {{{"openUrl", "https://example.com/help?topic=1"}}}},
	                              {"bad", {{{"openUrl", "javascript:alert(1)"}}}},
	                              {"file", {{{"openUrl", "file:///etc/passwd"}}}}}}}));
	CHECK(built.window->confirmation() == nullptr);
	built.click({10.0f, 10.0f});
	affresco::Panel* question = built.window->confirmation();
	REQUIRE(question != nullptr);
	bool shown = false;
	const std::function<void(const affresco::AccessibleNode&)> look = [&](const affresco::AccessibleNode& node) {
		shown = shown || node.value.contains("https://example.com/help?topic=1") ||
		        node.name.contains("https://example.com/help?topic=1");
		for(const auto& child : node.children) look(child);
	};
	look(affresco::accessibilityTree(*question));
	CHECK(shown);
	// Escape says no: the question goes, and nothing opens
	built.clock.advance(1000.0);
	built.window->render();
	built.window->routeKey({affresco::Key::Escape, 0, affresco::Action::Press, {}});
	built.window->routeKey({affresco::Key::Escape, 0, affresco::Action::Release, {}});
	built.window->render();
	CHECK(built.window->confirmation() == nullptr);

	built.screen.runAction("bad", built.window->root()); // reported, and nothing asked
	built.screen.runAction("file", built.window->root());
	CHECK(built.window->confirmation() == nullptr);

	// The opener refuses anything but a well-formed http or https address, before starting a browser
	CHECK_FALSE(affresco::net::openWebAddress("javascript:alert(1)"));
	CHECK_FALSE(affresco::net::openWebAddress("file:///etc/passwd"));
	CHECK_FALSE(affresco::net::openWebAddress("https://user@example.com/"));
}

TEST_CASE("closeWindow main: a load or a timer doesn't end the app; the person's click does") {
	const json root = {{"id", "root"}, {"onClick", "quit"}};
	Built built(screenOf(root, {{"onLoad", {{{"closeWindow", "main"}}}},
	                            {"actions",
	                             {{"quit", {{{"closeWindow", "main"}}}},
	                              {"later", {{{"after", 50}, {"do", {{{"closeWindow", "main"}}}}}}}}}}));
	CHECK_FALSE(built.window->shouldClose());
	built.screen.runAction("later", built.window->root());
	built.clock.advance(100.0);
	built.window->render();
	CHECK_FALSE(built.window->shouldClose());
	built.click({10.0f, 10.0f});
	CHECK(built.window->shouldClose());
}

TEST_CASE("clipboard images: paste one under a name on the person's click, and copy a named one") {
	const json root = {{"id", "root"}, {"onClick", "paste"}};
	Built built(screenOf(root, {{"state", {{"pasted", nullptr}}},
	                            {"actions",
	                             {{"paste", {{{"paste", "image"}, {"as", "clip"}, {"into", "state.pasted"}}}},
	                              {"copy", {{{"copyImage", "clip"}}}}}}}));
	built.click({10.0f, 10.0f}); // nothing on the clipboard yet
	CHECK(json::parse(built.screen.stateJson())["pasted"].is_null());

	const std::vector<std::uint8_t> red = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
	                                       255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
	const auto image = affresco::Bitmap::fromPixels(3, 2, red, affresco::Bitmap::Limits{});
	REQUIRE(built.window->setClipboardImage(image));
	built.screen.runAction("paste", built.window->root()); // the host's, not the person's: refused
	CHECK(json::parse(built.screen.stateJson())["pasted"].is_null());
	built.click({10.0f, 10.0f});
	CHECK(json::parse(built.screen.stateJson())["pasted"] == json({{"image", "clip"}, {"width", 3}, {"height", 2}}));

	REQUIRE(built.window->setClipboardImage(
	    affresco::Bitmap::fromPixels(1, 1, std::vector<std::uint8_t>{0, 0, 0, 255}, affresco::Bitmap::Limits{})));
	built.screen.runAction("copy", built.window->root());
	REQUIRE(built.window->clipboardImage() != nullptr);
	CHECK(built.window->clipboardImage()->width() == 3);
}

TEST_CASE("justify: rich text's wrapped lines fill the width, cut at their spaces; a paragraph's last line doesn't") {
	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 16.0f);
	REQUIRE(font != nullptr);
	affresco::TextLayout layout;
	const std::vector<affresco::TextRun> runs = {{.text = U"one two three "}, {.text = U"four five six seven eight"}};
	const float box = font->measure(U"one two three four") + 12.0f; // room left on the first line
	layout.layOut(runs, *font, box, box, affresco::Align::Start);
	REQUIRE(layout.lines().size() >= 2);
	const std::size_t firstLinePieces = layout.lines()[0].endPiece - layout.lines()[0].firstPiece;
	layout.justify(box);
	const auto& first = layout.lines()[0];
	CHECK(first.endPiece - first.firstPiece > firstLinePieces); // cut at the spaces
	const auto& lastPiece = layout.pieces()[first.endPiece - 1];
	CHECK(lastPiece.x + lastPiece.width >= box - 1.0f); // reaches the right edge
	const auto& last = layout.lines().back();
	CHECK(layout.pieces()[last.firstPiece].x == doctest::Approx(0.0f));
	CHECK(last.width < box); // the paragraph's last line keeps its spacing
}
