/// @file
/// @brief Components and "let" as tracked layers: a parameter's change reaches only what reads it, re-evaluated in
/// place; siblings are left alone and no component is built again.

#include "Headless.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Turns verify mode on or off for the runtimes made next (it is read as one starts).
/// @param on Whether it is on.
/// @return @p on.
bool verifying(bool on) {
	if(on) ::setenv("AFFRESCOUI_VERIFY_UPDATES", "1", 1);
	else ::unsetenv("AFFRESCOUI_VERIFY_UPDATES");
	return on;
}

/// @brief A screen built in a headless window, its errors gathered.
struct Built {
	bool verify = false;                                                              ///< Verify mode, as it started.
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository;                                             ///< The document.
	affresco::format::Runtime runtime;                                                ///< The panels.
	std::vector<std::string> errors;                                                  ///< What onError got.
	affresco::Connection listening;                                                   ///< Gathers them.

	/// @brief Builds a screen.
	/// @param document The document.
	/// @param checked Whether every partial update is checked against a full one (verify mode).
	explicit Built(const json& document, bool checked = false)
	    : verify(verifying(checked)), repository(rules, document), runtime(repository, ".") {
		verifying(false);
		listening = runtime.onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime.build(*window);
		window->render();
	}
	/// @brief Runs an action, then the update it queued.
	/// @param action The action.
	/// @param event Its event.
	void run(const char* action, const json& event = json::object()) {
		runtime.runAction(action, window->root(), event);
		window->routeKey({affresco::Key::Unknown, 0, affresco::Action::Release, {}}); // runs what was deferred
	}
	/// @brief Sets a place of the state, then runs the update it queued.
	/// @param path The place ("state.a.b").
	/// @param value Its value.
	void set(const std::string& path, const json& value) { run("set", {{"path", path}, {"value", value}}); }
	/// @brief What the last update cost.
	/// @return It.
	[[nodiscard]] const affresco::format::UpdateStatistics& last() const { return runtime.lastUpdate(); }
};

/// @brief Writes a panel tree down: each panel's id, visibility and label text, depth first.
/// @param panel The subtree's root.
/// @param out Gets the text.
void describe(const affresco::Panel& panel, std::string& out) {
	out += panel.id() + (panel.isVisible() ? " v" : " h");
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel)) out += " '" + label->text() + "'";
	out += " [";
	for(const auto& child : panel.children()) describe(*child, out);
	out += ']';
}

/// @brief Whether a screen's panels are those a full build of its document and state gives.
/// @param built The screen.
/// @return true if they are.
bool asBuiltAgain(const Built& built) {
	json document = built.repository.document();
	document["state"] = built.repository.state();
	const Built fresh(document);
	std::string kept;
	std::string full;
	describe(built.window->root(), kept);
	describe(fresh.window->root(), full);
	return kept == full;
}

/// @brief The texts of the labels below a panel, depth first.
/// @param panel The panel.
/// @param out Gets them.
void texts(const affresco::Panel& panel, std::vector<std::string>& out) {
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel)) out.push_back(label->text());
	for(const auto& child : panel.children()) texts(*child, out);
}

/// @brief The texts of a screen's labels, depth first.
/// @param built The screen.
/// @return They.
std::vector<std::string> texts(const Built& built) {
	std::vector<std::string> out;
	texts(built.window->root(), out);
	return out;
}

/// @brief A screen: a column of children, the components and the state given, and a "set" action.
/// @param children The column's children.
/// @param components The components.
/// @param state The state.
/// @return The document.
json screen(json children, json components, json state) {
	json document = json::parse(R"json({ "format": "affrescoui-screen", "version": 2,
		"actions": { "set": [{ "set": "{{event.path}}", "to": "{{event.value}}" }] } })json");
	document["state"] = std::move(state);
	document["components"] = std::move(components);
	document["root"] = {{"type", "panel"},
	                    {"children", json::array({{{"type", "column"}, {"children", std::move(children)}}})}};
	return document;
}

