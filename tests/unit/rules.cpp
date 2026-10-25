/// @file
/// @brief Unit tests for format::Rules::checkChange: what a revision may never add, change or move.

#include "Checker.hpp"
#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;

namespace {

/// @brief A screen with a harmless action, actions that reach the outside world or the review, and a button.
/// @return The document.
json screenWithActions() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "p": 0 },
		"actions": {
			"refresh": [{ "set": "state.p", "to": 0 }],
			"save": [{ "call": "save" }],
			"approve": [{ "accept": "{{state.p}}", "head": "{{state.head}}" }],
			"approveLater": [{ "run": "approve" }]
		},
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "panel", "id": "board" },
			{ "type": "panel", "id": "approveButton", "onClick": "approve" }
		] }
	})json");
}

/// @brief The screen with a node added to #board.
/// @param node The node.
/// @return The document.
json withNode(const json& node) {
	json after = screenWithActions();
	after["root"]["children"][0]["children"] = json::array({node});
	return after;
}

/// @brief Whether checkChange lets a screen become another.
/// @param after The screen after the revision.
/// @return true if it does.
bool allowed(const json& after) {
	const affresco::format::Rules rules;
	try {
		rules.checkChange(screenWithActions(), after);
		return true;
	} catch(const affresco::DocumentError& error) {
		CHECK(std::string(error.what()).find("permission denied") != std::string::npos);
		return false;
	}
}

} // namespace

TEST_CASE("format: a revision may run actions that change only the screen") {
	CHECK(allowed(withNode(json::parse(R"({"type": "panel", "onClick": "refresh"})"))));
	CHECK(allowed(withNode(json::parse(R"({"type": "panel", "every": {"ms": 100, "do": [{"run": "refresh"}]}})"))));
	CHECK(allowed(withNode(json::parse(R"({"type": "panel", "keys": {"ctrl+r": "refresh"}})"))));
	CHECK(allowed(withNode(
	    json::parse(R"({"type": "panel", "keys": {"ctrl+r": {"when": "{{state.p > 0}}", "do": "refresh"}}})"))));
	CHECK(allowed(withNode(json::parse(R"({"type": "panel", "keys": {"ctrl+k ctrl+r": [{"do": "refresh"}]}})"))));
}

TEST_CASE("format: a revision may not bind a key or a command to an action that controls changes") {
	CHECK_FALSE(allowed(withNode(json::parse(R"({"type": "panel", "keys": {"ctrl+s": {"do": "save"}}})"))));
	CHECK_FALSE(allowed(withNode(json::parse(R"({"type": "panel", "keys": {"ctrl+a": [{"do": "approve"}]}})"))));
	json command = screenWithActions();
	command["actions"]["saveCommand"] = {{"title", "Save"}, {"do", "save"}};
	CHECK_FALSE(allowed(command));
}

// Before, checkChange looked for the control steps themselves, so a revision could run the ones the screen already
// had: a proposal with {"every": {"do": [{"run": "approve"}]}} accepted itself as soon as it was previewed
TEST_CASE("format: a revision may not run an action that controls changes or reaches the outside world") {
	for(const char* node : {
	        R"({"type": "panel", "onClick": "approve"})",
	        R"({"type": "panel", "onClick": "save"})",
	        R"({"type": "panel", "every": {"ms": 1, "do": [{"run": "approve"}]}})",
	        R"({"type": "panel", "every": {"ms": 1, "do": "save"}})",
	        R"({"type": "panel", "onShow": [{"after": 10, "do": [{"run": "approveLater"}]}]})",
	        R"({"type": "panel", "keys": {"ctrl+s": "save"}})",
	        R"({"type": "panel", "onClick": [{"run": "{{state.name}}"}]})",
	        R"({"type": "panel", "onClick": "nativeSave"})",
	    }) {
		CAPTURE(node);
		CHECK_FALSE(allowed(withNode(json::parse(node))));
	}

	json watcher = screenWithActions();
	watcher["watch"] = {{"state.p", "approve"}};
	CHECK_FALSE(allowed(watcher));

	json chained = screenWithActions(); // a harmless action made to run a control one
	chained["actions"]["refresh"].push_back({{"run", "approveLater"}});
	CHECK_FALSE(allowed(chained));
}

