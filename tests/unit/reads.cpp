/// @file
/// @brief Unit tests for what values read and what changed (src/Reads.hpp), and for the children of a long list by
/// item (src/ItemIndex.hpp).

#include "ItemIndex.hpp"
#include "Reads.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

using affresco::format::ItemIndex;
using affresco::format::ReadSet;
using affresco::format::Scope;
using affresco::format::StateChanges;

namespace {

/// @brief Changes of some places.
/// @param paths The places.
/// @return The changes.
StateChanges changesOf(const std::vector<std::string>& paths) {
	StateChanges changes;
	for(const std::string& path : paths) changes.add(path);
	return changes;
}

/// @brief A read set of some places.
/// @param paths The places.
/// @return The set.
ReadSet readsOf(const std::vector<std::string>& paths) {
	ReadSet reads;
	for(const std::string& path : paths) reads.addPath(path);
	return reads;
}

} // namespace

TEST_CASE("format: places meet when one is the other or inside it, key by key") {
	using affresco::format::contains;
	using affresco::format::overlaps;
	CHECK(contains("/a", "/a"));
	CHECK(contains("/a", "/a/b"));
	CHECK_FALSE(contains("/a", "/ab"));
	CHECK_FALSE(contains("/a/b", "/a"));
	CHECK(contains("", "/a"));
	CHECK(contains("/a/~*/id", "/a/3/id/x"));
	CHECK_FALSE(contains("/a/3", "/a/~*/id"));
	CHECK(contains("/a/3", "/a/~*/id", true));
	CHECK(overlaps("/a/b", "/a"));
	CHECK(overlaps("/a", "/a/b"));
	CHECK(overlaps("/a/~*/id", "/a/7/id"));
	CHECK_FALSE(overlaps("/a/~*/id", "/a/7/title"));
	CHECK_FALSE(overlaps("/a/~#", "/a/7/title"));
	CHECK(overlaps("/a/~#", "/a"));
}

TEST_CASE("format: a read set keeps fewer places when it has too many, reading more, never less") {
	ReadSet reads;
	for(std::size_t i = 0; i < 100; ++i) reads.addPath("/items/" + std::to_string(i) + "/id");
	CHECK(reads.paths.size() <= affresco::format::maxReadPaths);
	CHECK(reads.touchedBy(changesOf({"/items/57/id"})));
	CHECK(reads.touchedBy(changesOf({"/items"})));
	CHECK_FALSE(reads.touchedBy(changesOf({"/items/57/title"})));
	CHECK_FALSE(reads.touchedBy(changesOf({"/other"})));

	ReadSet mixed;
	for(std::size_t i = 0; i < 100; ++i) mixed.addPath("/k" + std::to_string(i));
	CHECK(mixed.paths.size() <= affresco::format::maxReadPaths);
	CHECK(mixed.touchedBy(changesOf({"/k99"})));
}

TEST_CASE("format: a place read inside another is kept once, and reading always meets every change") {
	ReadSet reads = readsOf({"/a/b", "/a", "/a/c"});
	CHECK(reads.paths == std::vector<std::string>{"/a"});
	CHECK_FALSE(reads.touchedBy(StateChanges{}));
	reads.always = true;
	CHECK(reads.touchedBy(StateChanges{}));
	StateChanges all;
	all.addAll();
	CHECK(readsOf({"/x"}).touchedBy(all));
}

TEST_CASE("format: changes of too many places count as a change of everything") {
	StateChanges changes;
	for(std::size_t i = 0; i <= affresco::format::maxChangedPaths; ++i) changes.add("/p" + std::to_string(i));
	CHECK(changes.all);
	CHECK(changes.paths.empty());
}

TEST_CASE("format: the children of a long list are looked up by the item they read") {
	std::vector<Scope> items(70);
	for(std::size_t i = 0; i < items.size(); ++i) items[i].origin = "/items/" + std::to_string(i);
	ItemIndex index;
	index.start(items.size());
	for(std::size_t i = 0; i + 1 < items.size(); ++i)
		index.add(i, &items[i], readsOf({items[i].origin + "/title"}));
	index.add(69, &items[69], readsOf({"/count"})); // reads more than its item: visited every time

	const auto one = index.visits(changesOf({"/items/5/title"}), items.size());
	REQUIRE(one);
	CHECK(*one == std::vector<std::size_t>{5, 69});
	const auto inside = index.visits(changesOf({"/items/7/title/x", "/items/3"}), items.size());
	REQUIRE(inside);
	CHECK(*inside == std::vector<std::size_t>{3, 7, 69});
	const auto elsewhere = index.visits(changesOf({"/count"}), items.size());
	REQUIRE(elsewhere);
	CHECK(*elsewhere == std::vector<std::size_t>{69});

	CHECK_FALSE(index.visits(changesOf({"/items"}), items.size()));    // the list itself
	CHECK_FALSE(index.visits(changesOf({""}), items.size()));          // the whole state
	CHECK_FALSE(index.visits(changesOf({"/count"}), items.size() + 1)); // the children moved since
}

TEST_CASE("format: an item index is dropped when a child it looks up starts reading more") {
	std::vector<Scope> items(2);
	items[0].origin = "/items/0";
	items[1].origin = "/items/1";
	ItemIndex index;
	index.start(2);
	index.add(0, &items[0], readsOf({"/items/0/title"}));
	index.add(1, &items[1], readsOf({"/flag"}));
	CHECK(index.keeps(&items[0], readsOf({"/items/0/done"})));
	CHECK(index.keeps(&items[1], readsOf({"/flag", "/count"}))); // with the others already
	CHECK_FALSE(index.keeps(&items[0], readsOf({"/items/0/done", "/flag"})));
	index.drop();
	CHECK_FALSE(index.visits(changesOf({"/flag"}), 2));
}
