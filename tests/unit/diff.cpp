/// @file
/// @brief Unit tests for core's TextDiff: the scripts it finds cover both texts, are as short as possible (checked
/// against a dynamic-programming longest common subsequence on small random sequences), keep their shape, and stay
/// correct when the cost limits cut the search short.

#include <affresco/core/TextDiff.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using affresco::DiffEdit;
using affresco::DiffLimits;
using affresco::diffLines;
using affresco::diffSequences;
using affresco::splitLines;
using Kind = affresco::DiffEdit::Kind;

namespace {

/// @brief Checks that a script is well formed for two sequences and rebuilds both from it.
/// @param edits The script.
/// @param a The original.
/// @param b The modified sequence.
/// @return The elements the script keeps (its common subsequence).
std::size_t checkScript(const std::vector<DiffEdit>& edits, const std::vector<std::uint32_t>& a,
                        const std::vector<std::uint32_t>& b) {
	std::size_t i = 0, j = 0, kept = 0;
	std::vector<std::uint32_t> rebuiltA, rebuiltB;
	for(std::size_t e = 0; e < edits.size(); ++e) {
		const DiffEdit& edit = edits[e];
		REQUIRE(edit.count > 0);
		REQUIRE(edit.original == i);
		REQUIRE(edit.modified == j);
		if(e > 0) CHECK(edits[e - 1].kind != edit.kind); // neighbours of one kind are merged
		if(e > 0 && edit.kind == Kind::Delete) CHECK(edits[e - 1].kind != Kind::Insert); // deletions first
		for(std::size_t k = 0; k < edit.count; ++k) {
			if(edit.kind != Kind::Insert) rebuiltA.push_back(a.at(i + k));
			if(edit.kind != Kind::Delete) rebuiltB.push_back(b.at(j + k));
			if(edit.kind == Kind::Equal) REQUIRE(a.at(i + k) == b.at(j + k));
		}
		if(edit.kind == Kind::Equal) kept += edit.count;
		if(edit.kind != Kind::Insert) i += edit.count;
		if(edit.kind != Kind::Delete) j += edit.count;
	}
	CHECK(rebuiltA == a);
	CHECK(rebuiltB == b);
	return kept;
}

/// @brief The length of the longest common subsequence, by dynamic programming.
/// @param a One sequence.
/// @param b The other.
std::size_t longestCommon(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
	std::vector<std::size_t> row(b.size() + 1, 0), next(b.size() + 1, 0);
	for(std::size_t i = a.size(); i-- > 0;) {
		for(std::size_t j = b.size(); j-- > 0;)
			next[j] = a[i] == b[j] ? row[j + 1] + 1 : std::max(row[j], next[j + 1]);
		std::swap(row, next);
	}
	return row[0];
}

/// @brief A random sequence over a small alphabet, so that it shares much with another.
/// @param random The generator.
/// @param length Its length.
/// @param alphabet How many different elements.
std::vector<std::uint32_t> randomSequence(std::mt19937& random, std::size_t length, std::uint32_t alphabet) {
	std::uniform_int_distribution<std::uint32_t> element(0, alphabet - 1);
	std::vector<std::uint32_t> out(length);
	for(std::uint32_t& e : out) e = element(random);
	return out;
}

} // namespace

TEST_CASE("diff: equal, empty and one-sided sequences") {
	const std::vector<std::uint32_t> some{1, 2, 3}, none;
	CHECK(diffSequences(none, none).empty());
	CHECK(diffSequences(some, some) == std::vector<DiffEdit>{{Kind::Equal, 0, 0, 3}});
	CHECK(diffSequences(some, none) == std::vector<DiffEdit>{{Kind::Delete, 0, 0, 3}});
	CHECK(diffSequences(none, some) == std::vector<DiffEdit>{{Kind::Insert, 0, 0, 3}});
}

