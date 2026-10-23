/// @file
/// @brief The scene node in a screen: its scene and view applied from JSON, the view kept in state both ways, clicks
/// and boxes reaching onSelect with the ids, the camera step, and the values and files it refuses.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/scene3d/SceneView.hpp>
#include <affresco/scene3d/Viewport.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::Action;
using affresco::MouseButton;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::scene3d::SceneView;
using affresco::json;

namespace {

/// @brief A screen with a scene of a cube at the origin and a ball to its right, the view kept in state.
/// @param extra More of the scene node's keys, as JSON members after a comma (or empty).
/// @param actions The document's actions.
/// @return The document.
std::string sceneScreen(const std::string& extra = "", const std::string& actions = "{}") {
	return R"json({
		"format": "affrescoui-screen", "version": 2,
		"actions": )json" +
	       actions + R"json(,
		"state": {
			"view": { "camera": { "target": [0, 0, 0], "yaw": 0, "pitch": 0, "distance": 10 } },
			"scene": { "nodes": [{ "id": "cube", "mesh": "cube" },
			                     { "id": "ball", "mesh": "sphere", "translation": [3, 0, 0] }] },
			"picked": null, "boxed": null
		},
		"root": { "id": "root", "children": [
			{ "type": "scene", "id": "viewport", "width": 600, "height": 400, "bind": "state.view",
			  "scene": "{{state.scene}}", "grid": false,
			  "onSelect": [{ "if": "{{event.type == 'pick'}}", "then": [{ "set": "state.picked", "to": "{{event}}" }],
			                 "else": [{ "set": "state.boxed", "to": "{{event}}" }] }])json" +
	       (extra.empty() ? "" : ", " + extra) + R"json( }
		] }
	})json";
}

/// @brief The screen's state. @param screen The screen. @return Its state.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief Clicks with the left button. @param window The window. @param at Where.
void click(affresco::Window& window, Vec2 at) {
	window.routeMouseMove(at);
	window.routeMouseButton({MouseButton::Left, Action::Press, {}, at});
	window.routeMouseButton({MouseButton::Left, Action::Release, {}, at});
}

/// @brief Where a world point shows in a scene view, in window units.
/// @param scene The view. @param point The point. @return Its place.
Vec2 shown(const SceneView& scene, affresco::scene3d::Vec3 point) {
	const affresco::Rect frame = scene.frame();
	const auto at = scene.camera().project(point, frame.width(), frame.height());
	REQUIRE(at);
	return {frame.left + at->first, frame.top + at->second};
}

} // namespace

TEST_CASE("format: a scene node shows its scene and the view from state") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen());
	screen.build(*window);
	window->render();

	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);
	CHECK(scene->scene().nodes.size() == 2);
	CHECK(scene->camera().distance == doctest::Approx(10.0f));
	CHECK(scene->camera().yaw == doctest::Approx(0.0f));

	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"/state/view": {"camera": {"distance": 20, "orthographic": true},
		"selection": ["ball"]}}})",
	                             &error));
	window->render();
	CHECK(scene->camera().distance == doctest::Approx(20.0f));
	CHECK(scene->camera().lens.orthographic);
	CHECK(scene->selection() == std::vector<std::string>{"ball"});
}

TEST_CASE("format: a click on a scene picks the node under it, and writes the selection back") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen());
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);

	click(*window, shown(*scene, {3.0f, 0.0f, 0.0f}));
	json now = state(screen);
	CHECK(now["picked"]["type"] == "pick");
	CHECK(now["picked"]["id"] == "ball");
	CHECK(now["picked"]["button"] == "left");
	CHECK(now["picked"]["selection"] == json{"ball"});
	CHECK(now["view"]["selection"] == json{"ball"});

	click(*window, {scene->frame().left + 5.0f, scene->frame().top + 5.0f}); // the sky
	now = state(screen);
	CHECK(now["picked"]["id"].is_null());
	CHECK(now["view"]["selection"] == json::array());
}

