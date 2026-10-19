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
