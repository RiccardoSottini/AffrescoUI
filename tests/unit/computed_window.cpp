/// @file
/// @brief Computed values that read the window follow it: a resize makes them stale as a state change does.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A label showing a computed value that depends on the window's width only.
constexpr const char* layoutScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"computed": { "wide": "{{window.width >= 500}}" },
	"root": { "id": "root", "children": [
		{ "type": "label", "id": "mode", "text": "{{computed.wide ? 'wide' : 'narrow'}}", "width": 100, "height": 20 }
	] }
})json";

/// @brief Finds a label by id, depth first.
/// @param panel The subtree's root.
/// @param id The id.
/// @return The label, or nullptr.
const affresco::Label* findLabel(const affresco::Panel& panel, const std::string& id) {
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel); label && panel.id() == id) return label;
	for(const auto& child : panel.children())
		if(const affresco::Label* found = findLabel(*child, id)) return found;
	return nullptr;
}

} // namespace

TEST_CASE("computed: a value that reads the window follows a resize") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(640, 480, clock); // outlives the screen
	affresco::Screen screen = affresco::Screen::fromString(layoutScreen);
	screen.build(*window);
	window->render();
	const affresco::Label* mode = findLabel(window->root(), "mode");
	REQUIRE(mode);
	CHECK(mode->text() == "wide");

	window->setSize(320, 480);
	window->render();
	mode = findLabel(window->root(), "mode");
	REQUIRE(mode);
	CHECK(mode->text() == "narrow");

	window->setSize(800, 480);
	window->render();
	mode = findLabel(window->root(), "mode");
	REQUIRE(mode);
	CHECK(mode->text() == "wide");
}
