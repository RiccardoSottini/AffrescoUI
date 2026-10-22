/// @file
/// @brief Unit tests for the three-way JSON merge (vcs/src/Merge.hpp) and for proposals rebasing themselves onto a
/// newer main with it: a clean rebase, a conflict, and lists of nodes merged by id (D15).

#include "Helpers.hpp"
#include "Merge.hpp" // modules/vcs/src

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;
using affresco::vcs::detail::merge;

namespace {

/// @brief The ids of a list of nodes, in order.
/// @param list The list.
/// @return The ids.
std::vector<std::string> idsOf(const json& list) {
	std::vector<std::string> ids;
	for(const json& item : list) ids.push_back(item.at("id").get<std::string>());
	return ids;
}

} // namespace

TEST_CASE("merge: a value only one side changed takes that side's value") {
	const json base = {{"a", 1}, {"b", 1}, {"c", 1}};
	const json main = {{"a", 2}, {"b", 1}, {"c", 1}};
	const json proposal = {{"a", 1}, {"b", 3}, {"d", 4}};
	const auto result = merge(base, main, proposal);
	CHECK(result.clean());
	CHECK(result.merged == json({{"a", 2}, {"b", 3}, {"d", 4}})); // c removed by the proposal, d added
	CHECK(merge(base, base, base).merged == base);
}

TEST_CASE("merge: a value both sides changed differently is a conflict, and the merge keeps main's") {
	const json base = {{"title", "Tasks"}, {"color", "#fff"}};
	const auto result = merge(base, {{"title", "Today"}, {"color", "#fff"}}, {{"title", "Done"}, {"color", "#000"}});
	REQUIRE(result.conflicts.size() == 1);
	CHECK(result.conflicts[0]["path"] == "/title");
	CHECK(result.conflicts[0]["reason"] == "changed on main and in the proposal");
	CHECK(result.conflicts[0]["base"] == "Tasks");
	CHECK(result.conflicts[0]["main"] == "Today");
	CHECK(result.conflicts[0]["proposal"] == "Done");
	CHECK(result.merged == json({{"title", "Today"}, {"color", "#000"}}));

	// The same change on both sides is no conflict
	CHECK(merge(base, {{"title", "Same"}}, {{"title", "Same"}}).clean());

	// Removed on one side, changed on the other
	const auto removed = merge(base, {{"color", "#fff"}}, {{"title", "Done"}, {"color", "#fff"}});
	REQUIRE(removed.conflicts.size() == 1);
	CHECK(removed.conflicts[0]["reason"] == "removed on main, changed in the proposal");
}

TEST_CASE("merge: lists of nodes merge by id, wherever the items moved") {
	const json base = json::parse(R"([{"id": "a", "text": "A"}, {"id": "b", "text": "B"}, {"id": "c", "text": "C"}])");
	// main adds "first" at the start and edits "c"; the proposal edits "a", removes "b" and adds "d" after "a"
	const json main = json::parse(
	    R"([{"id": "first"}, {"id": "a", "text": "A"}, {"id": "b", "text": "B"}, {"id": "c", "text": "C2"}])");
	const json proposal =
	    json::parse(R"([{"id": "a", "text": "A2"}, {"id": "d", "text": "D"}, {"id": "c", "text": "C"}])");
	const auto result = merge(base, main, proposal);
	CHECK(result.clean());
	CHECK(idsOf(result.merged) == std::vector<std::string>{"first", "a", "d", "c"});
	CHECK(result.merged[1]["text"] == "A2");
	CHECK(result.merged[3]["text"] == "C2");

	// Both change one item: a conflict named from the item
	const json both = json::parse(R"([{"id": "a", "text": "Z"}, {"id": "b", "text": "B"}, {"id": "c", "text": "C"}])");
	const auto conflict = merge(base, both, proposal);
	REQUIRE(conflict.conflicts.size() == 1);
	CHECK(conflict.conflicts[0]["path"] == "#a/text");
}

