/// @file
/// @brief The scene node's `extras`: whether lights and cameras are drawn at all (a rendered image leaves them out),
/// read into the viewport's settings and refused when malformed, naming the key.

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

TEST_CASE("scene3d: extras are on unless a flag turns them off") {
	CHECK(viewportSettingsFromJson(json::object()).extras);
	CHECK_FALSE(viewportSettingsFromJson(json{{"extras", false}}).extras);
	CHECK(viewportSettingsFromJson(json{{"extras", true}}).extras);
	CHECK(viewportSettingsFromJson(json{{"extras", "no"}}).extras); // malformed: the default
}

TEST_CASE("format: a scene node takes extras, and refuses a value that isn't a flag, naming the key") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "scene": { "nodes": [{ "id": "lamp", "light": { "type": "point" }, "translation": [0, 2, 0] }] } },
		"root": { "id": "root", "children": [
			{ "type": "scene", "id": "viewport", "width": 300, "height": 200, "scene": "{{state.scene}}",
			  "extras": false }] } })json");
	screen.build(*window);
	window->render();
	REQUIRE(dynamic_cast<affresco::scene3d::SceneView*>(screen.find("viewport")));

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#viewport/extras": "off"}})", &error));
	CHECK_MESSAGE(error.find("extras") != std::string::npos, error);
	CHECK(screen.applyRevision(R"({"set": {"#viewport/extras": true}})", &error));
}
