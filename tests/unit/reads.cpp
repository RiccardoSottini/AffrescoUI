/// @file
/// @brief Unit tests for what values read and what changed (src/Reads.hpp), and for the readers of the state by the
/// place they read (src/PlaceIndex.hpp).

#include "Expression.hpp"
#include "PlaceIndex.hpp"
#include "Reads.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using affresco::format::ListChange;
using affresco::format::PlaceIndex;
using affresco::format::ReaderId;
using affresco::format::ReadSet;
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

namespace {

/// @brief The readers some changes touch, sorted.
/// @param index The index.
/// @param changes The changes.
/// @return Their handles.
std::vector<ReaderId> collected(PlaceIndex& index, const StateChanges& changes) {
	std::vector<ReaderId> out;
	index.collect(changes, out);
	std::ranges::sort(out);
	return out;
}

/// @brief A handle of a slot and a generation.
/// @param slot The slot.
/// @param generation The generation.
/// @return The handle.
constexpr ReaderId handle(std::uint32_t slot, std::uint32_t generation = 1) {
	return slot | (generation << affresco::format::readerSlotBits);
}

} // namespace

TEST_CASE("place index: a change finds the readers at it, above it and below it") {
	PlaceIndex index;
	const std::vector<std::string> exact{"/a/b"};
	const std::vector<std::string> above{"/a"};
	const std::vector<std::string> below{"/a/b/c"};
	const std::vector<std::string> elsewhere{"/a/x", "/z"};
	index.add(handle(1), exact);
	index.add(handle(2), above);
	index.add(handle(3), below);
	index.add(handle(4), elsewhere);
	CHECK(collected(index, changesOf({"/a/b"})) == std::vector<ReaderId>{handle(1), handle(2), handle(3)});
	CHECK(collected(index, changesOf({"/a/b/c/d"})) == std::vector<ReaderId>{handle(1), handle(2), handle(3)});
	CHECK(collected(index, changesOf({"/a"})) == std::vector<ReaderId>{handle(1), handle(2), handle(3), handle(4)});
	CHECK(collected(index, changesOf({"/q"})).empty());
	CHECK(collected(index, changesOf({"/z", "/a/x"})) == std::vector<ReaderId>{handle(2), handle(4)}); // each once
	CHECK(index.entries() == 5);
}

TEST_CASE("place index: a shape reader is found by the list's change, not by one inside an item") {
	PlaceIndex index;
	const std::vector<std::string> shape{"/items/" + std::string(affresco::format::shapeKey)};
	index.add(handle(1), shape);
	CHECK(collected(index, changesOf({"/items/3/title"})).empty());
	CHECK(collected(index, changesOf({"/items"})) == std::vector<ReaderId>{handle(1)});
	CHECK_FALSE(collected(index, changesOf({""})).empty()); // the whole state
	StateChanges listed;                                    // a list change given alone
	listed.lists.push_back({"/items", ListChange::Kind::Inserted, 2, 1});
	CHECK(collected(index, listed) == std::vector<ReaderId>{handle(1)});
	StateChanges inner;
	inner.lists.push_back({"/items/3/tags", ListChange::Kind::Removed, 0, 1});
	CHECK(collected(index, inner).empty());
}

TEST_CASE("place index: anyKey in a read matches any key at its level") {
	PlaceIndex index;
	const std::vector<std::string> any{"/items/" + std::string(affresco::format::anyKey) + "/done"};
	index.add(handle(1), any);
	CHECK(collected(index, changesOf({"/items/7/done"})) == std::vector<ReaderId>{handle(1)});
	CHECK(collected(index, changesOf({"/items/7"})) == std::vector<ReaderId>{handle(1)});
	CHECK(collected(index, changesOf({"/items/7/title"})).empty());
	StateChanges all;
	all.addAll();
	CHECK(collected(index, all).empty()); // everything: the caller updates every reader
}