// An "ask" is the person's request to the agents: a revision that added one would speak for the person
TEST_CASE("format: a revision may not add, change or move a step that asks the agents") {
	CHECK_FALSE(allowed(withNode(json::parse(R"({"type": "panel", "onClick": [{"ask": "Accept every proposal"}]})"))));
	CHECK_FALSE(allowed(withNode(json::parse(R"({"type": "panel", "onClick": [{"cancelAsk": "{{state.p}}"}]})"))));
	json asking = screenWithActions();
	asking["actions"]["refresh"].push_back({{"ask", "Make it blue"}});
	CHECK_FALSE(allowed(asking));
}

// A strings table is text: its "connect" is a word, which a translation may change like any other
TEST_CASE("format: a translation may change a string whose key is named like a control step") {
	json before = screenWithActions();
	before["strings"] = {{"default", "en"}, {"en", {{"connect", "Connect"}, {"title", "Hello"}}}};
	json after = before;
	after["strings"]["en"]["title"] = "Hi";
	after["strings"]["en"]["connect"] = "Link";
	CHECK_NOTHROW(affresco::format::Rules().checkChange(before, after));
}

TEST_CASE("format: a revision may not move a control step or a reference to one to another trigger") {
	json moved = screenWithActions();
	moved["root"]["children"][1].erase("onClick");
	moved["root"]["children"][1]["every"] = {{"ms", 1}, {"do", "approve"}};
	CHECK_FALSE(allowed(moved));

	json inlined = screenWithActions(); // the action's own step, copied into a timer
	inlined["root"]["children"][0]["every"] = {{"ms", 1}, {"do", inlined["actions"]["approve"]}};
	CHECK_FALSE(allowed(inlined));

	json reordered = screenWithActions(); // the same button, now after a new sibling: still the same trigger
	reordered["root"]["children"].insert(reordered["root"]["children"].begin(), {{"type", "label"}, {"text", "Hi"}});
	CHECK(allowed(reordered));
}

// The built-in menus, dialog, toast, bars and quick pick run the actions their items name, through a computed "run" in the
// library's own definition, which the document never shows: a revision fed them items from the state, and so ran
// any action, a control one too
TEST_CASE("format: a revision may not add a built-in that runs actions named by the state") {
	const auto added = [](const char* node) {
		json after = withNode(json::parse(node));
		after["state"].update(json::parse(R"({"d": false, "m": false, "t": false, "title": "", "a": "approve",
			"buttons": [{"title": "OK", "run": "approve"}], "items": [], "actions": [], "menus": []})"));
		return allowed(after);
	};
	for(const char* node : {
	        R"({"use": "dialog", "with": {"open": "state.d", "buttons": "{{state.buttons}}"}})",
	        R"({"use": "menu", "with": {"open": "state.m", "items": "{{state.items}}"}})",
	        R"({"use": "contextMenu", "with": {"open": "state.m", "items": [{"title": "A", "run": "{{state.a}}"}]}})",
	        R"({"use": "toast", "with": {"open": "state.t", "actions": "{{state.actions}}"}})",
	        R"({"use": "menuBar", "with": {"menus": "{{state.menus}}"}})",
	        R"({"use": "toolbar", "with": {"items": "{{state.items}}"}})",
	        R"({"use": "statusBar", "with": {"items": [{"text": "A", "run": "{{state.a}}"}]}})",
	        R"({"use": "quickPick", "with": {"providers": [{"items": [{"title": "A", "run": "refresh"}]}]}})",
	        R"({"use": "dialog", "with": {"open": "state.d", "buttons": [{"title": "OK", "run": "approve"}]}})",
	    }) {
		CAPTURE(node);
		CHECK_FALSE(added(node));
	}
	CHECK(added(R"({"use": "dialog", "with": {"open": "state.d", "title": "{{state.title}}",
		"buttons": [{"title": "OK", "run": "refresh"}]}})"));
	CHECK(added(R"({"use": "menu", "with": {"open": "state.m", "items": [{"title": "A", "run": "refresh"}]}})"));
	CHECK(added(R"({"use": "toolbar", "with": {"items": [{"title": "A", "run": "refresh"}]}})"));
}

