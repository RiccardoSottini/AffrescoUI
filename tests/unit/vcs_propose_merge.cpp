/// @file
/// @brief Unit tests for a merge opened as a proposal (vcs::Repository::proposeMerge()): what merges cleanly is its
/// patch, each conflict is left out and listed instead of blocking it, and it is judged and kept as any proposal is.

#include "Helpers.hpp"
#include "HistoryFiles.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;
using affresco::test::TemporaryFolder;
using affresco::vcs::Repository;

namespace {

/// @brief A node of the permissions screen's root, by id.
/// @param document The document.
/// @param id The node's id.
/// @return The node.
json& nodeOf(json& document, const std::string& id) {
	for(json& node : document["root"]["children"])
		if(node["id"] == id) return node;
	FAIL("no node " << id);
	return document;
}

/// @brief The screen as loaded, with the board's colour and the title changed: the version merged in.
/// @param title The title it gives.
/// @return The version.
json theirs(const std::string& title) {
	json version = permissionsScreen();
	nodeOf(version, "board")["color"] = "#ffcc00";
	nodeOf(version, "title")["text"] = title;
	return version;
}

/// @brief A merge request by an agent, based on the screen as loaded.
/// @param version The version merged in.
/// @return The request.
Repository::MergeRequest requestOf(json version) {
	return {permissionsScreen(), std::move(version), "agent", "Merge a shared version", false};
}

/// @brief Moves main on: the studio changes the title.
/// @param repository The repository.
void moveMain(Repository& repository) {
	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"#title/text", "Today"}}}}).status ==
	        RevisionStatus::Applied);
}

} // namespace

TEST_CASE("propose merge: what merges cleanly becomes the proposal's patch, and accepting it lands both sides") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	moveMain(repository);
	json version = permissionsScreen();
	nodeOf(version, "board")["color"] = "#ffcc00";
	const Repository::MergeProposed merged = repository.proposeMerge(requestOf(version));
	REQUIRE_MESSAGE(merged.status == RevisionStatus::Proposed, merged.error);
	CHECK(merged.conflicts.empty());
	const affresco::ProposalInfo proposal = repository.proposals().back();
	CHECK(proposal.id == merged.proposal);
	CHECK(proposal.author == "agent");
	CHECK(proposal.message == "Merge a shared version");
	CHECK(proposal.leftOut.empty());
	CHECK(proposal.conflicts.empty());
	CHECK(toJson(proposal)["leftOut"] == json::array());

	REQUIRE_FALSE(repository.accept(proposal.id, {"studio", proposal.head, false}));
	json document = repository.document();
	CHECK(nodeOf(document, "board")["color"] == "#ffcc00");
	CHECK(nodeOf(document, "title")["text"] == "Today"); // main's change stays
}

TEST_CASE("propose merge: a part that conflicts is left out and listed, and the rest still applies") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	moveMain(repository);
	const Repository::MergeProposed merged = repository.proposeMerge(requestOf(theirs("Done")));
	REQUIRE_MESSAGE(merged.status == RevisionStatus::Proposed, merged.error);
	const affresco::ProposalInfo proposal = repository.proposals().back();
	REQUIRE(proposal.leftOut.size() == 1);
	const json& left = proposal.leftOut[0];
	CHECK(left["path"] == "#title/text");
	CHECK(left["reason"] == "changed on main and in the proposal");
	CHECK(left["base"] == "Tasks");
	CHECK(left["main"] == "Today");
	CHECK(left["proposal"] == "Done");
	CHECK(merged.conflicts == proposal.leftOut);
	CHECK(proposal.conflicts.empty()); // left out is not blocked
	CHECK(toJson(proposal)["leftOut"] == proposal.leftOut);

	REQUIRE_FALSE(repository.accept(proposal.id, {"studio", proposal.head, false}));
	json document = repository.document();
	CHECK(nodeOf(document, "board")["color"] == "#ffcc00");
	CHECK(nodeOf(document, "title")["text"] == "Today"); // main's value where it conflicted
}

TEST_CASE("propose merge: left out stays as recorded when main moves and the proposal rebases") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	moveMain(repository);
	REQUIRE(repository.proposeMerge(requestOf(theirs("Done"))).status == RevisionStatus::Proposed);
	const json recorded = repository.proposals().back().leftOut;
	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"#title/text", "Tomorrow"}}}}).status ==
	        RevisionStatus::Applied);
	const affresco::ProposalInfo rebased = repository.proposals().back();
	CHECK(rebased.conflicts.empty());
	CHECK(rebased.leftOut == recorded);
	CHECK(rebased.leftOut[0]["main"] == "Today");
}

