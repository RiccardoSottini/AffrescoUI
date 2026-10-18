/// @file
/// @brief Unit tests for the vcs module: commits, reverts, permission rules, review and change sets.
///
/// The documents are screens, judged by the format's own rules (format::Rules), as in the app.

#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

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