TEST_CASE("format: a box dragged over a scene selects the nodes in it") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen());
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);

	// From the top left corner (nothing there) down past both models
	const Vec2 from{scene->frame().left + 2.0f, scene->frame().top + 2.0f};
	const Vec2 to{scene->frame().left + scene->frame().width() - 2.0f, scene->frame().top + scene->frame().height() - 2.0f};
	window->routeMouseMove(from);
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, from});
	window->routeMouseMove({(from.x + to.x) / 2.0f, (from.y + to.y) / 2.0f});
	window->routeMouseMove(to);
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, to});

	const json now = state(screen);
	CHECK(now["boxed"]["type"] == "boxSelect");
	CHECK(now["boxed"]["ids"].size() == 2);
	CHECK(now["view"]["selection"].size() == 2);
}

TEST_CASE("format: the camera step snaps to a view, frames, switches the projection, orbits and zooms") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen("", R"json({
		"top": [{ "camera": "#viewport", "view": "top" }],
		"flip": [{ "camera": "viewport", "projection": "toggle" }],
		"ball": [{ "camera": "viewport", "frame": ["ball"] }],
		"turn": [{ "camera": "viewport", "orbit": [90, 0], "zoom": 2 }] })json"));
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);

	/// Runs an action and lets the step's deferred move run.
	const auto press = [&](const char* action) {
		screen.runAction(action, *scene);
		clock.advance(1);
		window->render();
	};
	press("top");
	CHECK(scene->camera().pitch == doctest::Approx(1.5707963f).epsilon(0.01));
	press("flip");
	CHECK(scene->camera().lens.orthographic);
	press("ball");
	CHECK(scene->camera().target.x == doctest::Approx(3.0f));
	const float yaw = scene->camera().yaw;
	const float distance = scene->camera().distance;
	press("turn");
	CHECK(scene->camera().yaw != doctest::Approx(yaw));
	CHECK(scene->camera().distance < distance);
	CHECK(state(screen)["view"]["camera"]["orthographic"] == true); // moved by a step, written back too
}

TEST_CASE("format: a scene node refuses bad values, naming the key") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen());
	screen.build(*window);
	window->render();

	const auto refused = [&](const std::string& set, const std::string& key) {
		std::string error;
		CHECK_FALSE_MESSAGE(screen.applyRevision(R"({"set": {"#viewport/)" + set + "}}", &error), set);
		CHECK_MESSAGE(error.find(key) != std::string::npos, error);
	};
	refused(R"(shading": "toon")", "shading");
	refused(R"(gizmo": {"mode": "bend"})", "gizmo.mode");
	refused(R"(controls": {"orbit": "nose"})", "controls.orbit");
	refused(R"(animations": 3)", "animations");
	refused(R"(scene": {"nodes": [{"id": "a", "mesh": 3}]})", "scene");
	refused(R"(assets": {"robot": "/etc/robot.glb"})", "assets.robot");
	refused(R"(assets": {"robot": "robot.txt"})", "assets.robot");
	refused(R"(assets": {"bad name": "robot.glb"})", "assets");

	json many = json::object();
	for(int i = 0; i < 65; ++i) many["m" + std::to_string(i)] = "m.glb";
	refused(R"(assets": )" + many.dump(), "assets");
}

TEST_CASE("format: a scene node places a glTF model from the screen's folder") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	std::string text = sceneScreen(R"json("assets": { "box": "BoxAnimated/BoxAnimated.glb" })json");
	const std::string ball = R"({ "id": "ball", "mesh": "sphere", "translation": [3, 0, 0] })";
	text.replace(text.find(ball), ball.size(), R"({ "id": "model", "asset": "box" })");
	affresco::Screen screen = affresco::Screen::fromString(text, AFFRESCOUI_MODELLO_SAMPLES);
	screen.build(*window);
	window->render();

	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);
	CHECK(scene->scene().nodes.size() > 2); // the cube, the model's node and the nodes it holds
	bool held = false;
	for(const auto& node : scene->scene().nodes) held = held || node.id.starts_with("model/");
	CHECK(held);
}

