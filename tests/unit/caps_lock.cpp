/// @file
/// @brief Caps lock (render's Window, src/Events.cpp): a key event's lock state reaches `window.capsLock`, which
/// bindings follow when it changes, and `event.capsLock` on `onKey`; scenarios press keys with it as `capslock+`.

#include "Headless.hpp"
#include "Internal.hpp" // automation's parseKey

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

namespace {

/// @brief A screen with a field that keeps its keys' lock state, and a warning shown while caps lock is on.
constexpr const char* capsLockScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "locked": null },
	"root": { "id": "root", "children": [
		{ "id": "field", "type": "textarea", "width": 200, "height": 30, "autofocus": true,
		  "onKey": [{ "set": "state.locked", "to": "{{event.capsLock}}" }] },
		{ "id": "warning", "type": "label", "y": 40, "width": 200, "height": 20,
		  "text": "{{window.capsLock ? 'on' : 'off'}}" }
	] }
})json";

/// @brief A label's text.
/// @param screen The screen.
/// @param id The label's id.
/// @return Its text.
std::string text(affresco::Screen& screen, const std::string& id) {
	auto* label = dynamic_cast<affresco::Label*>(screen.find(id));
	REQUIRE(label);
	return label->text();
}

/// @brief Presses and releases a key.
/// @param window The window.
/// @param key The key.
/// @param mods The modifier bits.
void press(affresco::Window& window, affresco::Key key, int mods) {
	window.routeKey({key, 0, affresco::Action::Press, affresco::Mods{mods}});
	window.routeKey({key, 0, affresco::Action::Release, affresco::Mods{mods}});
	window.render();
}

} // namespace

TEST_CASE("format: window.capsLock and event.capsLock follow the lock state of the latest key") {
	ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(capsLockScreen);
	screen.build(*window);
	window->render();
	CHECK_FALSE(window->capsLock());
	CHECK(text(screen, "warning") == "off");

	press(*window, affresco::Key::A, 0x10);
	CHECK(window->capsLock());
	CHECK(text(screen, "warning") == "on");
	CHECK(json::parse(screen.stateJson())["locked"] == true);

	press(*window, affresco::Key::A, 0x1); // shift, without the lock
	CHECK_FALSE(window->capsLock());
	CHECK(text(screen, "warning") == "off");
	CHECK(json::parse(screen.stateJson())["locked"] == false);
}

TEST_CASE("automation: \"capslock+\" presses a key with caps lock on") {
	const auto press = affresco::automation::detail::parseKey("capslock+shift+a");
	REQUIRE(press);
	CHECK(press->key == affresco::Key::A);
	CHECK(press->mods.capsLock());
	CHECK(press->mods.shift());
	CHECK_FALSE(affresco::automation::detail::parseKey("capslock+")); // a modifier alone is no key
}

// NOLINTEND(bugprone-unchecked-optional-access)
