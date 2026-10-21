/// @file
/// @brief Unit tests for format::Rules::checkChange: what a revision may never add, change or move.

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
		"version": 1,
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

// The built-in menus, dialog, toast and quick pick run the actions their items name, through a computed "run" in the
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
	        R"({"use": "quickPick", "with": {"providers": [{"items": [{"title": "A", "run": "refresh"}]}]}})",
	        R"({"use": "dialog", "with": {"open": "state.d", "buttons": [{"title": "OK", "run": "approve"}]}})",
	    }) {
		CAPTURE(node);
		CHECK_FALSE(added(node));
	}
	CHECK(added(R"({"use": "dialog", "with": {"open": "state.d", "title": "{{state.title}}",
		"buttons": [{"title": "OK", "run": "refresh"}]}})"));
	CHECK(added(R"({"use": "menu", "with": {"open": "state.m", "items": [{"title": "A", "run": "refresh"}]}})"));
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
