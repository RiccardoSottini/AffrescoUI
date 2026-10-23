/// @file
/// @brief Splits and tabs on a headless window: a split's panes, minimums, sashes dragged, double-clicked and moved by
/// keyboard; a tab strip's picking, closing, reordering, keyboard and scrolling; and both as screen nodes, their
/// values bound to state and their events run.
///
/// A headless window has no default font, so a tab is only its padding and close button: 46 pixels wide.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Split.hpp>
#include <affresco/render/Tabs.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::json;
using affresco::Key;
using affresco::MouseButton;
using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::Split;
using affresco::Tab;
using affresco::TabMove;
using affresco::Tabs;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr float tabWidth = 46.0f; ///< A closable tab with no font: padding, the close button's gap and side, padding.

/// @brief A split filling a window with some panes.
struct SplitFixture {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< 200 by 100.
	Split* split = nullptr;                   ///< The split.
	std::vector<Panel*> panes;                ///< Its panes.
	std::vector<std::vector<float>> changes;  ///< Every onSizesChanged, oldest first.

	/// @brief Builds it.
	/// @param count How many panes.
	/// @param direction Its axis.
	explicit SplitFixture(std::size_t count, Split::Direction direction = Split::Direction::Row)
	    : window(createHeadlessWindow(200, 100, clock)) {
		split = &window->root().add<Split>(direction);
		split->setSize(px(200), px(100));
		for(std::size_t i = 0; i < count; ++i) panes.push_back(&split->add<Panel>());
		split->onSizesChanged.connect([this](const std::vector<float>& sizes) { changes.push_back(sizes); });
		window->render();
	}

	/// @brief Presses and releases the left button somewhere.
	/// @param position Where.
	void click(Vec2 position) const {
		window->routeMouseButton({MouseButton::Left, Action::Press, {}, position});
		window->routeMouseButton({MouseButton::Left, Action::Release, {}, position});
		window->render();
	}

	/// @brief Drags with the left button.
	/// @param from Where it goes down.
	/// @param to Where it goes up.
	void drag(Vec2 from, Vec2 to) const {
		window->routeMouseButton({MouseButton::Left, Action::Press, {}, from});
		window->routeMouseMove(to);
		window->routeMouseButton({MouseButton::Left, Action::Release, {}, to});
		window->render();
	}

	/// @brief Presses a key on the focused panel.
	/// @param key The key.
	void press(Key key) const {
		window->routeKey({key, 0, Action::Press, {}});
		window->render();
	}
};

/// @brief A strip of tabs a, b, c (and more), 'a' active, along the top of a window.
struct TabsFixture {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< 300 by 100.
	Tabs* tabs = nullptr;                     ///< The strip, 32 high.
	std::vector<std::string> activated;       ///< Every onActivated, oldest first.
	std::vector<std::string> closed;          ///< Every onCloseRequested.
	std::vector<TabMove> moved;               ///< Every onMoved.

	/// @brief Builds it.
	/// @param ids The tabs' ids.
	/// @param width The strip's width.
	explicit TabsFixture(const std::vector<std::string>& ids, float width = 300.0f)
	    : window(createHeadlessWindow(300, 100, clock)) {
		REQUIRE_FALSE(window->defaultFont()); // the widths below
		tabs = &window->root().add<Tabs>();
		tabs->setSize(px(width), px(32));
		std::vector<Tab> list;
		for(const std::string& id : ids) list.push_back(Tab{id, id});
		tabs->setTabs(list);
		tabs->setActive("a");
		tabs->onActivated.connect([this](const std::string& id) { activated.push_back(id); });
		tabs->onCloseRequested.connect([this](const std::string& id) { closed.push_back(id); });
		tabs->onMoved.connect([this](const TabMove& move) { moved.push_back(move); });
		window->render();
	}

	/// @brief The middle of a tab's label, away from its close button.
	/// @param index The tab.
	/// @return The point.
	[[nodiscard]] static Vec2 label(std::size_t index) { return {static_cast<float>(index) * tabWidth + 8.0f, 16.0f}; }
	/// @brief The middle of a tab's close button.
	/// @param index The tab.
	/// @return The point.
	[[nodiscard]] static Vec2 close(std::size_t index) {
		return {static_cast<float>(index + 1) * tabWidth - 20.0f, 16.0f};
	}
};

} // namespace

TEST_CASE("split: equal panes, given sizes, minimums, and the last pane taking the rest") {
	SplitFixture fixture(3);
	// 200 less two 4-pixel sashes, shared equally
	CHECK(fixture.split->sizes() == std::vector<float>{64.0f, 64.0f, 64.0f});
	CHECK(fixture.panes[0]->frame() == Rect{0, 0, 64, 100});
	CHECK(fixture.panes[1]->frame() == Rect{68, 0, 132, 100});
	CHECK(fixture.panes[2]->frame() == Rect{136, 0, 200, 100});

	fixture.split->setSizes({50.0f});
	fixture.window->render();
	CHECK(fixture.split->sizes() == std::vector<float>{50.0f, 71.0f, 71.0f});

	fixture.split->setMinimumSizes({60.0f});
	fixture.window->render();
	CHECK(fixture.split->sizes() == std::vector<float>{60.0f, 71.0f, 61.0f});

	// A pane too big for the room: the last keeps its minimum, the panes before it give way
	fixture.split->setSizes({150.0f, 30.0f});
	fixture.window->render();
	CHECK(fixture.split->sizes() == std::vector<float>{72.0f, 60.0f, 60.0f});
	CHECK(fixture.changes.empty()); // only the user's changes are reported
}

