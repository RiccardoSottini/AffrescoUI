/// @file
/// @brief Sinopia's edits (examples/sinopia): moving slides, and the numbers and decks a document may send them.

#include <sinopia/deck/Edit.hpp>

#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>

using affresco::json;
using sinopia::editDeck;

namespace {

/// @brief An edit of a deck of three slides, s1 to s3, the first holding a rectangle `a`, with an accent colour.
/// @param op The edit's op.
/// @return It, to add keys to.
json editOf(const char* op) {
	json edit = json::parse(R"({"deck": {"theme": {"accent": "#7c3aed"},
		"slides": [{"id": "s1", "objects": [{"id": "a", "kind": "rect"}]}, {"id": "s2"}, {"id": "s3"}]},
		"slide": 0, "ids": ["a"]})");
	edit["op"] = op;
	return edit;
}

/// @brief The slides' ids, in order.
/// @param result What editDeck() gave.
/// @return They, joined.
std::string slideIds(const json& result) {
	std::string ids;
	for(const json& slide : result["deck"]["slides"]) ids += slide["id"].get<std::string>() + " ";
	return ids;
}

} // namespace

TEST_CASE("sinopia: a slide moves, its places clamped to the deck") {
	json edit = editOf("moveSlide");
	edit["from"] = 0;
	edit["to"] = 1;
	CHECK(slideIds(editDeck(edit)) == "s2 s1 s3 ");
	edit["to"] = 1e300; // past the deck, and past what a float holds
	const json result = editDeck(edit);
	CHECK(slideIds(result) == "s2 s3 s1 ");
	CHECK(result["slide"] == 2);
	edit["from"] = -1e300;
	edit["to"] = -5;
	CHECK(slideIds(editDeck(edit)) == "s1 s2 s3 ");
}

TEST_CASE("sinopia: a deck with no slides refuses the edits of a slide") {
	for(const char* op : {"moveSlide", "duplicateSlide", "addSection", "nudge"}) {
		json edit = editOf(op);
		edit["deck"] = json::object();
		CHECK_THROWS_AS((void)editDeck(edit), std::exception);
	}
	json add = editOf("addSlide"); // but takes a new one
	add["deck"] = json::object();
	CHECK(editDeck(add)["deck"]["slides"].size() == 1);
}

TEST_CASE("sinopia: numbers past what a float holds are clamped, not converted") {
	json add = editOf("add");
	add["x"] = 1e300;
	add["w"] = -1e300;
	const json added = editDeck(add)["deck"]["slides"][0]["objects"][1];
	CHECK(added["x"] == 100000.0f);
	CHECK(added["w"] == 1.0f);
	json transform = editOf("transform");
	transform["items"] = json::parse(R"([{"id": "a", "x": -1e300, "height": 1e300, "rotate": 1e300}])");
	const json moved = editDeck(transform)["deck"]["slides"][0]["objects"][0];
	CHECK(moved["x"] == -100000.0f);
	CHECK(moved["h"] == 100000.0f);
	CHECK(moved["rotate"] == 360.0f);
	CHECK(sinopia::floatOf(json(1e300)) == std::numeric_limits<float>::max());
	CHECK(sinopia::floatOf(json(-1e300)) == -std::numeric_limits<float>::max());
	CHECK(sinopia::floatOf(json(2.5)) == 2.5f);
	CHECK(sinopia::floatAt(json::object(), "x", 7.0f) == 7.0f);
	CHECK_THROWS_AS((void)sinopia::floatAt(json({{"x", "far"}}), "x", 7.0f), json::type_error);
}

TEST_CASE("sinopia: duplicated objects take the first free ids, and a group's children follow its copy") {
	json edit = editOf("duplicate");
	edit["deck"]["slides"][0]["objects"] = json::parse(R"([{"id": "rect1", "kind": "rect", "x": 10, "locked": true},
		{"id": "rect2", "kind": "rect"}, {"id": "group1", "kind": "group", "children": [{"id": "c", "kind": "rect"}]}])");
	edit["ids"] = {"rect1", "rect2", "group1"};
	const json result = editDeck(edit);
	CHECK(result["selection"] == json({"rect3", "rect4", "group2"}));
	const json& objects = result["deck"]["slides"][0]["objects"];
	REQUIRE(objects.size() == 6);
	CHECK(objects[3]["x"] == 30);
	CHECK_FALSE(objects[3].value("locked", false));
	CHECK(objects[5]["children"][0]["id"] == "group2-c");
}