TEST_CASE("diff: a change in the middle keeps both ends") {
	const std::vector<std::uint32_t> a{1, 2, 3, 4, 5}, b{1, 2, 9, 4, 5};
	CHECK(diffSequences(a, b) == std::vector<DiffEdit>{{Kind::Equal, 0, 0, 2},
	                                                   {Kind::Delete, 2, 2, 1},
	                                                   {Kind::Insert, 3, 2, 1},
	                                                   {Kind::Equal, 3, 3, 2}});
}

TEST_CASE("diff: the scripts are the shortest on random sequences") {
	std::mt19937 random(2017); // NOLINT(bugprone-random-generator-seed): fixed, so a failure reproduces
	for(int round = 0; round < 400; ++round) {
		const auto lengthA = static_cast<std::size_t>(random() % 40), lengthB = static_cast<std::size_t>(random() % 40);
		const auto alphabet = static_cast<std::uint32_t>(1 + random() % 6);
		const std::vector<std::uint32_t> a = randomSequence(random, lengthA, alphabet);
		const std::vector<std::uint32_t> b = randomSequence(random, lengthB, alphabet);
		const std::size_t kept = checkScript(diffSequences(a, b), a, b);
		CHECK(kept == longestCommon(a, b));
	}
}

TEST_CASE("diff: past its limits a script is still correct, only longer") {
	std::mt19937 random(31); // NOLINT(bugprone-random-generator-seed): fixed, so a failure reproduces
	const std::vector<std::uint32_t> a = randomSequence(random, 3000, 4), b = randomSequence(random, 3000, 4);
	const std::size_t best = checkScript(diffSequences(a, b), a, b);
	CHECK(best == longestCommon(a, b));
	for(const DiffLimits limits : {DiffLimits{.maxCost = 0}, DiffLimits{.maxCost = 5000}, DiffLimits{.maxDistance = 3}}) {
		const std::size_t kept = checkScript(diffSequences(a, b, limits), a, b);
		CHECK(kept <= best);
	}
	// No budget at all: what the ends share, then one deletion and one insertion
	const std::vector<std::uint32_t> c{7, 1, 2, 7}, d{7, 3, 4, 7};
	CHECK(diffSequences(c, d, {.maxCost = 0}) == std::vector<DiffEdit>{{Kind::Equal, 0, 0, 1},
	                                                                   {Kind::Delete, 1, 1, 2},
	                                                                   {Kind::Insert, 3, 1, 2},
	                                                                   {Kind::Equal, 3, 3, 1}});
}

TEST_CASE("diff: large files with few changes are diffed fast and exactly") {
	std::vector<std::uint32_t> a(200'000);
	for(std::size_t i = 0; i < a.size(); ++i) a[i] = static_cast<std::uint32_t>(i);
	std::vector<std::uint32_t> b = a;
	b.erase(b.begin() + 1000);
	b.insert(b.begin() + 150'000, 4'000'000);
	b[90'000] = 4'000'001;
	const std::vector<DiffEdit> edits = diffSequences(a, b);
	CHECK(checkScript(edits, a, b) == a.size() - 2);
	CHECK(edits.size() == 8); // kept, deleted, kept, replaced (two), kept, inserted, kept
}

TEST_CASE("diff: lines split at line feeds, with or without carriage returns") {
	CHECK(splitLines("") == std::vector<std::string_view>{""});
	CHECK(splitLines("a\r\nb\n") == std::vector<std::string_view>{"a", "b", ""});
	CHECK(splitLines("a\rb") == std::vector<std::string_view>{"a\rb"});
}

TEST_CASE("diff: lines are compared by their text") {
	const std::vector<std::string_view> a = splitLines("one\ntwo\nthree\nfour"), b = splitLines("one\nthree\nfour\nfive");
	CHECK(diffLines(a, b) == std::vector<DiffEdit>{{Kind::Equal, 0, 0, 1},
	                                               {Kind::Delete, 1, 1, 1},
	                                               {Kind::Equal, 2, 1, 2},
	                                               {Kind::Insert, 4, 3, 1}});
}
