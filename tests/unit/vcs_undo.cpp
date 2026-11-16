/// @file
/// @brief Unit tests for undoing an earlier commit that keeps the later changes (VERSION_CONTROL.md, "Undo that keeps
/// what came later"): the options and the plan as they cross the protocol, the calls on the repository and the screen,
/// what is undone and what is a conflict, the choices, the undo mark, and an undo that only a reviewer applies.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/core/VersionControl.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include "HistoryFiles.hpp"

#include <doctest/doctest.h>

#include <optional>
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

TEST_CASE("vcs undo: a later change to a sibling of a node the commit added no longer refuses the undo") {
	const affresco::format::Rules rules;
	Repository repository(rules, threeChildren());
	appendThenChangeSibling(repository);
	const UndoPlan plan = repository.undoPlan(1, "studio");
	CHECK(plan.outcome == "clean");
	CHECK(plan.blocked.empty());
	CHECK(plan.of.number == 1);
	CHECK(plan.head == repository.head().id);
	CHECK(plan.untouched == 1);
	CHECK(plan.conflicts.empty());
	CHECK(plan.changes == std::vector<std::string>{"remove /root/children/3"});
	CHECK(repository.head().number == 2); // a plan changes nothing

	land(repository, repository.revertRevision(1, "studio"));
	const json& children = repository.document()["root"]["children"];
	CHECK(children.size() == 3);
	CHECK(children[0]["text"] == "Uno");
	CHECK(repository.head().undo == json({{"of", plan.of.id}, {"drop", json::array()}, {"then", 0}}));
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

TEST_CASE("vcs undo: the repository plans and does a plain undo as revert does") {
	const affresco::format::Rules rules;
	Repository repository(rules, threeChildren());
	land(repository, {{"author", "studio"}, {"message", "Rename the first"}, {"set", {{"#first/text", "Uno"}}}});

	const UndoPlan plan = repository.undoPlan("HEAD", "studio");
	CHECK(plan.outcome == "clean");
	CHECK(plan.blocked.empty());
	CHECK(plan.of.message == "Rename the first");
	CHECK(plan.head == repository.head().id);
	CHECK(repository.undoRevision("HEAD", "studio") == repository.revertRevision("HEAD", "studio"));

	UndoOptions keep; // with nothing to choose for, "all" chooses nothing
	keep.keep = "all";
	const UndoPlan kept = repository.undoPlan("HEAD", "studio", keep);
	CHECK(kept.outcome == "clean");
	CHECK(repository.undoRevision("HEAD", "studio", keep)["patch"] == repository.revertRevision("HEAD", "studio")["patch"]);
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
	CHECK_FALSE(screen.revert(1, R"({"keep": ["#first/text"]})", &error));
	CHECK(error == "revert: '#first/text' is not a conflict of this undo");
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

namespace {

/// @brief A revision of "studio" that sets paths.
/// @param edits Paths and values.
/// @param message Its message.
/// @return The revision.
json setting(const json& edits, const std::string& message = "Change") {
	return {{"author", "studio"}, {"message", message}, {"set", edits}};
}

/// @brief A revision of "studio" that appends a label to the root.
/// @param id The label's id.
/// @param text Its text.
/// @return The revision.
json appending(const std::string& id, const std::string& text) {
	return {{"author", "studio"},
	        {"message", "Add " + id},
	        {"patch", json::array({{{"op", "add"},
	                                {"path", "/root/children/-"},
	                                {"value", {{"type", "label"}, {"id", id}, {"text", text}}}}})}};
}

/// @brief The error an undo gives.
/// @param repository The repository.
/// @param revision The commit to undo.
/// @param options The choices.
/// @param author Who undoes it.
/// @return The error, or "" if it gave a revision.
std::string undoError(const Repository& repository, const json& revision, const UndoOptions& options = {},
                      const std::string& author = "studio") {
	try {
		static_cast<void>(repository.undoRevision(revision, author, options));
	} catch(const DocumentError& error) {
		return error.what();
	}
	return {};
}

/// @brief Choices that drop or keep every conflict.
/// @param drop true to drop them, false to keep them.
/// @return The choices.
UndoOptions every(bool drop) {
	UndoOptions options;
	(drop ? options.drop : options.keep) = "all";
	return options;
}

/// @brief The text of a child of the root of threeChildren().
/// @param repository The repository.
/// @param id The child's id.
/// @return Its text, or null if there is no such child.
json textOf(const Repository& repository, const std::string& id) {
	for(const json& child : repository.document()["root"]["children"])
		if(child["id"] == id) return child.value("text", json());
	return nullptr;
}

/// @brief A repository on threeChildren(), and rules to judge it.
struct Children {
	affresco::format::Rules rules;        ///< The format's rules.
	Repository repository{rules, threeChildren()}; ///< At revision 0.
};

} // namespace

TEST_CASE("vcs undo: what a later commit didn't change is undone, the rest kept") {
	Children f;
	Repository& repository = f.repository;

	SUBCASE("another node") {
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, setting({{"#second/text", "Dos"}}));
		const UndoPlan plan = repository.undoPlan(1, "studio");
		CHECK(plan.outcome == "clean");
		CHECK(plan.untouched == 1);
		land(repository, repository.undoRevision(1, "studio"));
		CHECK(textOf(repository, "first") == "One");
		CHECK(textOf(repository, "second") == "Dos");
	}
	SUBCASE("another property of the same node") {
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, setting({{"#first/tooltip", "Hi"}}));
		CHECK(repository.undoPlan(1, "studio").outcome == "clean");
		land(repository, repository.undoRevision(1, "studio"));
		CHECK(textOf(repository, "first") == "One");
		CHECK(repository.document()["root"]["children"][0]["tooltip"] == "Hi");
	}
	SUBCASE("a part a later change removed is gone: nothing to undo there") {
		land(repository, setting({{"#first/text", "Uno"}, {"#second/text", "Dos"}}));
		land(repository, {{"author", "studio"}, {"patch", json::array({{{"op", "remove"}, {"path", "#first"}}})}});
		const UndoPlan plan = repository.undoPlan(1, "studio");
		CHECK(plan.outcome == "clean");
		CHECK(plan.gone == json::array({"#first"}));
		land(repository, repository.undoRevision(1, "studio"));
		CHECK(textOf(repository, "first").is_null());
		CHECK(textOf(repository, "second") == "Two");
	}
	SUBCASE("everything it did replaced since: nothing to undo") {
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, {{"author", "studio"}, {"patch", json::array({{{"op", "remove"}, {"path", "#first"}}})}});
		const UndoPlan plan = repository.undoPlan(1, "studio");
		CHECK(plan.outcome == "blocked");
		CHECK(plan.blocked == "revert: nothing to undo: later changes already replaced or removed everything revision "
		                      "1 did");
		CHECK(undoError(repository, 1) == plan.blocked);
	}
	SUBCASE("a merge commit: the whole proposal is undone") {
		const affresco::RevisionResult proposed = repository.submit(
		    {{"author", "agent"}, {"message", "Agent's"}, {"set", {{"#second/text", "Deux"}}}});
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, false}));
		land(repository, setting({{"#third/text", "Trois"}}));
		land(repository, repository.undoRevision(1, "studio"));
		CHECK(textOf(repository, "second") == "Two");
		CHECK(textOf(repository, "third") == "Trois");
	}
	SUBCASE("an undo of an undo brings the change back") {
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, setting({{"#second/text", "Dos"}}));
		land(repository, repository.undoRevision(1, "studio"));
		land(repository, repository.undoRevision(3, "studio"));
		CHECK(textOf(repository, "first") == "Uno");
		CHECK(textOf(repository, "second") == "Dos");
	}
}

