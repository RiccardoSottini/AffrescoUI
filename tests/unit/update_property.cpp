/// @file
/// @brief The property of partial updates, over generated documents: after any series of steps, verify mode reports
/// nothing, and the panels are those a full build of the same document and state gives.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Writes a panel tree down: each panel's id, frame, visibility, colour and label text, depth first.
/// @param panel The subtree's root.
/// @param out Gets the text.
void describe(const affresco::Panel& panel, std::string& out) {
	const affresco::Rect& frame = panel.frame();
	const affresco::Color& color = panel.color();
	out += panel.id() + " " + std::to_string(frame.left) + "," + std::to_string(frame.top) + " " +
	       std::to_string(frame.width()) + "x" + std::to_string(frame.height()) + (panel.isVisible() ? " v " : " h ") +
	       std::to_string(color.r) + "," + std::to_string(color.g);
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel))
		out += " '" + label->text() + "' " + std::to_string(label->textColor().r);
	out += " [";
	for(const auto& child : panel.children()) describe(*child, out);
	out += "]";
}

/// @brief The state every generated document starts with: scalars, an object, and lists of objects and of text.
/// @return It.
json startState() {
	return json::parse(R"json({
		"n": 1, "flag": false, "name": "a", "watched": 0,
		"obj": { "x": 1, "y": "b" },
		"tags": ["a", "b"],
		"items": [
			{ "id": 1, "title": "a", "done": false, "children": [{ "t": "x" }, { "t": "y" }] },
			{ "id": 2, "title": "b", "done": true, "children": [] },
			{ "id": 3, "title": "c", "done": false, "children": [{ "t": "z" }] }
		]
	})json");
}

/// @brief The steps every generated document has, as actions run with "event" values.
/// @return They.
json actions() {
	return json::parse(R"json({
		"setN": [{ "set": "state.n", "to": "{{event.v}}" }],
		"incN": [{ "increment": "state.n" }],
		"flag": [{ "toggle": "state.flag" }],
		"name": [{ "set": "state.name", "to": "{{event.s}}" }],
		"objX": [{ "set": "state.obj.x", "to": "{{event.v}}" }],
		"obj": [{ "set": "state.obj", "to": { "x": "{{event.v}}", "y": "{{event.s}}" } }],
		"push": [{ "push": "state.items", "value": { "id": "{{event.id}}", "title": "{{event.s}}", "done": false, "children": [] } }],
		"remove": [{ "remove": "state.items", "at": "{{event.i}}" }],
		"done": [{ "toggle": "state.items[event.i].done" }],
		"title": [{ "set": "state.items[event.i].title", "to": "{{event.s}}" }],
		"child": [{ "push": "state.items[event.i].children", "value": { "t": "{{event.s}}" } }],
		"childT": [{ "set": "state.items[event.i].children[0].t", "to": "{{event.s}}" }],
		"replace": [{ "set": "state.items", "to": [{ "id": "{{event.id}}", "title": "{{event.s}}", "done": true, "children": [{ "t": "{{event.s}}" }] }] }],
		"reverse": [{ "set": "state.items", "to": "{{reverse(state.items)}}" }],
		"tag": [{ "push": "state.tags", "value": "{{event.s}}" }],
		"untag": [{ "remove": "state.tags", "at": "{{event.i}}" }],
		"size": [{ "increment": "settings.size" }],
		"ghost": [{ "set": "state.ghost.deep", "to": "{{event.s}}" }],
		"nowhere": [{ "toggle": "state.nowhere" }],
		"incGhost": [{ "increment": "state.missing" }]
	})json");
}

