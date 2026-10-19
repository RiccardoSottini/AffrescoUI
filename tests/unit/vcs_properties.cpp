/// @file
/// @brief Property tests for the vcs module: random revisions from seeded generators, and what must hold for every
/// one of them (a revert restores, a restore matches, a change set replays main, permissions follow the effect).
///
/// The generators use std::mt19937 and take numbers from it by `%`, not through a distribution: the engine's output
/// is the same everywhere, distributions aren't, so a seed gives the same revisions on every platform. A failure
/// names its seed and step (CAPTURE).

#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;

namespace {

constexpr std::uint32_t seeds = 20;      ///< Seeds each property is tried with.
constexpr int revisionsPerSeed = 12;     ///< Random revisions in each history.

/// @brief Random revisions of permissionsScreen(), as the studio would send them.
class RevisionGenerator {
public:
	/// @brief Starts a sequence.
	/// @param seed The seed: the same one gives the same revisions.
	explicit RevisionGenerator(std::uint32_t seed) : random_(seed) {}

	/// @brief A number below a bound.
	/// @param bound The bound (more than 0).
	/// @return 0 to bound - 1.
	std::uint32_t below(std::uint32_t bound) { return static_cast<std::uint32_t>(random_() % bound); }

	/// @brief A random value: a number, a text, a flag, or a small object holding them.
	/// @return The value.
	json value() {
		switch(below(4)) {
		case 0: return static_cast<int>(below(1000));
		case 1: return "text " + std::to_string(below(100));
		case 2: return below(2) == 1;
		default: return {{"a", static_cast<int>(below(10))}, {"b", {"x", below(2) == 1}}};
		}
	}

	/// @brief The next revision. Every one applies to the document as the earlier ones left it.
	/// @return The revision, authored by "studio".
	json next() {
		json edits;
		const std::string label = labels_.empty() ? "" : "#" + labels_[below(static_cast<std::uint32_t>(labels_.size()))];
		switch(below(labels_.empty() ? 5 : 8)) {
		case 0: edits = {{"set", {{"/state/count", static_cast<int>(below(100))}}}}; break;
		case 1: edits = {{"set", {{"/state/key" + std::to_string(below(4)), value()}}}}; break;
		case 2: edits = {{"set", {{"#title/text", "Title " + std::to_string(below(1000))}}}}; break;
		case 3: edits = {{"set", {{"#board/color", color()}}}}; break;
		case 4: {
			const std::string id = "label" + std::to_string(added_++);
			labels_.push_back(id);
			edits = {{"add", {{"#main/children/-", {{"type", "label"}, {"id", id}, {"text", id}}}}}};
			break;
		}
		case 5: edits = {{"set", {{label + "/text", "Edited " + std::to_string(below(100))}}}}; break;
		case 6: edits = {{"move", {{label, "#main/children/0"}}}}; break;
		default:
			std::erase(labels_, label.substr(1));
			edits = {{"remove", json::array({label})}};
		}
		edits["author"] = "studio";
		edits["message"] = "random";
		return edits;
	}

private:
	/// @brief A random colour.
	/// @return E.g. "#0a7f3c".
	std::string color() {
		static constexpr std::string_view digits = "0123456789abcdef";
		std::string text = "#";
		for(int i = 0; i < 6; ++i) text += digits[below(16)];
		return text;
	}

	std::mt19937 random_;             ///< The engine.
	std::vector<std::string> labels_; ///< Ids of the labels added and not removed.
	int added_ = 0;                   ///< Labels added so far (for new ids).
};

/// @brief Applies a revision, which must apply.
/// @param repository The repository.
/// @param revision The revision.
void applyRevision(affresco::vcs::Repository& repository, const json& revision) {
	const affresco::RevisionResult result = repository.submit(revision);
	CAPTURE(revision.dump());
	CAPTURE(result.error);
	REQUIRE(result.status == RevisionStatus::Applied);
}

/// @brief The decision on a revision submitted to a new repository of permissionsScreen().
/// @param revision The revision.
/// @return Applied, Proposed or Rejected.
RevisionStatus decisionOn(const json& revision) {
	static const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	return repository.submit(revision).status;
}

/// @brief One effect, a value set on a node or in the state, spelled as several different revisions.
struct Effect {
	std::string id;      ///< The node's id ("" for the state).
	std::string pointer; ///< Its JSON Pointer, or the state's ("/state").
	std::string type;    ///< The node's type (for a "test" operation).
	std::string key;     ///< The property, or the state key.
	json value;          ///< What it is set to.

	/// @brief The effect, spelled in every way the format allows.
	/// @return Revisions without an author.
	[[nodiscard]] std::vector<json> spellings() const {
		const std::string place = pointer + "/" + key;
		std::vector<json> result = {
		    {{"set", {{place, value}}}},
		    {{"patch", {{{"op", "add"}, {"path", place}, {"value", value}}}}},
		    {{"patch", {{{"op", "add"}, {"path", place}, {"value", "first"}}, // two steps, one effect
		                {{"op", "replace"}, {"path", place}, {"value", value}}}}},
		};
		if(!id.empty()) {
			result.push_back({{"set", {{"#" + id + "/" + key, value}}}});
			result.push_back({{"patch",
			                   {{{"op", "test"}, {"path", pointer + "/type"}, {"value", type}},
			                    {{"op", "add"}, {"path", "#" + id + "/" + key}, {"value", value}}}}});
		}
		return result;
	}
};

} // namespace