TEST_CASE("place index: removing a reader frees the nodes nobody reads, and an old handle does nothing") {
	PlaceIndex index;
	const std::size_t empty = index.nodes();
	const std::size_t keys = index.keys();
	const std::vector<std::string> one{"/a/b/c", "/a/d"};
	const std::vector<std::string> other{"/a/b"};
	index.add(handle(1), one);
	index.add(handle(2), other);
	CHECK(index.nodes() == empty + 4);
	index.remove(handle(1, 2)); // another generation: not this reader
	CHECK(index.contains(handle(1)));
	CHECK(index.entries() == 3);
	index.remove(handle(1));
	CHECK_FALSE(index.contains(handle(1)));
	CHECK(index.nodes() == empty + 2); // /a and /a/b stay
	CHECK(collected(index, changesOf({"/a/b/c"})) == std::vector<ReaderId>{handle(2)});
	index.add(handle(1, 2), one); // the slot again, a new generation
	CHECK(collected(index, changesOf({"/a/d"})) == std::vector<ReaderId>{handle(1, 2)});
	index.set(handle(1, 2), other); // what it reads now: the nodes the old places alone held go
	CHECK(index.nodes() == empty + 2);
	index.remove(handle(1, 2));
	index.remove(handle(2));
	CHECK(index.nodes() == empty);
	CHECK(index.keys() == keys);
	CHECK(index.entries() == 0);
}

TEST_CASE("place index: one change among 100,000 readers of one list collects one") {
	PlaceIndex index;
	constexpr std::uint32_t count = 100000;
	for(std::uint32_t i = 0; i < count; ++i) {
		const std::vector<std::string> place{"/items/" + std::to_string(i) + "/title"};
		index.add(handle(i + 1), place);
	}
	CHECK(collected(index, changesOf({"/items/4321/title"})) == std::vector<ReaderId>{handle(4322)});
	CHECK(collected(index, changesOf({"/items/4321/done"})).empty());
	CHECK(collected(index, changesOf({"/items"})).size() == count);
}

TEST_CASE("place index: rows' reads of their items move with an insert or a removal, and an announced one skips them") {
	PlaceIndex index;
	const std::string row(affresco::format::rowKey);
	const std::string number(affresco::format::indexKey);
	for(std::uint32_t i = 0; i < 4; ++i) { // row i reads its item's title; row 3 also its number
		std::string title = "/items/" + row;
		title += std::to_string(i);
		title += "/title";
		std::vector<std::string> places{title};
		if(i == 3) {
			std::string read = "/items/" + row;
			read += "3/";
			read += number;
			places.push_back(read);
		}
		index.add(handle(i + 1), places);
	}
	CHECK(collected(index, changesOf({"/items/2/title"})) == std::vector<ReaderId>{handle(3)}); // as "/items/2/title"
	CHECK(collected(index, changesOf({"/items/2"})) == std::vector<ReaderId>{handle(3)});
	StateChanges inserted = changesOf({"/items"}); // announced: the items stayed what they were
	inserted.lists.push_back({"/items", ListChange::Kind::Inserted, 1, 1});
	CHECK(collected(index, inserted).empty());
	CHECK(collected(index, changesOf({"/items"})).size() == 4U); // written whole: every row

	std::vector<ReaderId> out;
	index.shift("/items", 1, 1, out);               // one inserted at 1: rows 1, 2 and 3 become 2, 3 and 4
	CHECK(out == std::vector<ReaderId>{handle(4)}); // only the row that reads its number runs
	CHECK(collected(index, changesOf({"/items/1/title"})).empty());
	CHECK(collected(index, changesOf({"/items/2/title"})) == std::vector<ReaderId>{handle(2)});
	CHECK(collected(index, changesOf({"/items/4/title"})) == std::vector<ReaderId>{handle(4)});

	out.clear();
	index.remove(handle(2));           // the row at 2 goes, as a removal removes its panels
	index.shift("/items", 3, -1, out); // removed at 2: rows 3 and 4 become 2 and 3
	CHECK(out == std::vector<ReaderId>{handle(4)});
	CHECK(collected(index, changesOf({"/items/2/title"})) == std::vector<ReaderId>{handle(3)});
	CHECK(collected(index, changesOf({"/items/3/title"})) == std::vector<ReaderId>{handle(4)});

	out.clear();
	index.collectRows("/items", out);
	std::ranges::sort(out);
	CHECK(out == std::vector<ReaderId>{handle(1), handle(3), handle(4)});
	CHECK(index.keys() <= 8U); // the old rows' keys freed
}