TEST_CASE("vcs undo: a part a later commit built on is a conflict, which the person chooses for") {
	Children f;
	Repository& repository = f.repository;

	SUBCASE("the same property: drop takes it back to before the commit, keep leaves it") {
		land(repository, setting({{"#first/text", "Uno"}}, "Rename"));
		land(repository, setting({{"#first/text", "Eins"}}));
		land(repository, setting({{"#second/text", "Dos"}}));
		const UndoPlan plan = repository.undoPlan(1, "studio");
		CHECK(plan.outcome == "choose");
		REQUIRE(plan.conflicts.size() == 1);
		const json& conflict = plan.conflicts[0];
		CHECK(conflict["path"] == "#first/text");
		CHECK(conflict["reason"] == "changed since");
		CHECK(conflict["then"] == "Uno");
		CHECK(conflict["now"] == "Eins");
		CHECK(conflict["before"] == "One");
		CHECK(conflict["commits"] == json::array({2}));
		CHECK(conflict["choice"].is_null());
		CHECK(plan.untouched == 1);
		REQUIRE(plan.later.size() == 1);
		CHECK(plan.later[0]["number"] == 2);
		CHECK(plan.later[0]["parts"] == json::array({"#first/text"}));
		CHECK(plan.changes.empty());
		CHECK(plan.impact.is_null());

		const std::string short1 = plan.of.id.substr(0, 7);
		CHECK(undoError(repository, 1) == "revert: conflict: 1 later change was built on revision 1 (" + short1 +
		                                      "): #first/text (revision 2): read the plan (undoPlan), then revert "
		                                      "with \"drop\" or \"keep\" for each");
		CHECK(repository.undoPlan(1, "studio", every(true)).outcome == "ready");
		CHECK(repository.undoPlan(1, "studio", every(true)).conflicts[0]["choice"] == "drop");
		CHECK(repository.undoPlan(1, "studio", every(false)).outcome == "blocked"); // keeping it leaves nothing

		const json revision = repository.undoRevision(1, "studio", every(true));
		CHECK(revision["message"] == "Revert \"Rename\" (with what revision 2 built on it)");
		CHECK(revision["undo"]["drop"] == json::array({"#first/text"}));
		land(repository, revision);
		CHECK(textOf(repository, "first") == "One");
		CHECK(textOf(repository, "second") == "Dos");
	}
	SUBCASE("a node it added that was changed since: the undo would take it away") {
		land(repository, appending("fourth", "Four"));
		land(repository, setting({{"#fourth/text", "Vier"}}));
		const UndoPlan plan = repository.undoPlan(1, "studio");
		REQUIRE(plan.conflicts.size() == 1);
		CHECK(plan.conflicts[0]["path"] == "#fourth");
		CHECK(plan.conflicts[0]["reason"] == "changed since, and the undo takes it away");
		CHECK(plan.conflicts[0]["before"].is_null());
		land(repository, repository.undoRevision(1, "studio", every(true)));
		CHECK(textOf(repository, "fourth").is_null());
	}
	SUBCASE("a chain of later changes: each is listed, oldest first") {
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, setting({{"#first/text", "Eins"}}));
		land(repository, setting({{"#first/text", "Un"}}));
		const UndoPlan plan = repository.undoPlan(1, "studio");
		REQUIRE(plan.conflicts.size() == 1);
		CHECK(plan.conflicts[0]["commits"] == json::array({2, 3}));
		REQUIRE(plan.later.size() == 2);
		CHECK(plan.later[0]["number"] == 2);
		CHECK(plan.later[1]["number"] == 3);
		CHECK(plan.untouched == 0);
		CHECK(repository.undoRevision(1, "studio", every(true))["message"] ==
		      "Revert \"Change\" (with what revision 2 and 3 built on it)");
	}
	SUBCASE("a rebuild in the same commit (\"then\")") {
		land(repository, appending("fourth", "Four"));
		land(repository, setting({{"#fourth/text", "Vier"}}));
		UndoOptions options = every(true);
		options.then = json::array(
		    {{{"op", "add"}, {"path", "/root/children/-"}, {"value", {{"type", "label"}, {"id", "vier"}, {"text", "Vier"}}}}});
		const json revision = repository.undoRevision(1, "studio", options);
		CHECK(revision["undo"]["then"] == 1);
		land(repository, revision);
		CHECK(textOf(repository, "fourth").is_null());
		CHECK(textOf(repository, "vier") == "Vier");
		CHECK(repository.head().undo["then"] == 1);
	}
	SUBCASE("named one by one, with a message of the person's own") {
		land(repository, setting({{"#first/text", "Uno"}, {"#second/text", "Dos"}}));
		land(repository, setting({{"#first/text", "Eins"}, {"#second/text", "Zwei"}}));
		UndoOptions options;
		options.drop = json::array({"#first/text"});
		CHECK(repository.undoPlan(1, "studio", options).outcome == "choose");
		CHECK(undoError(repository, 1, options).starts_with("revert: conflict: 1 later change was built on"));
		options.keep = json::array({"#second/text"});
		options.message = "Back to One";
		const json revision = repository.undoRevision(1, "studio", options);
		CHECK(revision["message"] == "Back to One");
		land(repository, revision);
		CHECK(textOf(repository, "first") == "One");
		CHECK(textOf(repository, "second") == "Zwei");
	}
}

