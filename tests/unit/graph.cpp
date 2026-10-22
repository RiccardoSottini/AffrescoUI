/// @file
/// @brief Unit tests for the node graph: the link rules (kinds, duplicates, full ports, cycles, reconnecting), the
/// model and the camera, GraphView's pointer and keyboard edits, and the format's `graph` node with its handlers.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/GraphView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::checkLink;
using affresco::GraphCamera;
using affresco::GraphEvent;
using affresco::GraphLink;
using affresco::GraphModel;
using affresco::GraphNode;
using affresco::GraphPort;
using affresco::GraphView;
using affresco::json;
using affresco::Key;
using affresco::kindsMatch;
using affresco::LinkVerdict;
using affresco::Mods;
using affresco::MouseButton;
using affresco::px;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A small material graph: a texture and a colour feed a mix, which feeds the output.
/// @return The model: tex (out: color), tint (out: color), mix (a: color, takes 1; b: any; out: color),
/// output (surface: shader); linked tex.out to mix.a.
GraphModel material() {
	GraphModel model;
	model.nodes.push_back({"tex", "Texture", {0.0f, 0.0f}, 120.0f, 0.0f, {}, {{"out", "", "color", 0}}});
	model.nodes.push_back({"tint", "Tint", {0.0f, 120.0f}, 120.0f, 0.0f, {}, {{"out", "", "color", 0}}});
	model.nodes.push_back({"mix",
	                       "Mix",
	                       {200.0f, 40.0f},
	                       120.0f,
	                       0.0f,
	                       {{"a", "", "color", 1}, {"b", "", "any", 0}},
	                       {{"out", "", "color", 0}}});
	model.nodes.push_back({"output", "Output", {400.0f, 40.0f}, 120.0f, 0.0f, {{"surface", "", "shader", 0}}, {}});
	model.links.push_back({"tex", "out", "mix", "a"});
	return model;
}

/// @brief A GraphView filling a 600 by 400 window, catching its edits.
struct GraphFixture {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< 600 by 400.
	GraphView* view = nullptr;                ///< The graph.
	std::vector<GraphEvent> events;           ///< Every onEdit, oldest first.

	/// @brief Opens the window with the material graph.
	GraphFixture() : window(createHeadlessWindow(600, 400, clock)) {
		view = &window->root().add<GraphView>();
		view->setSize(px(600), px(400));
		view->setModel(material());
		view->onEdit.connect([this](const GraphEvent& event) { events.push_back(event); });
		window->render();
	}

	/// @brief Where a port is on screen.
	/// @param node The node's index.
	/// @param port The port's index.
	/// @param output Whether it is an output.
	/// @return In window units.
	[[nodiscard]] Vec2 port(std::size_t node, std::size_t port, bool output) const {
		return view->camera().toView(view->portPoint(node, port, output));
	}

	/// @brief Drags with the left button.
	/// @param from Where it is pressed.
	/// @param to Where it is released.
	/// @param mods The modifier keys held.
	void drag(Vec2 from, Vec2 to, Mods mods = {}) const {
		window->routeMouseButton({MouseButton::Left, Action::Press, mods, from});
		window->routeMouseMove({(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f});
		window->routeMouseMove(to);
		window->routeMouseButton({MouseButton::Left, Action::Release, mods, to});
		window->render();
	}

	/// @brief Presses a key on the graph.
	/// @param key The key.
	/// @param mods The modifier keys held.
	void press(Key key, Mods mods = {}) const {
		window->routeKey({key, 0, Action::Press, mods});
		window->render();
	}
};

/// @brief A screen whose graph keeps its nodes and links in state, edited by its handlers.
constexpr const char* graphScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": {
		"nodes": [
			{ "id": "tex", "title": "Texture", "x": 0, "y": 0, "outputs": [{ "id": "out", "kind": "color" }] },
			{ "id": "mix", "title": "Mix", "x": 200, "y": 0,
			  "inputs": [{ "id": "a", "kind": "color", "links": 1 }, { "id": "s", "kind": "shader" }] }
		],
		"links": [],
		"moved": null
	},
	"root": { "id": "root", "children": [
		{ "type": "graph", "id": "graph", "width": 600, "height": 400, "nodes": "{{state.nodes}}",
		  "links": "{{state.links}}", "minimap": false,
		  "onConnect": [{ "set": "state.links", "to": "{{concat(state.links, [{ from: event.from, output: event.output, to: event.to, input: event.input }])}}" }],
		  "onDisconnect": [{ "set": "state.links", "to": "{{filter(state.links, index != event.index)}}" }],
		  "onMove": [{ "set": "state.moved", "to": "{{event}}" }] }
	] }
})json";

} // namespace

