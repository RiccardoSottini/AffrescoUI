/// @file
/// @brief Fine-grained updates: after any series of state changes, the panels updated only where the changes are read
/// are those a full build of the same document and state gives (a differential test over random scenarios).

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <random>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A board: nested keyed and unkeyed repeats, "when", "let", a computed value, and values read whole.
constexpr const char* board = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": {
		"count": 0, "flag": false, "filter": "",
		"columns": [
			{ "name": "To do", "cards": [{ "id": 1, "title": "a", "done": false }, { "id": 2, "title": "b", "done": true }] },
			{ "name": "Done", "cards": [{ "id": 3, "title": "c", "done": false }] }
		]
	},
	"computed": { "twice": "{{state.count * 2}}" },
	"actions": {
		"title": [{ "set": "state.columns[event.c].cards[event.i].title", "to": "{{event.v}}" }],
		"done": [{ "toggle": "state.columns[event.c].cards[event.i].done" }],
		"add": [{ "push": "state.columns[event.c].cards", "value": { "id": "{{event.id}}", "title": "{{event.v}}", "done": false } }],
		"drop": [{ "remove": "state.columns[event.c].cards", "at": "{{event.i}}" }],
		"name": [{ "set": "state.columns[event.c].name", "to": "{{event.v}}" }],
		"count": [{ "increment": "state.count" }],
		"flag": [{ "toggle": "state.flag" }],
		"filter": [{ "set": "state.filter", "to": "{{event.v}}" }]
	},
	"root": { "type": "panel", "children": [{ "type": "row", "gap": 4, "children": [
		{ "type": "label", "id": "count", "text": "{{state.count}} / {{computed.twice}}" },
		{ "type": "label", "id": "flag", "when": "{{state.flag}}", "text": "on" },
		{ "type": "column", "repeat": { "for": "column", "in": "state.columns", "index": "c" }, "gap": 2, "children": [
			{ "type": "label", "text": "{{column.name}} ({{length(column.cards)}})" },
			{ "type": "column", "let": { "shown": "{{state.filter}}" }, "children": [
				{ "type": "row", "repeat": { "for": "card", "in": "column.cards", "key": "{{card.id}}" },
				  "when": "{{shown == '' || card.title == shown}}", "children": [
					{ "type": "label", "text": "{{card.title}}", "textColor": "{{card.done ? '#00ff00' : '#ff0000'}}" },
					{ "type": "label", "when": "{{state.flag && card.done}}", "text": "{{c}}:{{state.count}}" }
				] }
			] }
		] }
	] }] }
})json";

/// @brief Writes a panel tree down: each panel's id, frame, visibility, colour and label text, depth first.
/// @param panel The subtree's root.
/// @param out Gets the text.
void describe(const affresco::Panel& panel, std::string& out) {
	const affresco::Rect& frame = panel.frame();
	out += panel.id() + " " + std::to_string(frame.left) + "," + std::to_string(frame.top) + " " +
	       std::to_string(frame.width()) + "x" + std::to_string(frame.height()) + (panel.isVisible() ? " v" : " h");
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel))
		out += " '" + label->text() + "' " + std::to_string(label->textColor().r);
	out += " [";
	for(const auto& child : panel.children()) describe(*child, out);
	out += "]";
}

/// @brief The tree a full build of the same document and state gives.
/// @param screen The screen updated incrementally.
/// @param clock The clock.
/// @return describe() of it.
std::string builtAgain(const affresco::Screen& screen, const ManualClock& clock) {
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	const auto window = createHeadlessWindow(640, 480, clock); // outlives the screen
	affresco::Screen fresh = affresco::Screen::fromString(document.dump());
	fresh.build(*window);
	window->render();
	std::string out;
	describe(window->root(), out);
	return out;
}

/// @brief Runs one random change of the board's state.
/// @param screen The screen.
/// @param sender A panel to send it from.
/// @param random The generator.
/// @param nextId The next card id.
void change(affresco::Screen& screen, affresco::Panel& sender, std::mt19937& random, int& nextId) {
	const json state = json::parse(screen.stateJson());
	const auto pick = [&](std::size_t n) { return static_cast<int>(random() % n); };
	const int c = pick(state["columns"].size());
	const std::size_t cards = state["columns"][static_cast<std::size_t>(c)]["cards"].size();
	const std::string words[] = {"", "a", "b", "c", "d"};
	const std::string v = words[pick(5)];
	switch(pick(8)) {
	case 0:
		if(cards) screen.runAction("title", sender, json{{"c", c}, {"i", pick(cards)}, {"v", v}}.dump());
		break;
	case 1:
		if(cards) screen.runAction("done", sender, json{{"c", c}, {"i", pick(cards)}}.dump());
		break;
	case 2: screen.runAction("add", sender, json{{"c", c}, {"id", nextId++}, {"v", v}}.dump()); break;
	case 3:
		if(cards) screen.runAction("drop", sender, json{{"c", c}, {"i", pick(cards)}}.dump());
		break;
	case 4: screen.runAction("name", sender, json{{"c", c}, {"v", v}}.dump()); break;
	case 5: screen.runAction("count", sender); break;
	case 6: screen.runAction("flag", sender); break;
	default: screen.runAction("filter", sender, json{{"v", v}}.dump()); break;
	}
}

/// @brief Checks random changes of a board: after each update, the panels are those a full build gives.
/// @param document The board.
/// @param seeds The random runs.
void checkUpdates(const std::string& document, unsigned seeds) {
	const ManualClock clock;
	for(unsigned seed = 1; seed <= seeds; ++seed) {
		const auto window = createHeadlessWindow(640, 480, clock);
		affresco::Screen screen = affresco::Screen::fromString(document);
		screen.build(*window);
		window->render();
		std::mt19937 random(seed);
		int nextId = 1000;
		for(int step = 0; step < 40; ++step) {
			const int batch = 1 + static_cast<int>(random() % 3); // one update for several changes, sometimes
			for(int i = 0; i < batch; ++i) change(screen, window->root(), random, nextId);
			window->render();
			std::string incremental;
			describe(window->root(), incremental);
			INFO("seed " << seed << ", step " << step);
			REQUIRE(incremental == builtAgain(screen, clock));
		}
	}
}

} // namespace

TEST_CASE("format: updates that visit only what reads the change give the panels a full build gives") {
	checkUpdates(board, 20);
}

TEST_CASE("format: updates of a long list, looked up by item, give the panels a full build gives") {
	std::string text = board;
	const std::string shared = R"("when": "{{state.flag && card.done}}", "text": "{{c}}:{{state.count}}")";
	text.replace(text.find(shared), shared.size(), R"("when": "{{card.done}}", "text": "{{card.id}}")"); // only its item
	json document = json::parse(text);
	json& cards = document["state"]["columns"][0]["cards"];
	for(int id = 10; id < 90; ++id) // more than ItemIndex::minChildren
		cards.push_back({{"id", id}, {"title", id % 2 ? "a" : "b"}, {"done", id % 3 == 0}});
	checkUpdates(document.dump(), 5);
}