TEST_CASE("format: the transform step moves the selection with the pointer, and a click or a cancel ends it") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	std::string text = sceneScreen(R"json("onTransform": [{ "set": "state.moved", "to": "{{event}}" }])json", R"json({
		"grab": [{ "transform": "#viewport", "mode": "translate", "axis": "x" }],
		"free": [{ "transform": "viewport", "axis": "view" }],
		"keep": [{ "transform": "viewport", "end": "confirm" }],
		"drop": [{ "transform": "viewport", "end": "cancel" }] })json");
	affresco::Screen screen = affresco::Screen::fromString(text);
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);
	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"/state/view/selection": ["cube"]}})", &error));
	window->render();

	/// Runs an action and lets the step's deferred work run.
	const auto press = [&](const char* action) {
		screen.runAction(action, *scene);
		clock.advance(1);
		window->render();
	};
	const Vec2 middle = shown(*scene, {0.0f, 0.0f, 0.0f});
	window->routeMouseMove(middle);
	press("grab");
	CHECK(scene->transforming());
	window->routeMouseMove({middle.x + 60.0f, middle.y});
	/// Where the last report moved the cube to: a preview is drawn and reported, and the document applies it.
	const auto moved = [&] { return state(screen)["moved"]["nodes"][0]["translation"]; };
	CHECK(moved()[0].get<double>() > 0.5);
	CHECK(moved()[1].get<double>() == doctest::Approx(0.0)); // held to x
	CHECK(state(screen)["moved"]["phase"] == "preview");
	press("free");
	window->routeMouseMove({middle.x + 60.0f, middle.y - 40.0f});
	CHECK(moved()[1].get<double>() > 0.5);
	press("keep");
	CHECK_FALSE(scene->transforming());
	CHECK(state(screen)["moved"]["phase"] == "end");

	press("grab");
	window->routeMouseMove({middle.x + 200.0f, middle.y});
	press("drop");
	CHECK(state(screen)["moved"]["phase"] == "cancel");

	// A click ends one too, keeping it, and never selects what is under it
	press("grab");
	window->routeMouseMove({middle.x + 120.0f, middle.y});
	window->routeMouseButton({MouseButton::Left, Action::Press, {}, {middle.x + 120.0f, middle.y}});
	window->routeMouseButton({MouseButton::Left, Action::Release, {}, {middle.x + 120.0f, middle.y}});
	CHECK_FALSE(scene->transforming());
	CHECK(state(screen)["moved"]["phase"] == "end");
	CHECK(state(screen)["view"]["selection"] == json{"cube"});
}

TEST_CASE("format: a Z-up scene stands its nodes up, and picks and reports points in its own frame") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	std::string text = sceneScreen(R"json("up": "z", "cursor3d": [0, 0, 0])json");
	const std::string ball = R"("translation": [3, 0, 0])";
	text.replace(text.find(ball), ball.size(), R"("translation": [0, 0, 3])");
	affresco::Screen screen = affresco::Screen::fromString(text);
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);

	// The ball, 3 up the scene's Z, is 3 up the world's Y: above the cube on screen
	click(*window, shown(*scene, {0.0f, 3.0f, 0.0f}));
	const json picked = state(screen)["picked"];
	CHECK(picked["id"] == "ball");
	REQUIRE(picked["point"].size() == 3);
	CHECK(picked["point"][2].get<double>() == doctest::Approx(3.0).epsilon(0.1));
	CHECK(picked["point"][1].get<double>() < 0.0); // the near side, though the ray runs down the sphere's seam

	// The gizmo stands at the pivot, turned into the world's frame
	const auto settings = affresco::scene3d::viewportSettingsFromJson(
	    json::parse(R"({"up": "z", "selection": ["cube"], "gizmo": {"visible": true, "pivot": [1, 2, 3]}})"));
	CHECK(settings.zUp);
	const auto gizmo = affresco::scene3d::gizmoFor(scene->scene(), settings, 400.0f);
	REQUIRE(gizmo);
	CHECK(gizmo->origin.x == doctest::Approx(1.0f));
	CHECK(gizmo->origin.y == doctest::Approx(3.0f));
	CHECK(gizmo->origin.z == doctest::Approx(-2.0f));
}

