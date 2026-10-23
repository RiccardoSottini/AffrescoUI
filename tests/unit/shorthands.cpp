/// @file
/// @brief The format's shorthands: "anchors" as text ("left right:12 top:8"), each edge at 0 or at its length, and a
/// label's "align" as two words, horizontal then vertical ("start center"); both read like their object forms, the
/// checker accepts them and rejects a malformed one, and a revision swaps one form for the other. A tree's "expanded"
/// and a table's "sort" written as a state path keep their value there, before the node's own handler runs.

#include "Checker.hpp"
#include "Headless.hpp"
#include "PropertyValues.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/TableView.hpp>
#include <affresco/render/TreeView.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using affresco::Action;
using affresco::json;
using affresco::MouseButton;
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

/// @brief Clicks a point of a window and renders it.
/// @param window The window.
/// @param x, y The point.
void click(affresco::Window& window, float x, float y) {
	window.routeMouseButton({MouseButton::Left, Action::Press, {}, {x, y}});
	window.routeMouseButton({MouseButton::Left, Action::Release, {}, {x, y}});
	window.render();
}

/// @brief A table's rows in the order shown, by id.
/// @param table The table.
/// @return The ids.
[[nodiscard]] std::vector<std::string> shownIds(const affresco::TableView& table) {
	std::vector<std::string> ids;
	for(std::size_t row = 0; row < table.rowCount(); ++row) ids.push_back(table.model()->item(table.itemAt(row)).id);
	return ids;
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

TEST_CASE("shorthands: a tree's expanded as a state path keeps the open ids there, then runs its own onExpand") {
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 120, clock);
	const json document =
	    json::parse(R"({"version": 2, "state": {"files": [{"id": "src", "children": ["main"]}, "README"],
		"ui": {}, "count": 0},
		"actions": {"noted": {"do": [{"set": "state.count", "to": "{{state.count + 1}}"}]}},
		"root": {"id": "root", "children": [
			{"type": "tree", "id": "files", "width": 200, "height": 120, "items": "{{state.files}}",
			 "expanded": "state.ui.open", "onExpand": "noted"}]}})");
	checkClean(document);
	auto screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	window->render();
	const auto state = [&screen] { return json::parse(screen.stateJson()); };
	auto* tree = dynamic_cast<affresco::TreeView*>(screen.find("files"));
	REQUIRE(tree);
	CHECK(tree->rowCount() == 2); // nothing kept yet: none open

	click(*window, 12.0f, 12.0f);
	CHECK(state()["ui"]["open"] == json{"src"});
	CHECK(state()["count"] == 1);
	CHECK(tree->rowCount() == 3);
	clock.advance(1000.0);
	click(*window, 12.0f, 12.0f);
	CHECK(state()["ui"]["open"] == json::array());
	CHECK(state()["count"] == 2);
	CHECK(tree->rowCount() == 2);

	// The state opens it too, and a path that isn't plain is read as written: not ids, so it doesn't build
	REQUIRE(screen.applyRevision(R"({"set": {"/state/ui/open": ["src"]}})"));
	window->render();
	CHECK(tree->rowCount() == 3);
	for(const char* bad : {"state.ui..open", "state.", "state.ui open", "files.open"}) {
		CAPTURE(bad);
		json broken = document;
		broken["root"]["children"][0]["expanded"] = bad;
		CHECK_FALSE(builds(broken));
	}
}

TEST_CASE("shorthands: a table's sort as a state path keeps the order there, then runs its own onSort steps") {
	ManualClock clock;
	const auto window = createHeadlessWindow(240, 148, clock);
	const json document = json::parse(R"({"version": 2, "state": {"order": null, "sorted": ""},
		"root": {"id": "root", "children": [
			{"type": "table", "id": "files", "width": 240, "height": 148,
			 "columns": ["Name", {"id": "size", "title": "Size", "width": 80}],
			 "items": [{"id": "b", "cells": ["beta", 2]}, {"id": "a", "cells": ["alpha", 1]}],
			 "sort": "state.order", "onSort": [{"set": "state.sorted", "to": "{{state.order.column}}"}]}]}})");
	checkClean(document);
	auto screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	window->render();
	const auto state = [&screen] { return json::parse(screen.stateJson()); };
	auto* table = dynamic_cast<affresco::TableView*>(screen.find("files"));
	REQUIRE(table);
	CHECK(shownIds(*table) == std::vector<std::string>{"b", "a"}); // null: the items' order

	click(*window, 60.0f, 14.0f);
	CHECK(state()["order"] == json{{"column", "Name"}, {"descending", false}});
	CHECK(state()["sorted"] == "Name"); // its own steps see the order kept
	CHECK(shownIds(*table) == std::vector<std::string>{"a", "b"});
	clock.advance(1000.0);
	click(*window, 60.0f, 14.0f);
	CHECK(state()["order"] == json{{"column", "Name"}, {"descending", true}});
	CHECK(shownIds(*table) == std::vector<std::string>{"b", "a"});

	REQUIRE(screen.applyRevision(R"({"set": {"/state/order": {"column": "size"}}})"));
	window->render();
	CHECK(shownIds(*table) == std::vector<std::string>{"a", "b"});
}
