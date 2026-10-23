/// @file
/// @brief Sinopia's arranging (examples/sinopia): align, distribute, z-order, nudge, group and ungroup, and locked
/// objects staying put.

#include <sinopia/deck/Arrange.hpp>

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

using sinopia::SlideObject;

namespace {

/// @brief An object.
/// @param id Its id.
/// @param x Its left.
/// @param y Its top.
/// @param w Its width.
/// @param h Its height.
/// @return It.
SlideObject box(std::string id, float x, float y, float w = 10.0f, float h = 10.0f) {
	SlideObject object;
	object.id = std::move(id);
	object.x = x;
	object.y = y;
	object.w = w;
	object.h = h;
	return object;
}

/// @brief The ids of objects, back to front.
/// @param objects Them.
/// @return Their ids.
std::vector<std::string> ids(const std::vector<SlideObject>& objects) {
	std::vector<std::string> result;
	for(const SlideObject& object : objects) result.push_back(object.id);
	return result;
}

} // namespace

TEST_CASE("sinopia: objects align to the box around them, or to the slide") {
	std::vector<SlideObject> objects{box("a", 10, 10), box("b", 50, 30, 20, 20), box("c", 0, 0)};
	const std::vector<std::string> chosen{"a", "b"};
	sinopia::alignObjects(objects, chosen, "right", false, {800, 450});
	CHECK(objects[0].x == 60.0f);
	CHECK(objects[1].x == 50.0f);
	sinopia::alignObjects(objects, chosen, "middle", false, {800, 450});
	CHECK(objects[0].y == 25.0f);
	CHECK(objects[1].y == 20.0f);
	CHECK(objects[2].x == 0.0f); // not chosen
	const std::vector<std::string> one{"c"};
	sinopia::alignObjects(objects, one, "center", false, {800, 450}); // one object: to the slide
	CHECK(objects[2].x == 395.0f);
	sinopia::alignObjects(objects, chosen, "bottom", true, {800, 450});
	CHECK(objects[0].y == 440.0f);
	CHECK_THROWS_AS(sinopia::alignObjects(objects, chosen, "diagonal", false, {800, 450}), std::runtime_error);
}

TEST_CASE("sinopia: distributing makes the gaps equal") {
	std::vector<SlideObject> objects{box("a", 0, 0), box("b", 15, 0, 20), box("c", 90, 0)};
	const std::vector<std::string> all{"a", "b", "c"};
	sinopia::distributeObjects(objects, all, true);
	CHECK(objects[0].x == 0.0f);
	CHECK(objects[1].x == 40.0f); // gaps of 30: 0-10, 40-60, 90-100
	CHECK(objects[2].x == 90.0f);
	const std::vector<std::string> two{"a", "b"};
	objects[1].x = 12.0f;
	sinopia::distributeObjects(objects, two, true); // fewer than three: nothing
	CHECK(objects[1].x == 12.0f);
}

TEST_CASE("sinopia: the z-order moves to the front, the back, and one step") {
	std::vector<SlideObject> objects{box("a", 0, 0), box("b", 0, 0), box("c", 0, 0), box("d", 0, 0)};
	const std::vector<std::string> a{"a"};
	sinopia::reorderObjects(objects, a, "front");
	CHECK(ids(objects) == std::vector<std::string>{"b", "c", "d", "a"});
	sinopia::reorderObjects(objects, a, "backward");
	CHECK(ids(objects) == std::vector<std::string>{"b", "c", "a", "d"});
	const std::vector<std::string> c{"c"};
	sinopia::reorderObjects(objects, c, "back");
	CHECK(ids(objects) == std::vector<std::string>{"c", "b", "a", "d"});
	sinopia::reorderObjects(objects, c, "forward");
	CHECK(ids(objects) == std::vector<std::string>{"b", "c", "a", "d"});
	CHECK_THROWS_AS(sinopia::reorderObjects(objects, c, "sideways"), std::runtime_error);
}

TEST_CASE("sinopia: grouping holds objects relative to the group, and ungrouping puts them back") {
	std::vector<SlideObject> objects{box("a", 10, 20), box("b", 40, 60), box("c", 0, 0)};
	const std::vector<std::string> chosen{"a", "b"};
	REQUIRE(sinopia::groupObjects(objects, chosen, "g"));
	REQUIRE(objects.size() == 2);
	CHECK(objects[0].id == "g");
	CHECK(objects[0].x == 10.0f);
	CHECK(objects[0].w == 40.0f);
	CHECK(objects[0].h == 50.0f);
	CHECK(objects[0].children[1].x == 30.0f);
	const std::vector<std::string> g{"g"};
	CHECK_FALSE(sinopia::groupObjects(objects, g, "h")); // one object
	sinopia::ungroupObjects(objects, g);
	CHECK(ids(objects) == std::vector<std::string>{"a", "b", "c"});
	CHECK(objects[1].x == 40.0f);
	CHECK(objects[1].y == 60.0f);
}

TEST_CASE("sinopia: locked objects stay where they are") {
	std::vector<SlideObject> objects{box("a", 10, 10), box("b", 50, 50)};
	objects[1].locked = true;
	const std::vector<std::string> both{"a", "b"};
	sinopia::nudgeObjects(objects, both, 5, -5);
	CHECK(objects[0].x == 15.0f);
	CHECK(objects[1].x == 50.0f);
	sinopia::alignObjects(objects, both, "left", true, {800, 450});
	CHECK(objects[1].x == 50.0f);
	CHECK_FALSE(sinopia::groupObjects(objects, both, "g"));
}
