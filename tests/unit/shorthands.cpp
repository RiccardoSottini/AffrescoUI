/// @file
/// @brief The format's shorthands: "anchors" as text ("left right:12 top:8"), each edge at 0 or at its length, and a
/// label's "align" as two words, horizontal then vertical ("start center"); both read like their object forms, the
/// checker accepts them and rejects a malformed one, and a revision swaps one form for the other.

#include "Checker.hpp"
#include "Headless.hpp"
#include "PropertyValues.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A 400 by 300 screen with a bar across the top, a side panel and a label, placed by text anchors.
/// @return The document.
std::string anchoredScreen() {
	return R"({"version": 2, "root": {"id": "root", "children": [
		{"id": "bar", "anchors": "left right top", "height": 40},
		{"id": "side", "anchors": "left:8 top:48 bottom:10%", "width": 60},
		{"id": "body", "anchors": " right:-4  top:50px bottom ", "width": 100}
	]}})";
}

/// @brief A screen built in a headless 400 by 300 window and laid out.
struct Anchored {
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< The window.
	affresco::Screen screen{affresco::Screen::fromString(anchoredScreen())};          ///< The screen.

	/// @brief Builds it.
	Anchored() {
		screen.build(*window);
		layout();
	}

	/// @brief Lays the window out again, after a change.
	void layout() {
		window->render();
		clock.advance(1.0);
		window->render();
	}

	/// @brief A node's frame, required.
	/// @param id The node's id.
	/// @return Its frame.
	affresco::Rect frame(const char* id) {
		const affresco::Panel* panel = screen.find(id);
		REQUIRE(panel);
		return panel->frame();
	}
};

/// @brief Tells whether a document builds, in a headless window.
/// @param document The document.
/// @return true if it loads and builds without an error.
bool builds(const json& document) {
	try {
		ManualClock clock;
		const std::unique_ptr<affresco::Window> window = createHeadlessWindow(200, 200, clock);
		affresco::Screen screen{affresco::Screen::fromString(document.dump())};
		screen.build(*window);
		window->render();
		return true;
	} catch(const std::exception&) {
		return false;
	}
}

/// @brief Checks the checker finds nothing in a document.
/// @param document The document.
void checkClean(const json& document) {
	const auto findings = affresco::format::check(document, document.value("state", json::object()));
	REQUIRE(findings);
	for(const auto& finding : *findings) CAPTURE(finding.text());
	CHECK(findings->empty());
}

/// @brief Checks a frame's edges.
/// @param frame The frame.
/// @param left, top, right, bottom The edges expected.
void checkFrame(const affresco::Rect& frame, float left, float top, float right, float bottom) {
	CHECK(frame.left == doctest::Approx(left));
	CHECK(frame.top == doctest::Approx(top));
	CHECK(frame.right == doctest::Approx(right));
	CHECK(frame.bottom == doctest::Approx(bottom));
}

} // namespace

TEST_CASE("shorthands: text anchors put each named edge at 0 or at the length after its colon") {
	Anchored anchored;
	checkFrame(anchored.frame("bar"), 0, 0, 400, 40);
	checkFrame(anchored.frame("side"), 8, 48, 68, 270);
	checkFrame(anchored.frame("body"), 304, 50, 404, 300);

	// A revision swaps the text for an object and back, each edge it no longer names let go
	REQUIRE(anchored.screen.applyRevision(R"({"set": {"#bar/anchors": {"left": 10, "top": 5}, "#bar/width": 50}})"));
	anchored.layout();
	checkFrame(anchored.frame("bar"), 10, 5, 60, 45);
	REQUIRE(anchored.screen.applyRevision(R"({"set": {"#bar/anchors": "right bottom:20"}})"));
	anchored.layout();
	checkFrame(anchored.frame("bar"), 350, 240, 400, 280);
}

TEST_CASE("shorthands: the checker accepts text anchors; unknown edges and malformed lengths don't build") {
	const json document = json::parse(anchoredScreen());
	checkClean(document);
	CHECK(builds(document));

	for(const char* bad : {"left middle", "", "left:", "left:abc", "left:nan", "left:1e999", "left right:12em"}) {
		CAPTURE(bad);
		json broken = document;
		broken["root"]["children"][0]["anchors"] = bad;
		CHECK_FALSE(builds(broken));
	}
}

TEST_CASE("shorthands: a label's align as two words is horizontal then vertical, like its object form") {
	using affresco::Align;
	using affresco::format::alignments;
	CHECK(alignments(json("start center"), "align") == std::pair{Align::Start, Align::Center});
	CHECK(alignments(json("end start"), "align") == std::pair{Align::End, Align::Start});
	CHECK(alignments(json("center"), "align") == std::pair{Align::Center, Align::Center});
	CHECK(alignments(json{{"horizontal", "end"}, {"vertical", "center"}}, "align") ==
	      std::pair{Align::End, Align::Center});
	CHECK(alignments(json{{"vertical", "end"}}, "align") == std::pair{Align::Start, Align::End});
	for(const char* bad : {"start middle", "start center end", "", "left"}) {
		CAPTURE(bad);
		CHECK_THROWS(static_cast<void>(alignments(json(bad), "align")));
	}

	const json label = json::parse(R"({"version": 2, "root": {"id": "root", "children": [
		{"id": "title", "type": "label", "text": "Hi", "width": 100, "height": 30, "align": "end center"}]}})");
	checkClean(label);
	CHECK(builds(label));
	json broken = label;
	broken["root"]["children"][0]["align"] = "end middle";
	CHECK_FALSE(builds(broken));
}
