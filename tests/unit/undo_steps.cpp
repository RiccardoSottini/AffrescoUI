/// @file
/// @brief The steps of an undo that keeps later changes (SCREEN_FORMAT.md, the steps table): "revert" with the
/// person's choice ("drop", "keep", "head"), "undoPlan", which reads what an undo would do from any handler, and
/// Rules::brokenLines(), the features an undo would leave out; on a headless window.

#include "Headless.hpp"
#include "Helpers.hpp"

#include "Composition.hpp"
#include "FeatureManifest.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with its own history view: actions that plan and undo revisions, a title and a subtitle that
/// revisions change, and a person who asks the agents (whose changes wait for review).
constexpr const char* historyScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "plan": null, "outcome": "", "error": "", "asked": "", "ticks": 0 },
	"permissions": { "allow": ["studio"], "ask": ["agent"], "reviewers": ["studio"] },
	"actions": {
		"undoLast": [{ "revert": 2 }],
		"undoFirstKeeping": [{ "revert": 1, "drop": "all", "head": "{{state.plan.head}}" }],
		"undoStale": [{ "revert": 1, "drop": "all", "head": "0000000000000000000000000000000000000000000000000000000000000000" }],
		"undoMalformed": [{ "revert": 1, "drop": 3 }],
		"plan": [{ "undoPlan": 1, "into": "state.plan", "then": [{ "set": "state.outcome", "to": "{{result.outcome}}" }],
		           "onError": [{ "set": "state.error", "to": "{{error}}" }] }],
		"planNothing": [{ "undoPlan": "ffffffffffff", "into": "state.plan",
		                  "onError": [{ "set": "state.error", "to": "{{error}}" }] }],
		"planMalformed": [{ "undoPlan": 1, "keep": [3], "onError": [{ "set": "state.error", "to": "{{error}}" }] }],
		"send": [{ "ask": "Tidy the titles", "into": "state.asked" }],
		"ask:reply": [{ "revert": 1, "drop": "all" }]
	},
	"root": { "type": "column", "id": "root", "children": [
		{ "type": "label", "id": "title", "text": "Tasks" },
		{ "type": "label", "id": "subtitle", "text": "Today" }
	] }
})json";

/// @brief A screen built in a headless window, with two revisions by the studio: the title, then the subtitle. The
/// channel comes first: it outlives the screen.
struct History {
	affresco::test::MemoryChannel agent;      ///< What the agent is sent.
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.
	std::vector<std::string> errors;          ///< What the screen reported.

	/// @brief Builds a screen written as JSON and makes its two revisions.
	/// @param text The screen.
	explicit History(const std::string& text = historyScreen) : History(affresco::Screen::fromString(text)) {}

	/// @brief Builds a screen and makes its two revisions by the studio.
	/// @param made The screen, loaded.
	/// @param first The first revision: by default, it renames the title.
	/// @param second The second: by default, it changes the subtitle.
	explicit History(
	    affresco::Screen made,
	    const char* first = R"({"author": "studio", "message": "Rename", "set": {"#title/text": "Jobs"}})",
	    const char* second = R"({"author": "studio", "message": "Retime", "set": {"#subtitle/text": "Tomorrow"}})")
	    : window(createHeadlessWindow(400, 300, clock)), screen(std::move(made)) {
		(void)screen.onError([this](const std::string& message) { errors.push_back(message); });
		screen.build(*window);
		window->render();
		for(const char* revision : {first, second}) {
			const affresco::RevisionResult result = screen.submitRevision(revision);
			INFO(result.error);
			REQUIRE(result.status == affresco::RevisionStatus::Applied);
		}
		window->render();
	}

	/// @brief Runs one of the screen's actions, as the end of the event would.
	/// @param name The action.
	void run(const char* name) {
		screen.runAction(name, window->root(), "{}");
		window->runDeferredTasks();
		window->render();
	}

	/// @brief Clicks the window at a point, as the person would.
	/// @param x Horizontal position.
	/// @param y Vertical position.
	void click(float x, float y) const {
		for(const affresco::Action action : {affresco::Action::Press, affresco::Action::Release})
			window->routeMouseButton({affresco::MouseButton::Left, action, {}, {x, y}});
		window->runDeferredTasks();
		window->render();
	}

	/// @brief The screen's state.
	/// @return It.
	[[nodiscard]] const json& state() { return screen.versions().state(); }

	/// @brief A label's text in the document on main.
	/// @param index The label's place among the root's children.
	/// @return Its text.
	[[nodiscard]] std::string text(std::size_t index) {
		return screen.versions().document()["root"]["children"][index]["text"].get<std::string>();
	}

	/// @brief Whether the screen reported an error containing a text.
	/// @param part The text.
	/// @return true if it did.
	[[nodiscard]] bool reported(const std::string& part) const {
		return std::ranges::any_of(errors, [&part](const std::string& error) { return error.contains(part); });
	}
};

/// @brief A feature that puts a label in a contribution point, as an installed record.
/// @param name Its name.
/// @param order Its place among the features.
/// @return The record.
json clockFeature(const std::string& name = "clock", int order = 10) {
	json manifest = json::parse(R"json({
		"format": "affrescoui-feature",
		"version": 2,
		"name": "clock",
		"release": "1.0.0",
		"title": "Clock",
		"description": "Shows the time in the footer.",
		"license": "Apache-2.0",
		"requires": { "format": 2, "points": ["footer.right"] },
		"provides": { "views": { "footer.right": { "clock-label": { "type": "label", "text": "12:00" } } } }
	})json");
	manifest["name"] = name;
	manifest["provides"]["views"]["footer.right"] = {{name + "-label", {{"type", "label"}, {"text", "12:00"}}}};
	return affresco::format::recordFor(manifest, "revision", "", "", order);
}

} // namespace

