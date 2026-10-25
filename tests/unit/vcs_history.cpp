/// @file
/// @brief Unit tests for a history in proportion to the changes (VERSION_CONTROL.md): deltas, snapshots, packs, ids
/// that stay what they were, histories written before deltas, and blame.

#include "Helpers.hpp"
#include "HistoryFiles.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include "revisions/Deltas.hpp"     // modules/vcs/src
#include "history/HistoryStore.hpp" // modules/vcs/src: packObjects

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::forgeCommit;
using affresco::test::objectFile;
using affresco::test::permissionsScreen;
using affresco::test::readText;
using affresco::test::TemporaryFolder;
using affresco::test::writeText;
using affresco::vcs::Repository;
using affresco::vcs::detail::sha256;

namespace {

/// @brief A random JSON value: what documents and states are made of, with the cases deltas must keep apart (1 and
/// 1.0, 0.0 and -0.0, keys that need escaping in a pointer).
/// @param random The generator.
/// @param depth How deep it may nest.
/// @return The value.
json randomValue(std::mt19937& random, int depth) {
	static const std::vector<std::string> keys = {"a", "b", "c", "x/y", "t~0", ""};
	switch(random() % (depth > 0 ? 9 : 6)) {
	case 0:
		return nullptr;
	case 1:
		return random() % 2 == 0;
	case 2:
		return static_cast<int>(random() % 5);
	case 3:
		return std::vector<double>{1.0, -0.0, 0.0, 2.5}[random() % 4];
	case 4:
		return std::vector<std::string>{"", "x", "é", "{{a}}"}[random() % 4];
	case 5:
		return static_cast<unsigned>(random() % 3);
	case 6:
	case 7: {
		json list = json::array();
		for(auto i = static_cast<unsigned>(random() % 6); i > 0; --i) list.push_back(randomValue(random, depth - 1));
		return list;
	}
	default: {
		json object = json::object();
		for(auto i = static_cast<unsigned>(random() % 5); i > 0; --i)
			object[keys[random() % keys.size()]] = randomValue(random, depth - 1);
		return object;
	}
	}
}

/// @brief Changes a value a little, as revisions do: one member, an item added, removed or moved, keys reordered, or
/// the whole value replaced.
/// @param value The value.
/// @param random The generator.
/// @return The changed value.
json mutate(json value, std::mt19937& random) {
	const auto choice = static_cast<unsigned>(random() % 6);
	if(choice == 0 || !value.is_structured() || value.empty()) return randomValue(random, 3);
	if(value.is_array()) {
		const auto at = static_cast<std::ptrdiff_t>(random() % value.size());
		if(choice == 1) value.insert(value.begin() + at, randomValue(random, 2));
		else if(choice == 2) value.erase(value.begin() + at);
		else if(choice == 3) std::ranges::reverse(value);
		else value[static_cast<std::size_t>(at)] = mutate(value[static_cast<std::size_t>(at)], random);
		return value;
	}
	if(choice == 1) { // the same fields in another order: ordered JSON tells them apart
		std::vector<std::pair<std::string, json>> fields;
		for(const auto& [key, item] : value.items()) fields.emplace_back(key, item);
		std::ranges::shuffle(fields, random);
		json reordered = json::object();
		for(auto& [key, item] : fields) reordered[key] = std::move(item);
		return reordered;
	}
	auto member = value.begin();
	std::advance(member, static_cast<std::ptrdiff_t>(random() % value.size()));
	if(choice == 2) value.erase(member.key());
	else if(choice == 3) value["new"] = randomValue(random, 2);
	else *member = mutate(*member, random);
	return value;
}

/// @brief Applies a delta to a copy.
/// @param from The document before.
/// @param delta The delta.
/// @return The document after.
json applied(json from, const json& delta) {
	affresco::vcs::detail::applyDelta(from, delta, "test");
	return from;
}

/// @brief A value with its objects' keys in any order: for comparing documents a JSON Patch from json::diff rebuilt,
/// which may put keys in another order (the history's own rebuilds check the order against the commit's tree).
/// @param value The value.
/// @return It, as nlohmann::json (whose objects are sorted by key).
nlohmann::json unordered(const json& value) { return nlohmann::json::parse(value.dump()); }

/// @brief A commit's id as it was computed when every commit kept its whole document: the SHA-256 of its tree (the
/// SHA-256 of the document's JSON), parents, author, committer, time, patch, proposal and message.
/// @param commit The commit.
/// @param document Its document.
/// @param patch Its patch.
/// @return The id.
std::string idWithDocument(const affresco::CommitInfo& commit, const json& document, const json& patch) {
	std::string object = "tree " + sha256(document.dump()) + "\n";
	for(const std::string& parent : commit.parents) object += "parent " + parent + "\n";
	object += "author " + commit.author + "\ncommitter " + commit.committer + "\ntime " + commit.time + "\npatch " +
	          sha256(patch.dump()) + "\n";
	if(commit.proposal) object += "proposal " + std::to_string(commit.proposal) + "\n";
	return sha256(object + "\n" + commit.message);
}

/// @brief A random revision on permissionsScreen(): a text, a node added, removed or moved, the colours replaced with
/// their keys in any order, or the live state.
/// @param random The generator.
/// @param step Which revision, for unique ids and texts.
/// @param extra How many nodes revisions added that are still there (after the screen's three).
/// @return The revision.
json randomRevision(std::mt19937& random, int step, std::size_t extra) {
	const std::string n = std::to_string(step);
	const auto child = [&](std::size_t i) { return "/root/children/" + std::to_string(3 + i); };
	switch(random() % 6) {
	case 0:
		return {{"author", "studio"}, {"set", {{"#title/text", "Title " + n}}}};
	case 1:
		if(extra > 0) return {{"author", "studio"}, {"remove", child(random() % extra)}};
		[[fallthrough]];
	case 2:
		if(extra > 1) return {{"author", "studio"}, {"move", {{child(random() % extra), child(random() % extra)}}}};
		[[fallthrough]];
	case 3: {
		json colours = json::object();
		std::vector<std::string> names = {"accent", "muted", "text"};
		std::ranges::shuffle(names, random);
		for(const std::string& name : names)
			colours[name] = "#00" + std::string(4, static_cast<char>('a' + random() % 6));
		return {{"author", "studio"}, {"set", {{"/colors", colours}}}};
	}
	case 4:
		return {{"author", "studio"}, {"set", {{"/state/count", step}}}};
	default:
		return {{"author", "studio"},
		        {"add", {{"/root/children/-", {{"type", "label"}, {"id", "n" + n}, {"text", "Node " + n}}}}}};
	}
}

/// @brief Commits a revision that must apply.
/// @param repository The history.
/// @param revision The revision.
void commit(Repository& repository, const json& revision) {
	const auto error = repository.commit(revision);
	INFO(revision.dump());
	REQUIRE_FALSE(error);
}

/// @brief Every object a history folder holds, loose and packed, by id.
/// @param history The folder.
/// @return Their texts.
std::vector<std::pair<std::string, std::string>> objectsIn(const std::filesystem::path& history) {
	std::vector<std::pair<std::string, std::string>> objects;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(history / "objects"))
		if(entry.is_regular_file())
			objects.emplace_back(entry.path().parent_path().filename().string() + entry.path().filename().string(),
			                     readText(entry.path()));
	if(std::filesystem::exists(history / "packs"))
		for(const auto& pack : std::filesystem::directory_iterator(history / "packs")) {
			const std::string text = readText(pack.path());
			for(std::size_t start = 0; start < text.size();) {
				const std::size_t end = text.find('\n', start);
				objects.emplace_back(text.substr(start, 64), text.substr(start + 65, end - start - 65));
				start = end + 1;
			}
		}
	return objects;
}

} // namespace

