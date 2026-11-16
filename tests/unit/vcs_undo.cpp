/// @file
/// @brief Unit tests for undoing an earlier commit that keeps the later changes (VERSION_CONTROL.md, "History"): the
/// options and the plan as they cross the protocol, the calls on the repository and the screen, and today's refusal
/// when a later commit changed a sibling of a node the commit added.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/core/VersionControl.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::DocumentError;
using affresco::json;
using affresco::RevisionStatus;
using affresco::UndoOptions;
using affresco::undoOptionsFrom;
using affresco::UndoPlan;
using affresco::vcs::Repository;

namespace {

/// @brief A screen whose root has three children with ids, that "studio" may change and review.
/// @return The document.
json threeChildren() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"permissions": { "allow": ["studio"], "reviewers": ["studio"] },
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "label", "id": "first", "text": "One" },
			{ "type": "label", "id": "second", "text": "Two" },
			{ "type": "label", "id": "third", "text": "Three" }
		] }
	})json");
}

/// @brief Applies a revision that must land on main.
/// @param repository The repository.
/// @param revision The revision.
void land(Repository& repository, const json& revision) {
	const affresco::RevisionResult result = repository.submit(revision);
	INFO(result.error);
	REQUIRE(result.status == RevisionStatus::Applied);
}

/// @brief Revision 1 appends a fourth child to the root; revision 2 changes the text of the first child.
/// @param repository The repository, at revision 0.
void appendThenChangeSibling(Repository& repository) {
	land(repository, {{"author", "studio"},
	                  {"message", "Add a fourth"},
	                  {"patch", json::array({{{"op", "add"},
	                                          {"path", "/root/children/-"},
	                                          {"value", {{"type", "label"}, {"id", "fourth"}, {"text", "Four"}}}}})}});
	land(repository, {{"author", "studio"}, {"message", "Rename the first"}, {"set", {{"#first/text", "Uno"}}}});
}

/// @brief The error undoOptionsFrom() gives for a message.
/// @param message The message.
/// @return Its text, or "" if it reads.
std::string optionsError(const json& message) {
	try {
		static_cast<void>(undoOptionsFrom(message, "revert"));
	} catch(const DocumentError& error) {
		return error.what();
	}
	return {};
}

} // namespace

TEST_CASE("vcs undo: today a later change to a sibling refuses the undo (f6b turns this into a clean undo)") {
	const affresco::format::Rules rules;
	Repository repository(rules, threeChildren());
	appendThenChangeSibling(repository);
	std::string error;
	try {
		static_cast<void>(repository.revertRevision(1, "studio"));
	} catch(const DocumentError& refused) {
		error = refused.what();
	}
	INFO(error);
	CHECK(error.contains("conflict:"));
	// The plan built on it says the same, and the repository is as it was
	const UndoPlan plan = repository.undoPlan(1, "studio");
	CHECK(plan.outcome == "blocked");
	CHECK(plan.blocked == error);
	CHECK(plan.of.number == 1);
	CHECK(plan.head == repository.head().id);
	CHECK(repository.head().number == 2);
}

TEST_CASE("vcs undo: the options read lists of paths or \"all\", a patch, the head and the message") {
	const UndoOptions none = undoOptionsFrom({{"type", "revert"}, {"revision", 3}}, "revert");
	CHECK(none.empty());
	CHECK(undoOptionsFrom(json::array(), "revert").empty());

	const UndoOptions options =
	    undoOptionsFrom({{"drop", "all"},
		                 {"keep", json::array({"#first/text"})},
		                 {"then", json::array({{{"op", "replace"}, {"path", "/root/id"}, {"value", "x"}}})},
		                 {"head", "abcd"},
		                 {"message", "Undo it"}},
		                "revert");
	CHECK_FALSE(options.empty());
	CHECK(options.drop == "all");
	CHECK(options.keep == json::array({"#first/text"}));
	CHECK(options.then.size() == 1);
	CHECK(options.head == "abcd");
	CHECK(options.message == "Undo it");
	CHECK_FALSE(undoOptionsFrom({{"head", "abcd"}}, "revert").empty());
	CHECK_FALSE(undoOptionsFrom({{"message", "Why"}}, "revert").empty());
}

TEST_CASE("vcs undo: malformed options are refused, naming the key") {
	CHECK(optionsError({{"drop", "some"}}) == "revert.drop: expected a list of paths or \"all\"");
	CHECK(optionsError({{"drop", 3}}) == "revert.drop: expected a list of paths or \"all\"");
	CHECK(optionsError({{"keep", json::array({1})}}) == "revert.keep: expected a list of paths or \"all\"");
	CHECK(optionsError({{"then", json::object()}}).starts_with("revert.then:"));
	CHECK(optionsError({{"head", 7}}).starts_with("revert.head:"));
	CHECK(optionsError({{"message", json::array()}}).starts_with("revert.message:"));
	CHECK(optionsError({{"drop", json::array({std::string(1025, 'a')})}}).starts_with("revert.drop: a path is longer"));
	CHECK(optionsError({{"drop", json::array({std::string(1024, 'a')})}}).empty());

	// At most 256 paths in drop and keep together
	const json many = json(std::vector<std::string>(200, "#first/text"));
	CHECK(optionsError({{"drop", many}, {"keep", json(std::vector<std::string>(56, "#second"))}}).empty());
	CHECK(optionsError({{"drop", many}, {"keep", json(std::vector<std::string>(57, "#second"))}})
	          .starts_with("revert.keep: at most 256 paths"));
}

