/// @file
/// @brief Unit tests for budgets and data rules (VERSION_CONTROL.md, "Budgets" and "Data rules"): a table of revisions,
/// authors and the decision each gets (allow, ask or deny), the reasons, what a limited author reads, and the format.

#include "Helpers.hpp"
#include "permissions/Permissions.hpp"
#include "permissions/Policy.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;

namespace {

/// @brief The permissions screen with three cards on the board, a secret in state, budgets and a data rule.
/// @return The screen.
json budgetScreen() {
	json screen = permissionsScreen();
	screen["state"] = {{"count", 0}, {"cards", {1, 2}}, {"secret", "s"}, {"theme", "light"}};
	screen["permissions"]["allow"] = {"studio", "agent(#board)", "agent(/state)", "writer", "role:design(#title)"};
	screen["permissions"]["budgets"] = {{{"author", "agent"}, {"added", 2}, {"removed", 1}, {"changed", 3}},
	                                    {{"author", "writer"}, {"added", 1}, {"over", "ask"}}};
	screen["permissions"]["data"] = {{{"author", "agent"},
	                                  {"read", {"/state/count", "/state/cards"}},
	                                  {"write", {"/state/cards"}},
	                                  {"never", {"/state/secret"}}}};
	json& board = screen["root"]["children"][0];
	for(const char* id : {"card1", "card2", "card3"}) board["children"].push_back({{"type", "panel"}, {"id", id}});
	return screen;
}

/// @brief A revision that adds panels to the board.
/// @param author Its author.
/// @param count How many.
/// @return The revision.
json addPanels(const std::string& author, int count) {
	json patch = json::array();
	for(int i = 0; i < count; ++i)
		patch.push_back({{"op", "add"},
		                 {"path", "#board/children/-"},
		                 {"value", {{"type", "panel"}, {"id", "new" + std::to_string(i)}}}});
	return {{"author", author}, {"patch", patch}};
}

/// @brief A revision with one operation.
/// @param author Its author.
/// @param operation The operation.
/// @return The revision.
json single(const std::string& author, const json& operation) {
	return {{"author", author}, {"patch", json::array({operation})}};
}

/// @brief One row of the table: a revision and the decision it gets.
struct Row {
	const char* name;     ///< What it shows.
	json revision;        ///< The revision.
	const char* decision; ///< "allow", "ask" or "deny".
	const char* reason;   ///< A part of the reason; empty for allow.
};

} // namespace

TEST_CASE("policy: budgets and data rules decide allow, ask or deny") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, budgetScreen());
	const json replaceCard = {{"op", "replace"}, {"path", "/state/cards/0"}, {"value", 5}};
	const json replaceCount = {{"op", "replace"}, {"path", "/state/count"}, {"value", 1}};
	const json replaceSecret = {{"op", "replace"}, {"path", "/state/secret"}, {"value", "t"}};
	const json testSecret = {{"op", "test"}, {"path", "/state/secret"}, {"value", "s"}};
	const json copySecret = {{"op", "copy"}, {"from", "/state/secret"}, {"path", "/state/cards/0"}};
	const json readTheme = {{"op", "test"}, {"path", "/state/theme"}, {"value", "light"}};
	const json removeTwo =
	    json::array({{{"op", "remove"}, {"path", "#card1"}}, {{"op", "remove"}, {"path", "#card2"}}});
	const json colorTitle = {{"op", "replace"}, {"path", "#title/text"}, {"value", "Done"}};
	const Row table[] = {
	    {"within the agent's rules and budget", addPanels("agent/claude-code", 2), "allow", ""},
	    {"one panel over the agent's budget", addPanels("agent/claude-code", 3), "deny",
		 "over the budget \"agent: 2 added, 1 removed, 3 changed\""},
	    {"two removals, over a budget of one",
		 {{"author", "agent"}, {"patch", removeTwo}},
		 "deny",
		 "2 removals, at most 1"},
	    {"one removal, within it", single("agent", {{"op", "remove"}, {"path", "#card1"}}), "allow", ""},
	    {"a path it may write", single("agent", replaceCard), "allow", ""},
	    {"a path it may read but not write", single("agent", replaceCount), "deny", "may write only"},
	    {"a path it may never touch", single("agent", replaceSecret), "deny", "may not read /state/secret"},
	    {"a test reads a path it may never touch", single("agent", testSecret), "deny", "/state/secret"},
	    {"a copy reads it too", single("agent", copySecret), "deny", "/state/secret"},
	    {"a path outside its read paths", single("agent", readTheme), "deny", "/state/theme"},
	    {"over a budget that asks", addPanels("writer", 2), "ask", "over the budget \"writer: 1 added\""},
	    {"within that budget", addPanels("writer", 1), "allow", ""},
	    {"the deny rule beats every budget", single("studio", {{"op", "remove"}, {"path", "#statusBar"}}), "deny",
		 "#statusBar"},
	    {"no budget for the studio", addPanels("studio", 5), "allow", ""},
	    {"the data rule is the agent's only", single("studio", replaceSecret), "allow", ""},
	    {"no rule allows it", single("human/ann", colorTitle), "ask", "no rule allows"},
	};
	for(const Row& row : table) {
		const std::string name = row.name;
		CAPTURE(name);
		const affresco::vcs::Repository::Judgement judgement = repository.judge(row.revision);
		CHECK(judgement.decision == row.decision);
		CAPTURE(judgement.reason);
		CHECK(judgement.reason.find(row.reason) != std::string::npos);
	}
	// Judged by effect, as merges are: writing a path it may never touch names the rule
	const json before = affresco::vcs::detail::view(budgetScreen(), budgetScreen()["state"]);
	json after = before;
	after["state"]["secret"] = "t";
	const auto denied = affresco::vcs::detail::dataDenial(rules, affresco::vcs::detail::policyOf(budgetScreen()),
	                                                      before, after, {"agent", repository.roles()});
	REQUIRE(denied);
	CHECK(denied->find("may never touch /state/secret") != std::string::npos);
	// judge() changes nothing
	CHECK(repository.document() == budgetScreen());
}