TEST_CASE("render: kinds match when equal or when either is any") {
	CHECK(kindsMatch("color", "color"));
	CHECK(kindsMatch("any", "shader"));
	CHECK(kindsMatch("color", "any"));
	CHECK_FALSE(kindsMatch("color", "shader"));
	CHECK_FALSE(kindsMatch("", "color"));
}

TEST_CASE("render: a link is checked for its ports, kinds, duplicates and room") {
	const GraphModel model = material();
	CHECK(checkLink(model, {"tint", "out", "mix", "b"}, false) == LinkVerdict::Allowed);         // color into any
	CHECK(checkLink(model, {"tint", "out", "nowhere", "a"}, false) == LinkVerdict::UnknownPort); // no such node
	CHECK(checkLink(model, {"tint", "nope", "mix", "b"}, false) == LinkVerdict::UnknownPort);    // no such output
	CHECK(checkLink(model, {"tint", "out", "mix", "out"}, false) ==
	      LinkVerdict::UnknownPort); // an output, not an input
	CHECK(checkLink(model, {"mix", "out", "output", "surface"}, false) == LinkVerdict::KindMismatch);
	CHECK(checkLink(model, {"tex", "out", "mix", "a"}, false) == LinkVerdict::Duplicate);
	CHECK(checkLink(model, {"tint", "out", "mix", "a"}, false) == LinkVerdict::Full); // a takes one link

	// An output that takes one link is full too
	GraphModel single = model;
	single.nodes[0].outputs[0].links = 1;
	CHECK(checkLink(single, {"tex", "out", "mix", "b"}, false) == LinkVerdict::Full);
}

TEST_CASE("render: cycles are refused unless allowed, a node linked to itself included") {
	GraphModel model = material();
	model.nodes.push_back({"loop", "Loop", {}, 120.0f, 0.0f, {{"in", "", "color", 0}}, {{"out", "", "color", 0}}});
	model.links.push_back({"mix", "out", "loop", "in"});
	CHECK(checkLink(model, {"loop", "out", "mix", "b"}, false) == LinkVerdict::Cycle); // mix -> loop -> mix
	CHECK(checkLink(model, {"loop", "out", "mix", "b"}, true) == LinkVerdict::Allowed);
	CHECK(checkLink(model, {"loop", "out", "loop", "in"}, false) == LinkVerdict::Cycle);
	CHECK(checkLink(model, {"loop", "out", "loop", "in"}, true) == LinkVerdict::Allowed);
	CHECK(checkLink(model, {"tint", "out", "loop", "in"}, false) == LinkVerdict::Allowed); // no path back
}

TEST_CASE("render: reconnecting leaves the replaced link out of the checks") {
	const GraphModel model = material();
	const GraphLink& old = model.links[0]; // tex.out -> mix.a, which takes one link
	CHECK(checkLink(model, {"tint", "out", "mix", "a"}, false) == LinkVerdict::Full);
	CHECK(checkLink(model, {"tint", "out", "mix", "a"}, false, &old) == LinkVerdict::Allowed);
	CHECK(checkLink(model, old, false, &old) == LinkVerdict::Allowed); // dropped back where it was
	CHECK(checkLink(model, {"tex", "out", "output", "surface"}, false, &old) == LinkVerdict::KindMismatch);
}

TEST_CASE("render: the model finds nodes by id") {
	const GraphModel model = material();
	CHECK(model.find("tex") == 0);
	CHECK(model.find("output") == 3);
	CHECK(model.find("missing") == model.nodes.size());
}

TEST_CASE("render: the camera maps graph and view points both ways and zooms about a point") {
	GraphCamera camera;
	camera.pan = {10.0f, 20.0f};
	camera.zoom = 2.0f;
	CHECK(camera.toView({5.0f, 5.0f}) == Vec2{20.0f, 30.0f});
	CHECK(camera.toGraph({20.0f, 30.0f}) == Vec2{5.0f, 5.0f});

	const Vec2 under = camera.toGraph({100.0f, 80.0f});
	camera.zoomAt({100.0f, 80.0f}, 3.0f);
	CHECK(camera.zoom == 3.0f);
	CHECK(camera.toGraph({100.0f, 80.0f}).x == doctest::Approx(under.x));
	CHECK(camera.toGraph({100.0f, 80.0f}).y == doctest::Approx(under.y));

	camera.zoomAt({}, 1000.0f);
	CHECK(camera.zoom == GraphCamera::maximumZoom);
	camera.zoomAt({}, 0.0f);
	CHECK(camera.zoom == GraphCamera::minimumZoom);
	camera.zoomAt({}, std::numeric_limits<float>::quiet_NaN());
	CHECK(camera.zoom == GraphCamera::minimumZoom);
}