TEST_CASE("undo steps: a revert with a choice undoes an earlier revision and keeps a later change") {
	History history;
	history.run("plan");
	REQUIRE(history.state()["plan"]["outcome"] == "clean");
	history.run("undoFirstKeeping");
	CHECK(history.errors.empty());
	CHECK(history.screen.revision() == 3);
	CHECK(history.text(0) == "Tasks");    // undone
	CHECK(history.text(1) == "Tomorrow"); // the later change stays
}

TEST_CASE("undo steps: a revert without a choice, on the head, does what it did") {
	History history;
	history.run("undoLast");
	CHECK(history.errors.empty());
	CHECK(history.screen.revision() == 3);
	CHECK(history.text(0) == "Jobs");
	CHECK(history.text(1) == "Today");
}

TEST_CASE("undo steps: undoPlan writes the plan into state, runs then, and onError for no commit") {
	History history;
	history.run("plan");
	const json& plan = history.state()["plan"];
	for(const char* key : {"of", "head", "outcome", "conflicts", "later", "state"}) CHECK(plan.contains(key));
	CHECK(plan["of"]["number"] == 1);
	CHECK(plan["head"] == history.screen.versions().head().id);
	CHECK(history.state()["outcome"] == plan["outcome"]);
	CHECK(history.screen.revision() == 2); // it changes nothing

	history.run("planNothing");
	CHECK_FALSE(history.state()["error"].get<std::string>().empty());

	History malformed;
	malformed.run("planMalformed");
	CHECK(malformed.state()["error"].get<std::string>().contains(".keep: expected a list of paths"));
}

