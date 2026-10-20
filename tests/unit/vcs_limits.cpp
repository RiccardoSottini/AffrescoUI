/// @file
/// @brief Unit tests for what preparing one revision may cost (vcs/src/Budget.hpp): its operations, the members it
/// gives an object, and what its copies copy. Found in the security review of wave 2: a peer could make the host run
/// out of memory, or freeze it for minutes, with one small revision.

#include "Budget.hpp"
#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;
using affresco::vcs::detail::maxCopiedValues;
using affresco::vcs::detail::maxMembers;
using affresco::vcs::detail::maxOperations;

namespace {

/// @brief Compact edits that set this many keys of the live state, read as a peer's message is.
/// @param count How many.
/// @return {"/state/k0": 0, ...}.
json stateEdits(std::size_t count) {
	std::string text = "{";
	for(std::size_t i = 0; i < count; ++i) text += (i ? ",\"/state/k" : "\"/state/k") + std::to_string(i) + "\":0";
	return affresco::parseUntrusted(text + "}");
}

/// @brief An object of this many members, read as a peer's message is.
/// @param count How many.
/// @return {"k0": 0, "k1": 0, ...}.
json members(std::size_t count) {
	std::string text = "{";
	for(std::size_t i = 0; i < count; ++i) text += (i ? ",\"k" : "\"k") + std::to_string(i) + "\":0";
	return affresco::parseUntrusted(text + "}");
}

} // namespace

// Each copy of a list into itself doubles it: 40 of them, about 2 KB, asked for 2^40 values
TEST_CASE("vcs: copies that double a list are refused before they take the memory") {
	const affresco::format::Rules rules;
	json screen = permissionsScreen();
	screen["state"]["list"] = json::array({1});
	affresco::vcs::Repository repository(rules, screen);

	json patch = json::array();
	for(int i = 0; i < 40; ++i) patch.push_back({{"op", "copy"}, {"from", "/state/list"}, {"path", "/state/list/-"}});
	const auto start = std::chrono::steady_clock::now();
	const affresco::RevisionResult result = repository.submit({{"author", "studio"}, {"patch", patch}});
	const std::chrono::duration<double> took = std::chrono::steady_clock::now() - start;

	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.find("(copy /state/list/-): the revision's copies would copy more than 100000 values") !=
	      std::string::npos);
	CHECK(took.count() < 10.0); // milliseconds; before, 20 copies took a second and each one more twice as long
	CHECK(repository.state() == screen["state"]);

	// A few copies, and copies of large parts, still apply
	json fewer = json::array();
	for(int i = 0; i < 10; ++i) fewer.push_back({{"op", "copy"}, {"from", "/state/list"}, {"path", "/state/list/-"}});
	REQUIRE(repository.submit({{"author", "studio"}, {"patch", fewer}}).status == RevisionStatus::Applied);
	CHECK(repository.state()["list"].size() == 11);
	CHECK(maxCopiedValues == 100'000);
}

TEST_CASE("vcs: a revision of more than maxOperations operations is refused before any is applied") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	const affresco::RevisionResult result =
	    repository.submit({{"author", "studio"}, {"set", stateEdits(maxOperations + 1)}});
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error == "AffrescoUI screen: revision: 10001 operations, more than the 10000 a revision (or a "
	                      "proposal, with all its commits) may have: split it into smaller ones");
	CHECK(repository.head().number == 0);

	// JSON Patch and compact edits count together
	json patch = json::array({{{"op", "add"}, {"path", "/state/extra"}, {"value", 1}}});
	CHECK(repository.submit({{"author", "studio"}, {"patch", patch}, {"set", stateEdits(maxOperations)}}).status ==
	      RevisionStatus::Rejected);
	CHECK(repository.submit({{"author", "studio"}, {"set", stateEdits(100)}}).status == RevisionStatus::Applied);
}

// An object finds a key by looking at its members one by one: 60,000 keys added to one object took 25 s
TEST_CASE("vcs: a revision may not give an object more than maxMembers members") {
	const affresco::format::Rules rules;
	json screen = permissionsScreen();
	screen["state"] = {{"wide", members(maxMembers)}}; // trusted: the screen file may hold one this wide
	affresco::vcs::Repository repository(rules, screen);

	// Adding to an object that has maxMembers already
	const affresco::RevisionResult added =
	    repository.submit({{"author", "studio"}, {"set", {{"/state/wide/one more", 1}}}});
	CHECK(added.status == RevisionStatus::Rejected);
	CHECK(added.error == "AffrescoUI screen: revision: the screen would hold an object of 10001 members, more than "
	                     "10000");

	// A value holding a wider object: refused before it is in the document
	const affresco::RevisionResult wider =
	    repository.submit({{"author", "studio"}, {"set", {{"/state/other", {{"inside", members(maxMembers + 1)}}}}}});
	CHECK(wider.status == RevisionStatus::Rejected);
	CHECK(wider.error.find("(set /state/other): the value holds an object of more than 10000 members") !=
	      std::string::npos);

	// The wide object can still change
	CHECK(repository.submit({{"author", "studio"}, {"set", {{"/state/wide/k7", "seven"}}}}).status ==
	      RevisionStatus::Applied);
	CHECK(repository.state().at("wide").at("k7") == "seven");
	CHECK(repository.state().at("wide").size() == maxMembers);
}

// accept() replays a proposal's commits as one patch: a proposal that grew past the limit could never be accepted
TEST_CASE("vcs: a push that would give a proposal more than maxOperations operations is refused") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());

	json appends = json::array({{{"op", "add"}, {"path", "/state/list"}, {"value", json::array()}}});
	for(int i = 1; i < 6000; ++i) appends.push_back({{"op", "add"}, {"path", "/state/list/-"}, {"value", i}});
	const affresco::RevisionResult first = repository.submit({{"author", "agent"}, {"patch", appends}});
	REQUIRE(first.status == RevisionStatus::Proposed);
	appends.erase(0);
	appends.push_back({{"op", "add"}, {"path", "/state/list/-"}, {"value", 0}});
	const affresco::RevisionResult second =
	    repository.submit({{"author", "agent"}, {"proposal", first.proposal}, {"patch", appends}});
	CHECK(second.status == RevisionStatus::Rejected);
	CHECK(second.error.find("12000 operations, more than the 10000") != std::string::npos);

	REQUIRE(repository.proposals().size() == 1);
	CHECK(repository.proposals()[0].head == first.commit); // unchanged
	CHECK_FALSE(repository.accept(first.proposal, {"studio", first.commit, false}));
	CHECK(repository.state().at("list").size() == 5999);
}
