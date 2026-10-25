/// @file
/// @brief What only the person accepts, whoever else reviews (SECURITY.md, "Services"): a service step that writes or
/// runs, a terminal node (its program runs), and new state for a step that writes or runs to read.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::RevisionStatus;

namespace {

/// @brief The permissions screen, with a second reviewer "lead", a pty service "sh" and an accepted action that writes
/// `state.command` to it.
/// @return The screen.
json shellScreen() {
	json screen = affresco::test::permissionsScreen();
	screen["permissions"]["reviewers"].push_back("lead");
	screen["services"] = {{"sh", {{"provider", "pty"}}}};
	screen["state"]["command"] = "ls";
	screen["actions"] = {{"go", json::array({{{"invoke", "sh.write"},
	                                          {"with", {{"session", "main"}, {"data", "{{state.command}}"}}}}})}};
	return screen;
}

/// @brief Submits a revision, which must become a proposal for the person only.
/// @param repository The repository.
/// @param revision The revision.
/// @return The proposal's lines for the person, joined.
std::string personOnly(affresco::vcs::Repository& repository, const json& revision) {
	const affresco::RevisionResult result = repository.submit(revision);
	REQUIRE(result.status == RevisionStatus::Proposed);
	const affresco::ProposalInfo proposal = repository.proposals().back();
	CHECK(proposal.reason.contains("only the person accepts"));
	const std::string reviewer = revision["author"] == "studio" ? "lead" : "studio"; // a reviewer, not the person
	const auto refused = repository.accept(proposal.id, {reviewer, proposal.head, false});
	REQUIRE(refused);
	CHECK_MESSAGE(refused->contains("only the person may accept"), *refused);
	std::string lines;
	for(const std::string& line : proposal.person) lines += line + "\n";
	return lines;
}

} // namespace

TEST_CASE("review: a service step that writes or runs is the person's to accept, not another reviewer's") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, shellScreen());
	repository.setMode(affresco::Mode::AcceptEdits);
	const json write = json::array({{{"invoke", "sh.write"}, {"with", {{"session", "main"}, {"data", "exit\n"}}}}});
	for(const std::string author : {"studio", "agent"}) {
		CAPTURE(author);
		CHECK(personOnly(repository, {{"author", author}, {"add", {{"/actions/again", write}}}}).contains("sh.write"));
	}
	const affresco::ProposalInfo agents = repository.proposals().back();
	CHECK_FALSE(repository.accept(agents.id, {"user", agents.head, false}));
	CHECK(repository.document()["actions"].contains("again"));
}

TEST_CASE("review: a terminal node is the person's to accept; a line typed into a terminal is never added") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, shellScreen());
	repository.setMode(affresco::Mode::AcceptEdits);
	const json terminal = {{"type", "terminal"}, {"id", "shell"}, {"service", "sh"}, {"session", "login"}};
	CHECK(
	    personOnly(repository, {{"author", "agent"}, {"set", {{"/root/children/0/children", json::array({terminal})}}}})
	        .contains("uses sh.open"));
	const json typed = json::array({{{"runInTerminal", "main"}, {"text", "ls"}}}); // a control step: never added
	CHECK(repository.submit({{"author", "agent"}, {"add", {{"/actions/type", typed}}}}).status ==
	      RevisionStatus::Rejected);
	// A terminal on a service the screen doesn't declare is refused outright
	json elsewhere = terminal;
	elsewhere["service"] = "other";
	CHECK(repository.submit({{"author", "agent"}, {"set", {{"/root/children/0/children", json::array({elsewhere})}}}})
	          .status == RevisionStatus::Rejected);
}

TEST_CASE("review: new state for an accepted step that writes or runs is the person's to accept, other state isn't") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, shellScreen());
	repository.setMode(affresco::Mode::AcceptEdits);
	CHECK(personOnly(repository, {{"author", "agent"}, {"set", {{"/state/command", "rm -r ."}}}})
	          .contains("uses sh.write (a method the screen can't describe yet), with state.command changed"));
	CHECK(repository.submit({{"author", "agent"}, {"set", {{"/state/count", 1}}}}).status == RevisionStatus::Applied);
}
