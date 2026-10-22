/// @file
/// @brief Sinopia's deck model (examples/sinopia): reading and writing a deck, its limits, ids, theme tokens, page
/// sizes, text autofit and slides made from layouts.

#include <sinopia/Deck.hpp>
#include <sinopia/ShapeGeometry.hpp>

#include <affresco/render/VectorPath.hpp>

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>

using affresco::json;
using sinopia::Deck;
using sinopia::readDeck;
using sinopia::writeDeck;

namespace {

/// @brief A small deck: a theme, a layout with placeholders, a section and two slides.
/// @return It as JSON.
json sampleDeck() {
	return json::parse(R"({
		"title": "Talk", "size": "4:3", "theme": {"accent": "#7c3aed", "ink": "#111"},
		"masters": [{"id": "plain", "background": "#fff"}],
		"layouts": [{"id": "title", "master": "plain", "objects": [
			{"id": "t", "kind": "text", "placeholder": "title", "x": 40, "y": 40, "w": 700, "h": 80, "text": "Title"}]}],
		"sections": [{"id": "intro", "title": "Intro"}],
		"slides": [
			{"id": "s1", "layout": "title", "section": "intro", "notes": "Say hello", "transition": {"kind": "fade", "ms": 300},
			 "objects": [
				{"id": "a", "kind": "rect", "x": 10, "y": 20, "w": 30, "h": 40, "fill": "@accent", "radius": 6,
				 "gradient": {"from": "#fff", "to": "@ink", "angle": 45}, "shadow": {"x": 2, "y": 3, "blur": 4, "color": "#00000044"}},
				{"id": "b", "kind": "text", "runs": [{"text": "Bold", "bold": true}, {"text": " plain"}],
				 "build": {"kind": "fly", "order": 1}},
				{"id": "g", "kind": "group", "children": [{"kind": "star", "sides": 6, "inner": 0.4}]}]},
			{"id": "s2", "skip": true}]
	})");
}

} // namespace

TEST_CASE("sinopia: a deck reads, writes and reads back the same") {
	const Deck deck = readDeck(sampleDeck());
	CHECK(deck.title == "Talk");
	CHECK(deck.page == std::pair{960.0f, 720.0f});
	REQUIRE(deck.slides.size() == 2);
	const sinopia::Slide& first = deck.slides[0];
	CHECK(first.notes == "Say hello");
	CHECK(first.transition.kind == "fade");
	CHECK(first.transition.ms == 300);
	REQUIRE(first.objects.size() == 3);
	CHECK(first.objects[0].fill == "@accent");
	REQUIRE(first.objects[0].gradient.has_value());
	CHECK(first.objects[0].gradient->to == "@ink");
	CHECK(first.objects[1].runs.size() == 2);
	CHECK(first.objects[1].build.kind == "fly");
	CHECK(first.objects[2].children.size() == 1);
	CHECK(deck.slides[1].skip);
	CHECK(readDeck(writeDeck(deck)) == deck);
}

TEST_CASE("sinopia: ids are unique and safe, and a missing one is made") {
	const Deck deck = readDeck(json::parse(R"({"slides": [{"id": "x", "objects": [{"id": "a"}, {"id": "a"}, {},
		{"id": "a b.c"}]}, {"id": "x"}, {}]})"));
	const auto& objects = deck.slides[0].objects;
	CHECK(objects[0].id == "a");
	CHECK(objects[1].id == "a-2");
	CHECK(objects[2].id == "rect3");
	CHECK(objects[3].id == "a-b-c");
	CHECK(deck.slides[1].id == "x-2");
	CHECK(deck.slides[2].id != deck.slides[0].id);
}

TEST_CASE("sinopia: bad decks and decks past their limits are refused") {
	CHECK_THROWS_AS((void)readDeck(json::array()), std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"size": "huge"})")), std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"kind": "blob"}]}]})")),
	                std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"fill": "@nope"}]}]})")),
	                std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"fill": "red"}]}]})")), std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"image": "../secret.png"}]}]})")),
	                std::runtime_error);
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"kind": "group", "children": [
		{"kind": "group", "children": [{}]}]}]}]})")),
	                std::runtime_error);
	sinopia::DeckLimits limits;
	limits.slides = 1;
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{}, {}]})"), limits), std::runtime_error);
	limits = {};
	limits.textBytes = 4;
	CHECK_THROWS_AS((void)readDeck(json::parse(R"({"slides": [{"objects": [{"text": "too long"}]}]})"), limits),
	                std::runtime_error);
	// Numbers are clamped, not refused
	const Deck clamped = readDeck(json::parse(R"({"slides": [{"objects": [{"w": -5, "x": 1e30}]}]})"));
	CHECK(clamped.slides[0].objects[0].w == 1.0f);
	CHECK(clamped.slides[0].objects[0].x == 100000.0f);
}

TEST_CASE("sinopia: slides are 800 wide, and pages fit in 800 by 600") {
	CHECK(sinopia::slideSize(readDeck(json::object())) == std::pair{800.0f, 450.0f});
	CHECK(sinopia::slideSize(readDeck(json::parse(R"({"size": "4:3"})"))) == std::pair{800.0f, 600.0f});
	CHECK(sinopia::slideSize(readDeck(json::parse(R"({"size": "a4"})"))) == std::pair{424.0f, 600.0f});
	CHECK(sinopia::slideSize(readDeck(json::parse(R"({"size": "custom", "page": {"w": 1000, "h": 250}})"))) ==
	      std::pair{800.0f, 200.0f});
}

TEST_CASE("sinopia: text that doesn't fit shrinks, or overflows when autofit is off") {
	sinopia::SlideObject frame;
	frame.kind = "text";
	frame.w = 200.0f;
	frame.h = 60.0f;
	frame.text = "Short";
	sinopia::fitText(frame);
	CHECK(frame.fit == 1.0f);
	CHECK_FALSE(frame.overflow);
	frame.text = std::string(400, 'x');
	sinopia::fitText(frame);
	CHECK(frame.fit < 1.0f);
	frame.autofit = false;
	sinopia::fitText(frame);
	CHECK(frame.fit == 1.0f);
	CHECK(frame.overflow);
}

TEST_CASE("sinopia: a slide made from a layout copies its placeholders") {
	const Deck deck = readDeck(sampleDeck());
	const sinopia::Slide slide = sinopia::slideFromLayout(deck, "title", "s9");
	CHECK(slide.layout == "title");
	REQUIRE(slide.objects.size() == 1);
	CHECK(slide.objects[0].id == "s9-t");
	CHECK(slide.objects[0].placeholder == "title");
	CHECK(sinopia::slideFromLayout(deck, "missing", "s10").layout == "title"); // the first layout
}

TEST_CASE("sinopia: every shape's outline is path data VectorPath reads") {
	for(const char* kind : {"rect", "ellipse", "polygon", "star", "line", "arrow"}) {
		sinopia::ShapeOutline shape{kind, 120.0f, 80.0f, 12.0f, 6, 0.4f};
		const affresco::VectorPath path = affresco::VectorPath::parse(sinopia::shapePath(shape));
		const auto contours = path.flatten({{1.0f, 1.0f}, {0.0f, 0.0f}}, 0.25f);
		REQUIRE(contours.size() == 1);
		CHECK(contours[0].closed == !sinopia::isOpenShape(kind));
		for(const affresco::Vec2 point : contours[0].points) {
			CHECK(point.x >= -0.01f);
			CHECK(point.x <= 120.01f);
			CHECK(point.y >= -0.01f);
			CHECK(point.y <= 80.01f);
		}
	}
}
