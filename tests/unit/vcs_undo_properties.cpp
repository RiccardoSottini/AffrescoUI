/// @file
/// @brief Property tests for undoing an earlier commit that keeps the later changes: random histories of texts set on
/// labels, an earlier commit picked at random, and what must hold for every one of them (a plan changes nothing, what
/// no later commit touched is undone, a conflict is what a later commit touched, dropping takes it back, keeping leaves
/// it, the undo lands with its mark, and an undo of the undo restores).
///
/// The generators use std::mt19937 and take numbers from it by `%`, as vcs_properties.cpp does, so a seed gives the
/// same histories on every platform. A failure names its seed (CAPTURE).

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::UndoOptions;
using affresco::UndoPlan;
using affresco::vcs::Repository;

namespace {

constexpr std::uint32_t seeds = 20;  ///< Seeds each property is tried with.
constexpr std::uint32_t labels = 6;  ///< Labels on the screen.
constexpr std::uint32_t commits = 8; ///< Commits in each history.

/// @brief The texts of the labels, by label number.
using Texts = std::map<std::uint32_t, std::string>;

/// @brief A screen of labels l0, l1, ... that "studio" may change and review.
/// @return The document.
json labelScreen() {
	json children = json::array();
	for(std::uint32_t label = 0; label < labels; ++label)
		children.push_back({{"type", "label"}, {"id", "l" + std::to_string(label)}, {"text", "start"}});
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"permissions", {{"allow", json::array({"studio"})}, {"reviewers", json::array({"studio"})}}},
	        {"root", {{"type", "column"}, {"id", "main"}, {"children", children}}}};
}

/// @brief The path of a label's text.
/// @param label The label's number.
/// @return "#l<n>/text".
std::string textPath(std::uint32_t label) { return "#l" + std::to_string(label) + "/text"; }

/// @brief A random history on labelScreen(), and the texts after each commit.
struct History {
	affresco::format::Rules rules;                ///< The format's rules.
	Repository repository{rules, labelScreen()};  ///< At its last commit.
	std::vector<Texts> texts;                     ///< texts[n]: the texts after commit n (0: as loaded).
	std::vector<std::set<std::uint32_t>> changed; ///< changed[n]: the labels commit n set (0: none).
	std::uint32_t undone = 0;                     ///< The commit picked to undo (1 to commits).

	/// @brief Makes the history.
	/// @param seed The seed: the same one gives the same history.
	explicit History(std::uint32_t seed) {
		std::mt19937 random(seed);
		Texts now;
		for(std::uint32_t label = 0; label < labels; ++label) now[label] = "start";
		texts.push_back(now);
		changed.emplace_back();
		for(std::uint32_t number = 1; number <= commits; ++number) {
			std::set<std::uint32_t> set;
			const std::uint32_t count = 1u + static_cast<std::uint32_t>(random() % 2u);
			for(std::uint32_t k = 0; k < count; ++k) set.insert(static_cast<std::uint32_t>(random() % labels));
			json edits = json::object();
			for(const std::uint32_t label : set) {
				now[label] = "v" + std::to_string(number) + "." + std::to_string(label);
				edits[textPath(label)] = now[label];
			}
			const affresco::RevisionResult result = repository.submit(
			    {{"author", "studio"}, {"message", "Commit " + std::to_string(number)}, {"set", edits}});
			REQUIRE(result.status == RevisionStatus::Applied);
			texts.push_back(now);
			changed.push_back(set);
		}
		undone = 1u + static_cast<std::uint32_t>(random() % commits);
	}

	/// @brief The labels the undone commit set that a later commit set too: the conflicts.
	/// @return Their numbers.
	[[nodiscard]] std::set<std::uint32_t> touchedLater() const {
		std::set<std::uint32_t> touched;
		for(const std::uint32_t label : changed[undone])
			for(std::uint32_t later = undone + 1u; later <= commits; ++later)
				if(changed[later].contains(label)) touched.insert(label);
		return touched;
	}

	/// @brief The texts on main's head.
	/// @return By label number.
	[[nodiscard]] Texts current() const {
		Texts result;
		const json& children = repository.document()["root"]["children"];
		for(std::uint32_t label = 0; label < labels; ++label)
			result[label] = children[static_cast<std::size_t>(label)]["text"].get<std::string>();
		return result;
	}

