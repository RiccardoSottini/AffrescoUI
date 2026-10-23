/// @file
/// @brief Unit tests for images and icons in screens, on a headless window: the "image" node (a file, decoded at once
/// without an event loop; an image the host names; fit), files only from the screen's folder unless the host allows
/// more, the "icon" property of labels, looked up by name in the font and its fallbacks, and "snapshot" (a node drawn
/// into a named image), where it is refused.
///
/// The images are the rendering check's (tests/render/screens/images), the fonts the examples' (Roboto, Lucide).

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/ImageView.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief The rendering check's screens, whose images/ folder the test screen uses.
std::filesystem::path screens() { return std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR).parent_path() / "screens"; }
/// @brief The examples' fonts.
std::filesystem::path fonts() { return AFFRESCOUI_FONTS_DIR; }

/// @brief A screen with images (a file, a named one) and labels with icons, laid out on a headless window.
/// @return The document.
json imageScreen() {
	const std::string roboto = (fonts() / "Roboto-Regular.ttf").string(), lucide = (fonts() / "Lucide.ttf").string();
	return {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"fonts",
		 {{"default", {{"file", roboto}, {"size", 16}, {"fallback", {"icons"}}}},
		  {"icons", {{"file", lucide}, {"size", 16}}}}},
	    {"root",
		 {{"children",
		   {{{"type", "image"}, {"id", "photo"}, {"width", 80}, {"height", 50}, {"file", "images/badge.png"}},
		    {{"type", "image"}, {"id", "chart"}, {"width", 80}, {"height", 50}, {"source", "chart"}, {"fit", "cover"}},
		    {{"type", "label"}, {"id", "plain"}, {"text", "Search"}, {"fitText", true}},
		    {{"type", "label"}, {"id", "withIcon"}, {"text", "Search"}, {"icon", "search"}, {"fitText", true}},
		    {{"type", "label"}, {"id", "unknownIcon"}, {"text", "Search"}, {"icon", "no-such"}, {"fitText", true}},
		    {{"type", "label"}, {"id", "iconOnly"}, {"icon", "search"}, {"fitText", true}}}}}}};
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

} // namespace