/// @brief Generates a document: a seeded choice of the parts below, nested in panels, rows and columns.
/// @param random The generator.
/// @param lisp Whether the item labels are written as a program's function, called as lisp('name', item).
/// @return The document.
json generate(std::mt19937& random, bool lisp) {
	const auto chance = [&] { return random() % 2 == 0; };
	json document = {{"format", "affrescoui-screen"}, {"version", 2}, {"state", startState()}, {"actions", actions()}};
	document["settings"] = {{"size", {{"type", "integer"}, {"default", 3}, {"description", "A size."}}}};
	document["styles"] = {{"on", {{"color", "#00ff00"}}}, {"off", {{"color", "#ff0000"}}}};
	document["computed"] = {{"a", "{{state.n * 2}}"}, {"b", "{{computed.a + length(state.items)}}"}};
	document["components"] = {{"chip",
	                           {{"params", {{"label", ""}, {"on", false}}},
	                            {"node", json::parse(R"({ "type": "row", "style": "{{on ? 'on' : 'off'}}", "children": [
	                  { "type": "label", "text": "{{label}}" }, { "slot": true } ] })")}}}};
	if(lisp)
		document["program"] = {
		    {"app.lisp", {{"title-of", "(defn title-of [x] (str \"#\" (:title x) (if (:done x) \"!\" \"\")))"}}}};
	if(chance())
		document["watch"] = {{"state.n", json::array({{{"set", "state.watched"}, {"to", "{{event.value}}"}}})}};

	const std::string itemText = lisp ? "{{lisp('title-of', item)}}" : "{{item.title}}{{item.done ? '!' : ''}}";
	std::vector<json> parts = {
	    json::parse(R"({ "type": "label", "text": "{{state.n}} {{state.name}} {{state.watched}}" })"),
	    json::parse(R"({ "type": "label", "when": "{{state.flag}}", "text": "on {{computed.b}}" })"),
	    json::parse(R"({ "type": "label", "text": "{{state.obj.x}}-{{state.obj.y}}" })"),
	    json::parse(R"({ "type": "column", "let": { "o": "{{state.obj}}", "k": "{{state.n + 1}}" }, "children": [
	        { "type": "label", "text": "{{o.x}} {{k}}" } ] })"),
	    {{"type", "column"},
		 {"children",
		  json::array({{{"type", "row"},
		                {"repeat", {{"for", "item"}, {"in", "state.items"}, {"key", "{{item.id}}"}, {"index", "i"}}},
		                {"children", json::array({{{"type", "label"}, {"text", itemText}},
		                                          {{"type", "label"},
		                                           {"repeat", {{"for", "child"}, {"in", "item.children"}}},
		                                           {"text", "{{i}}.{{child.t}} {{state.flag ? 'f' : ''}}"}}})}}})}},
	    {{"type", "column"},
		 {"children", json::array({{{"type", "label"},
		                            {"repeat", {{"for", "item"}, {"in", "state.items"}}},
		                            {"when", "{{!item.done || state.flag}}"},
		                            {"text", itemText}}})}},
	    json::parse(R"({ "type": "row", "children": [{ "type": "label", "repeat": { "for": "tag", "in": "state.tags" },
	        "text": "{{tag}}" }] })"),
	    json::parse(R"({ "use": "chip", "with": { "label": "{{state.name}}", "on": "{{state.flag}}" }, "children": [
	        { "type": "label", "text": "{{computed.a}}" } ] })"),
	    json::parse(
	        R"({ "type": "label", "style": "{{state.n % 2 == 0 ? 'on' : 'off'}}", "text": "{{settings.size}}" })"),
	    json::parse(
	        R"({ "type": "label", "text": "{{count(state.items, it.done)}} {{length(filter(state.items, !it.done))}} {{join(map(state.tags, it + '.'), ',')}} {{find(state.items, it.done).title ?? 'none'}}" })"),
	    json::parse(
	        R"({ "type": "label", "text": "{{state.ghost.deep ?? '-'}} {{state.nowhere ? 'y' : 'n'}} {{state.missing ?? 0}}" })"),
	};
	json children = json::array();
	for(json& part : parts)
		if(chance()) children.push_back(std::move(part));
	if(children.empty()) children.push_back(parts.front());
	// Nested once more, sometimes: the parts split between a row and a column inside the root
	json root = {{"type", "panel"}, {"children", json::array()}};
	if(chance()) {
		json row = {{"type", "row"}, {"gap", 2}, {"children", json::array()}};
		json column = {{"type", "column"}, {"children", json::array()}};
		for(std::size_t i = 0; i < children.size(); ++i) (i % 2 ? row : column)["children"].push_back(children[i]);
		root["children"] = {row, column};
	} else {
		root["children"] = std::move(children);
	}
	document["root"] = std::move(root);
	return document;
}

