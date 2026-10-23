/// @file
/// @brief The "raster" node: its first view fits the image and publishes it, the pointer on it plays the current tool
/// through the "pointer" operation's path, the space bar pans and the wheel zooms, it names itself for assistive
/// technology, and a node that goes stops hearing of its image.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a whiteboard of 64 × 32 pixels shown by a raster node of 128 × 64, while state.shown.
/// @param image The image the node names.
/// @return The document.
std::string boardScreen(const std::string& image = "board") {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "board": null, "shown": true },
		"images": { "board": { "width": 64, "height": 32, "into": "state.board",
		                       "tools": { "marker": { "gesture": "paint", "size": 4, "hardness": 100,
		                                              "foreground": "#000000" } } } },
		"actions": { "hide": [{ "set": "state.shown", "to": false }], "clear": [{ "image": "clearHistory" }] },
		"root": { "id": "root", "type": "column", "children": [
			{ "id": "view", "type": "raster", "image": ")json" +
	       image + R"json(", "width": 128, "height": 64, "when": "{{state.shown}}" }] }
	})json";
}

/// @brief Where a document point of the board lands in the window, from its published view.
/// @param board The board's snapshot.
/// @param frame The node's frame.
/// @param x Across, in document pixels.
/// @param y Down.
/// @return In window units (the headless window has one pixel per unit).
affresco::Vec2 toWindow(const json& board, const affresco::Rect& frame, double x, double y) {
	const double zoom = board["view"]["zoom"].get<double>();
	return {static_cast<float>(frame.left + frame.width() / 2.0 + (x - board["view"]["x"].get<double>()) * zoom),
	        static_cast<float>(frame.top + frame.height() / 2.0 + (y - board["view"]["y"].get<double>()) * zoom)};
}

/// @brief Drags with the left button.
/// @param window The window.
/// @param from Where it presses.
/// @param to Where it releases.
void drag(affresco::Window& window, affresco::Vec2 from, affresco::Vec2 to) {
	window.routeMouseMove(from);
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, from});
	window.routeMouseMove({(from.x + to.x) / 2.0f, (from.y + to.y) / 2.0f});
	window.routeMouseMove(to);
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, to});
}

} // namespace

TEST_CASE("format: a raster node fits its image, and the pointer on it paints, pans and zooms") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen());
	screen.build(*window);
	window->render();
	affresco::Panel* view = screen.find("view");
	REQUIRE(view != nullptr);
	const auto board = [&] { return json::parse(screen.stateJson())["board"]; };

	// The first view to size the image fits it (0.9 of the node) and the screen hears of it
	REQUIRE(board().is_object());
	CHECK(board()["view"]["zoom"].get<double>() == doctest::Approx(1.8));
	CHECK(board()["canUndo"] == false);
	CHECK(view->accessibleRole() == "image");
	CHECK(view->accessibleLabel() == "board");

	// A drag paints a stroke, as the pointer operation would: one step of history
	const affresco::Rect frame = view->frame();
	drag(*window, toWindow(board(), frame, 10, 16), toWindow(board(), frame, 50, 16));
	CHECK(board()["canUndo"] == true);
	CHECK(board()["history"].back()["label"] == "Brush"); // a paint stroke's label
	const std::size_t steps = board()["history"].size();

	// The space bar held pans whatever the tool, and paints nothing
	const double x = board()["view"]["x"].get<double>();
	window->routeKey({affresco::Key::Space, 0, affresco::Action::Press, {}});
	drag(*window, toWindow(board(), frame, 30, 16), toWindow(board(), frame, 20, 16));
	window->routeKey({affresco::Key::Space, 0, affresco::Action::Release, {}});
	CHECK(board()["view"]["x"].get<double>() == doctest::Approx(x + 10.0));
	CHECK(board()["history"].size() == steps);

	// The wheel zooms about the pointer: the point under it stays there
	const affresco::Vec2 at = toWindow(board(), frame, 40, 10);
	window->routeScroll({{0.0f, 1.0f}, at});
	CHECK(board()["view"]["zoom"].get<double>() != doctest::Approx(1.8));
	const affresco::Vec2 still = toWindow(board(), frame, 40, 10);
	CHECK(still.x == doctest::Approx(at.x).epsilon(0.01));
	CHECK(still.y == doctest::Approx(at.y).epsilon(0.01));
}

TEST_CASE("format: a raster node that goes stops hearing of its image, and one naming no image is an error") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(boardScreen());
	screen.build(*window);
	window->render();
	affresco::Panel* root = screen.find("root");
	REQUIRE(root != nullptr);
	screen.runAction("hide", *root);
	window->render();
	CHECK(screen.find("view") == nullptr);
	screen.runAction("clear", *root); // tells the store's watchers: none is left
	window->render();
	CHECK(json::parse(screen.stateJson())["board"]["canUndo"] == false);

	affresco::Screen stranger = affresco::Screen::fromString(boardScreen("drawing"));
	CHECK_THROWS_WITH(stranger.build(*window), doctest::Contains("root.children[0].image"));
}