/// @brief 400 uses of a component that shows its parameter, each passed its own key of the state.
/// @param value What each use passes, "{}" standing for its key ("state.keys.k7").
/// @return The document.
json parameterScreen(const std::string& value = "{{{}}}") {
	json keys = json::object();
	json children = json::array();
	for(int i = 0; i < 400; ++i) {
		const std::string key = "k" + std::to_string(i);
		keys[key] = 0;
		std::string passed = value;
		passed.replace(passed.find("{}"), 2, "state.keys." + key);
		children.push_back({{"use", "cell"}, {"with", {{"value", passed}}}});
	}
	const json components = json::parse(R"json({
		"cell": { "params": { "value": 0 }, "node": { "type": "label", "text": "{{value}}" } } })json");
	return screen(std::move(children), components, {{"keys", std::move(keys)}});
}

} // namespace

TEST_CASE("scopes: one of 400 uses' parameters changes: only its label is evaluated, no row and no panel is built") {
	Built built(parameterScreen());
	built.set("state.keys.k7", 5);
	CHECK(built.errors.empty());
	CHECK(built.last().nodesEvaluated == 1);
	CHECK(built.last().rowsExpanded == 0);
	CHECK(built.last().panelsCreated == 0);
	CHECK(texts(built)[7] == "5");
	CHECK(texts(built)[8] == "0");
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a parameter whose value stays the same evaluates nothing") {
	Built built(parameterScreen("{{{} > 0}}"));
	built.set("state.keys.k3", 1);
	CHECK(built.last().nodesEvaluated == 1);
	CHECK(texts(built)[3] == "true");
	built.set("state.keys.k3", 2);
	CHECK(built.errors.empty());
	CHECK(built.last().nodesEvaluated == 0);
	CHECK(built.last().rowsExpanded == 0);
	CHECK(built.last().panelsCreated == 0);
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a default that reads a passed parameter, a let that reads a with, a parameter passed two levels") {
	const json components = json::parse(R"json({
		"double": { "params": { "a": 0, "b": "{{a * 2}}" }, "node": { "type": "label", "text": "{{b}}" } },
		"outer": { "params": { "v": 0 },
		           "node": { "type": "column", "children": [{ "use": "inner", "with": { "w": "{{v + 1}}" } }] } },
		"inner": { "params": { "w": 0 }, "node": { "type": "label", "text": "{{w}}" } } })json");
	const json children = json::parse(R"json([
		{ "use": "double", "with": { "a": "{{state.a}}" } },
		{ "type": "label", "with": { "x": "{{state.x}}" }, "let": { "y": "{{x + 10}}" }, "text": "{{y}}" },
		{ "use": "outer", "with": { "v": "{{state.v}}" } }
	])json");
	Built built(screen(children, components, {{"a", 1}, {"x", 1}, {"v", 1}}), true);
	CHECK(texts(built) == std::vector<std::string>{"2", "11", "2"});
	built.set("state.a", 4);
	CHECK(built.last().nodesEvaluated == 1);
	built.set("state.x", 5);
	CHECK(built.last().nodesEvaluated == 1);
	built.set("state.v", 7);
	CHECK(built.last().nodesEvaluated == 1);
	CHECK(built.last().panelsCreated == 0);
	CHECK(texts(built) == std::vector<std::string>{"8", "15", "8"});
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a parameter read by \"when\", \"check\" or a dynamic style gives the screen a full build gives") {
	json document = screen(json::parse(R"json([
		{ "use": "shown", "with": { "v": "{{state.v}}" } },
		{ "type": "label", "with": { "v": "{{state.v}}" },
		  "check": { "value": "{{v}}", "rule": "{{value > 1}}", "message": "small" }, "text": "{{error}}" },
		{ "type": "label", "with": { "v": "{{state.v}}" }, "style": "{{v > 1 ? 'big' : 'small'}}", "text": "{{v}}" }
	])json"),
	                       json::parse(R"json({
		"shown": { "params": { "v": 0 }, "node": { "type": "label", "when": "{{v > 1}}", "text": "shown {{v}}" } } })json"),
	                       {{"v", 1}});
	document["styles"] = json::parse(R"json({ "big": { "padding": 8 }, "small": { "padding": 2 } })json");
	Built built(document, true);
	CHECK(texts(built) == std::vector<std::string>{"small", "1"});
	built.set("state.v", 2);
	CHECK(texts(built) == std::vector<std::string>{"shown 2", "", "2"});
	CHECK(asBuiltAgain(built));
	built.set("state.v", 0);
	CHECK(texts(built) == std::vector<std::string>{"small", "0"});
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a repeat's item passed into a component builds one row at most, and follows inserts and removals") {
	json items = json::array();
	for(int i = 0; i < 50; ++i) items.push_back({{"id", i}, {"v", i}});
	const json children = json::parse(R"json([{ "type": "row",
		"repeat": { "for": "item", "in": "state.items", "key": "{{item.id}}" },
		"children": [{ "use": "cell", "with": { "value": "{{item.v}}", "n": "{{index}}", "t": "{{state.t}}" } }] }])json");
	const json components = json::parse(R"json({
		"cell": { "params": { "value": 0, "n": 0, "t": 0 }, "node": { "type": "label", "text": "{{n}}:{{value}}:{{t}}" } } })json");
	Built built(screen(children, components, {{"items", std::move(items)}, {"t", 0}}), true);
	built.set("state.items[3].v", 100);
	CHECK(built.last().rowsExpanded <= 1);
	CHECK(built.last().panelsCreated == 0);
	CHECK(texts(built)[3] == "3:100:0");
	built.run("set", {{"path", "state.items"}, {"value", json::parse(R"([{"id": 99, "v": 99}])")}});
	built.set("state.t", 1);
	CHECK(texts(built)[0] == "0:99:1");
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a removal before a row and a change its parameter reads, in one update") {
	json items = json::array();
	for(int i = 0; i < 5; ++i) items.push_back({{"id", i}, {"v", i}});
	json document = screen(json::parse(R"json([{ "type": "row",
		"repeat": { "for": "item", "in": "state.items", "key": "{{item.id}}" },
		"children": [{ "use": "cell", "with": { "value": "{{item.v}}", "t": "{{state.t}}" } }] }])json"),
	                       json::parse(R"json({
		"cell": { "params": { "value": 0, "t": 0 }, "node": { "type": "label", "text": "{{value}}:{{t}}" } } })json"),
	                       {{"items", std::move(items)}, {"t", 0}});
	document["actions"]["both"] =
	    json::parse(R"json([{ "remove": "state.items", "at": 1 }, { "increment": "state.t" }])json");
	Built built(document, true);
	built.run("both");
	CHECK(texts(built) == std::vector<std::string>{"0:1", "2:1", "3:1", "4:1"});
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a slot's children read the caller's variables") {
	const json children = json::parse(R"json([{ "type": "column", "with": { "name": "{{state.name}}" }, "children": [
		{ "use": "frame", "with": { "title": "{{state.title}}" }, "children": [{ "type": "label", "text": "{{name}}" }] }
	] }])json");
	const json components = json::parse(R"json({ "frame": { "params": { "title": "" },
		"node": { "type": "column", "children": [{ "type": "label", "text": "{{title}}" }, { "slot": true }] } } })json");
	Built built(screen(children, components, {{"name", "a"}, {"title", "t"}}), true);
	CHECK(texts(built) == std::vector<std::string>{"t", "a"});
	built.set("state.name", "b");
	CHECK(built.last().nodesEvaluated == 1);
	built.set("state.title", "u");
	CHECK(built.last().nodesEvaluated == 1);
	CHECK(texts(built) == std::vector<std::string>{"u", "b"});
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("scopes: a handler reads a parameter's value after it changed") {
	const json children = json::parse(R"json([{ "use": "button", "with": { "value": "{{state.v}}" } }])json");
	const json components = json::parse(R"json({ "button": { "params": { "value": 0 },
		"node": { "type": "panel", "id": "hit", "width": 100, "height": 40,
		          "onClick": [{ "set": "state.seen", "to": "{{value}}" }] } } })json");
	Built built(screen(children, components, {{"v", 1}, {"seen", 0}}));
	built.set("state.v", 9);
	affresco::Panel* hit = built.window->root().findById("hit");
	REQUIRE(hit != nullptr);
	const affresco::Rect bounds = hit->bounds();
	const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
	built.window->render();
	CHECK(built.repository.state()["seen"] == 9);
	CHECK(built.errors.empty());
}
