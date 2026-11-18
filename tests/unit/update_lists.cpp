/// @file
/// @brief Lists kept in place: an insert, a removal and a reorder touch only the rows that changed, a row keeps its
/// panel by its key and its number until it moves, and Panel's moves keep the focus, hover and capture.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
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
	affresco::test::verifyNextRuntimes(on);
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
	/// @brief What the last update cost.
	/// @return It.
	[[nodiscard]] const affresco::format::UpdateStatistics& last() const { return runtime.lastUpdate(); }
	/// @brief The panel that holds the rows (the root's first child).
	/// @return It.
	[[nodiscard]] affresco::Panel& list() const { return *window->root().children()[0]; }
};

/// @brief A screen with a keyed list of rows, each a row and a label.
/// @param count The rows.
/// @param row Its row node, with "children" filled in if it has none.
/// @param label The label's text.
/// @return The document.
json listScreen(int count, json row = json::object(), const std::string& label = "{{item.v}}") {
	json items = json::array();
	for(int i = 0; i < count; ++i) items.push_back({{"id", i}, {"v", i}});
	json document = json::parse(R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "filter": -1, "other": 0 },
		"actions": {
			"push": [{ "push": "state.items", "value": { "id": "{{event.id}}", "v": "{{event.id}}" } }],
			"insert": [{ "set": "state.items", "to":
				"{{concat(slice(state.items, 0, event.at), [{ id: event.id, v: event.id }], slice(state.items, event.at))}}" }],
			"remove": [{ "remove": "state.items", "at": "{{event.at}}" }],
			"list": [{ "set": "state.items", "to": "{{event.list}}" }],
			"reverse": [{ "set": "state.items", "to": "{{reverse(state.items)}}" }],
			"touch": [{ "set": "state.items[event.at].v", "to": "{{event.v}}" }],
			"key": [{ "set": "state.items[event.at].id", "to": "{{event.v}}" }],
			"filter": [{ "set": "state.filter", "to": "{{event.v}}" }],
			"other": [{ "increment": "state.other" }]
		} })json");
	document["state"]["items"] = std::move(items);
	row["type"] = "row";
	if(!row.contains("repeat")) row["repeat"] = {{"for", "item"}, {"in", "state.items"}, {"key", "{{item.id}}"}};
	if(!row.contains("children")) row["children"] = json::array({{{"type", "label"}, {"text", label}}});
	document["root"] = {{"type", "panel"}, {"children", json::array({{{"type", "column"}, {"children", {row}}}})}};
	return document;
}

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

/// @brief The texts of a list's labels, in order.
/// @param built The screen.
/// @return They.
std::vector<std::string> texts(const Built& built) {
	std::vector<std::string> out;
	for(const auto& row : built.list().children())
		if(const auto* label = dynamic_cast<const affresco::Label*>(row->children()[0].get()))
			out.push_back(label->text());
	return out;
}

/// @brief The panels of a list's rows, by their item's id.
/// @param built The screen.
/// @return They.
std::map<std::string, const affresco::Panel*> byId(const Built& built) {
	std::map<std::string, const affresco::Panel*> out;
	for(const auto& row : built.list().children())
		if(const auto* label = dynamic_cast<const affresco::Label*>(row->children()[0].get()))
			out[label->text()] = row.get();
	return out;
}

/// @brief A plain panel at a place, of a size.
/// @param x Its left.
/// @param y Its top.
/// @return It.
std::unique_ptr<affresco::Panel> box(float x, float y) {
	auto panel = std::make_unique<affresco::Panel>();
	panel->setPosition(affresco::px(x), affresco::px(y));
	panel->setSize(affresco::px(40), affresco::px(40));
	return panel;
}

} // namespace

