/// @file
/// @brief Unit tests for the vcs module: commits, reverts, permission rules, review and change sets.
///
/// The documents are screens, judged by the format's own rules (format::Rules), as in the app.

#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;

TEST_CASE("vcs: submitting and then reverting a revision restores the document exactly") {
	const affresco::format::Rules rules;
	const json original = permissionsScreen();
	affresco::vcs::Repository repository(rules, original);

	const affresco::RevisionResult applied = repository.submit(
	    {{"author", "studio"}, {"message", "Rename"}, {"set", {{"#title/text", "Done"}, {"#board/color", "#00ff00"}}}});
	REQUIRE(applied.status == RevisionStatus::Applied);
	CHECK(applied.revision == 1);
	CHECK(repository.document().at(json::json_pointer("/root/children/1/text")) == "Done");

	const affresco::RevisionResult reverted = repository.submit(repository.revertRevision("HEAD", "studio"));
	REQUIRE(reverted.status == RevisionStatus::Applied);
	CHECK(reverted.revision == 2);
	CHECK(repository.document().dump() == original.dump()); // same keys, same order
	CHECK(repository.log().size() == 3);                    // the revert is a commit too: main only grows
}

TEST_CASE("vcs: a deny rule refuses a revision at submit") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	for(const char* author : {"agent", "studio"}) { // deny holds for reviewers too
		CAPTURE(author);
		const affresco::RevisionResult result =
		    repository.submit({{"author", author}, {"set", {{"#statusBar/color", "#ff0000"}}}});
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.find("permission denied") != std::string::npos);
		CHECK(result.error.find("*(#statusBar)") != std::string::npos);
	}
	CHECK(repository.head().number == 0);
	CHECK(repository.proposals().empty());
	CHECK(repository.document() == permissionsScreen());
}

TEST_CASE("vcs: an accept that names an outdated head is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	// The agent may only change #board: a change to #title waits for review
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"message", "Retitle"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	const std::string reviewed = proposed.commit;

	// The agent pushes again while the reviewer looks: the head the reviewer saw is now out of date
	const affresco::RevisionResult pushed = repository.submit(
	    {{"author", "agent"}, {"proposal", proposed.proposal}, {"set", {{"#title/text", "Tomorrow"}}}});
	REQUIRE(pushed.status == RevisionStatus::Proposed);
	REQUIRE(pushed.commit != reviewed);

	const auto refused = repository.accept(proposed.proposal, {"studio", reviewed.substr(0, 7), false});
	REQUIRE(refused);
	CHECK(refused->find("changed since you reviewed it") != std::string::npos);
	CHECK(repository.proposals().size() == 1); // still open, for another look
	CHECK(repository.head().number == 0);

	CHECK_FALSE(repository.accept(proposed.proposal, {"studio", pushed.commit, false}));
	CHECK(repository.proposals().empty());
	CHECK(repository.document().at(json::json_pointer("/root/children/1/text")) == "Tomorrow");
	CHECK(repository.head().committer == "studio");
	CHECK(repository.head().author == "agent");
}

// Before, an approval without a head (an "accept" step in a screen, Screen::acceptProposal) merged whatever the
// proposal had become, including commits pushed after the review
TEST_CASE("vcs: an accept that names no head is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"message", "Retitle"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);

	for(const char* head : {"", "abc"}) {
		CAPTURE(head);
		const auto refused = repository.accept(proposed.proposal, {"user", head, false});
		REQUIRE(refused);
		CHECK(refused->find("needs its head commit") != std::string::npos);
	}
	CHECK(repository.proposals().size() == 1);
	CHECK(repository.head().number == 0);
}