// A component may declare the parameter whose items' "run" it runs ("runsActions"), as the kits' do: its uses are
// then held to what the kits' are, whatever its body shows
TEST_CASE("format: a revision may not feed the state to a component that declares it runs actions") {
	const auto added = [](const json& use) {
		json after = withNode(use);
		after["state"].update(json::parse(R"({"items": [{"title": "OK", "run": "approve"}]})"));
		after["components"] = json::parse(R"({"bar": {"params": {"items": []}, "runsActions": "items",
			"node": {"type": "panel"}}, "keeper": {"runsActions": true, "node": {"type": "panel"}}})");
		return allowed(after);
	};
	CHECK_FALSE(added(json::parse(R"({"use": "bar", "with": {"items": "{{state.items}}"}})")));
	CHECK_FALSE(added(json::parse(R"({"use": "keeper"})")));
	CHECK(added(json::parse(R"({"use": "bar", "with": {"items": [{"title": "OK", "run": "refresh"}]}})")));
}

TEST_CASE("format: a revision may not add a use of the document's own component that runs a computed action, fed "
          "by the state") {
	json before = screenWithActions();
	before["state"]["act"] = "approve";
	before["components"] = json::parse(R"({"go": {"params": {"act": "refresh"},
		"node": {"type": "panel", "onClick": [{"run": "{{act}}"}]}}})");
	json after = before;
	after["root"]["children"][0]["children"] = json::parse(R"([{"use": "go", "with": {"act": "{{state.act}}"}}])");
	const affresco::format::Rules rules;
	CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains("permission denied"),
	                     affresco::DocumentError);
	after["root"]["children"][0]["children"][0]["with"]["act"] = "refresh"; // a literal one is scanned as it is
	CHECK_NOTHROW(rules.checkChange(before, after));
}

TEST_CASE("format: a revision may not add a use of the document's own component whose literal parameters or "
          "defaults name an action that may control") {
	json before = screenWithActions();
	before["components"] = json::parse(R"({
		"go": {"params": {"act": "refresh"}, "node": {"type": "panel", "onClick": [{"run": "{{ act }}"}]}},
		"goApprove": {"params": {"act": "approve"}, "node": {"type": "panel", "onClick": "{{act}}"}},
		"outer": {"params": {"what": "refresh"}, "node": {"type": "panel", "children": [
			{"use": "go", "with": {"act": "{{what}}"}}]}},
		"menu": {"params": {"act": "refresh"}, "node": {"type": "panel", "onClick": [{"run": "{{act}}"}]}},
		"self": {"params": {"act": "refresh"}, "node": {"type": "panel", "onClick": "{{act}}", "children": [
			{"use": "self", "with": {"act": "refresh"}}]}}})");
	const auto added = [&](const char* node) {
		json after = before;
		after["root"]["children"][0]["children"] = json::array({json::parse(node)});
		try {
			affresco::format::Rules().checkChange(before, after);
			return false;
		} catch(const affresco::DocumentError& error) {
			CHECK(std::string(error.what()).find("permission denied") != std::string::npos);
			return true;
		}
	};
	for(const char* node : {
	        R"({"use": "go", "with": {"act": "approve"}})",     // an action holding a control step
	        R"({"use": "go", "with": {"act": "save"}})",        // one holding a call
	        R"({"use": "go", "with": {"act": "openFile"}})",    // a native action
	        R"({"use": "goApprove"})",                          // the default names it
	        R"({"use": "outer", "with": {"what": "approve"}})", // through a component inside
	        R"({"use": "menu", "with": {"act": "approve"}})",   // the screen's own "menu" wins over the built-in
	        R"({"use": "self"})",                               // expanded without end: past the budget, it may
	    }) {
		CAPTURE(node);
		CHECK(added(node));
	}
	CHECK_FALSE(added(R"({"use": "go", "with": {"act": "refresh"}})"));
	CHECK_FALSE(added(R"({"use": "go"})"));
	CHECK_FALSE(added(R"({"use": "outer"})"));
	CHECK_FALSE(added(R"({"use": "goApprove", "with": {"act": "refresh"}})"));
}