	/// @brief The texts an undo should leave.
	/// @param dropped Whether the conflicts are dropped (else kept).
	/// @return By label number.
	[[nodiscard]] Texts expected(bool dropped) const {
		Texts result = texts.back();
		const std::set<std::uint32_t> touched = touchedLater();
		for(const std::uint32_t label : changed[undone])
			if(dropped || !touched.contains(label)) result[label] = texts[undone - 1u].at(label);
		return result;
	}
};

/// @brief Choices that drop or keep every conflict.
/// @param drop true to drop them, false to keep them.
/// @return The choices.
UndoOptions every(bool drop) {
	UndoOptions options;
	(drop ? options.drop : options.keep) = "all";
	return options;
}

/// @brief Applies an undo that must land on main.
/// @param history The history.
/// @param options The choices.
void landUndo(History& history, const UndoOptions& options) {
	const json revision = history.repository.undoRevision(static_cast<int>(history.undone), "studio", options);
	const affresco::RevisionResult result = history.repository.submit(revision);
	INFO(result.error);
	REQUIRE(result.status == RevisionStatus::Applied);
	CHECK(history.repository.head().undo == revision["undo"]);
}

} // namespace

TEST_CASE("vcs undo properties: P1 a plan changes nothing, and its conflicts are what later commits touched") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		const History history(seed);
		const std::string head = history.repository.head().id;
		const json document = history.repository.document();
		const UndoPlan plan = history.repository.undoPlan(static_cast<int>(history.undone), "studio");
		CHECK(history.repository.head().id == head);
		CHECK(history.repository.document() == document);
		std::set<std::string> paths;
		for(const json& conflict : plan.conflicts) paths.insert(conflict["path"].get<std::string>());
		std::set<std::string> touched;
		for(const std::uint32_t label : history.touchedLater()) touched.insert(textPath(label));
		CHECK(paths == touched);
		CHECK(plan.outcome == (touched.empty() ? "clean" : "choose"));
	}
}

TEST_CASE("vcs undo properties: P2 with no conflict, what the commit did is undone and every later change kept") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		History history(seed);
		if(!history.touchedLater().empty()) continue;
		landUndo(history, {});
		CHECK(history.current() == history.expected(true));
	}
}

TEST_CASE("vcs undo properties: P3 dropping every conflict takes each part back to before the commit") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		History history(seed);
		landUndo(history, every(true));
		CHECK(history.current() == history.expected(true));
	}
}

TEST_CASE("vcs undo properties: P4 keeping every conflict leaves it as later commits made it") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		History history(seed);
		const std::set<std::uint32_t> touched = history.touchedLater();
		if(touched.size() == history.changed[history.undone].size() && !touched.empty()) {
			// Keeping all of it leaves nothing to undo
			CHECK(history.repository.undoPlan(static_cast<int>(history.undone), "studio", every(false)).outcome ==
			      "blocked");
			continue;
		}
		landUndo(history, every(false));
		CHECK(history.current() == history.expected(false));
	}
}

TEST_CASE("vcs undo properties: P5 a chosen plan is ready, and its conflicts carry the choice") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		const History history(seed);
		const UndoPlan plan = history.repository.undoPlan(static_cast<int>(history.undone), "studio", every(true));
		CHECK(plan.outcome == (history.touchedLater().empty() ? "clean" : "ready"));
		for(const json& conflict : plan.conflicts) CHECK(conflict["choice"] == "drop");
	}
}

TEST_CASE("vcs undo properties: P6 undoing the undo restores main as it was") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		History history(seed);
		const json before = history.repository.document();
		landUndo(history, every(true));
		const int undo = history.repository.head().number;
		const affresco::RevisionResult result =
		    history.repository.submit(history.repository.undoRevision(undo, "studio"));
		REQUIRE(result.status == RevisionStatus::Applied);
		CHECK(history.repository.document() == before);
	}
}

TEST_CASE("vcs undo properties: P7 the undo of the head is the revert of the head") {
	for(std::uint32_t seed = 1; seed <= seeds; ++seed) {
		CAPTURE(seed);
		const History history(seed);
		const json undo = history.repository.undoRevision("HEAD", "studio");
		const json revert = history.repository.revertRevision("HEAD", "studio");
		CHECK(undo["patch"] == revert["patch"]);
		CHECK(undo["undo"]["drop"].empty());
	}
}