TEST_CASE("propose merge: nothing to merge, or nothing that merges cleanly, opens no proposal") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());

	Repository::MergeProposed same = repository.proposeMerge(requestOf(permissionsScreen()));
	CHECK(same.status == RevisionStatus::Rejected);
	CHECK(same.error == "nothing to merge: main already has it");

	moveMain(repository);
	json version = permissionsScreen();
	nodeOf(version, "title")["text"] = "Today";
	same = repository.proposeMerge(requestOf(version)); // main made the same change already
	CHECK(same.status == RevisionStatus::Rejected);
	CHECK(same.error == "nothing to merge: main already has it");

	version = permissionsScreen();
	nodeOf(version, "title")["text"] = "Done";
	const Repository::MergeProposed none = repository.proposeMerge(requestOf(version));
	CHECK(none.status == RevisionStatus::Rejected);
	CHECK(none.error.starts_with("nothing merges cleanly"));
	CHECK_MESSAGE(none.error.contains("#title/text"), none.error);
	REQUIRE(none.conflicts.size() == 1);
	CHECK(none.conflicts[0]["proposal"] == "Done");
	CHECK(repository.proposals().empty());
}

TEST_CASE("propose merge: the live state is never merged") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	json version = permissionsScreen();
	version["state"]["count"] = 99;
	const Repository::MergeProposed merged = repository.proposeMerge(requestOf(version));
	CHECK(merged.status == RevisionStatus::Rejected);
	CHECK(merged.error == "nothing to merge: main already has it");
}

TEST_CASE("propose merge: a deny rule still refuses it, whoever proposes it") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	json version = permissionsScreen();
	nodeOf(version, "statusBar")["color"] = "#ff0000"; // "*(#statusBar)" is denied
	for(const std::string author : {"agent", "studio", "user"}) {
		CAPTURE(author);
		Repository::MergeRequest request = requestOf(version);
		request.author = author;
		const Repository::MergeProposed merged = repository.proposeMerge(request);
		CHECK(merged.status == RevisionStatus::Rejected);
		CHECK_FALSE(merged.error.empty());
	}
	CHECK(repository.proposals().empty());
	json document = repository.document();
	CHECK(nodeOf(document, "statusBar")["color"] == "#202020");
}

TEST_CASE("propose merge: one opened for the person is refused to any other reviewer") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	Repository::MergeRequest request = requestOf(theirs("Tasks"));
	request.personOnly = true;
	const Repository::MergeProposed merged = repository.proposeMerge(request);
	REQUIRE_MESSAGE(merged.status == RevisionStatus::Proposed, merged.error);
	const affresco::ProposalInfo proposal = repository.proposals().back();
	REQUIRE_FALSE(proposal.person.empty());
	CHECK(proposal.person.back() == "was opened for the person to accept");

	const auto refused = repository.accept(proposal.id, {"studio", proposal.head, false});
	REQUIRE(refused);
	const std::string reason = refused.value_or(std::string());
	CHECK_MESSAGE(reason.contains("only the person may accept"), reason);
	CHECK_FALSE(repository.accept(proposal.id, {"user", proposal.head, false}));
	json document = repository.document();
	CHECK(nodeOf(document, "board")["color"] == "#ffcc00");
}

TEST_CASE("propose merge: a history saved and loaded keeps what was left out, and who may accept it") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "merge.history";
	json leftOut;
	{
		Repository first(rules, permissionsScreen());
		static_cast<void>(first.openHistory(history));
		moveMain(first);
		Repository::MergeRequest request = requestOf(theirs("Done"));
		request.personOnly = true;
		REQUIRE(first.proposeMerge(request).status == RevisionStatus::Proposed);
		leftOut = first.proposals().back().leftOut;
		REQUIRE(leftOut.size() == 1);
	}
	Repository second(rules, permissionsScreen());
	REQUIRE(second.openHistory(history).loaded);
	REQUIRE(second.proposals().size() == 1);
	const affresco::ProposalInfo proposal = second.proposals().back();
	CHECK(proposal.leftOut == leftOut);
	CHECK(proposal.person.back() == "was opened for the person to accept");
	CHECK(second.accept(proposal.id, {"studio", proposal.head, false}));
}

TEST_CASE("propose merge: a history written before merges loads with nothing left out") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "old.history";
	{
		Repository first(rules, permissionsScreen());
		static_cast<void>(first.openHistory(history));
		REQUIRE(first.propose({{"author", "agent"}, {"set", {{"#board/color", "#000000"}}}}).status ==
		        RevisionStatus::Proposed);
	}
	// As a history written before merges: no "leftOut" and no "personOnly" in its refs
	affresco::test::forgeRefs(history, [](json& refs) {
		for(json& proposal : refs["proposals"]) {
			proposal.erase("leftOut");
			proposal.erase("personOnly");
		}
	});

	Repository second(rules, permissionsScreen());
	REQUIRE(second.openHistory(history).loaded);
	REQUIRE(second.proposals().size() == 1);
	CHECK(second.proposals().back().leftOut == json::array());
	CHECK(second.proposals().back().person.empty());
}