TEST_CASE("vcs: a delta rebuilds the document exactly, keys in order and numbers as written") {
	std::mt19937 random(2017); // NOLINT(bugprone-random-generator-seed): the same cases on every run
	for(int i = 0; i < 3000; ++i) {
		const json from = randomValue(random, 4), to = mutate(from, random);
		const json delta = affresco::vcs::detail::delta(from, to);
		CAPTURE(from.dump());
		CAPTURE(to.dump());
		CAPTURE(delta.dump());
		CHECK(applied(from, delta).dump() == to.dump());
	}
	// Without a change, nothing; 1 and 1.0, 0.0 and -0.0, {a, b} and {b, a} are changes
	CHECK(affresco::vcs::detail::delta(json::parse(R"({"a": [1, {"b": 2}]})"), json::parse(R"({"a": [1, {"b": 2}]})"))
	          .empty());
	CHECK(affresco::vcs::detail::delta(json(1), json(1.0)).size() == 1);
	CHECK(affresco::vcs::detail::delta(json(0.0), json(-0.0)).size() == 1);
	CHECK(affresco::vcs::detail::delta(json::parse(R"({"a": 1, "b": 2})"), json::parse(R"({"b": 2, "a": 1})")).size() ==
	      1);
}

TEST_CASE("vcs: a delta is in proportion to the change, not to the document") {
	json cards = json::array();
	for(int i = 0; i < 12; ++i)
		cards.push_back({{"id", "card" + std::to_string(i)}, {"title", "Card " + std::to_string(i)}});
	json inserted = cards, removed = cards, renamed = cards;
	inserted.insert(inserted.begin(), json::object({{"id", "new"}}));
	removed.erase(removed.begin() + 5);
	renamed[7]["title"] = "Renamed";

	CHECK(affresco::vcs::detail::delta(cards, inserted) ==
	      json::parse(R"([{"op": "add", "path": "/0", "value": {"id": "new"}}])"));
	CHECK(affresco::vcs::detail::delta(cards, removed) == json::parse(R"([{"op": "remove", "path": "/5"}])"));
	CHECK(affresco::vcs::detail::delta(cards, renamed) ==
	      json::parse(R"([{"op": "replace", "path": "/7/title", "value": "Renamed"}])"));
	CHECK(affresco::vcs::detail::delta(json::parse(R"({"a/b": {"~": 1}})"), json::parse(R"({"a/b": {"~": 2}})")) ==
	      json::parse(R"([{"op": "replace", "path": "/a~1b/~0", "value": 2}])"));
}