TEST_CASE("vcs undo: the plan's JSON has every key of the contract") {
	UndoPlan plan;
	plan.of.id = std::string(64, 'a');
	plan.of.number = 4;
	const json written = affresco::toJson(plan);
	for(const char* key : {"of", "head", "outcome", "blocked", "untouched", "later", "laterMore", "conflicts",
	                       "conflictsMore", "gone", "state", "changes", "impact", "proposals", "warnings"}) {
		CAPTURE(key);
		CHECK(written.contains(key));
	}
	CHECK(written.size() == 15);
	CHECK(written["outcome"] == "blocked");
	CHECK(written["of"]["number"] == 4);
	CHECK(written["of"]["short"] == "aaaaaaa");
	CHECK(written["state"] == json({{"undone", json::array()}, {"kept", json::array()}}));
	CHECK(written["impact"].is_null());
	CHECK(written["later"] == json::array());
}

TEST_CASE("vcs undo: a commit and a proposal carry \"undo\" only when they are one") {
	affresco::CommitInfo commit;
	CHECK_FALSE(affresco::toJson(commit).contains("undo"));
	commit.undo = {{"of", "abcd"}};
	CHECK(affresco::toJson(commit)["undo"] == commit.undo);

	affresco::ProposalInfo proposal;
	CHECK_FALSE(affresco::toJson(proposal).contains("undo"));
	proposal.undo = {{"of", "abcd"}};
	CHECK(affresco::toJson(proposal)["undo"] == proposal.undo);
}

TEST_CASE("vcs undo: the repository plans and does a plain undo as revert does, and nothing more yet") {
	const affresco::format::Rules rules;
	Repository repository(rules, threeChildren());
	land(repository, {{"author", "studio"}, {"message", "Rename the first"}, {"set", {{"#first/text", "Uno"}}}});

	const UndoPlan plan = repository.undoPlan("HEAD", "studio");
	CHECK(plan.outcome == "clean");
	CHECK(plan.blocked.empty());
	CHECK(plan.of.message == "Rename the first");
	CHECK(plan.head == repository.head().id);
	CHECK(repository.undoRevision("HEAD", "studio") == repository.revertRevision("HEAD", "studio"));

	UndoOptions keep;
	keep.keep = "all";
	const UndoPlan kept = repository.undoPlan("HEAD", "studio", keep);
	CHECK(kept.outcome == "blocked");
	CHECK(kept.blocked == "this build can't keep later changes yet");
	CHECK_THROWS_WITH_AS(static_cast<void>(repository.undoRevision("HEAD", "studio", keep)),
	                     "undo: this history can't keep later changes", DocumentError);
	CHECK_THROWS_AS(static_cast<void>(repository.undoPlan(9, "studio")), DocumentError);

	land(repository, repository.undoRevision("HEAD", "studio"));
	CHECK(repository.document()["root"]["children"][0]["text"] == "One");
}

TEST_CASE("vcs undo: the screen plans and reverts with options, and malformed ones are an error, never a throw") {
	affresco::Screen screen = affresco::Screen::fromString(threeChildren().dump());
	std::string error;
	REQUIRE(screen.applyRevision(R"({"author": "user", "set": {"#first/text": "Uno"}})", &error));

	const json plan = json::parse(screen.undoPlan(1));
	CHECK(plan["outcome"] == "clean");
	CHECK(plan["of"]["number"] == 1);
	CHECK(json::parse(screen.undoPlan(1, R"({"drop": 3})"))["blocked"] ==
	      "undo.drop: expected a list of paths or \"all\"");
	CHECK(json::parse(screen.undoPlan(1, "{"))["outcome"] == "blocked");
	CHECK(json::parse(screen.undoPlan(1, "[]"))["blocked"] == "undo: expected an object of choices");
	CHECK(json::parse(screen.undoPlan(7))["outcome"] == "blocked");

	CHECK_FALSE(screen.revert(1, R"({"keep": [1]})", &error));
	CHECK(error == "undo.keep: expected a list of paths or \"all\"");
	CHECK_FALSE(screen.revert(1, "{", &error));
	CHECK(error.contains("invalid"));
	CHECK_FALSE(screen.revert(1, R"({"keep": "all"})", &error));
	CHECK(error == "undo: this history can't keep later changes");
	CHECK(screen.revision() == 1);
	REQUIRE(screen.revert(1, "", &error));
	CHECK(screen.revision() == 2);
}

TEST_CASE("vcs undo: a peer asks for the plan at the changes' rate, and one whose reads are limited is refused") {
	json screen = affresco::test::permissionsScreen();
	const affresco::format::Rules rules;
	Repository versions(rules, screen);
	land(versions, {{"author", "studio"}, {"set", {{"#title/text", "Done"}}}});
	affresco::test::MemoryChannel channel;
	affresco::channel::PeerLimits limits;
	limits.revisionBurst = 1;
	const affresco::channel::Session session(channel, versions, "studio", {}, nullptr, limits);
	channel.receive(R"({"type": "undoPlan", "revision": 1, "id": 1})");
	const json answer = channel.sent.back();
	CHECK(answer["type"] == "undoPlan");
	CHECK(answer["id"] == 1);
	CHECK(answer["revision"] == 1);
	CHECK(answer["plan"]["outcome"] == "clean");
	channel.receive(R"({"type": "undoPlan", "revision": 1, "id": 2})"); // the burst of one change is spent
	CHECK(channel.sent.back()["type"] == "rejected");
	CHECK(channel.sent.back()["error"].get<std::string>().starts_with("too many changes"));

	screen["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", json::array({"/state/count"})}}});
	Repository limited(rules, screen);
	affresco::test::MemoryChannel agentChannel;
	const affresco::channel::Session agent(agentChannel, limited, "agent");
	agentChannel.receive(R"({"type": "undoPlan", "revision": 0, "id": 3})");
	CHECK(agentChannel.sent.back()["type"] == "rejected");
}
