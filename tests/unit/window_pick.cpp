/// @file
/// @brief Unit tests for Window::pick(): the person points at a panel in a mode the window owns, the screen hearing no
/// input meanwhile; the mouse, the keys of ACCESSIBILITY.md ("Pointing with the keyboard"), what ends it, and what
/// it draws.

#include "Headless.hpp"
#include "window/Picking.hpp"

#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using affresco::Action;
using affresco::Confirmation;
using affresco::Key;
using affresco::KeyEvent;
using affresco::LiveRegion;
using affresco::Mods;
using affresco::MouseButton;
using affresco::MouseButtonEvent;
using affresco::Panel;
using affresco::PickOptions;
using affresco::px;
using affresco::ScrollEvent;
using affresco::Vec2;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::detail::PickLayer;

namespace {

constexpr int shiftKey = 0x0001; ///< Mods::shift()'s bit.

/// @brief A 400 by 300 headless window holding two columns, a (left) and b (right), each with cards:
/// a1 and a2 in a, b1 in b. Every panel counts the clicks, keys and moves it hears.
struct Fixture {
	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(400, 300, clock); ///< The window.
	Panel* a = nullptr;                                                     ///< The left column.
	Panel* a1 = nullptr;                                                    ///< Its first card.
	Panel* a2 = nullptr;                                                    ///< Its second card.
	Panel* b = nullptr;                                                     ///< The right column.
	Panel* b1 = nullptr;                                                    ///< Its card.
	int clicks = 0;                                                         ///< Buttons any panel heard.
	int keys = 0;                                                           ///< Keys any panel heard.
	int moves = 0;                                                          ///< Moves any panel heard.
	int windowSignals = 0;       ///< The window's onKey, onChar, onMouseMove, onMouseButton, onScroll and onDrop.
	std::vector<Panel*> results; ///< What each pick's callback got.

	/// @brief Builds the tree and lays it out.
	Fixture() {
		window->root().setId("root");
		a = &add(window->root(), "a", 0.0f, 0.0f, 200.0f, 300.0f);
		a1 = &add(*a, "a1", 10.0f, 2.0f, 80.0f, 40.0f); // at the top: no room for a caption above it
		a2 = &add(*a, "a2", 10.0f, 60.0f, 80.0f, 40.0f);
		b = &add(window->root(), "b", 200.0f, 0.0f, 200.0f, 300.0f);
		b1 = &add(*b, "b1", 10.0f, 10.0f, 80.0f, 40.0f);
		window->onKey.connect([this](const KeyEvent&) { ++windowSignals; });
		window->onChar.connect([this](const affresco::CharEvent&) { ++windowSignals; });
		window->onMouseMove.connect([this](const affresco::MouseMoveEvent&) { ++windowSignals; });
		window->onMouseButton.connect([this](const MouseButtonEvent&) { ++windowSignals; });
		window->onScroll.connect([this](const ScrollEvent&) { ++windowSignals; });
		window->onDrop.connect([this](const affresco::DropEvent&) { ++windowSignals; });
		window->render();
	}

	/// @brief Adds a counting panel.
	/// @param parent Its parent.
	/// @param id Its id.
	/// @param x Its left, in its parent.
	/// @param y Its top, in its parent.
	/// @param width Its width.
	/// @param height Its height.
	/// @return It.
	Panel& add(Panel& parent, const std::string& id, float x, float y, float width, float height) {
		auto& panel = parent.add<Panel>();
		panel.setId(id);
		panel.setPosition(px(x), px(y));
		panel.setSize(px(width), px(height));
		panel.onMouseButton.connect([this](const MouseButtonEvent&) { ++clicks; });
		panel.onKey.connect([this](const KeyEvent&) { ++keys; });
		panel.onMouseMove.connect([this](const affresco::MouseMoveEvent&) { ++moves; });
		return panel;
	}

	/// @brief Starts a pick whose callback records what it got.
	/// @param options Its options.
	/// @return Its id.
	std::uint64_t start(PickOptions options = {}) {
		return window->pick(std::move(options), [this](Panel* picked) { results.push_back(picked); });
	}

	/// @brief Presses and lets go of a button at a point.
	/// @param at The point.
	/// @param button The button.
	void click(Vec2 at, MouseButton button = MouseButton::Left) const {
		window->routeMouseButton({button, Action::Press, Mods{}, at});
		window->routeMouseButton({button, Action::Release, Mods{}, at});
	}