TEST_CASE("vcs undo: choices that can't be made are refused, naming what is wrong") {
	Children f;
	Repository& repository = f.repository;
	land(repository, setting({{"#first/text", "Uno"}}));
	land(repository, setting({{"#first/text", "Eins"}}));

	const auto blocked = [&](const json& revision, const UndoOptions& options) {
		const UndoPlan plan = repository.undoPlan(revision, "studio", options);
		CHECK(plan.outcome == "blocked");
		CHECK(undoError(repository, revision, options) == plan.blocked);
		return plan.blocked;
	};
	CHECK(blocked(0, {}) == "revert: revision 0 is the screen as loaded: there is nothing to revert");
	UndoOptions both;
	both.drop = both.keep = "all";
	CHECK(blocked(1, both) == R"(revert: "drop" and "keep" can't both be "all")");
	both.drop = both.keep = json::array({"#first/text"});
	CHECK(blocked(1, both) == "revert: '#first/text' is both dropped and kept");
	UndoOptions other;
	other.drop = json::array({"#second/text"});
	CHECK(blocked(1, other) == "revert: '#second/text' is not a conflict of this undo");
	UndoOptions malformed;
	malformed.drop = 3;
	CHECK(blocked(1, malformed) == R"(revert: drop: expected a list of paths or "all")");

	// Choices made on another head
	UndoOptions moved = every(true);
	moved.head = repository.log()[1].id;
	CHECK(undoError(repository, 1, moved) ==
	      "revert: main moved since you looked (it is now at " + repository.head().id.substr(0, 7) + "): look again");
	moved.head = repository.head().id.substr(0, 7);
	CHECK(undoError(repository, 1, moved).empty());

	// A commit of a proposal isn't on main
	const affresco::RevisionResult proposed = repository.submit({{"author", "agent"}, {"set", {{"#third/text", "x"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	CHECK(blocked(proposed.commit, {}).ends_with(" is not on main"));
}

TEST_CASE("vcs undo: a model changed after the commit is never undone part of the way") {
	const json screen = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"permissions": { "allow": ["studio"], "reviewers": ["studio"] },
		"models": { "notes": { "version": 1, "fields": { "title": { "type": "string", "required": true } },
			"seed": [{ "id": "n1", "title": "First" }] } },
		"root": { "type": "column", "id": "main", "children": [{ "type": "label", "id": "count", "text": "Notes" }] }
	})json");
	const affresco::format::Rules rules;
	Repository repository(rules, screen);
	land(repository, setting({{"/models/notes/seed/0/title", "Primo"}}));
	land(repository, setting({{"#count/text", "All notes"}}));
	CHECK(repository.undoPlan(1, "studio").outcome == "clean"); // nothing changed it since
	land(repository, setting({{"/models/notes/seed/0/title", "Erst"}}));
	const UndoPlan plan = repository.undoPlan(1, "studio");
	CHECK(plan.outcome == "blocked");
	CHECK(plan.blocked == "revert: revision 3 changed the model 'notes' after it: undo that first");
}

TEST_CASE("vcs undo: a program's definition is one value: another definition stays, the same one conflicts") {
	constexpr const char* program = R"lisp((def greeting "The greeting." "Hello")
(def farewell "The farewell." "Bye")
(defscreen main (column#list (label#title :text greeting) (label#end :text farewell)))
)lisp";
	const affresco::format::Rules rules;
	Repository repository(rules, affresco::lisp::sourceDocument(program, "app.lisp"));
	const std::string greeting = "/program/app.lisp/greeting", farewell = "/program/app.lisp/farewell";
	const json greetingSource = repository.document()["program"]["app.lisp"]["greeting"];
	land(repository, {{"author", "studio"}, {"set", {{greeting, R"lisp((def greeting "The greeting." "Hi"))lisp"}}}});
	land(repository, {{"author", "studio"}, {"set", {{farewell, R"lisp((def farewell "The farewell." "Ciao"))lisp"}}}});
	CHECK(repository.undoPlan(1, "studio").outcome == "clean");
	land(repository, {{"author", "studio"}, {"set", {{greeting, R"lisp((def greeting "The greeting." "Hey"))lisp"}}}});
	const UndoPlan plan = repository.undoPlan(1, "studio");
	CHECK(plan.outcome == "choose");
	REQUIRE(plan.conflicts.size() == 1);
	CHECK(plan.conflicts[0]["path"] == greeting);
	land(repository, repository.undoRevision(1, "studio", every(true)));
	CHECK(repository.document()["program"]["app.lisp"]["greeting"] == greetingSource);
	CHECK(repository.document()["program"]["app.lisp"]["farewell"].dump().contains("Ciao"));
}

TEST_CASE("vcs undo: the mark is in the log, read back from the history folder, and checked when it lands") {
	const affresco::test::TemporaryFolder folder;
	const affresco::format::Rules rules;
	json mark;
	{
		Repository repository(rules, threeChildren());
		repository.openHistory(folder.path() / "screen.history");
		land(repository, setting({{"#first/text", "Uno"}}));
		land(repository, setting({{"#first/text", "Eins"}}));
		const json revision = repository.undoRevision(1, "studio", every(true));
		mark = revision["undo"];

		SUBCASE("a patch that isn't the undo, or more than an undo carries, is refused") {
			json changed = revision;
			changed["patch"][0]["value"] = "Other";
			CHECK(repository.submit(changed).error.contains("revision.undo: this is not the undo of"));
			changed = revision;
			changed["undo"]["drop"] = json::array();
			CHECK(repository.submit(changed).error.contains("revision.undo: this is not the undo of"));
			changed = revision;
			changed["proposal"] = 1;
			CHECK(repository.submit(changed).error.contains("revision.undo: an undo opens a proposal of its own"));
			changed = revision;
			changed["run"] = json::array({"x"});
			CHECK(repository.submit(changed).error.contains("an undo carries only"));
			changed = revision;
			changed["undo"]["of"] = "abc";
			CHECK(repository.submit(changed).error.contains("revision.undo.of: expected a commit id"));
			changed = revision;
			changed.erase("parent");
			CHECK(repository.commit(changed).value_or("").contains("names main's head as its parent"));
			CHECK(repository.head().number == 2);
		}
		land(repository, revision);
		CHECK(repository.log().back().undo == mark);
		CHECK(affresco::toJson(repository.log().back())["undo"] == mark);
		CHECK_FALSE(repository.log()[1].undo.is_null() == false);
	}
	Repository reloaded(rules, threeChildren());
	REQUIRE(reloaded.openHistory(folder.path() / "screen.history").loaded);
	CHECK(reloaded.head().undo == mark);
	CHECK(reloaded.head().number == 3);
}

namespace {

/// @brief A screen where "agent" may change anything and "studio" reviews.
/// @return The document.
json agentScreen() {
	json screen = threeChildren();
	screen["permissions"] = {{"allow", json::array({"studio", "agent"})}, {"reviewers", json::array({"studio"})}};
	return screen;
}

} // namespace

TEST_CASE("vcs undo: an agent's undo that takes out a later change, or rebuilds one, is always a proposal") {
	const affresco::format::Rules rules;
	Repository repository(rules, agentScreen());
	land(repository, appending("fourth", "Four"));
	land(repository, setting({{"#fourth/text", "Vier"}}));
	land(repository, setting({{"#first/text", "Uno"}}));

	SUBCASE("a plain undo is applied as any change the rules allow") {
		land(repository, repository.undoRevision(3, "agent"));
		CHECK(textOf(repository, "first") == "One");
	}
	SUBCASE("drop: proposed even though the rules allow it, never remembered, the mark kept through a rebase") {
		const affresco::RevisionResult proposed = repository.submit(repository.undoRevision(1, "agent", every(true)));
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		std::vector<affresco::ProposalInfo> open = repository.proposals();
		REQUIRE(open.size() == 1);
		CHECK(open[0].reason == "it takes out what later changes built: #fourth");
		CHECK(open[0].remember.empty());
		CHECK(open[0].undo["drop"] == json::array({"#fourth"}));
		CHECK(repository.submit({{"author", "agent"}, {"proposal", proposed.proposal}, {"set", {{"#third/text", "x"}}}})
		          .error.contains("is an undo: it takes no more commits"));

		land(repository, setting({{"#second/text", "Dos"}})); // main moves: the proposal rebases
		open = repository.proposals();
		REQUIRE(open.size() == 1);
		CHECK(open[0].undo["drop"] == json::array({"#fourth"}));
		CHECK(open[0].remember.empty());
		const std::optional<std::string> refused = repository.accept(proposed.proposal, {"studio", open[0].head, false});
		INFO(refused.value_or(""));
		REQUIRE_FALSE(refused);
		CHECK(repository.head().undo["drop"] == json::array({"#fourth"}));
		CHECK(textOf(repository, "fourth").is_null());
		CHECK(textOf(repository, "second") == "Dos");
	}
	SUBCASE("then: proposed, even in acceptEdits") {
		repository.setMode(affresco::Mode::AcceptEdits);
		UndoOptions options;
		options.then = json::array({{{"op", "replace"}, {"path", "#third/text"}, {"value", "Drei"}}});
		const affresco::RevisionResult proposed = repository.submit(repository.undoRevision(3, "agent", options));
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		CHECK(repository.proposals()[0].reason == "it rebuilds a later change");
	}
	SUBCASE("a reviewer's undo is applied") {
		land(repository, repository.undoRevision(1, "studio", every(true)));
		CHECK(textOf(repository, "fourth").is_null());
	}
}

TEST_CASE("vcs undo: the plan judges the undo as a change: the rules, the warnings, the proposals it would break") {
	const affresco::format::Rules rules;
	Repository repository(rules, agentScreen());
	land(repository, setting({{"#first/text", "Uno"}}));
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"proposal", 0}, {"set", {{"#first/text", "Ein"}}}});
	static_cast<void>(proposed);
	const affresco::RevisionResult opened = repository.propose({{"author", "agent"}, {"set", {{"#first/text", "Un"}}}});
	REQUIRE(opened.status == RevisionStatus::Proposed);
	const UndoPlan plan = repository.undoPlan(1, "studio");
	CHECK(plan.outcome == "clean");
	CHECK(plan.changes == std::vector<std::string>{"set /root/children/0/text to One"});
	CHECK(plan.impact.is_object());
	CHECK(plan.proposals == std::vector<int>{opened.proposal});
}