TEST_CASE("vcs: an author can't review its own proposal") {
	const affresco::format::Rules rules;
	json screen = permissionsScreen();
	screen["permissions"]["ask"] = {"studio(#title)"}; // even the reviewer's own changes to #title wait
	affresco::vcs::Repository repository(rules, screen);

	SUBCASE("a reviewer") {
		const affresco::RevisionResult proposed =
		    repository.submit({{"author", "studio"}, {"set", {{"#title/text", "x"}}}});
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		const auto refused = repository.accept(proposed.proposal, {"studio", proposed.commit, false});
		REQUIRE(refused);
		CHECK(refused->find("may not accept its own proposal") != std::string::npos);
	}
	SUBCASE("another name on the author's channel") {
		const affresco::RevisionResult proposed =
		    repository.submit({{"author", "agent/claude-code"}, {"set", {{"#title/text", "x"}}}});
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		const auto refused = repository.accept(proposed.proposal, {"agent/other", proposed.commit, false});
		REQUIRE(refused);
		CHECK(refused->find("may not accept its own proposal") != std::string::npos);
	}
	CHECK(repository.proposals().size() == 1);
	CHECK(repository.head().number == 0);
}

TEST_CASE("vcs: a change set exported and imported elsewhere gives the same document") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository source(rules, permissionsScreen());
	REQUIRE(source.submit({{"author", "studio"}, {"set", {{"#title/text", "Week"}, {"/state/count", 3}}}}).status ==
	        RevisionStatus::Applied);
	REQUIRE(source
	            .submit({{"author", "studio"},
	                     {"add", {{"#main/children/-", {{"type", "label"}, {"id", "hint"}, {"text", "Drag cards"}}}}}})
	            .status == RevisionStatus::Applied);
	REQUIRE(source.submit({{"author", "agent"}, {"move", {{"#hint", "#main/children/0"}}}}).status ==
	        RevisionStatus::Proposed); // not on main: not exported

	const json changes = source.exportChanges(0, "week view");
	CHECK(changes.at("format") == "affrescoui-changes");
	CHECK(changes.at("revisions").size() == 2);

	affresco::vcs::Repository target(rules, permissionsScreen());
	REQUIRE_FALSE(target.importChanges(affresco::parseUntrusted(changes.dump()))); // as it would travel
	CHECK(target.document().dump() == source.document().dump());
	CHECK(target.state() == source.state());
	CHECK(target.head().number == 2);
	CHECK(target.head().author == "plugin:week view");
}

// Found by fuzzing (tests/fuzz, revision and session): nlohmann::json asserts when a value is added inside a scalar
TEST_CASE("vcs: a revision that adds inside a number, a string or a boolean is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	const std::vector<std::pair<json, std::string>> cases = {
	    {{{"author", "studio"}, {"set", {{"/version/x", 1}}}}, "'/version' is a number"},
	    {{{"author", "studio"}, {"add", {{"#title/text/x", 1}}}}, "is a string"},
	    {{{"author", "studio"}, {"patch", {{{"op", "copy"}, {"from", "/state/count"}, {"path", "/state/count/x"}}}}},
		 "'/state/count' is a number"},
	    // A move removes first: the item it lands in is then the 5, not the {} that was there before
	    {{{"author", "studio"},
		  {"patch",
		   {{{"op", "add"}, {"path", "/state/list"}, {"value", {json::object(), json::object(), 5}}},
		    {{"op", "move"}, {"from", "/state/list/0"}, {"path", "/state/list/1/x"}}}}},
		 "'/state/list/1' is a number"},
	};
	for(const auto& [revision, error] : cases) {
		CAPTURE(revision.dump());
		const affresco::RevisionResult result = repository.submit(revision);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.find(error) != std::string::npos);
	}
	CHECK(repository.head().number == 0);
	CHECK(repository.document() == permissionsScreen());

	// A move into an object still works, and an add into null, which becomes an object
	const affresco::RevisionResult moved =
	    repository.submit({{"author", "studio"},
		                   {"patch",
		                    {{{"op", "add"}, {"path", "/state/list"}, {"value", {json::object(), nullptr}}},
		                     {{"op", "move"}, {"from", "/state/count"}, {"path", "/state/list/0/count"}},
		                     {{"op", "add"}, {"path", "/state/list/1/x"}, {"value", 1}}}}});
	REQUIRE(moved.status == RevisionStatus::Applied);
	CHECK(repository.state() == json{{"list", {{{"count", 0}}, {{"x", 1}}}}});
}