// The live state is data, not history (VERSION_CONTROL.md): a revert gives back the document, and a revision that
// changed only the state has nothing to revert
TEST_CASE("vcs properties: applying a random revision, then reverting it, restores the document") {
	const affresco::format::Rules rules;
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		RevisionGenerator generate(seed);
		affresco::vcs::Repository repository(rules, permissionsScreen());
		for(int step = 0; step < revisionsPerSeed; ++step) {
			CAPTURE(step);
			const std::string document = repository.document().dump();
			const json revision = generate.next();
			applyRevision(repository, revision);
			if(repository.document().dump() == document) {
				CHECK_THROWS_WITH_AS((void)repository.revertRevision("HEAD", "studio"),
				                     doctest::Contains("only changed the live state"), affresco::DocumentError);
				continue;
			}
			applyRevision(repository, repository.revertRevision("HEAD", "studio"));
			CHECK(repository.document().dump() == document);
			applyRevision(repository, revision); // and on: the next revision builds on this one
		}
	}
}

TEST_CASE("vcs properties: restoring revision n gives the document of revision n") {
	const affresco::format::Rules rules;
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		RevisionGenerator generate(seed);
		affresco::vcs::Repository repository(rules, permissionsScreen());
		std::vector<std::string> documents = {repository.document().dump()};
		for(int step = 0; step < revisionsPerSeed; ++step) {
			applyRevision(repository, generate.next());
			documents.push_back(repository.document().dump());
		}
		for(int restore = 0; restore < 4; ++restore) {
			const auto n = static_cast<int>(generate.below(static_cast<std::uint32_t>(documents.size())));
			CAPTURE(n);
			const std::string& wanted = documents[static_cast<std::size_t>(n)];
			if(repository.document().dump() == wanted) {
				CHECK_THROWS_WITH_AS((void)repository.restoreRevision(n, "studio"),
				                     doctest::Contains("nothing to restore"), affresco::DocumentError);
				continue;
			}
			applyRevision(repository, repository.restoreRevision(n, "studio"));
			CHECK(repository.document().dump() == wanted);
		}
	}
}

TEST_CASE("vcs properties: a change set exported and imported elsewhere gives the same main") {
	const affresco::format::Rules rules;
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		RevisionGenerator generate(seed);
		affresco::vcs::Repository source(rules, permissionsScreen());
		for(int step = 0; step < revisionsPerSeed; ++step) applyRevision(source, generate.next());

		affresco::vcs::Repository target(rules, permissionsScreen());
		const std::optional<std::string> error =
		    target.importChanges(affresco::parseUntrusted(source.exportChanges(0, "random").dump()));
		CAPTURE(error.value_or(""));
		REQUIRE_FALSE(error);
		REQUIRE(target.log().size() == source.log().size());
		for(int n = 1; n <= revisionsPerSeed; ++n) { // every commit on main has the same document
			CAPTURE(n);
			CHECK(target.diff(0, n) == source.diff(0, n));
		}
		CHECK(target.document().dump() == source.document().dump());
		CHECK(target.state() == source.state());
	}
}

TEST_CASE("vcs properties: one effect spelled as different revisions gets the same permission decision") {
	// The agent may change #board only, nobody #statusBar; what isn't allowed asks for review
	const std::vector<Effect> places = {
	    {"board", "/root/children/0", "panel", "color", {}},  {"board", "/root/children/0", "panel", "note", {}},
	    {"title", "/root/children/1", "label", "text", {}},   {"statusBar", "/root/children/2", "panel", "color", {}},
	    {"", "/state", "", "count", {}},                       {"", "/state", "", "fresh", {}},
	};
	std::mt19937 random(7);
	std::set<RevisionStatus> seen; // all three decisions, or the property says little
	for(int trial = 0; trial < 60; ++trial) {
		Effect effect = places[random() % places.size()];
		effect.value = effect.key == "color" ? json("#00ff00") : json("value " + std::to_string(random() % 100));
		for(const char* author : {"agent", "studio", "agent/claude-code"}) {
			CAPTURE(effect.pointer + "/" + effect.key);
			CAPTURE(author);
			std::vector<RevisionStatus> decisions;
			for(json revision : effect.spellings()) {
				revision["author"] = author;
				decisions.push_back(decisionOn(revision));
			}
			for(const RevisionStatus decision : decisions) CHECK(decision == decisions.front());
			seen.insert(decisions.front());
		}
	}
	CHECK(seen == std::set<RevisionStatus>{RevisionStatus::Applied, RevisionStatus::Proposed, RevisionStatus::Rejected});
}