TEST_CASE("panel: insertChild, takeChild and reorderChildren keep the focus, hover and capture on a panel that moves") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Panel& root = window->root();
	affresco::Panel& a = root.addChild(box(0, 0));
	affresco::Panel& c = root.addChild(box(100, 0));
	affresco::Panel& b = root.insertChild(1, box(50, 0));
	REQUIRE(root.children().size() == 3);
	CHECK(root.children()[1].get() == &b);
	CHECK_THROWS_AS(static_cast<void>(root.insertChild(4, box(0, 0))), std::out_of_range);
	window->render();

	window->setFocus(&b);
	window->routeMouseMove({60, 10});
	window->routeMouseButton({affresco::MouseButton::Right, affresco::Action::Press, {}, {60, 10}});
	REQUIRE(window->hoveredPanel() == &b);
	REQUIRE(window->mouseGrabber() == &b);
	root.reorderChildren(0, {2, 1, 0});
	CHECK(root.children()[0].get() == &c);
	CHECK(root.children()[2].get() == &a);
	CHECK(window->focusedPanel() == &b);
	CHECK(window->hoveredPanel() == &b);
	CHECK(window->mouseGrabber() == &b);
	root.reorderChildren(1, {2, 1});
	CHECK(root.children()[1].get() == &a);
	CHECK_THROWS_AS(root.reorderChildren(1, {1, 1}), std::invalid_argument);
	CHECK_THROWS_AS(root.reorderChildren(0, {0, 1}), std::invalid_argument);
	CHECK(root.children()[2].get() == &b); // nothing moved by a refused order

	const std::unique_ptr<affresco::Panel> taken = root.takeChild(2);
	CHECK(taken.get() == &b);
	CHECK(taken->parent() == nullptr);
	CHECK(window->focusedPanel() == nullptr);
	CHECK(window->hoveredPanel() == nullptr);
	CHECK(window->mouseGrabber() == nullptr);
	CHECK_THROWS_AS(static_cast<void>(root.takeChild(2)), std::out_of_range);
}

