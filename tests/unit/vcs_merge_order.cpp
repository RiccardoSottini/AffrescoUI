/// @file
/// @brief Unit tests for the order of an object's keys after a three-way merge (vcs/Merge.hpp): a key one side added
/// stays after the key it followed there, so a definition added in the middle of a program file stays in the middle;
/// and a definition is merged whole, never inside its text.

#include <affresco/vcs/Merge.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::json;
using affresco::vcs::mergeDocuments;

namespace {

/// @brief The keys of an object, in order.
/// @param object The object.
/// @return Its keys.
std::vector<std::string> keysOf(const json& object) {
	std::vector<std::string> keys;
	for(auto it = object.begin(); it != object.end(); ++it) keys.push_back(it.key());
	return keys;
}

/// @brief Keys a, b, c and d, each holding its own name.
/// @return The object.
json abcd() { return {{"a", "a"}, {"b", "b"}, {"c", "c"}, {"d", "d"}}; }

} // namespace

TEST_CASE("merge order: a key the proposal added in the middle stays in the middle") {
	json proposal = {{"a", "a"}, {"b", "b"}, {"x", "x"}, {"c", "c"}, {"d", "d"}};
	json main = abcd();
	main["d"] = "changed"; // main moved on elsewhere
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"a", "b", "x", "c", "d"});
	CHECK(result.merged["d"] == "changed");
	CHECK(result.merged["x"] == "x");
}

TEST_CASE("merge order: a key main added in the middle stays where main put it") {
	const json main = {{"a", "a"}, {"y", "y"}, {"b", "b"}, {"c", "c"}, {"d", "d"}};
	json proposal = abcd();
	proposal["a"] = "changed";
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"a", "y", "b", "c", "d"});
	CHECK(result.merged["a"] == "changed");
}

TEST_CASE("merge order: both sides add keys in different places, each after the key it followed") {
	const json main = {{"a", "a"}, {"m", "m"}, {"b", "b"}, {"c", "c"}, {"d", "d"}};
	const json proposal = {{"p0", "p0"}, {"a", "a"}, {"b", "b"}, {"c", "c"}, {"p1", "p1"}, {"p2", "p2"}, {"d", "d"}};
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"p0", "a", "m", "b", "c", "p1", "p2", "d"});
}

TEST_CASE("merge order: a key both sides added keeps main's place") {
	const json main = {{"a", "a"}, {"both", 1}, {"b", "b"}, {"c", "c"}, {"d", "d"}};
	const json proposal = {{"a", "a"}, {"b", "b"}, {"c", "c"}, {"both", 1}, {"d", "d"}};
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"a", "both", "b", "c", "d"});
}

TEST_CASE("merge order: a key that followed one the merge removes goes after the nearest key kept, or first") {
	json main = abcd();
	main.erase("b");
	main.erase("a");
	// The proposal adds w after a and x after b: main removed both, so neither follows a key the merge keeps
	const json proposal = {{"a", "a"}, {"w", "w"}, {"b", "b"}, {"x", "x"}, {"c", "c"}, {"d", "d"}};
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"w", "x", "c", "d"});
}

TEST_CASE("merge order: keys all three sides have keep main's order, whatever order the proposal writes them in") {
	const json main = {{"d", "d"}, {"c", "c"}, {"b", "b"}, {"a", "a"}};
	const json proposal = {{"a", "a"}, {"b", "B"}, {"c", "c"}, {"d", "d"}};
	const auto result = mergeDocuments(abcd(), main, proposal);
	CHECK(result.clean());
	CHECK(keysOf(result.merged) == std::vector<std::string>{"d", "c", "b", "a"});
	CHECK(result.merged["b"] == "B");
	CHECK(keysOf(mergeDocuments(abcd(), abcd(), abcd()).merged) == keysOf(abcd()));
}

TEST_CASE("merge order: a definition added between two others in a program file stays between them") {
	const json base = {
	    {"program",
		 {{"main.lisp",
		   {{"first", "(defn first [] 1)"}, {"last", "(defn last [] 3)"}, {"view", "(defn view [] 4)"}}}}}};
	json main = base;
	main["program"]["main.lisp"]["view"] = "(defn view [] 40)"; // main changes another definition of the file
	json proposal = json::parse(R"json({"program": {"main.lisp": {
		"first": "(defn first [] 1)", "middle": "(defn middle [] 2)", "last": "(defn last [] 3)",
		"view": "(defn view [] 4)"}}})json");
	const auto result = mergeDocuments(base, main, proposal);
	CHECK(result.clean());
	const json& file = result.merged["program"]["main.lisp"];
	CHECK(keysOf(file) == std::vector<std::string>{"first", "middle", "last", "view"});
	CHECK(file["view"] == "(defn view [] 40)");
}

TEST_CASE("merge: a definition both sides changed differently is one conflict holding the three texts") {
	const json base = {{"program", {{"main.lisp", {{"count", "(defn count [] 1)"}, {"other", "(def other 1)"}}}}}};
	json main = base, proposal = base;
	main["program"]["main.lisp"]["count"] = "(defn count []\n  2)";
	proposal["program"]["main.lisp"]["count"] = "(defn count [] 1)\n";
	proposal["program"]["main.lisp"]["other"] = "(def other 2)";
	const auto result = mergeDocuments(base, main, proposal);
	REQUIRE(result.conflicts.size() == 1);
	const json& conflict = result.conflicts[0];
	CHECK(conflict["path"] == "/program/main.lisp/count");
	CHECK(conflict["reason"] == "changed on main and in the proposal");
	CHECK(conflict["base"] == "(defn count [] 1)");
	CHECK(conflict["main"] == "(defn count []\n  2)");
	CHECK(conflict["proposal"] == "(defn count [] 1)\n");
	CHECK(keysOf(conflict) == std::vector<std::string>{"path", "reason", "base", "main", "proposal"});
	// The text is never merged inside: main's stays whole, and the other definition merged
	CHECK(result.merged["program"]["main.lisp"]["count"] == "(defn count []\n  2)");
	CHECK(result.merged["program"]["main.lisp"]["other"] == "(def other 2)");
}

TEST_CASE("merge: a definition one side removed and the other changed is one conflict, and main's side stays") {
	const json base = {{"program", {{"main.lisp", {{"count", "(defn count [] 1)"}}}}}};
	json main = base, proposal = base;
	main["program"]["main.lisp"].erase("count");
	proposal["program"]["main.lisp"]["count"] = "(defn count [] 2)";
	const auto result = mergeDocuments(base, main, proposal);
	REQUIRE(result.conflicts.size() == 1);
	CHECK(result.conflicts[0]["reason"] == "removed on main, changed in the proposal");
	CHECK(result.conflicts[0]["main"].is_null());
	CHECK_FALSE(result.merged["program"]["main.lisp"].contains("count"));
}

TEST_CASE("merge order: an object of many keys, each added by the proposal, merges in its order and in linear time") {
	json base = json::object(), proposal = json::object();
	for(int i = 0; i < 10000; ++i) {
		const std::string key = "k" + std::to_string(i);
		if(i % 2 == 0) base[key] = i;
		proposal[key] = i;
	}
	json main = base;
	main["k0"] = -1; // main changed one, so the objects merge key by key
	const auto result = mergeDocuments(base, main, proposal);
	CHECK(result.clean());
	proposal["k0"] = -1;
	CHECK(result.merged == proposal); // the same keys in the same order
}
