/// @file
/// @brief A window resized by the program: onResize fires once, before the OS's next events, on every platform (X11
/// reports the size only with them, and a scenario's resize step doesn't wait for them).

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <vector>

TEST_CASE("window: setSize fires onResize once, before the OS's next events") {
	affresco::Application app({nullptr, true});
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"window": { "title": "resize", "width": 200, "height": 120 },
		"root": { "type": "panel", "id": "main", "color": "#202020" }
	})json");
	affresco::Window& window = screen.createWindow(app);
	std::vector<affresco::ResizeEvent> resizes;
	window.onResize.connect([&](const affresco::ResizeEvent& event) { resizes.push_back(event); });

	window.setSize(320, 240);
	CHECK(window.width() == 320);
	CHECK(window.height() == 240);
	REQUIRE(resizes.size() == 1);
	CHECK(resizes[0].width == 320);
	CHECK(resizes[0].height == 240);

	window.setSize(320, 240); // the same size: nothing to report
	CHECK(resizes.size() == 1);
}
