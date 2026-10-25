/// @file
/// @brief Unit tests for review policies (the "review" key of a screen's permissions, VERSION_CONTROL.md): more
/// approvals for behaviour changes, owners per part of the screen, and "don't ask again" grants that expire.

#include "Helpers.hpp"
#include "HistoryFiles.hpp"

#include <affresco/core/Portable.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;
using affresco::test::readText;
using affresco::test::TemporaryFolder;
using affresco::test::writeText;
using affresco::vcs::detail::sha256;

namespace {

/// @brief The permissions screen with a review policy.
/// @param review The policy.
/// @return The screen, reviewed by "studio" and "human".
json policyScreen(const json& review) {
	json screen = permissionsScreen();
	screen["permissions"]["reviewers"] = {"studio", "human"};
	screen["permissions"]["review"] = review;
	return screen;
}

/// @brief A UTC time this many seconds from now.
/// @param seconds How far.
/// @return ISO 8601.
std::string utcIn(long long seconds) {
	const std::time_t then = std::time(nullptr) + static_cast<std::time_t>(seconds);
	const std::tm parts = affresco::utcTime(then); // Windows has no gmtime_r
	std::array<char, 32> text{};
	std::strftime(text.data(), text.size(), "%Y-%m-%dT%H:%M:%SZ", &parts);
	return text.data();
}

} // namespace

TEST_CASE("review policy: the format checks it") {
	const affresco::format::Rules rules;
	CHECK_NOTHROW(rules.validate(policyScreen({{"approvals", 1},
	                                           {"behaviourApprovals", 2},
	                                           {"owners", {{"#title", {"human/anna"}}}},
	                                           {"grantsLast", "8h"}})));
	CHECK_THROWS(rules.validate(policyScreen({{"approvals", 0}})));
	CHECK_THROWS(rules.validate(policyScreen({{"grantsLast", "soon"}})));
	CHECK_THROWS(rules.validate(policyScreen({{"owners", {{"title", {"anna"}}}}})));
	CHECK_THROWS(rules.validate(policyScreen({{"owners", {{"#title", json::array()}}}})));
	CHECK_THROWS(rules.validate(policyScreen({{"everyone", true}})));

	const affresco::ReviewPolicy policy =
	    rules.permissions(policyScreen({{"behaviourApprovals", 2}, {"owners", {{"#title", {"human/anna"}}}}})).review;
	CHECK(policy.approvals == 1);
	CHECK(policy.behaviourApprovals == 2);
	REQUIRE(policy.owners.size() == 1);
	CHECK(policy.owners[0].first == "#title");
}

TEST_CASE("review policy: a change in behaviour waits for two approvals from different reviewers") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, policyScreen({{"behaviourApprovals", 2}}));
	const affresco::RevisionResult proposed = repository.submit(
	    {{"author", "agent"}, {"set", {{"#title/onClick", json::array({{{"set", "state.count"}, {"to", 1}}})}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE(repository.proposals().size() == 1);
	CHECK(repository.proposals()[0].behaviour == std::vector<std::string>{"adds onClick of #title"});
	CHECK(repository.proposals()[0].policy == "2 approvals");

	// The first approval is counted, and the proposal waits
	CHECK_FALSE(repository.accept(proposed.proposal, {"studio/bob", proposed.commit, false}));
	REQUIRE(repository.proposals().size() == 1);
	CHECK(repository.proposals()[0].approvals == std::vector<std::string>{"studio/bob"});
	CHECK(repository.proposals()[0].reason.find("waiting for 1 more approval") != std::string::npos);
	CHECK(repository.proposals()[0].reason.starts_with("approved by 1,")); // what counted, not the three names
	CHECK_FALSE(repository.document()["root"]["children"][1].contains("onClick"));

	// The same reviewer again doesn't count twice
	CHECK_FALSE(repository.accept(proposed.proposal, {"studio/bob", proposed.commit, false}));
	REQUIRE(repository.proposals().size() == 1);

	// A second reviewer merges it
	CHECK_FALSE(repository.accept(proposed.proposal, {"human/carol", proposed.commit, false}));
	CHECK(repository.proposals().empty());
	CHECK(repository.document()["root"]["children"][1].contains("onClick"));

	// A change of text only needs one
	const affresco::RevisionResult text = repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(text.status == RevisionStatus::Proposed);
	CHECK_FALSE(repository.accept(text.proposal, {"studio/bob", text.commit, false}));
	CHECK(repository.proposals().empty());
}

TEST_CASE("review policy: a change to an owned part needs an approval from one of its owners") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, policyScreen({{"owners", {{"#title", {"human/anna"}}}}}));
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	CHECK(repository.proposals()[0].policy == "1 approval, one from an owner of #title (human/anna)");

	CHECK_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, false}));
	REQUIRE(repository.proposals().size() == 1);
	CHECK(repository.proposals()[0].reason.find("an approval from an owner of #title (human/anna)") !=
	      std::string::npos);

	CHECK_FALSE(repository.accept(proposed.proposal, {"human/anna", proposed.commit, false}));
	CHECK(repository.proposals().empty());
	CHECK(repository.document()["root"]["children"][1]["text"] == "Today");
}