TEST_CASE("vcs: commit ids are what they were when every commit kept its document") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	std::vector<json> documents{repository.document()};
	commit(repository, {{"author", "studio"}, {"message", "Rename"}, {"set", {{"#title/text", "Done"}}}});
	documents.push_back(repository.document());
	commit(repository, {{"author", "studio"}, {"set", {{"/state/count", 3}}}}); // the document stays
	documents.push_back(repository.document());
	const affresco::RevisionResult proposed =
	    repository.submit({{"author", "agent"}, {"message", "Retitle"}, {"set", {{"#title/text", "Week"}}}});
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, false})); // a merge commit
	documents.push_back(repository.document());

	const std::vector<affresco::CommitInfo> log = repository.log();
	const json revisions = repository.exportChanges(0, "all")["revisions"];
	REQUIRE(log.size() == documents.size());
	for(std::size_t i = 0; i < log.size(); ++i) {
		CAPTURE(i);
		CHECK(log[i].id == idWithDocument(log[i], documents[i], i == 0 ? json::array() : revisions[i - 1]["patch"]));
	}
	CHECK(log.back().proposal == proposed.proposal);
}

TEST_CASE("vcs: every revision of random histories is rebuilt as it was, in memory and from disk") {
	const affresco::format::Rules rules;
	for(const unsigned seed : {1u, 2u, 3u}) {
		CAPTURE(seed);
		const TemporaryFolder folder;
		std::mt19937 random(seed); // NOLINT(bugprone-random-generator-seed): the same histories on every run
		std::vector<json> documents;
		std::vector<affresco::CommitInfo> log;
		{
			Repository repository(rules, permissionsScreen());
			repository.openHistory(folder.path() / "board.history");
			documents.push_back(repository.document());
			const int commits = 3 * Repository::snapshotInterval + 17; // across snapshots, and past one
			for(int step = 1; step <= commits; ++step) {
				const std::size_t extra = repository.document()["root"]["children"].size() - 3;
				commit(repository, randomRevision(random, step, extra));
				documents.push_back(repository.document());
			}
			// A replay from the nearest snapshot (checked against the commit's tree) gives each revision's document
			for(std::size_t k = 0; k < documents.size(); ++k) {
				CAPTURE(k);
				json patch;
				REQUIRE_NOTHROW(patch = repository.diff(0, static_cast<int>(k)));
				CHECK(unordered(applied(documents[0], patch)) == unordered(documents[k]));
			}
			log = repository.log();
		}
		Repository reloaded(rules, permissionsScreen());
		REQUIRE(reloaded.openHistory(folder.path() / "board.history").loaded);
		CHECK(reloaded.document().dump() == documents.back().dump());
		const std::vector<affresco::CommitInfo> restored = reloaded.log();
		REQUIRE(restored.size() == log.size());
		for(std::size_t k = 1; k < log.size(); ++k) {
			CAPTURE(k);
			CHECK(restored[k].id == log[k].id);
			CHECK(unordered(applied(documents[k - 1], reloaded.diff(static_cast<int>(k) - 1, static_cast<int>(k)))) ==
			      unordered(documents[k]));
		}
	}
}

