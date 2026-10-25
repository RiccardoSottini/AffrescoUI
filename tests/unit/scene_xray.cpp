/// @file
/// @brief The scene node's `xray` (see-through surfaces) and `lighting` (the scene's lights or a studio light): read
/// into the viewport's settings, bounded, and refused when malformed, naming the key.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/scene3d/SceneView.hpp>
#include <affresco/scene3d/Viewport.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::scene3d::viewportSettingsFromJson;

namespace {

/// @brief A screen with a scene of a cube, with more of the scene node's keys.
/// @param extra The keys, as JSON members after a comma (or empty). @return The document.
std::string xrayScreen(const std::string& extra) {
	return R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "scene": { "nodes": [{ "id": "cube", "mesh": "cube" }] } },
		"root": { "id": "root", "children": [
			{ "type": "scene", "id": "viewport", "width": 300, "height": 200, "scene": "{{state.scene}}")json" +
	       (extra.empty() ? "" : ", " + extra) + R"json( }] } })json";
}

} // namespace

TEST_CASE("scene3d: xray and lighting are read into the settings, bounded") {
	const auto plain = viewportSettingsFromJson(json::object());
	CHECK(plain.xray == doctest::Approx(0.0f));
	CHECK_FALSE(plain.studio);

	const auto set = viewportSettingsFromJson(json{{"xray", 0.5}, {"lighting", "studio"}});
	CHECK(set.xray == doctest::Approx(0.5f));
	CHECK(set.studio);

	CHECK(viewportSettingsFromJson(json{{"xray", 3}}).xray == doctest::Approx(0.95f)); // the surfaces still show
	CHECK(viewportSettingsFromJson(json{{"xray", -1}}).xray == doctest::Approx(0.0f));
	CHECK(viewportSettingsFromJson(json{{"xray", "half"}}).xray == doctest::Approx(0.0f));
	CHECK_FALSE(viewportSettingsFromJson(json{{"lighting", "scene"}}).studio);
	CHECK_FALSE(viewportSettingsFromJson(json{{"lighting", 1}}).studio);
}

TEST_CASE("format: a scene node takes xray and lighting, and refuses bad ones naming the key") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(xrayScreen(R"("xray": 0.5, "lighting": "studio")"));
	screen.build(*window);
	window->render();
	REQUIRE(dynamic_cast<affresco::scene3d::SceneView*>(screen.find("viewport")));

	const auto refused = [&](const std::string& set, const std::string& key) {
		std::string error;
		CHECK_FALSE_MESSAGE(screen.applyRevision(R"({"set": {"#viewport/)" + set + "}}", &error), set);
		CHECK_MESSAGE(error.find(key) != std::string::npos, error);
	};
	refused(R"(xray": 2)", "xray");
	refused(R"(xray": "half")", "xray");
	refused(R"(lighting": "moon")", "lighting");
	std::string error;
	CHECK(screen.applyRevision(R"({"set": {"#viewport/xray": 0, "#viewport/lighting": "scene"}})", &error));
}