TEST_CASE("format: a typed value moves a transform by that much, and null gives it back to the pointer") {
	ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	std::string text = sceneScreen(R"json("onTransform": [{ "set": "state.moved", "to": "{{event}}" }])json", R"json({
		"grab": [{ "transform": "viewport", "mode": "translate", "axis": "x" }],
		"two": [{ "transform": "viewport", "value": 2 }],
		"free": [{ "transform": "viewport", "value": null }],
		"keep": [{ "transform": "viewport", "end": "confirm" }] })json");
	affresco::Screen screen = affresco::Screen::fromString(text);
	screen.build(*window);
	window->render();
	auto* scene = dynamic_cast<SceneView*>(screen.find("viewport"));
	REQUIRE(scene);
	std::string error;
	REQUIRE(screen.applyRevision(R"({"set": {"/state/view/selection": ["cube"]}})", &error));
	window->render();
	const auto press = [&](const char* action) {
		screen.runAction(action, *scene);
		clock.advance(1);
		window->render();
	};
	const auto moved = [&] { return state(screen)["moved"]["nodes"][0]["translation"]; };

	const Vec2 middle = shown(*scene, {0.0f, 0.0f, 0.0f});
	window->routeMouseMove(middle);
	press("grab");
	window->routeMouseMove({middle.x + 90.0f, middle.y});
	press("two");
	CHECK(moved()[0].get<double>() == doctest::Approx(2.0));
	press("free");
	CHECK(moved()[0].get<double>() != doctest::Approx(2.0));
	press("two");
	press("keep");
	CHECK(state(screen)["moved"]["phase"] == "end");
	CHECK(moved()[0].get<double>() == doctest::Approx(2.0));
}

TEST_CASE("format: a scene node refuses a bad up, cursor3d, pivot or typed value") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(600, 400, clock);
	affresco::Screen screen = affresco::Screen::fromString(sceneScreen());
	screen.build(*window);
	window->render();
	const auto refused = [&](const std::string& set, const std::string& key) {
		std::string error;
		CHECK_FALSE_MESSAGE(screen.applyRevision(R"({"set": {"#viewport/)" + set + "}}", &error), set);
		CHECK_MESSAGE(error.find(key) != std::string::npos, error);
	};
	refused(R"(up": "w")", "up");
	refused(R"(cursor3d": [1, 2])", "cursor3d");
	refused(R"(cursor3d": [1, 2, 1e9])", "cursor3d[2]");
	refused(R"(gizmo": {"pivot": "here"})", "gizmo.pivot");
	std::string error;
	CHECK(screen.applyRevision(R"({"set": {"#viewport/cursor3d": [1, 2, 3]}})", &error));

	affresco::Screen typed = affresco::Screen::fromString(
	    sceneScreen("", R"json({ "bad": [{ "transform": "viewport", "value": "far" }] })json"));
	typed.build(*window);
	std::vector<std::string> errors;
	affresco::setLogSink([&errors](affresco::LogLevel level, std::string_view message) {
		if(level == affresco::LogLevel::Error) errors.emplace_back(message);
	});
	typed.runAction("bad", *window->root().findById("viewport"));
	affresco::setLogSink(nullptr);
	REQUIRE(errors.size() == 1);
	CHECK(errors[0].find("bad[0].value: expected a number from -1000000 to 1000000, or null") != std::string::npos);
}