TEST_CASE("image nodes: a file shows, a named image follows the host, and fit is read") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(imageScreen().dump(), screens());
	screen.build(*window);
	window->render();

	const auto& photo = panel<affresco::ImageView>(screen, "photo");
	REQUIRE(photo.bitmap()); // no event loop: decoded when asked for
	CHECK(photo.bitmap()->width() == 48);
	CHECK(photo.fit() == affresco::ImageFit::contain);

	const auto& chart = panel<affresco::ImageView>(screen, "chart");
	CHECK(chart.fit() == affresco::ImageFit::cover);
	CHECK_FALSE(chart.bitmap());
	const std::array<std::uint8_t, 8> pixels{0, 0, 255, 255, 255, 0, 0, 255};
	screen.setImage("chart", 2, 1, pixels);
	REQUIRE(chart.bitmap());
	CHECK(chart.bitmap()->width() == 2);
	screen.setImage("chart", nullptr);
	CHECK_FALSE(chart.bitmap());

	// A revision changes the file and the fit
	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"#photo/file": "images/banner.png", "#photo/fit": "fill"}})", &error));
	window->render();
	REQUIRE(photo.bitmap());
	CHECK(photo.bitmap()->width() == 320);
	CHECK(photo.fit() == affresco::ImageFit::fill);

	CHECK_FALSE(screen.applyRevision(R"({"set": {"#photo/fit": "stretch"}})", &error));
	CHECK(error.find("fit") != std::string::npos);
}

TEST_CASE("image nodes: a revision can't show a file outside the screen's folder, unless the host allows its folder") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(imageScreen().dump(), screens());
	screen.build(*window);
	window->render();

	std::string error;
	const std::string outside = (fonts() / "LICENSE-Lucide.txt").string();
	CHECK_FALSE(screen.applyRevision(json{{"set", {{"#photo/file", outside}}}}.dump(), &error));
	CHECK(error.find("root.children[0].file") != std::string::npos); // the place
	CHECK(error.find("folder") != std::string::npos);                // and why
	CHECK_FALSE(
	    screen.applyRevision(R"({"set": {"#photo/file": "../../../examples/shared/fonts/Lucide.ttf"}})", &error));
	CHECK(panel<affresco::ImageView>(screen, "photo").bitmap()->width() == 48); // unchanged

	// The host may allow another folder: a file there that isn't an image then shows nothing, without an error
	screen.allowImageDirectory(fonts());
	CHECK(screen.applyRevision(json{{"set", {{"#photo/file", outside}}}}.dump(), &error));
	window->render();
	CHECK_FALSE(panel<affresco::ImageView>(screen, "photo").bitmap());
}

TEST_CASE("icons: a label's icon is a glyph named in its font or a fallback, drawn before the text") {
	const auto lucide = affresco::Font::load(fonts() / "Lucide.ttf", 16.0f);
	CHECK(lucide->codepointNamed("search") == 0xE151);
	CHECK(lucide->codepointNamed("settings") == 0xE154);
	CHECK(lucide->codepointNamed("no-such") == 0);
	CHECK(lucide->codepointNamed("") == 0);
	CHECK(lucide->codepointNamed(std::string(200, 'a')) == 0);
	const std::array<std::filesystem::path, 1> fallback{fonts() / "Lucide.ttf"};
	const auto roboto = affresco::Font::load(fonts() / "Roboto-Regular.ttf", 16.0f, fallback);
	CHECK(roboto->codepointNamed("search") == 0xE151); // from the fallback

	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(imageScreen().dump(), screens());
	screen.build(*window);
	window->render();

	const float plain = panel<affresco::Label>(screen, "plain").frame().width();
	const float withIcon = panel<affresco::Label>(screen, "withIcon").frame().width();
	const float iconOnly = panel<affresco::Label>(screen, "iconOnly").frame().width();
	CHECK(iconOnly > 10.0f);
	CHECK(withIcon == doctest::Approx(plain + iconOnly + roboto->glyph(U' ').advance).epsilon(0.02));
	CHECK(panel<affresco::Label>(screen, "unknownIcon").frame().width() == plain); // a name no font has: nothing
	CHECK(panel<affresco::Label>(screen, "withIcon").text() == "Search");          // the icon isn't part of the text

	std::string error;
	REQUIRE(screen.applyRevision(R"({"remove": ["#withIcon/icon"]})", &error));
	window->render();
	CHECK(panel<affresco::Label>(screen, "withIcon").frame().width() == plain);
}

TEST_CASE("image nodes and icons are in the catalog") {
	const affresco::Screen screen = affresco::Screen::fromString(imageScreen().dump(), screens());
	const json catalog = json::parse(screen.catalogJson());
	const json& image = catalog["types"]["image"]["properties"];
	for(const char* property : {"file", "source", "fit"}) CHECK(std::ranges::find(image, property) != image.end());
	const json& label = catalog["types"]["label"]["properties"];
	CHECK(std::ranges::find(label, "icon") != label.end());
}

TEST_CASE("snapshot: a node drawn out of the layout into a named image, refused where it can't be") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	json document = imageScreen();
	document["state"] = {{"open", false}};
	document["root"]["id"] = "main";
	document["root"]["children"].push_back({{"id", "menu"}, {"overlay", {{"open", "state.open"}}}});
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), screens());
	screen.build(*window);
	window->render();

	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"#photo/snapshot": {"image": "thumb", "scale": 2}}})", &error));
	CHECK(panel<affresco::ImageView>(screen, "photo").isSnapshot());
	REQUIRE(screen.applyRevision(R"({"set": {"#photo/snapshot": ""}})", &error));
	CHECK_FALSE(panel<affresco::ImageView>(screen, "photo").isSnapshot()); // an empty name draws it in place again

	const std::array<std::pair<const char*, const char*>, 6> refused = {{
	    {R"({"set": {"#photo/snapshot": {"image": "thumb", "colour": 1}}})", "unknown property"},
	    {R"({"set": {"#photo/snapshot": {"scale": 2}}})", "\"image\""},
	    {R"({"set": {"#photo/snapshot": {"image": "thumb", "scale": 0}}})", "scale"},
	    {R"({"set": {"#photo/snapshot": {"image": "thumb", "scale": 5}}})", "scale"},
	    {R"({"set": {"#main/snapshot": "thumb"}})", "root"},
	    {R"({"set": {"#menu/snapshot": "thumb"}})", "overlay"},
	}};
	for(const auto& [revision, why] : refused) {
		CAPTURE(revision);
		error.clear();
		CHECK_FALSE(screen.applyRevision(revision, &error));
		CHECK(error.find("snapshot") != std::string::npos);
		CHECK(error.find(why) != std::string::npos);
	}
	CHECK_FALSE(panel<affresco::ImageView>(screen, "photo").isSnapshot()); // nothing of a refused revision applies

	const json catalog = json::parse(screen.catalogJson());
	CHECK(std::ranges::find(catalog["common"], "snapshot") != catalog["common"].end());
}