TEST_CASE("policy: rules name roles, which only the host or a reviewer sets") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, budgetScreen());
	const json title = single("human/ann", {{"op", "replace"}, {"path", "#title/text"}, {"value", "Done"}});
	CHECK(repository.judge(title).decision == "ask");
	REQUIRE_FALSE(repository.roles().set("human/ann", "design"));
	CHECK(repository.judge(title).decision == "allow");
	// The role covers #title only, and a role is no author: "design" itself gets nothing
	CHECK(repository.judge(single("human/ann", {{"op", "remove"}, {"path", "#board"}})).decision == "ask");
	CHECK(repository.judge(single("design", {{"op", "replace"}, {"path", "#title/text"}, {"value", "x"}})).decision ==
	      "ask");
	// A revision can't carry a role or a budget: a "role" key means nothing, and permissions are a capability
	CHECK(repository.judge({{"author", "agent"}, {"role", "design"}, {"set", {{"#title/text", "x"}}}}).decision ==
	      "ask");
	CHECK(repository.judge(single("agent", {{"op", "remove"}, {"path", "/permissions/budgets"}})).decision == "deny");
	CHECK(repository.roles().set("*", "design"));
	CHECK(repository.roles().set("human/ann", "Design!"));
}

TEST_CASE("policy: a limited author reads only its paths, and a submit naming another is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, budgetScreen());
	const std::optional<json> view = repository.readable("agent/claude-code");
	REQUIRE(view);
	CHECK((*view)["state"]["count"] == 0);
	CHECK((*view)["state"]["cards"] == json({1, 2}));
	CHECK_FALSE((*view)["state"].contains("secret"));
	CHECK_FALSE((*view)["state"].contains("theme"));
	CHECK((*view)["root"]["id"] == "main");
	CHECK_FALSE(repository.readable("studio"));

	const affresco::RevisionResult refused =
	    repository.submit(single("agent", {{"op", "test"}, {"path", "/state/secret"}, {"value", "s"}}));
	CHECK(refused.status == RevisionStatus::Rejected);
	CHECK(refused.error.find("permission denied") != std::string::npos);
	CHECK(repository.submit(single("agent", {{"op", "replace"}, {"path", "/state/cards/1"}, {"value", 7}})).status ==
	      RevisionStatus::Applied);
}

TEST_CASE("policy: nodes are counted by id, however the patch was written") {
	const json before = budgetScreen();
	json after = before;
	after["root"]["children"][0]["children"].erase(0);                                          // card1 removed
	after["root"]["children"][0]["children"][0]["color"] = "#000000";                           // card2 changed
	after["root"]["children"][1]["children"] = json::array({{{"type", "label"}, {"id", "x"}}}); // x added
	const affresco::vcs::detail::NodeCounts counts = affresco::vcs::detail::countNodes(before, after);
	CHECK(counts.added == 1);
	CHECK(counts.removed == 1);
	CHECK(counts.changed == 1); // card2; title only gained children
}

TEST_CASE("policy: nodes without ids, or with an id seen before, count too") {
	const json before = budgetScreen();
	json after = before;
	json many = json::array();
	for(int i = 0; i < 5; ++i) many.push_back({{"type", "label"}, {"text", std::to_string(i)}}); // no ids
	many.push_back({{"type", "label"}, {"id", "card2"}});                                        // an id taken
	after["root"]["children"][1]["children"] = many;
	const affresco::vcs::detail::NodeCounts counts = affresco::vcs::detail::countNodes(before, after);
	CHECK(counts.added == 6); // once, nodes without ids passed any budget
	CHECK(counts.removed == 0);
}

TEST_CASE("policy: the format checks budgets and data rules") {
	const affresco::format::Rules rules;
	const auto with = [](const char* key, const json& value) {
		json screen = permissionsScreen();
		screen["permissions"][key] = value;
		return screen;
	};
	CHECK_NOTHROW(rules.validate(budgetScreen()));
	CHECK_THROWS(rules.validate(with("budgets", {{{"author", "agent"}}})));
	CHECK_THROWS(rules.validate(with("budgets", {{{"author", "agent"}, {"added", -1}}})));
	CHECK_THROWS(rules.validate(with("budgets", {{{"author", "agent"}, {"added", 1}, {"over", "allow"}}})));
	CHECK_THROWS(rules.validate(with("budgets", {{{"author", "agent(#board)"}, {"added", 1}}})));
	CHECK_THROWS(rules.validate(with("budgets", {{{"added", 1}}})));
	CHECK_THROWS(rules.validate(with("data", {{{"author", "agent"}, {"read", {"state.count"}}}})));
	CHECK_THROWS(rules.validate(with("data", {{{"author", "agent"}, {"share", {"/state"}}}})));
	json many = json::array();
	for(int i = 0; i < 65; ++i) many.push_back({{"author", "agent"}, {"added", 1}});
	CHECK_THROWS(rules.validate(with("budgets", many)));
}