TEST_CASE("merge: a list reordered differently on both sides is a conflict; a list without ids merges whole") {
	const json base = json::parse(R"([{"id": "a"}, {"id": "b"}, {"id": "c"}])");
	const json main = json::parse(R"([{"id": "c"}, {"id": "a"}, {"id": "b"}])");
	const json proposal = json::parse(R"([{"id": "b"}, {"id": "a"}, {"id": "c"}])");
	const auto result = merge(base, main, proposal);
	REQUIRE(result.conflicts.size() == 1);
	CHECK(result.conflicts[0]["reason"] == "reordered on main and in the proposal");
	CHECK(idsOf(result.merged) == std::vector<std::string>{"c", "a", "b"}); // main's

	// One side reordering is no conflict
	CHECK(idsOf(merge(base, base, proposal).merged) == std::vector<std::string>{"b", "a", "c"});

	// Plain lists are values: changed on both sides, they conflict
	CHECK_FALSE(merge(json::array({1, 2}), json::array({1, 2, 3}), json::array({0, 1, 2})).clean());
}

TEST_CASE("vcs: a proposal rebases itself onto a newer main, and keeps its approval when nothing else changed") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);

	// main moves on somewhere else
	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"#board/color", "#eeeeee"}}}}).status ==
	        RevisionStatus::Applied);
	REQUIRE(repository.proposals().size() == 1);
	const affresco::ProposalInfo rebased = repository.proposals()[0];
	CHECK(rebased.base == repository.head().id);
	CHECK(rebased.head != proposed.commit);
	CHECK(rebased.conflicts.empty());

	// Its changes are the ones reviewed: the head the reviewer saw still counts
	REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit.substr(0, 7), false}));
	CHECK(repository.document()["root"]["children"][0]["color"] == "#eeeeee");
	CHECK(repository.document()["root"]["children"][1]["text"] == "Today");
	CHECK(repository.proposals().empty());
}

TEST_CASE("vcs: a proposal whose operations no longer apply as written rebases through the merge, and must be "
          "reviewed again") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	// Added by position: on the new main, position 1 is somewhere else
	const affresco::RevisionResult proposed = repository.submit(
	    {{"author", "agent"},
		 {"patch",
		  json::array(
	          {{{"op", "add"}, {"path", "/root/children/1"}, {"value", {{"type", "panel"}, {"id", "extra"}}}}})}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE(repository
	            .submit({{"author", "studio"},
	                     {"patch", json::array({{{"op", "add"},
	                                             {"path", "/root/children/0"},
	                                             {"value", {{"type", "panel"}, {"id", "first"}}}}})}})
	            .status == RevisionStatus::Applied);

	REQUIRE(repository.proposals().size() == 1);
	const affresco::ProposalInfo rebased = repository.proposals()[0];
	CHECK(rebased.conflicts.empty());
	const auto refused = repository.accept(proposed.proposal, {"studio", proposed.commit.substr(0, 7), false});
	REQUIRE(refused);
	CHECK(refused->find("changed since you reviewed it") != std::string::npos);

	// What it merges is the keyed merge: "extra" after "board", where the agent put it
	REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", rebased.head, false}));
	CHECK(idsOf(repository.document()["root"]["children"]) ==
	      std::vector<std::string>{"first", "board", "extra", "title", "statusBar"});
}

TEST_CASE("vcs: a proposal that conflicts with main says where, and can't be accepted until its author pushes") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"#title/text", "Tomorrow"}}}}).status ==
	        RevisionStatus::Applied);

	REQUIRE(repository.proposals().size() == 1);
	const affresco::ProposalInfo conflicted = repository.proposals()[0];
	CHECK(conflicted.head == proposed.commit); // left where it was
	REQUIRE(conflicted.conflicts.size() == 1);
	CHECK(conflicted.conflicts[0]["path"] == "#title/text");
	CHECK(conflicted.conflicts[0]["main"] == "Tomorrow");
	CHECK(conflicted.conflicts[0]["proposal"] == "Today");
	CHECK(conflicted.reason.find("conflicts with main at #title/text") != std::string::npos);

	const auto refused = repository.accept(proposed.proposal, {"studio", proposed.commit, false});
	REQUIRE(refused);
	CHECK(refused->find("conflicts with main") != std::string::npos);
	CHECK(repository.document()["root"]["children"][1]["text"] == "Tomorrow"); // main's side kept

	// The author resolves it by pushing again: the branch moves onto main
	const affresco::RevisionResult pushed = repository.submit(
	    {{"author", "agent"}, {"proposal", proposed.proposal}, {"set", {{"#title/text", "Tomorrow, then today"}}}});
	REQUIRE(pushed.status == RevisionStatus::Proposed);
	REQUIRE(repository.proposals()[0].conflicts.empty());
	REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", pushed.commit, false}));
	CHECK(repository.document()["root"]["children"][1]["text"] == "Tomorrow, then today");
}