TEST_CASE("undo steps: undoPlan is not a control step: it runs from a timer, even while a proposal is previewed") {
	std::string text = historyScreen;
	const std::string subtitle = R"({ "type": "label", "id": "subtitle", "text": "Today" })";
	text.replace(text.find(subtitle), subtitle.size(),
	             R"({ "type": "label", "id": "subtitle", "text": "Today", "every": { "ms": 16, "do": "plan" } })");
	History history(text);
	const affresco::RevisionResult proposed = history.screen.submitRevision(
	    R"({"author": "agent", "message": "Recolour", "set": {"#title/color": "#ff0000"}})");
	REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
	REQUIRE(history.screen.previewProposal(proposed.proposal));
	for(int frame = 0; frame < 3; ++frame) {
		history.clock.advance(40.0);
		history.window->render();
	}
	CHECK(history.state()["plan"].is_object());
	CHECK_FALSE(history.reported("control steps run only from the user's input"));
	history.screen.endPreview();
}

TEST_CASE("undo steps: a revert with drop on a head that is not main's is refused, and reported") {
	History history;
	history.run("undoStale");
	CHECK_FALSE(history.errors.empty());
	CHECK(history.screen.revision() == 2);
	CHECK(history.text(0) == "Jobs");

	history.errors.clear();
	history.run("undoMalformed");
	CHECK(history.reported(".drop: expected a list of paths or \"all\""));
	CHECK(history.screen.revision() == 2);
}

TEST_CASE("undo steps: a revert with drop is a control step: refused from a timer while previewing, and from a reply") {
	SUBCASE("a timer while a proposal is previewed") {
		std::string text = historyScreen;
		const std::string subtitle = R"({ "type": "label", "id": "subtitle", "text": "Today" })";
		text.replace(
		    text.find(subtitle), subtitle.size(),
		    R"({ "type": "label", "id": "subtitle", "text": "Today", "every": { "ms": 16, "do": "undoFirstKeeping" } })");
		History history(text);
		const affresco::RevisionResult proposed = history.screen.submitRevision(
		    R"({"author": "agent", "message": "Recolour", "set": {"#title/color": "#ff0000"}})");
		REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
		REQUIRE(history.screen.previewProposal(proposed.proposal));
		history.clock.advance(40.0);
		history.window->render();
		history.window->runDeferredTasks();
		CHECK(history.reported("control steps run only from the user's input"));
		history.screen.endPreview();
		CHECK(history.screen.revision() == 2);
	}
	SUBCASE("an agent's reply") {
		History history;
		history.screen.connect(history.agent, "agent");
		history.run("send");
		json reply = {{"type", "reply"}, {"request", history.state()["asked"]}, {"text", "Undo it"}};
		history.agent.receive(reply.dump());
		history.window->runDeferredTasks();
		history.window->render();
		CHECK(history.screen.revision() == 2);
		CHECK(std::ranges::any_of(history.agent.sent, [](const json& message) {
			return message.value("type", "") == "error" &&
			       message.value("message", "").contains("never the person's decision");
		}));
	}
}

TEST_CASE("undo steps: both steps are functions in Lisp, through the step registry") {
	constexpr const char* program = R"lisp(
(defn plan "Works out the undo of the first revision." [] (undoPlan 1 :into "state.plan"))
(defn undo-first "Undoes the first revision, taking out what conflicts." []
  (revert 1 :drop "all" :head state.plan.head))
(defscreen main :state {:plan nil} :permissions {:allow [studio]}
  (column#root (label#undoButton :text "Undo" :width 120 :height 24 :onClick undo-first)
    (label#title :text "Tasks") (label#subtitle :text "Today")))
)lisp";
	History history(
	    affresco::Screen::fromString(affresco::lisp::sourceDocument(program, "app.lisp").dump()),
	    R"json({"author": "studio", "message": "One", "set": {"/program/app.lisp/one": "(def one 1)"}})json",
	    R"json({"author": "studio", "message": "Two", "set": {"/program/app.lisp/two": "(def two 2)"}})json");
	history.run("plan");
	REQUIRE(history.state()["plan"].is_object());
	CHECK(history.state()["plan"]["of"]["number"] == 1);
	history.click(10.0f, 10.0f);           // a review step in Lisp runs only from the person's own press
	CHECK(history.screen.revision() == 3); // the choice reached the undo, and it landed
}

TEST_CASE("undo steps: no revision from an agent adds or alters a revert with drop") {
	History history;
	const affresco::RevisionResult added = history.screen.submitRevision(
	    R"({"author": "studio", "message": "Undo from a key", "set": {"/actions/sneak": [{"revert": 1, "drop": "all"}]}})");
	CHECK(added.status == affresco::RevisionStatus::Rejected);
	const affresco::RevisionResult altered = history.screen.submitRevision(
	    R"({"author": "agent", "message": "Keep it", "set": {"/actions/undoFirstKeeping/0/drop": []}})");
	CHECK(altered.status == affresco::RevisionStatus::Rejected);
	CHECK(history.screen.revision() == 2);
}

TEST_CASE("undo steps: brokenLines names the feature an undo would leave out") {
	const affresco::format::Rules rules;
	const json before = {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"features", {{"clock", clockFeature()}}},
	    {"root",
		 {{"type", "column"},
		  {"id", "main"},
		  {"children", json::array({{{"type", "row"}, {"id", "footer"}, {"point", "footer.right"}}})}}}};
	json after = before;
	after["root"]["children"] = json::array();

	const std::vector<std::string> lines = rules.brokenLines(before, after);
	REQUIRE(lines.size() == 1);
	CHECK(lines[0].starts_with("the feature 'clock' would be left out: features.clock.requires.points"));
	CHECK(lines[0].ends_with("(and 1 more)")); // its view's point, the same cause
	CHECK(rules.brokenLines(before, before).empty());
	CHECK(rules.brokenLines(after, after).size() == 0); // the problem was already there
	json plain = before;
	plain.erase("features");
	CHECK(rules.brokenLines(plain, plain).empty());
	CHECK(rules.brokenLines(json::array(), json(3)).empty()); // never throws

	json many = before;
	for(int i = 0; i < 20; ++i) {
		const std::string name = "clock" + std::to_string(i);
		many["features"][name] = clockFeature(name, 20 + i);
	}
	REQUIRE(affresco::format::compose(many).problems.empty());
	json manyAfter = many;
	manyAfter["root"]["children"] = json::array();
	const std::vector<std::string> capped = rules.brokenLines(many, manyAfter);
	REQUIRE(capped.size() == 16);
	CHECK(capped.back() == "and 6 more"); // 21 features, 15 lines for them
}