TEST_CASE("split: a sash drags within the minimums, and a double click resets the defaults") {
	SplitFixture fixture(2);
	fixture.split->setMinimumSizes({40.0f});
	fixture.split->setDefaultSizes({50.0f});
	fixture.window->render();
	REQUIRE(fixture.split->sizes() == std::vector<float>{50.0f, 146.0f});

	fixture.drag({51.0f, 50.0f}, {81.0f, 50.0f});
	CHECK(fixture.split->sizes() == std::vector<float>{80.0f, 116.0f});
	REQUIRE(fixture.changes.size() == 1);
	CHECK(fixture.changes.back() == std::vector<float>{80.0f, 116.0f});

	// Far past the end (a second later, so not a double click): the second pane keeps its minimum
	fixture.clock.advance(1000.0);
	fixture.drag({81.0f, 50.0f}, {400.0f, 50.0f});
	CHECK(fixture.split->sizes() == std::vector<float>{156.0f, 40.0f});

	// Two presses apart are not a double click; two close together are
	fixture.clock.advance(1000.0);
	fixture.click({157.0f, 50.0f});
	fixture.clock.advance(1000.0);
	fixture.click({157.0f, 50.0f});
	CHECK(fixture.split->sizes() == std::vector<float>{156.0f, 40.0f});
	fixture.clock.advance(100.0);
	fixture.click({157.0f, 50.0f});
	CHECK(fixture.split->sizes() == std::vector<float>{50.0f, 146.0f});
	CHECK(fixture.changes.back() == std::vector<float>{50.0f, 146.0f});
}

TEST_CASE("split: arrow keys move the focused split's sash, Page keys pick one, Home resets") {
	SplitFixture fixture(3, Split::Direction::Column);
	// 100 less two sashes: 92, the last pane takes the rest
	REQUIRE(fixture.split->sizes() == std::vector<float>{92.0f / 3.0f, 92.0f / 3.0f, 92.0f - 2.0f * 92.0f / 3.0f});
	fixture.split->setSizes({30.0f, 30.0f});
	fixture.window->render();
	CHECK(fixture.panes[1]->frame() == Rect{0, 34, 200, 64});
	fixture.window->setFocus(fixture.split);

	fixture.press(Key::Down);
	CHECK(fixture.split->sizes() == std::vector<float>{38.0f, 22.0f, 32.0f});
	fixture.press(Key::Right); // the cross axis: nothing
	CHECK(fixture.split->sizes() == std::vector<float>{38.0f, 22.0f, 32.0f});
	fixture.press(Key::PageDown);
	fixture.press(Key::Up);
	CHECK(fixture.split->sizes() == std::vector<float>{38.0f, 14.0f, 40.0f});
	fixture.press(Key::Home);
	CHECK(fixture.split->sizes()[0] == doctest::Approx(92.0f / 3.0f));
	CHECK(fixture.changes.size() == 3);
}

TEST_CASE("tabs: a click picks a tab, its close button asks to close it, and keys move and close") {
	TabsFixture fixture({"a", "b", "c"});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(1)});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Release, {}, TabsFixture::label(1)});
	CHECK(fixture.tabs->active() == "b");
	CHECK(fixture.activated == std::vector<std::string>{"b"});

	fixture.window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::close(2)});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Release, {}, TabsFixture::close(2)});
	CHECK(fixture.closed == std::vector<std::string>{"c"});
	CHECK(fixture.tabs->active() == "b"); // closing doesn't pick it

	fixture.window->setFocus(fixture.tabs);
	fixture.window->routeKey({Key::Right, 0, Action::Press, {}});
	CHECK(fixture.tabs->active() == "c");
	fixture.window->routeKey({Key::Right, 0, Action::Press, {}}); // the last stays
	fixture.window->routeKey({Key::Home, 0, Action::Press, {}});
	CHECK(fixture.tabs->active() == "a");
	fixture.window->routeKey({Key::End, 0, Action::Press, {}});
	fixture.window->routeKey({Key::Delete, 0, Action::Press, {}});
	CHECK(fixture.activated == std::vector<std::string>{"b", "c", "a", "c"});
	CHECK(fixture.closed == std::vector<std::string>{"c", "c"});

	// A tab that can't close has no close button
	fixture.tabs->setTabs({Tab{"a", "a"}, Tab{"b", "b", false, false}});
	fixture.window->render();
	CHECK(fixture.tabs->tabAt(TabsFixture::label(1)) == 1);
	CHECK(fixture.tabs->tabAt({300.0f, 16.0f}) == 2);
}