TEST_CASE("lists: on 25,000 keyed rows, an insert or a removal builds or destroys only its row") {
	Built built(listScreen(25'000));
	REQUIRE(built.list().children().size() == 25'000U);
	affresco::Panel* focused = built.list().children()[10].get();
	built.window->setFocus(focused);
	int id = 100'000;
	for(const int at : {0, 12'000, 25'000}) {
		CAPTURE(at);
		built.run(at == 25'000 ? "push" : "insert", {{"at", at}, {"id", id}});
		CHECK_FALSE(built.last().full);
		CHECK(built.last().panelsCreated == 2U);
		CHECK(built.last().rowsExpanded == 1U);
		CHECK(built.last().nodesEvaluated == 2U); // no other row
		CHECK(built.list().children().size() == 25'001U);
		CHECK(texts(built)[static_cast<std::size_t>(at)] == std::to_string(id));
		CHECK(built.window->focusedPanel() == focused);
		built.run("remove", {{"at", at}});
		CHECK_FALSE(built.last().full);
		CHECK(built.last().panelsCreated == 0U);
		CHECK(built.last().nodesEvaluated == 0U); // no row reads its number
		CHECK(built.list().children().size() == 25'000U);
		CHECK(built.window->focusedPanel() == focused);
		++id;
	}
	built.run("remove", {{"at", 0}});
	CHECK(built.last().panelsCreated == 0U);
	CHECK(texts(built).front() == "1");
	CHECK(built.errors.empty());
	CHECK(asBuiltAgain(built));
}

TEST_CASE("lists: a removal before a row that shows its number numbers it again, and only those rows") {
	for(const bool checked : {false, true}) {
		Built built(listScreen(5, json::object(), "{{index}}:{{item.v}}"), checked);
		built.run("remove", {{"at", 1}});
		CHECK(built.last().panelsCreated == 0U);
		CHECK(built.last().nodesEvaluated == 3U); // the three rows after it
		CHECK(texts(built) == std::vector<std::string>{"0:0", "1:2", "2:3", "3:4"});
		built.run("insert", {{"at", 3}, {"id", 9}});
		CHECK(built.last().panelsCreated == 2U);
		CHECK(texts(built) == std::vector<std::string>{"0:0", "1:2", "2:3", "3:9", "4:4"});
		built.run("touch", {{"at", 4}, {"v", 7}}); // the row numbered again reads its item where it lies now
		CHECK(texts(built) == std::vector<std::string>{"0:0", "1:2", "2:3", "3:9", "4:7"});
		built.run("remove", {{"at", 0}});
		built.run("touch", {{"at", 0}, {"v", 5}});
		CHECK(texts(built) == std::vector<std::string>{"0:5", "1:3", "2:9", "3:7"});
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: a reverse or a shuffle keeps every row's panel by its key") {
	for(const bool checked : {false, true}) {
		Built built(listScreen(5), checked);
		const auto before = byId(built);
		built.run("reverse");
		CHECK(built.last().panelsCreated == 0U);
		CHECK(texts(built) == std::vector<std::string>{"4", "3", "2", "1", "0"});
		CHECK(byId(built) == before);
		const json shuffled =
		    json::parse(R"([{"id":2,"v":2},{"id":0,"v":0},{"id":4,"v":4},{"id":1,"v":1},{"id":3,"v":3}])");
		built.run("list", {{"list", shuffled}});
		CHECK(built.last().panelsCreated == 0U);
		CHECK(texts(built) == std::vector<std::string>{"2", "0", "4", "1", "3"});
		CHECK(byId(built) == before);
		built.run("touch", {{"at", 0}, {"v", 8}});
		CHECK(texts(built).front() == "8");
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: a row's own \"when\" builds or destroys that row alone") {
	for(const bool checked : {false, true}) {
		Built built(listScreen(25, {{"when", "{{item.v >= 0}}"}}), checked);
		built.run("touch", {{"at", 3}, {"v", -1}});
		CHECK(built.last().rowsExpanded <= 1U);
		CHECK(built.last().panelsCreated == 0U);
		CHECK(built.list().children().size() == 24U);
		built.run("touch", {{"at", 3}, {"v", 30}});
		CHECK(built.last().rowsExpanded <= 1U);
		CHECK(built.last().panelsCreated == 2U);
		CHECK(texts(built)[3] == "30");
		built.run("other");
		CHECK(built.last().rowsExpanded == 0U);
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: a filter every row's \"when\" reads evaluates them all, and builds only the rows that show now") {
	for(const bool checked : {false, true}) {
		Built built(listScreen(25, {{"when", "{{item.v > state.filter}}"}}), checked);
		const auto before = byId(built);
		built.run("filter", {{"v", 9}});
		CHECK(built.last().rowsExpanded == 25U);
		CHECK(built.last().panelsCreated == 0U);
		CHECK(built.list().children().size() == 15U);
		built.run("filter", {{"v", 4}});
		CHECK(built.last().panelsCreated == 10U); // five rows of two panels
		const auto after = byId(built);
		for(int v = 10; v < 25; ++v) CHECK(after.at(std::to_string(v)) == before.at(std::to_string(v)));
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: duplicate keys, a key that changes type and a list that becomes null") {
	for(const bool checked : {false, true}) {
		json document = listScreen(4);
		document["state"]["items"][2]["id"] = 1; // two rows keyed 1
		Built built(document, checked);
		built.run("insert", {{"at", 0}, {"id", 1}});
		built.run("remove", {{"at", 2}});
		built.run("push", {{"id", 1}});
		CHECK(asBuiltAgain(built));
		built.run("key", {{"at", 0}, {"v", "1"}}); // a number to a text
		CHECK(asBuiltAgain(built));
		built.run("list", {{"list", nullptr}});
		CHECK(built.list().children().empty());
		built.run("push", {{"id", 5}});
		CHECK(texts(built) == std::vector<std::string>{"5"});
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: keyed repeats inside keyed repeats keep their rows") {
	for(const bool checked : {false, true}) {
		json document = listScreen(0);
		document["state"]["items"] = json::parse(R"([{"id":"a","v":"a","rows":[{"id":1},{"id":2}]},
			{"id":"b","v":"b","rows":[{"id":3}]}])");
		document["actions"]["inner"] =
		    json::parse(R"([{ "push": "state.items[event.at].rows", "value": { "id": "{{event.id}}" } }])");
		document["actions"]["dropInner"] = json::parse(R"([{ "remove": "state.items[event.at].rows", "at": 0 }])");
		json& row = document["root"]["children"][0]["children"][0];
		row["children"].push_back({{"type", "label"},
		                           {"repeat", {{"for", "sub"}, {"in", "item.rows"}, {"key", "{{sub.id}}"}}},
		                           {"text", "{{item.v}}{{sub.id}}"}});
		Built built(document, checked);
		const affresco::Panel* first = built.list().children()[0].get();
		built.run("inner", {{"at", 0}, {"id", 7}});
		CHECK(built.last().panelsCreated == 1U);
		CHECK(built.list().children()[0].get() == first);
		CHECK(built.list().children()[0]->children().size() == 4U);
		built.run("insert", {{"at", 0}, {"id", "z"}});
		built.run("dropInner", {{"at", 1}});
		CHECK(built.list().children()[1].get() == first);
		CHECK(built.list().children()[1]->children().size() == 3U);
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: a keyed repeat over a computed list keeps its rows' panels") {
	for(const bool checked : {false, true}) {
		json document =
		    listScreen(10, {{"repeat", {{"for", "item"}, {"in", "computed.shown"}, {"key", "{{item.id}}"}}}});
		document["computed"] = {{"shown", "{{filter(state.items, it.v > state.filter)}}"}};
		Built built(document, checked);
		const auto before = byId(built);
		built.run("insert", {{"at", 0}, {"id", 20}});
		CHECK(built.last().panelsCreated == 2U);
		built.run("filter", {{"v", 4}});
		CHECK(built.last().panelsCreated == 0U);
		const auto after = byId(built);
		for(int v = 5; v < 10; ++v) CHECK(after.at(std::to_string(v)) == before.at(std::to_string(v)));
		CHECK(built.errors.empty());
		CHECK(asBuiltAgain(built));
	}
}

TEST_CASE("lists: a series of inserts, removals, moves and changes inside rows, checked against full builds") {
	for(const bool checked : {false, true}) {
		Built built(listScreen(6, {{"when", "{{item.v % 3 != 1}}"}}, "{{index}} {{item.v}}"), checked);
		int id = 50;
		for(int i = 0; i < 30; ++i) {
			CAPTURE(i);
			const auto size = static_cast<int>(built.repository.state()["items"].size());
			switch(i % 5) {
			case 0:
				built.run("insert", {{"at", (i * 7) % (size + 1)}, {"id", id++}});
				break;
			case 1:
				built.run("push", {{"id", id++}});
				break;
			case 2:
				if(size > 0) built.run("remove", {{"at", (i * 5) % size}});
				break;
			case 3:
				if(size > 0) built.run("touch", {{"at", (i * 3) % size}, {"v", i}});
				break;
			default:
				built.run("reverse");
				break;
			}
			REQUIRE(built.errors.empty());
			REQUIRE(asBuiltAgain(built));
		}
	}
}

TEST_CASE("lists: two repeats over one list, the first reconciled in full, the second in part, keep every row's reads") {
	for(const bool checked : {false, true}) {
		for(const bool keyed : {false, true}) {
			CAPTURE(checked);
			CAPTURE(keyed);
			json document = listScreen(3);
			json first = document["root"]["children"][0]["children"][0];
			if(!keyed) first["repeat"].erase("key");
			// Its number and its value in two labels: the number makes the other rows change too
			first["children"] = json::array({json{{"type", "label"}, {"text", "{{index}}"}},
			                                  json{{"type", "label"}, {"text", "{{item.v}}"}}});
			json column = {{"type", "column"}, {"children", json::array({first})}};
			document["root"]["children"].insert(document["root"]["children"].begin(), std::move(column));
			Built built(document, checked);
			built.run("remove", {{"at", 0}});
			REQUIRE(asBuiltAgain(built));
			built.run("touch", {{"at", 1}, {"v", 7}});
			CHECK(built.errors.empty());
			CHECK(asBuiltAgain(built));
			built.run("touch", {{"at", 0}, {"v", 8}});
			CHECK(built.errors.empty());
			CHECK(asBuiltAgain(built));
		}
	}
}

// Found by fuzzing (tests/fuzz, updates): a push, then a remove from a list that isn't there, in one update
TEST_CASE("lists: a state step that fails after making the places on its way updates what reads them") {
	// A remove from a list that isn't there, or a push whose value fails, makes the list's place first (the list of
	// columns grows with nulls): what reads it updates as a full pass does
	json document = listScreen(2);
	document["state"]["columns"] = json::array({json{{"cards", json::array({1})}}});
	document["actions"]["drop"] = json::parse(R"([{ "remove": "state.columns[event.c].cards", "at": 0 }])");
	document["actions"]["add"] =
	    json::parse(R"([{ "push": "state.columns[event.c].cards", "value": "{{nosuch(event.c)}}" }])");
	document["root"]["children"].push_back(
	    json{{"type", "column"},
	         {"children", json::array({json{{"type", "label"},
	                                        {"repeat", {{"for", "column"}, {"in", "state.columns"}}},
	                                        {"text", "{{length(column.cards)}}"}}})}});
	for(const std::string action : {"drop", "add"}) {
		CAPTURE(action);
		Built built(document, true);
		built.runtime.runAction(action, built.window->root(), json{{"c", 3}});
		built.window->routeKey({affresco::Key::Unknown, 0, affresco::Action::Release, {}}); // runs what was deferred
		CHECK(std::ranges::none_of(built.errors, [](const std::string& e) { return e.contains("update check"); }));
		CHECK(asBuiltAgain(built));
	}
}