TEST_CASE("render: dragging from an output to a compatible input connects; an incompatible one doesn't") {
	GraphFixture fixture;
	fixture.drag(fixture.port(1, 0, true), fixture.port(2, 1, false)); // tint.out -> mix.b
	REQUIRE(fixture.events.size() == 1);
	CHECK(fixture.events[0].kind == GraphEvent::Kind::Connect);
	CHECK(fixture.events[0].link == GraphLink{"tint", "out", "mix", "b"});

	fixture.events.clear();
	fixture.drag(fixture.port(2, 0, true), fixture.port(3, 0, false)); // color into shader: refused
	CHECK(fixture.events.empty());
	fixture.drag(fixture.port(1, 0, true), fixture.port(2, 0, false)); // mix.a is full: refused
	CHECK(fixture.events.empty());

	fixture.drag(fixture.port(1, 0, true), {300.0f, 340.0f}); // empty space
	REQUIRE(fixture.events.size() == 1);
	CHECK(fixture.events[0].kind == GraphEvent::Kind::LinkDrop);
	CHECK(fixture.events[0].node == "tint");
	CHECK(fixture.events[0].port == "out");
}

TEST_CASE("render: dragging a linked input's end moves the link, or detaches it on empty space") {
	GraphFixture fixture;
	fixture.drag(fixture.port(2, 0, false), fixture.port(2, 1, false)); // mix.a -> mix.b
	REQUIRE(fixture.events.size() == 2);
	CHECK(fixture.events[0].kind == GraphEvent::Kind::Disconnect);
	CHECK(fixture.events[0].linkIndex == 0);
	CHECK(fixture.events[1].kind == GraphEvent::Kind::Connect);
	CHECK(fixture.events[1].link == GraphLink{"tex", "out", "mix", "b"});

	fixture.events.clear();
	fixture.drag(fixture.port(2, 0, false), {300.0f, 340.0f});
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events[0].kind == GraphEvent::Kind::Disconnect);
	for(const GraphEvent& event : fixture.events) CHECK(event.kind != GraphEvent::Kind::Connect);
}

TEST_CASE("render: dragging a header moves the selection; the keyboard selects, moves, connects and deletes") {
	GraphFixture fixture;
	const Vec2 header = fixture.view->camera().toView({30.0f, 10.0f}); // tex's header
	fixture.drag(header, Vec2{header.x + 40.0f, header.y + 20.0f});
	bool moved = false;
	for(const GraphEvent& event : fixture.events)
		if(event.kind == GraphEvent::Kind::Move) {
			moved = true;
			CHECK(event.nodes == std::vector<std::size_t>{0});
			CHECK(event.delta == Vec2{40.0f, 20.0f});
		}
	CHECK(moved);

	fixture.events.clear();
	fixture.window->setFocus(fixture.view);
	fixture.press(Key::PageDown); // tint
	CHECK(fixture.view->selection() == std::vector<std::string>{"tint"});
	fixture.press(Key::Right);
	REQUIRE_FALSE(fixture.events.empty());
	CHECK(fixture.events.back().kind == GraphEvent::Kind::Move);
	CHECK(fixture.events.back().delta == Vec2{8.0f, 0.0f});

	// tint's only port is its output; then mix's b (inputs a, b, then out)
	fixture.events.clear();
	fixture.press(Key::Enter);
	fixture.press(Key::PageDown);
	fixture.press(Key::RightBracket);
	fixture.press(Key::Enter);
	bool connected = false;
	for(const GraphEvent& event : fixture.events)
		if(event.kind == GraphEvent::Kind::Connect) {
			connected = true;
			CHECK(event.link == GraphLink{"tint", "out", "mix", "b"});
		}
	CHECK(connected);

	fixture.events.clear();
	fixture.press(Key::Delete);
	REQUIRE(fixture.events.size() == 1);
	CHECK(fixture.events[0].kind == GraphEvent::Kind::Delete);
	CHECK(fixture.events[0].nodes == std::vector<std::size_t>{2});
}

TEST_CASE("render: a marquee selects the nodes it touches") {
	GraphFixture fixture;
	const GraphCamera& camera = fixture.view->camera();
	fixture.drag(camera.toView({160.0f, 260.0f}), camera.toView({60.0f, 150.0f})); // from empty space over tint
	CHECK(fixture.view->selection() == std::vector<std::string>{"tint"});
}