	/// @brief Presses and lets go of a key.
	/// @param key The key.
	/// @param mods The modifiers.
	void type(Key key, int mods = 0) const {
		window->routeKey({key, 0, Action::Press, Mods{mods}});
		window->routeKey({key, 0, Action::Release, Mods{mods}});
	}

	/// @brief The pick's layer, if it is in what is drawn.
	[[nodiscard]] const PickLayer* layer() const {
		const affresco::detail::Frame frame = affresco::detail::takeFrame(*window);
		for(Panel* drawn : frame.overlays)
			if(const auto* found = dynamic_cast<const PickLayer*>(drawn)) return found;
		return nullptr;
	}

	/// @brief The panel offered, as the caption names it (each test's caption gives the id).
	/// @return It, or nullptr.
	[[nodiscard]] Panel* offeredById() const {
		const PickLayer* found = layer();
		if(!found || !found->caption().isVisible()) return nullptr;
		return window->root().findById(found->caption().text());
	}
};

/// @brief Options whose caption is the panel's id, so a test reads what is offered (Fixture::offeredById()).
/// @return The options.
PickOptions captioned() {
	PickOptions options;
	options.caption = [](const Panel& panel) { return panel.id(); };
	return options;
}

} // namespace

TEST_CASE("pick: a click picks the panel under it, and no panel or window signal hears the click") {
	Fixture f;
	f.window->setFocus(f.b1);
	CHECK(f.start() != 0);
	CHECK(f.window->picking());
	f.click({30.0f, 20.0f});
	CHECK_FALSE(f.window->picking());
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == f.a1);
	CHECK(f.clicks == 0);
	CHECK(f.windowSignals == 0);
	CHECK(f.window->focusedPanel() == f.b1); // the focus didn't move
	CHECK(f.window->mouseGrabber() == nullptr);
}

TEST_CASE("pick: a click that ends on another panel than it began on picks nothing") {
	Fixture f;
	f.start();
	f.window->routeMouseButton({MouseButton::Left, Action::Press, Mods{}, {30.0f, 20.0f}});
	f.window->routeMouseButton({MouseButton::Left, Action::Release, Mods{}, {30.0f, 80.0f}});
	CHECK(f.window->picking());
	CHECK(f.results.empty());
}

TEST_CASE("pick: the caller's candidate maps a hit panel to the panel offered") {
	Fixture f;
	PickOptions options = captioned();
	options.candidate = [&f](Panel* panel) -> Panel* { // the columns only
		for(; panel; panel = panel->parent())
			if(panel == f.a || panel == f.b) return panel;
		return nullptr;
	};
	f.start(std::move(options));
	f.window->routeMouseMove({30.0f, 20.0f});
	CHECK(f.offeredById() == f.a);
	f.click({230.0f, 20.0f});
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == f.b);
}

TEST_CASE("pick: Escape and a right click cancel, with done(nullptr) once, and nothing of them reaches a panel") {
	Fixture f;
	f.start();
	f.type(Key::Escape);
	CHECK_FALSE(f.window->picking());
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == nullptr);
	f.start();
	f.window->routeMouseButton({MouseButton::Right, Action::Press, Mods{}, {30.0f, 20.0f}});
	CHECK(f.window->picking()); // on the release
	f.window->routeMouseButton({MouseButton::Right, Action::Release, Mods{}, {30.0f, 20.0f}});
	CHECK_FALSE(f.window->picking());
	REQUIRE(f.results.size() == 2);
	CHECK(f.results[1] == nullptr);
	CHECK(f.clicks == 0);
	CHECK(f.keys == 0);
}

TEST_CASE("pick: the key that started the mode can't end it") {
	Fixture f;
	f.window->routeKey({Key::Enter, 0, Action::Press, Mods{}}); // a binding starts a pick on this press
	f.start();
	f.window->routeKey({Key::Enter, 0, Action::Release, Mods{}});
	f.window->routeKey({Key::Escape, 0, Action::Release, Mods{}}); // released, never pressed while picking
	CHECK(f.window->picking());
	CHECK(f.results.empty());
}