TEST_CASE("vcs: a history keeps a document every snapshotInterval commits, and packs its files") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	constexpr int commits = 300;
	std::vector<affresco::CommitInfo> log;
	{
		Repository repository(rules, permissionsScreen());
		repository.openHistory(history);
		for(int i = 0; i < commits; ++i)
			commit(repository, {{"author", "studio"}, {"set", {{"#title/text", "T" + std::to_string(i)}}}});
		log = repository.log();
	}

	// Documents for revisions 0, 64, 128, 192 and 256; every other commit is a delta
	const auto objects = objectsIn(history);
	const auto documents =
	    std::ranges::count_if(objects, [](const auto& object) { return object.second.contains("\"root\""); });
	CHECK(documents == 1 + commits / Repository::snapshotInterval);
	CHECK(objects.size() == static_cast<std::size_t>(commits + 1 + documents));
	for(const auto& [id, text] : objects)
		if(!text.contains("\"root\"")) CHECK(text.size() < 1200); // a commit holds its patch and delta, not a document

	// The loose ones went into packs, which the refs name; fewer than a pack's worth are loose
	const json refs = json::parse(readText(history / "refs.json"));
	CHECK(refs["version"] == 2);
	REQUIRE(refs["packs"].size() >= 1);
	for(const json& pack : refs["packs"])
		CHECK(std::filesystem::is_regular_file(history / "packs" / pack.get<std::string>()));
	std::size_t loose = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(history / "objects"))
		loose += entry.is_regular_file();
	CHECK(loose < affresco::vcs::detail::HistoryStore::packObjects);

	Repository reloaded(rules, permissionsScreen());
	REQUIRE(reloaded.openHistory(history).loaded);
	REQUIRE(reloaded.log().size() == log.size());
	CHECK(reloaded.log().back().id == log.back().id);
	CHECK(reloaded.document().at(json::json_pointer("/root/children/1/text")) == "T299");

	SUBCASE("a damaged pack is never loaded, and the error names it") {
		const std::filesystem::path pack = history / "packs" / refs["packs"][0].get<std::string>();
		std::string text = readText(pack);
		text[text.size() / 2] = text[text.size() / 2] == 'a' ? 'b' : 'a';
		writeText(pack, text);
		Repository damaged(rules, permissionsScreen());
		CHECK_THROWS_WITH_AS(damaged.openHistory(history),
		                     doctest::Contains((pack.string() + ": its content doesn't match its SHA-256").c_str()),
		                     affresco::DocumentError);
	}
}