TEST_CASE("render: the model is bounded and off-screen nodes are culled") {
	GraphFixture fixture;
	GraphModel big;
	for(std::size_t i = 0; i < GraphView::maximumNodes + 5; ++i) {
		GraphNode& node = big.nodes.emplace_back();
		node.id = "n" + std::to_string(i);
		node.position = {static_cast<float>(i % 100) * 200.0f, static_cast<float>(i / 100) * 100.0f};
	}
	fixture.view->setModel(big);
	fixture.window->render();
	CHECK(fixture.view->model().nodes.size() == GraphView::maximumNodes);
	CHECK(fixture.view->visibleNodes() < 100);
	CHECK(fixture.view->visibleNodes() > 0);

	GraphCamera camera;
	camera.zoom = 1e9f;
	fixture.view->setCamera(camera);
	CHECK(fixture.view->camera().zoom == GraphCamera::maximumZoom);
}

TEST_CASE("format: a graph node keeps its links in state through its handlers, and refuses broken data") {
	const ManualClock clock;
	const auto headless = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(graphScreen);
	screen.build(*headless);
	headless->render();
	auto* graph = dynamic_cast<GraphView*>(screen.find("graph"));
	REQUIRE(graph);
	REQUIRE(graph->model().nodes.size() == 2);
	CHECK(graph->model().nodes[1].inputs[0].links == 1);

	const auto at = [&](std::size_t node, std::size_t port, bool output) {
		return graph->camera().toView(graph->portPoint(node, port, output));
	};
	const auto drag = [&](Vec2 from, Vec2 to) {
		headless->routeMouseButton({MouseButton::Left, Action::Press, {}, from});
		headless->routeMouseMove(to);
		headless->routeMouseButton({MouseButton::Left, Action::Release, {}, to});
		headless->render();
	};
	drag(at(0, 0, true), at(1, 0, false));
	CHECK(json::parse(screen.stateJson())["links"] ==
	      json::parse(R"([{"from": "tex", "output": "out", "to": "mix", "input": "a"}])"));
	headless->render();
	CHECK(graph->model().links.size() == 1);

	drag(at(0, 0, true), at(1, 1, false)); // color into shader: refused
	CHECK(json::parse(screen.stateJson())["links"].size() == 1);

	drag(at(1, 0, false), {300.0f, 350.0f}); // detached
	CHECK(json::parse(screen.stateJson())["links"] == json::array());

	const Vec2 header = graph->camera().toView({30.0f, 10.0f}); // tex's header
	drag(header, Vec2{header.x + 10.0f, header.y + 20.0f});
	const json moved = json::parse(screen.stateJson())["moved"];
	CHECK(moved["ids"] == json::parse(R"(["tex"])"));
	CHECK(moved["dx"] == 10);
	CHECK(moved["dy"] == 20);

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#graph/nodes": [{"title": "no id"}]}})", &error));
	CHECK(error.find("nodes") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#graph/nodes": [{"id": "a"}, {"id": "a"}]}})", &error));
	CHECK(error.find("nodes") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#graph/links": [{"from": "a"}]}})", &error));
	CHECK(error.find("links") != std::string::npos);
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#graph/zoom": "big"}})", &error));
	CHECK(error.find("zoom") != std::string::npos);
}

TEST_CASE("render: a drag that would close a cycle is refused unless cycles are allowed") {
	GraphFixture fixture;
	GraphModel model = material();
	model.nodes.push_back({"loop", "Loop", {200.0f, 200.0f}, 120.0f, 0.0f, {{"in", "", "color", 0}}, {}});
	model.nodes.back().outputs.push_back({"out", "", "color", 0});
	model.links.push_back({"mix", "out", "loop", "in"});
	fixture.view->setModel(model);
	fixture.window->render();

	fixture.drag(fixture.port(4, 0, true), fixture.port(2, 1, false)); // loop.out -> mix.b: mix -> loop -> mix
	CHECK(fixture.events.empty());
	fixture.drag(fixture.port(2, 1, false), fixture.port(4, 0, true)); // the same, drawn from the input
	CHECK(fixture.events.empty());
	fixture.drag(fixture.port(4, 0, true), fixture.port(4, 0, false)); // loop to itself
	CHECK(fixture.events.empty());

	fixture.view->setCyclesAllowed(true);
	fixture.drag(fixture.port(4, 0, true), fixture.port(2, 1, false));
	REQUIRE(fixture.events.size() == 1);
	CHECK(fixture.events[0].link == GraphLink{"loop", "out", "mix", "b"});
}
