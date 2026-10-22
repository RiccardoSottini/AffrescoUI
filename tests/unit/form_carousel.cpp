/// @file
/// @brief Carousels (render's Carousel, format's CarouselType): one page shown at a time, turned by the arrows, the
/// dots, the keyboard (mirrored right to left) and a sideways swipe, with the page bound to state.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Carousel.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A carousel of three pages bound to state.page, focused.
constexpr const char* carouselScreen = R"json({
	"format": "affrescoui-screen", "version": 2, "state": { "page": 0 },
	"root": { "id": "root", "children": [{ "id": "shots", "type": "carousel", "bind": "state.page",
		"focusable": true, "autofocus": true, "width": 300, "height": 200, "children": [
		{ "id": "one", "color": "#ff0000" }, { "id": "two", "color": "#00ff00" }, { "id": "three", "color": "#0000ff" }
	] }] }
})json";

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const char* text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief Presses and releases a key.
	/// @param key The key.
	void press(affresco::Key key) {
		window->routeKey({key, 0, affresco::Action::Press, {}});
		window->routeKey({key, 0, affresco::Action::Release, {}});
		window->render();
	}

	/// @brief Clicks a point.
	/// @param at The point, in window units.
	void click(affresco::Vec2 at) {
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		window->render();
	}

	/// @brief The bound page.
	/// @return state.page.
	[[nodiscard]] json page() const { return json::parse(screen.stateJson())["page"]; }
};

} // namespace

TEST_CASE("carousel: only the current page is shown, and the keys turn pages within the ends") {
	Built built(carouselScreen);
	auto* carousel = dynamic_cast<affresco::Carousel*>(built.screen.find("shots"));
	REQUIRE(carousel);
	CHECK(carousel->pageCount() == 3);
	CHECK(built.screen.find("one")->frame().width() > 0.0f);
	CHECK(built.screen.find("one")->frame().left >= carousel->frame().left);
	built.press(affresco::Key::Right);
	CHECK(built.page() == 1);
	built.press(affresco::Key::End);
	CHECK(built.page() == 2);
	built.press(affresco::Key::Right); // no loop: stays on the last
	CHECK(built.page() == 2);
	built.press(affresco::Key::Home);
	CHECK(built.page() == 0);
}

TEST_CASE("carousel: right to left, Left goes forward") {
	Built built(carouselScreen);
	built.screen.find("shots")->setWritingDirection(affresco::WritingDirection::RightToLeft);
	built.window->render();
	built.press(affresco::Key::Left);
	CHECK(built.page() == 1);
}

TEST_CASE("carousel: the arrows and the dots turn pages, and loop wraps around") {
	Built built(carouselScreen);
	auto* carousel = dynamic_cast<affresco::Carousel*>(built.screen.find("shots"));
	REQUIRE(carousel);
	const affresco::Rect frame = carousel->frame();
	const float middle = (frame.top + frame.bottom - affresco::Carousel::strip) / 2.0f;
	CHECK(carousel->controlAt({frame.right - 4.0f, middle}) == 1);
	built.click({frame.right - 4.0f, middle});
	CHECK(built.page() == 1);
	built.click({frame.left + 4.0f, middle});
	CHECK(built.page() == 0);
	carousel->setLoop(true);
	carousel->turn(-1);
	CHECK(carousel->index() == 2);
}

TEST_CASE("carousel: a state change shows that page, and a sideways swipe turns it") {
	Built built(carouselScreen);
	auto* carousel = dynamic_cast<affresco::Carousel*>(built.screen.find("shots"));
	REQUIRE(carousel);
	carousel->setIndex(2);
	CHECK(carousel->index() == 2);
	carousel->setIndex(99); // past the end: the last page, once laid out
	built.window->render();
	CHECK(carousel->index() == 2);
	carousel->setIndex(0);
	const affresco::Rect frame = carousel->frame();
	built.window->routeScroll({{-80.0f, 0.0f}, {(frame.left + frame.right) / 2.0f, frame.top + 20.0f}});
	built.window->render();
	CHECK(carousel->index() == 1);
}