/// @brief Runs one generated step: an action on a place that exists or one that doesn't.
/// @param screen The screen.
/// @param sender A panel to send it from.
/// @param random The generator.
/// @param nextId The next item id.
/// @param ran Gets the step, for a failure's message.
void step(affresco::Screen& screen, affresco::Panel& sender, std::mt19937& random, int& nextId, std::string& ran) {
	static const char* const names[] = {"setN",   "incN",  "flag",  "name",  "objX",    "obj",     "push",
	                                    "remove", "done",  "title", "child", "childT",  "replace", "reverse",
	                                    "tag",    "untag", "size",  "ghost", "nowhere", "incGhost"};
	static const char* const words[] = {"", "a", "b", "c"};
	const json state = json::parse(screen.stateJson());
	const auto items = static_cast<int>(state["items"].size());
	const json event = {{"v", static_cast<int>(random() % 5)},
	                    {"s", words[random() % 4]},
	                    {"i", static_cast<int>(random() % static_cast<unsigned>(items + 2))}, // sometimes past the end
	                    {"id", nextId++}};
	const char* const name = names[random() % std::size(names)];
	ran += std::string(name) + event.dump() + " ";
	screen.runAction(name, sender, event.dump());
}

/// @brief Runs generated documents and steps, and checks every update.
/// @param seeds The seeds.
/// @param steps The updates per seed.
void checkProperty(unsigned seeds, int steps) {
	const ManualClock clock;
	::setenv("AFFRESCOUI_VERIFY_UPDATES", "1", 1);
	for(unsigned seed = 1; seed <= seeds; ++seed) {
		std::mt19937 random(seed);
		const json document = generate(random, seed % 2 == 0);
		const auto window = createHeadlessWindow(640, 480, clock);
		affresco::Screen screen = affresco::Screen::fromString(document.dump());
		std::vector<std::string> checks;
		[[maybe_unused]] const affresco::Connection listening = screen.onError([&](const std::string& message) {
			if(message.contains("update check")) checks.push_back(message);
		});
		screen.build(*window);
		window->render();
		int nextId = 100;
		std::string ran;
		for(int i = 0; i < steps; ++i) {
			const int batch = 1 + static_cast<int>(random() % 3);
			for(int k = 0; k < batch; ++k) step(screen, window->root(), random, nextId, ran);
			window->render();
			INFO("seed " << seed << ", step " << i << ", steps " << ran << "\nroot " << document["root"].dump()
			             << "\nstate " << screen.stateJson());
			REQUIRE(checks.empty());
			// A full build of the same document and state (it makes no partial update to check)
			json again = json::parse(screen.documentJson());
			again["state"] = json::parse(screen.stateJson());
			const auto other = createHeadlessWindow(640, 480, clock);
			affresco::Screen fresh = affresco::Screen::fromString(again.dump());
			fresh.build(*other);
			other->render();
			std::string incremental;
			std::string full;
			describe(window->root(), incremental);
			describe(other->root(), full);
			REQUIRE(incremental == full);
		}
	}
	::unsetenv("AFFRESCOUI_VERIFY_UPDATES");
}

} // namespace

TEST_CASE("update property: generated documents and steps, each update checked against a full one") {
	checkProperty(200, 40);
}

TEST_CASE("update property, regression: a set past a list's end grows the list, and what reads its length sees it") {
	::setenv("AFFRESCOUI_VERIFY_UPDATES", "1", 1);
	const ManualClock clock;
	const auto window = createHeadlessWindow(640, 480, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "items": [{ "title": "a" }] },
		"actions": { "far": [{ "set": "state.items[2].title", "to": "c" }], "deep": [{ "toggle": "state.made.on" }] },
		"root": { "type": "column", "children": [
			{ "type": "label", "repeat": { "for": "item", "in": "state.items" }, "text": "{{item.title ?? '-'}}" },
			{ "type": "label", "text": "{{length(state.items)}} {{state.made ? 'made' : ''}}" } ] } })json");
	::unsetenv("AFFRESCOUI_VERIFY_UPDATES");
	std::vector<std::string> errors;
	[[maybe_unused]] const affresco::Connection listening =
	    screen.onError([&](const std::string& message) { errors.push_back(message); });
	screen.build(*window);
	window->render();
	screen.runAction("far", window->root());
	window->render();
	screen.runAction("deep", window->root());
	window->render();
	CHECK(errors.empty());
	std::string labels;
	describe(window->root(), labels);
	CHECK(labels.contains("'3 made'"));
	CHECK(labels.contains("'c'"));
}