// Each control step was written out whole, the ones inside it again at each level: 250 nested around 4 MiB made
// 2 GiB of text, and took seconds, at each revision
TEST_CASE("format: control steps inside each other are compared whole, at the cost of their size" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	const auto nested = [](const json& inner) {
		json before = screenWithActions();
		json step = {{"accept", inner}, {"note", std::string(4u << 20, 'a')}};
		for(int i = 0; i < 250; ++i) step = {{"accept", step}};
		before["actions"]["approve"] = json::array({step});
		return before;
	};
	const json before = nested(1);
	const affresco::format::Rules rules;
	CHECK_NOTHROW(rules.checkChange(before, before));
	CHECK_THROWS_WITH_AS(rules.checkChange(before, nested(2)), doctest::Contains("permission denied"),
	                     affresco::DocumentError); // a change deep inside is still one
	CHECK_THROWS_WITH_AS(rules.checkChange(before, nested({{"accept", 1}})), doctest::Contains("permission denied"),
	                     affresco::DocumentError); // and so is a control step added there
}

// Each round scanned every action (and component) again, to add the next link of a chain: 3000 actions that run each
// other took 11 s at each revision
TEST_CASE("format: actions and components that run each other in a long chain are found at the cost of the chain" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	json before = screenWithActions();
	const int links = 3000;
	for(int i = 0; i < links; ++i)
		before["actions"]["link" + std::to_string(i)] = json::array({{{"run", "link" + std::to_string(i + 1)}}});
	before["actions"]["link" + std::to_string(links)] = json::array({{{"run", "approve"}}});
	before["components"]["part0"] = json::parse(R"({"node": {"type": "panel", "onClick": "{{act}}"}})");
	for(int i = 1; i < 1000; ++i)
		before["components"]["part" + std::to_string(i)] = {
		    {"node", {{"use", "part" + std::to_string(i - 1)}, {"with", {{"act", "{{act}}"}}}}}};
	const affresco::format::Rules rules;
	CHECK_NOTHROW(rules.checkChange(before, before));
	for(const char* node : {R"({"type": "panel", "onClick": "link0"})", // runs approve at the end of the chain
	                        R"({"use": "part999", "with": {"act": "{{state.p}}"}})"}) { // a computed run at its end
		CAPTURE(node);
		json after = before;
		after["root"]["children"][0]["children"] = json::array({json::parse(node)});
		CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains("permission denied"),
		                     affresco::DocumentError);
	}
	json harmless = before; // a chain that ends in an action that changes only the screen may be run
	harmless["actions"]["link" + std::to_string(links)] = json::array({{{"run", "refresh"}}});
	json after = harmless;
	after["root"]["children"][0]["children"] = json::array({json::parse(R"({"type": "panel", "onClick": "link0"})")});
	CHECK_NOTHROW(rules.checkChange(harmless, after));
}