TEST_CASE("tabs: dragging a tab along the strip moves it, and off the strip doesn't") {
	TabsFixture fixture({"a", "b", "c"});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(0)});
	fixture.window->routeMouseMove({130.0f, 16.0f});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Release, {}, {130.0f, 16.0f}});
	REQUIRE(fixture.moved.size() == 1);
	CHECK(fixture.moved[0].id == "a");
	CHECK(fixture.moved[0].from == 0);
	CHECK(fixture.moved[0].to == 2); // past c's middle, counted without a

	// A small wobble is a click, not a drag
	fixture.window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(2)});
	fixture.window->routeMouseMove({TabsFixture::label(2).x + 2.0f, 16.0f});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Release, {}, TabsFixture::label(2)});
	CHECK(fixture.moved.size() == 1);
	CHECK(fixture.tabs->active() == "c");

	fixture.window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(2)});
	fixture.window->routeMouseMove({20.0f, 80.0f});
	fixture.window->routeMouseButton({MouseButton::Left, Action::Release, {}, {20.0f, 80.0f}});
	CHECK(fixture.moved.size() == 1);

	CHECK(fixture.tabs->insertionAt({0.0f, 16.0f}) == 0);
	CHECK(fixture.tabs->insertionAt({tabWidth + 30.0f, 16.0f}) == 2);
	CHECK(fixture.tabs->insertionAt({299.0f, 16.0f}) == 3);
}

TEST_CASE("tabs: a strip too narrow scrolls, and keeps the active tab in view") {
	TabsFixture fixture({"a", "b", "c", "d", "e"}, 100.0f);
	CHECK(fixture.tabs->scrollPosition() == 0.0f);
	fixture.tabs->setActive("e");
	fixture.window->render();
	CHECK(fixture.tabs->scrollPosition() == 5.0f * tabWidth - 100.0f);
	CHECK(fixture.tabs->tabAt({99.0f, 16.0f}) == 4);

	fixture.window->routeScroll({{0.0f, 100.0f}, {50.0f, 16.0f}});
	CHECK(fixture.tabs->scrollPosition() == 0.0f);
	CHECK(fixture.tabs->tabAt({1.0f, 16.0f}) == 0);
	fixture.window->routeScroll({{0.0f, 100.0f}, {50.0f, 16.0f}}); // already at the start
	CHECK(fixture.tabs->scrollPosition() == 0.0f);
}

TEST_CASE("split and tabs nodes: sizes and the active tab bound to state, close and move events, bad keys refused") {
	ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"state": { "sizes": [100], "open": [{ "id": "a" }, { "id": "b" }, { "id": "c" }], "active": "a", "moves": 0 },
		"root": { "id": "root", "children": [
			{ "type": "tabs", "id": "strip", "width": 300, "height": 32, "tabs": "{{state.open}}", "bind": "state.active",
			  "onClose": [{ "set": "state.open", "to": "{{filter(state.open, it.id != event.id)}}" }],
			  "onMove": [{ "set": "state.moves", "to": "{{event.to}}" }] },
			{ "type": "split", "id": "panes", "y": 40, "width": 300, "height": 160, "bind": "state.sizes",
			  "minSize": 50, "sashSize": 4, "children": [{ "id": "left" }, { "id": "right" }] }
		] }
	})");
	screen.build(*window);
	window->render();
	const auto state = [&screen] { return json::parse(screen.stateJson()); };

	auto* split = dynamic_cast<Split*>(screen.find("panes"));
	REQUIRE(split);
	CHECK(split->sizes() == std::vector<float>{100.0f, 196.0f});
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {101.0f, 100.0f}});
	window->routeMouseMove({121.0f, 100.0f});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {121.0f, 100.0f}});
	window->render();
	CHECK(state()["sizes"] == json{120, 176});

	auto* tabs = dynamic_cast<Tabs*>(screen.find("strip"));
	REQUIRE(tabs);
	REQUIRE(tabs->tabs().size() == 3);
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(1)});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, TabsFixture::label(1)});
	window->render();
	CHECK(state()["active"] == "b");

	window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::close(0)});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, TabsFixture::close(0)});
	window->render();
	CHECK(state()["open"] == json::parse(R"([{ "id": "b" }, { "id": "c" }])"));
	CHECK(tabs->tabs().size() == 2);

	window->routeMouseButton({MouseButton::Left, Action::Press, {}, TabsFixture::label(0)});
	window->routeMouseMove({90.0f, 16.0f});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {90.0f, 16.0f}});
	window->render();
	CHECK(state()["moves"] == 1);

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#panes/direction": "diagonal"}})", &error));
	CHECK(error.find("direction") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#strip/tabs": [{ "label": "no id" }]}})", &error));
	CHECK(error.find("id") != std::string::npos);
	CHECK_FALSE(tabs->accentOnTop());
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#strip/accentEdge": "left"}})", &error));
	CHECK(error.find("accentEdge") != std::string::npos);
	REQUIRE(screen.applyRevision(R"({"set": {"#strip/accentEdge": "top"}})", &error));
	window->render();
	CHECK(tabs->accentOnTop());
	REQUIRE(screen.applyRevision(R"({"set": {"#panes/direction": "column"}})", &error));
	window->render();
	CHECK(split->direction() == Split::Direction::Column);
}