TEST_CASE("pick: the arrow keys and Tab walk the panels offered, and Enter picks") {
	Fixture f;
	PickOptions options = captioned();
	options.start = f.a;
	f.start(std::move(options));
	CHECK(f.offeredById() == f.a);
	f.type(Key::Down);
	CHECK(f.offeredById() == f.a1);
	f.type(Key::Right);
	CHECK(f.offeredById() == f.a2);
	f.type(Key::Right); // the last sibling: it stays
	CHECK(f.offeredById() == f.a2);
	f.type(Key::Left);
	CHECK(f.offeredById() == f.a1);
	f.type(Key::Up);
	CHECK(f.offeredById() == f.a);
	f.type(Key::Up); // the root, which is offered too
	CHECK(f.offeredById() == &f.window->root());
	f.type(Key::Down);
	CHECK(f.offeredById() == f.a);
	f.type(Key::Right);
	CHECK(f.offeredById() == f.b);
	f.type(Key::Tab); // tree order: root, a, a1, a2, b, b1
	CHECK(f.offeredById() == f.b1);
	f.type(Key::Tab); // wraps
	CHECK(f.offeredById() == &f.window->root());
	f.type(Key::Tab, shiftKey);
	CHECK(f.offeredById() == f.b1);
	CHECK(f.keys == 0);
	CHECK(f.windowSignals == 0);
	f.window->routeKey({Key::Enter, 0, Action::Press, Mods{}});
	CHECK(f.results.empty()); // on the release
	f.window->routeKey({Key::Enter, 0, Action::Release, Mods{}});
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == f.b1);
}

TEST_CASE("pick: the keyboard starts on the focused panel, else the first offered in tree order") {
	Fixture f;
	PickOptions options = captioned();
	options.candidate = [&f](Panel* panel) { return panel == &f.window->root() ? nullptr : panel; };
	f.window->setFocus(f.a2);
	f.start(options);
	CHECK(f.offeredById() == f.a2);
	f.window->cancelPick(1);
	f.window->setFocus(nullptr);
	f.start(options);
	CHECK(f.offeredById() == f.a);
}

TEST_CASE("pick: a key pressKey() presses for a screen can't pick") {
	Fixture f;
	PickOptions options;
	options.start = f.a1;
	f.start(std::move(options));
	f.window->pressKey(*f.a1, KeyEvent{Key::Enter, 0, Action::Press, Mods{}}, 'a');
	CHECK(f.window->picking());
	CHECK(f.results.empty());
	// Regression: the key reached the panel, though no panel gets a key while the person points
	CHECK(f.keys == 0);
}

TEST_CASE("pick: a candidate destroyed mid-pick is dropped, and the next move finds one") {
	Fixture f;
	f.start(captioned());
	f.window->routeMouseMove({30.0f, 20.0f});
	CHECK(f.offeredById() == f.a1);
	f.window->routeMouseButton({MouseButton::Left, Action::Press, Mods{}, {30.0f, 20.0f}});
	f.a->moveChild(0, 1); // a1 last, then gone
	f.a->removeChildren(1);
	f.a1 = nullptr;
	CHECK(f.offeredById() == nullptr);
	f.window->routeMouseButton({MouseButton::Left, Action::Release, Mods{}, {30.0f, 20.0f}});
	CHECK(f.window->picking()); // the press was on a panel gone: nothing is picked
	f.window->routeMouseMove({30.0f, 20.0f});
	CHECK(f.offeredById() == f.a);
}

TEST_CASE("pick: a panel picked but destroyed before the callback runs comes as nullptr") {
	Fixture f;
	f.start();
	f.window->routeMouseButton({MouseButton::Left, Action::Press, Mods{}, {30.0f, 20.0f}});
	f.window->defer([&f] { // queued before the pick's callback: a1 goes
		f.a->moveChild(0, 1);
		f.a->removeChildren(1);
	});
	f.window->routeMouseButton({MouseButton::Left, Action::Release, Mods{}, {30.0f, 20.0f}});
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == nullptr);
}

TEST_CASE("pick: a question wins; one pick at a time") {
	Fixture f;
	CHECK(f.start() != 0);
	CHECK(f.start() == 0); // one is running
	const std::uint64_t question = f.window->confirm(Confirmation{"Sure?", "", "", "OK", "Cancel"}, [](bool) {});
	CHECK(question != 0);
	f.window->runDeferredTasks();
	CHECK_FALSE(f.window->picking());
	REQUIRE(f.results.size() == 1);
	CHECK(f.results[0] == nullptr);
	CHECK(f.start() == 0); // a question is shown
	f.window->dismissConfirmation(question);
	CHECK(f.start() != 0);
}

TEST_CASE("pick: cancelPick() and closing the window end it without the callback") {
	Fixture f;
	const std::uint64_t id = f.start();
	f.window->cancelPick(id + 1); // not it
	CHECK(f.window->picking());
	f.window->cancelPick(id);
	CHECK_FALSE(f.window->picking());
	f.start();
	f.window->setShouldClose(true);
	CHECK_FALSE(f.window->picking());
	f.window->runDeferredTasks();
	CHECK(f.results.empty());
}