TEST_CASE("review policy: an author never approves its own proposal, whatever its channel's other names") {
	const affresco::format::Rules rules;
	json screen = policyScreen({{"owners", {{"#title", {"agent"}}}}});
	screen["permissions"]["reviewers"] = {"studio", "agent"};
	affresco::vcs::Repository repository(rules, screen);
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent/one"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	const std::string refused =
	    repository.accept(proposed.proposal, {"agent/two", proposed.commit, false}).value_or("");
	REQUIRE_FALSE(refused.empty());
	CHECK(refused.find("may not accept its own proposal") != std::string::npos);
	CHECK(repository.proposals()[0].approvals.empty());
}

TEST_CASE("review policy: one channel's approvals count once, whatever names it gives its reviewers") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, policyScreen({{"approvals", 2}}));
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);

	CHECK_FALSE(repository.accept(proposed.proposal, {"studio/bob", proposed.commit, false}));
	CHECK_FALSE(repository.accept(proposed.proposal, {"studio/anna", proposed.commit, false}));
	CHECK_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, false}));
	REQUIRE(repository.proposals().size() == 1); // the channel names its users: one channel, one approval
	CHECK(repository.proposals()[0].reason.find("waiting for 1 more approval") != std::string::npos);

	CHECK_FALSE(repository.accept(proposed.proposal, {"human/carol", proposed.commit, false}));
	CHECK(repository.proposals().empty());
}

TEST_CASE("review policy: a rebase that changes what a proposal does dismisses its approvals") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, policyScreen({{"approvals", 2}}));
	const affresco::RevisionResult proposed = repository.submit(
	    {{"author", "agent"},
		 {"patch",
		  json::array(
	          {{{"op", "add"}, {"path", "/root/children/1"}, {"value", {{"type", "panel"}, {"id", "extra"}}}}})}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	CHECK_FALSE(repository.accept(proposed.proposal, {"studio/bob", proposed.commit, false}));
	REQUIRE(repository.proposals()[0].approvals.size() == 1);

	REQUIRE(repository
	            .submit({{"author", "studio"},
	                     {"patch", json::array({{{"op", "add"},
	                                             {"path", "/root/children/0"},
	                                             {"value", {{"type", "panel"}, {"id", "first"}}}}})}})
	            .status == RevisionStatus::Applied);
	REQUIRE(repository.proposals().size() == 1);
	CHECK(repository.proposals()[0].approvals.empty());
	const std::string refused =
	    repository.accept(proposed.proposal, {"human/carol", proposed.commit, false}).value_or("");
	REQUIRE_FALSE(refused.empty()); // the head bob and carol saw is no longer what it merges
	CHECK(refused.find("changed since you reviewed it") != std::string::npos);
}

TEST_CASE("review policy: grants expire, at most when the policy says") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	{
		affresco::vcs::Repository repository(rules, policyScreen({{"grantsLast", "1h"}}));
		repository.openHistory(history);
		const affresco::RevisionResult proposed =
		    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Today"}}}});
		REQUIRE(proposed.status == RevisionStatus::Proposed);

		const std::string bad =
		    repository.accept(proposed.proposal, {"studio", proposed.commit, true, "forever"}).value_or("");
		REQUIRE_FALSE(bad.empty());
		CHECK(bad.find("is not a duration for grants") != std::string::npos);

		REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, true, "7d"}));
		REQUIRE(repository.grants().size() == 1);
		const std::string expires = repository.grants()[0].expires;
		CHECK(expires > utcIn(3500)); // capped at the policy's hour
		CHECK(expires <= utcIn(3700));
		CHECK(repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Later"}}}}).status ==
		      RevisionStatus::Applied);
	}

	// An hour later, as far as the history says: the grant no longer allows
	json refs = json::parse(readText(history / "refs.json"));
	REQUIRE(refs["grants"].size() == 1);
	refs["grants"][0]["expires"] = "2000-01-01T00:00:00Z";
	refs.erase("checksum");
	refs["checksum"] = sha256(refs.dump());
	writeText(history / "refs.json", refs.dump(2));

	affresco::vcs::Repository repository(rules, policyScreen({{"grantsLast", "1h"}}));
	REQUIRE(repository.openHistory(history).loaded);
	CHECK(repository.grants().empty());
	CHECK(repository.submit({{"author", "agent"}, {"set", {{"#title/text", "Again"}}}}).status ==
	      RevisionStatus::Proposed);

	// An expiry that isn't a UTC time would sort after every time, and never expire: the history is refused
	for(const char* never : {"never", "9999", "2000-01-01 00:00:00"}) {
		INFO(never);
		refs["grants"][0]["expires"] = never;
		refs.erase("checksum");
		refs["checksum"] = sha256(refs.dump());
		writeText(history / "refs.json", refs.dump(2));
		affresco::vcs::Repository reopened(rules, policyScreen({{"grantsLast", "1h"}}));
		CHECK_THROWS_WITH_AS(reopened.openHistory(history), doctest::Contains("grants[0].expires: a UTC time"),
		                     affresco::DocumentError);
		CHECK(reopened.grants().empty());
	}
}