TEST_CASE("vcs: a history written before deltas (version 1) loads, and goes on in version 2") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";

	// Written as version 1 did: every commit's document in a file of its own, no deltas, refs at version 1
	const json original = permissionsScreen();
	json renamed = original, recoloured;
	renamed["root"]["children"][1]["text"] = "Done";
	recoloured = renamed;
	recoloured["colors"] = {{"accent", "#ff0000"}};
	const std::string zero = forgeCommit(history, {{"parents", json::array()},
	                                               {"author", "file"},
	                                               {"committer", "file"},
	                                               {"time", "2026-10-18T10:00:00Z"},
	                                               {"proposal", 0},
	                                               {"message", "original"},
	                                               {"patch", json::array()},
	                                               {"document", original}});
	const std::string one =
	    forgeCommit(history, {{"parents", {zero}},
		                      {"author", "studio"},
		                      {"committer", "studio"},
		                      {"time", "2026-10-18T10:01:00Z"},
		                      {"proposal", 0},
		                      {"message", "Rename"},
		                      {"patch", json::parse(R"([{"op": "replace", "path": "#title/text", "value": "Done"}])")},
		                      {"document", renamed}});
	const std::string two = forgeCommit(
	    history, {{"parents", {one}},
		          {"author", "studio"},
		          {"committer", "studio"},
		          {"time", "2026-10-18T10:02:00Z"},
		          {"proposal", 0},
		          {"message", "Recolour"},
		          {"patch", json::parse(R"([{"op": "add", "path": "/colors", "value": {"accent": "#ff0000"}}])")},
		          {"document", recoloured}});
	json refs = {{"format", "affrescoui-history"},
	             {"version", 1},
	             {"main", two},
	             {"nextProposal", 1},
	             {"mode", "default"},
	             {"proposals", json::array()},
	             {"grants", json::array()},
	             {"imported", json::array()}};
	refs["checksum"] = sha256(refs.dump());
	writeText(history / "refs.json", refs.dump(2));
	const std::string written = readText(objectFile(history, one));

	{
		Repository repository(rules, original);
		REQUIRE(repository.openHistory(history).loaded);
		const std::vector<affresco::CommitInfo> log = repository.log();
		REQUIRE(log.size() == 3);
		CHECK(log[1].id == one);
		CHECK(log[2].id == two);
		CHECK(repository.document() == recoloured);
		CHECK(repository.blame("#title").id == one); // worked out from the documents
		CHECK(repository.blame("/colors/accent").id == two);
		CHECK(repository.blame("#board").id == zero);
		CHECK(repository.diff(0, 2).size() == 2);

		commit(repository, {{"author", "studio"}, {"message", "Later"}, {"set", {{"#title/text", "Later"}}}});
		CHECK(repository.blame("#title").number == 3);
	}
	CHECK(readText(objectFile(history, one)) == written); // what was written stays as it was
	CHECK(json::parse(readText(history / "refs.json"))["version"] == 2);

	Repository reloaded(rules, original);
	REQUIRE(reloaded.openHistory(history).loaded);
	REQUIRE(reloaded.log().size() == 4);
	CHECK(reloaded.log()[2].id == two);
	CHECK(reloaded.blame("#title").number == 3);
	CHECK(reloaded.blame("/colors").id == two);
	CHECK(reloaded.document().at(json::json_pointer("/root/children/1/text")) == "Later");
}

TEST_CASE("vcs: blame names the commit on main that last changed a node or a value") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	const std::string original = repository.head().id;
	commit(repository, {{"author", "studio"}, {"message", "Rename"}, {"set", {{"#title/text", "Done"}}}});
	const affresco::CommitInfo renamed = repository.head();

	CHECK(repository.blame("#title").id == renamed.id);
	CHECK(repository.blame("#title/text").id == renamed.id);
	CHECK(repository.blame("#title").message == "Rename");
	CHECK(repository.blame("#title").author == "studio");
	CHECK(repository.blame("#board").id == original); // as the screen file has it
	CHECK(repository.blame("/root/children/1").id == renamed.id);

	SUBCASE("an item added or removed before a node only moves it") {
		commit(repository, {{"author", "studio"},
		                    {"add", {{"/root/children/0", {{"type", "label"}, {"id", "first"}, {"text", "First"}}}}}});
		CHECK(repository.blame("#title").id == renamed.id);
		CHECK(repository.blame("#first").number == 2);
		commit(repository, {{"author", "studio"}, {"remove", "#first"}});
		CHECK(repository.blame("#title").id == renamed.id);
	}
	SUBCASE("a node moved is changed where it is") {
		commit(repository, {{"author", "studio"}, {"move", {{"#title", "/root/children/0"}}}});
		CHECK(repository.blame("#title").number == 2);
		CHECK(repository.blame("#title/text").number == 2);
	}
	SUBCASE("something changed inside a node changes it") {
		commit(repository, {{"author", "studio"}, {"set", {{"#board/color", "#000000"}}}});
		CHECK(repository.blame("#board").number == 2);
		CHECK(repository.blame("#main").number == 2); // it holds #board
		CHECK(repository.blame("#title").id == renamed.id);
	}
	SUBCASE("the live state: the last commit whose patch reached the path") {
		commit(repository, {{"author", "studio"}, {"message", "Count"}, {"set", {{"/state/count", 5}}}});
		commit(repository, {{"author", "studio"}, {"set", {{"/state/other", 1}}}});
		CHECK(repository.blame("/state/count").message == "Count");
		CHECK(repository.blame("/state").number == 3);
		CHECK(repository.blame("/state/never").id == original);
		CHECK(repository.blame("#title").id == renamed.id); // state changes don't change the document
	}
	SUBCASE("a merged proposal: its author, and the reviewer who approved it") {
		const affresco::RevisionResult proposed =
		    repository.submit({{"author", "agent"}, {"message", "Retitle"}, {"set", {{"#title/text", "Week"}}}});
		REQUIRE(proposed.status == RevisionStatus::Proposed);
		CHECK(repository.blame("#title").id == renamed.id); // not on main yet
		REQUIRE_FALSE(repository.accept(proposed.proposal, {"studio", proposed.commit, false}));
		const affresco::CommitInfo merged = repository.blame("#title");
		CHECK(merged.author == "agent");
		CHECK(merged.committer == "studio");
		CHECK(merged.proposal == proposed.proposal);
	}
	SUBCASE("what isn't there is an error") {
		CHECK_THROWS_WITH_AS((void)repository.blame("#nope"), doctest::Contains("no node with id 'nope'"),
		                     affresco::DocumentError);
		CHECK_THROWS_WITH_AS((void)repository.blame("/nothing/here"), doctest::Contains("nothing at '/nothing/here'"),
		                     affresco::DocumentError);
		CHECK_THROWS_WITH_AS((void)repository.blame("title"), doctest::Contains("expected a node"),
		                     affresco::DocumentError);
	}
}

TEST_CASE("vcs: a peer asks who changed something with a blame message") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	commit(repository, {{"author", "studio"}, {"message", "Rename"}, {"set", {{"#title/text", "Done"}}}});
	affresco::test::MemoryChannel channel;
	const affresco::channel::Session session(channel, repository, "agent", {}, nullptr);

	channel.receive(R"({"type": "blame", "target": "#title", "id": 7})");
	const json& answer = channel.sent.back();
	CHECK(answer["type"] == "blame");
	CHECK(answer["target"] == "#title");
	CHECK(answer["commit"]["number"] == 1);
	CHECK(answer["commit"]["message"] == "Rename");
	CHECK(answer["commit"]["committer"] == "studio");
	CHECK(answer["id"] == 7);

	channel.receive(R"({"type": "blame", "id": 8})");
	CHECK(channel.sent.back()["type"] == "rejected");
	CHECK(channel.sent.back()["error"].get<std::string>().contains("blame.target"));
	channel.receive(R"({"type": "blame", "target": "#nope", "id": 9})");
	CHECK(channel.sent.back()["error"].get<std::string>().contains("no node with id 'nope'"));
}

// Found by the revision fuzzer (tests/fuzz/regressions): a revert undid a commit's change to the document but not
// its change to the live state, so `{"set": {"#title/text": "Done", "/state/count": 5}}` reverted kept count at 5
TEST_CASE("vcs: reverting a commit undoes what it changed in the live state too") {
	const affresco::format::Rules rules;
	Repository repository(rules, permissionsScreen());
	const json document = repository.document(), state = repository.state();

	SUBCASE("a commit that changes the document and the live state") {
		commit(repository, {{"author", "studio"}, {"set", {{"#title/text", "Done"}, {"/state/count", 5}}}});
		commit(repository, repository.revertRevision("HEAD", "studio"));
		CHECK(repository.document() == document);
		CHECK(repository.state() == state);
	}
	SUBCASE("a commit that changes only the live state, which couldn't be reverted before") {
		commit(repository, {{"author", "studio"}, {"set", {{"/state/count", 5}}}, {"add", {{"/state/list", {1, 2}}}}});
		commit(repository, repository.revertRevision("HEAD", "studio"));
		CHECK(repository.state() == state);
		commit(repository, repository.revertRevision("HEAD", "studio")); // and a revert reverts too
		CHECK(repository.state()["count"] == 5);
	}
	SUBCASE("the live state changed since, by an action: a conflict, as for the document") {
		commit(repository, {{"author", "studio"}, {"set", {{"/state/count", 5}}}});
		repository.state()["count"] = 6;
		CHECK_THROWS_WITH_AS((void)repository.revertRevision("HEAD", "studio"),
		                     doctest::Contains("conflict: /state/count changed after revision 1"),
		                     affresco::DocumentError);
	}
	SUBCASE("a commit that changes nothing has nothing to revert") {
		commit(repository, {{"author", "studio"}, {"set", {{"/state/count", 0}}}});
		CHECK_THROWS_WITH_AS((void)repository.revertRevision("HEAD", "studio"), doctest::Contains("nothing to revert"),
		                     affresco::DocumentError);
	}
}

TEST_CASE("vcs: the revision fuzzer's input that found it: a value copied in the state and moved into the document") {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/flow/index.screen.json");
	REQUIRE(file);
	json kanban = json::parse(file);
	kanban.erase("include"); // a Repository does not merge included files; this test needs none
	const affresco::format::Rules rules;
	Repository repository(rules, kanban);
	const json document = repository.document(), state = repository.state();
	// The input as the fuzzer wrote it: a "path" given twice, of which JSON keeps the second
	commit(repository, json::parse(R"({"author":"studio","patch":[{"op":"add","path":"/state/extra","value":[1,2]},)"
	                               R"({"op":"copy","from":"/state/extra","path":"/state/copy"},{"op":"move",)"
	                               R"("from":"/state/copy","path":"/statowe","path":"#topBar/chia"}]})"));
	commit(repository, repository.revertRevision("HEAD", "studio"));
	CHECK(repository.document() == document);
	CHECK(repository.state() == state);
}

#ifndef _WIN32
TEST_CASE("vcs history: the lock file is never opened through a link") {
	const TemporaryFolder folder;
	const std::filesystem::path canary = folder.path() / "canary.txt";
	writeText(canary, "canary");
	std::filesystem::create_symlink(canary, folder.path() / "lock");
	std::string problem;
	CHECK_FALSE(affresco::vcs::detail::FileLock::tryLock(folder.path() / "lock", problem));
	CHECK(problem.find("can't be locked") != std::string::npos);
	std::ifstream in(canary);
	CHECK(std::string(std::istreambuf_iterator<char>(in), {}) == "canary");
}
#endif