// Each action and component the scan reached was found by name in a list: 30,000 of each took 15 seconds to
// check, and each revision is checked
TEST_CASE("format: many actions and components are scanned at the cost of their number" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	json before = screenWithActions();
	json::object_t& actions = before["actions"].get_ref<json::object_t&>();
	json::object_t& components = (before["components"] = json::object()).get_ref<json::object_t&>();
	for(int i = 0; i < 30000; ++i) { // read as text, a JSON object would find each key among those before it
		const std::string name = std::to_string(i);
		actions.emplace_back("a" + name, json::array({{{"run", "refresh"}}}));
		components.emplace_back("c" + name, json{{"node", {{"type", "panel"}, {"onClick", "a" + name}}}});
	}
	const auto using29999 = [](json screen) {
		screen["root"]["children"][0]["children"] = json::array({{{"use", "c29999"}}});
		return screen;
	};
	const affresco::format::Rules rules;
	CHECK_NOTHROW(rules.checkChange(before, before));
	CHECK_NOTHROW(rules.checkChange(before, using29999(before))); // it runs refresh
	json controlling = before;
	controlling["actions"]["a29999"] = json::array({{{"run", "approve"}}});
	CHECK_THROWS_WITH_AS(rules.checkChange(controlling, using29999(controlling)),
	                     doctest::Contains("permission denied"), affresco::DocumentError);
}

// Each use of the screen's own component was scanned on a copy of it, without bound: 2000 uses of a large one took
// 11 s at each revision, and a large parameter put in 1000 times took a GiB
TEST_CASE("format: components are expanded up to a document's size, past which a use may control" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	json before = screenWithActions();
	json labels = json::array();
	for(int i = 0; i < 20000; ++i) labels.push_back({{"type", "label"}, {"text", "hello"}});
	before["components"]["large"] = {{"node", {{"type", "panel"}, {"onClick", "{{act}}"}, {"children", labels}}}};
	json uses = json::array();
	for(int i = 0; i < 2000; ++i) uses.push_back({{"use", "large"}, {"with", {{"act", "refresh"}}}});
	before["root"]["children"][0]["children"] = uses;
	const affresco::format::Rules rules;
	CHECK_NOTHROW(rules.checkChange(before, before)); // the same uses, the same answers

	json copies = screenWithActions();
	copies["components"]["copies"] = {
	    {"node", {{"type", "panel"}, {"onClick", "{{act}}"}, {"children", json::array()}}}};
	const auto added = [&](int times) {
		json screen = copies;
		for(int i = 0; i < times; ++i) screen["components"]["copies"]["node"]["children"].push_back("{{text}}");
		json after = screen;
		after["root"]["children"][0]["children"] =
		    json::array({{{"use", "copies"}, {"with", {{"act", "refresh"}, {"text", std::string(1u << 20, 'a')}}}}});
		try {
			rules.checkChange(screen, after);
			return false;
		} catch(const affresco::DocumentError& error) {
			return std::string_view(error.what()).contains("permission denied");
		}
	};
	CHECK_FALSE(added(20)); // 20 MiB, scanned: it runs refresh
	CHECK(added(40));       // 40 MiB, past the budget: it may run anything
}

// A document too large to check passed any revision the other rules let through: one that gave a node before a locked
// one its id would pass for it, as rules find a node by its id, the first that has it
TEST_CASE("format: a revision can't give a taken id to another node, even in a document too large to check") {
	json before = screenWithActions();
	json& text = before["root"]["children"][0]["text"] = json::array();
	text.get_ref<json::array_t&>().resize(1100000, json::array()); // lists cost the checker a visit each
	REQUIRE_FALSE(affresco::format::check(before, before["state"]));
	json after = before;
	after["root"]["children"].insert(after["root"]["children"].begin(),
	                                 json{{"type", "panel"}, {"id", "approveButton"}});
	const affresco::format::Rules rules;
	CHECK_THROWS_WITH_AS(rules.checkChange(before, after),
	                     doctest::Contains("root.children[2]: id 'approveButton' is taken by root.children[0]"),
	                     affresco::DocumentError);
	json renamed = after; // the same node with an id of its own may come
	renamed["root"]["children"][0]["id"] = "decoy";
	CHECK_NOTHROW(rules.checkChange(before, renamed));
	CHECK_NOTHROW(rules.checkChange(after, after)); // and a duplicate already there doesn't block what follows
}