TEST_CASE("pick: cancelPick() after the pick ended, before its callback ran, stops the callback") {
	// Regression: once the pick ended, cancelPick() found nothing to cancel, and the callback still ran
	Fixture f;
	const std::uint64_t id = f.start();
	f.window->confirm(Confirmation{"Sure?", "", "", "OK", "Cancel"}, [](bool) {}); // ends the pick, outside an event
	CHECK_FALSE(f.window->picking());
	f.window->cancelPick(id);
	f.window->runDeferredTasks();
	CHECK(f.results.empty());
}

TEST_CASE("pick: its layer is drawn above the overlays and the inspection layer, and can be hidden") {
	Fixture f;
	auto& menu = f.b1->add<Panel>();
	menu.setOverlay("", affresco::Placement::Below);
	menu.setSize(px(50.0f), px(50.0f));
	f.window->setInspection(std::make_unique<Panel>());
	f.window->render();
	CHECK(f.layer() == nullptr);
	f.start(captioned());
	const affresco::detail::Frame frame = affresco::detail::takeFrame(*f.window);
	const std::vector<Panel*> drawn(frame.overlays.begin(), frame.overlays.end());
	const auto at = [&drawn](const Panel* panel) { return std::ranges::find(drawn, panel) - drawn.begin(); };
	REQUIRE(f.layer() != nullptr);
	CHECK(at(&menu) < at(f.layer()));
	CHECK(at(f.window->inspection()) < at(f.layer()));
	CHECK(f.layer() == drawn.back());                 // nothing above it while no question is shown
	CHECK(f.window->panelAt({30.0f, 20.0f}) == f.a1); // never hit
	f.window->setPickLayerVisible(false);
	CHECK(f.layer() == nullptr);
	CHECK(f.window->picking());
	f.window->setPickLayerVisible(true);
	CHECK(f.layer() != nullptr);
}

TEST_CASE("pick: the caption names the panel offered, beside it, and is a polite live region") {
	Fixture f;
	PickOptions options = captioned();
	options.hint = "Point at something";
	f.start(std::move(options));
	f.window->routeMouseMove({30.0f, 80.0f});
	const PickLayer* layer = f.layer();
	REQUIRE(layer != nullptr);
	CHECK(layer->caption().text() == "a2");
	CHECK(layer->caption().accessibleState().live == LiveRegion::Polite);
	CHECK(layer->hint().isVisible());
	CHECK(layer->hint().text() == "Point at something");
	CHECK(layer->highlight().frame().top == doctest::Approx(f.a2->frame().top));
	CHECK(layer->caption().frame().bottom <= f.a2->frame().top); // room above it
	f.window->routeMouseMove({30.0f, 20.0f});
	CHECK(f.layer()->caption().frame().top >= f.a1->frame().bottom); // none above it: below
}

TEST_CASE("pick: the hover stays as it was, and no panel hears a move, a key or a character") {
	Fixture f;
	f.window->routeMouseMove({30.0f, 20.0f});
	REQUIRE(f.window->hoveredPanel() == f.a1);
	f.window->setFocus(f.a1);
	f.moves = 0;
	f.windowSignals = 0;
	f.start();
	f.window->routeMouseMove({230.0f, 20.0f});
	f.window->routeCursorEnter(false);
	CHECK(f.window->hoveredPanel() == f.a1);
	CHECK(f.moves == 0);
	f.type(Key::A);
	f.window->routeChar({U'a'});
	f.window->routeDrop({{"file.txt"}, {30.0f, 20.0f}});
	CHECK(f.keys == 0);
	CHECK(f.windowSignals == 0);
}

TEST_CASE("pick: a scroll still scrolls, no handler hears it, and the panel under the pointer is offered again") {
	Fixture f;
	auto& list = f.b->add<Panel>();
	list.setId("list");
	list.setPosition(px(0.0f), px(100.0f));
	list.setSize(px(200.0f), px(100.0f));
	list.setScrollable(false, true);
	auto& tall = list.add<Panel>();
	tall.setId("tall");
	tall.setSize(px(200.0f), px(1000.0f));
	int scrolls = 0;
	list.onScroll.connect([&scrolls](const ScrollEvent&) { ++scrolls; });
	f.window->render();
	f.start(captioned());
	f.window->routeScroll({{0.0f, -1.0f}, {250.0f, 150.0f}});
	CHECK(list.scrollOffset().y > 0.0f);
	CHECK(scrolls == 0);
	CHECK(f.windowSignals == 0);
	CHECK(f.offeredById() == &tall);
}
