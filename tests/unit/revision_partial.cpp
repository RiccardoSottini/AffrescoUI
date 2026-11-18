/// @file
/// @brief Unit tests for a revision of the nodes: only the lists of children it changed are reconciled, and only the
/// nodes it changed are applied again (modules/format/src/runtime/EngineVersions.cpp, CompiledCache::carryOver()).
/// Every screen runs in verify mode, so each partial update is compared with a full pass.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// Groups of the large screen, and labels in each: with the root and the button, 5,002 panels.
constexpr int groups = 50;
constexpr int labels = 99;

/// @brief The large screen: a button, a label of the state, then groups of labels, each with an id.
/// @return Its document.
json largeScreen() {
	json list = json::array();
	for(int g = 0; g < groups; ++g) {
		json group = {{"id", "g" + std::to_string(g)}, {"type", "column"}, {"children", json::array()}};
		for(int l = 0; l < labels; ++l) {
			const std::string name = std::to_string(g) + "_" + std::to_string(l);
			group["children"].push_back({{"id", "l" + name}, {"type", "label"}, {"text", "t" + name}});
		}
		list.push_back(std::move(group));
	}
	return {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"state", {{"count", 0}, {"clicked", 0}}},
	    {"styles", json::object()},
	    {"actions",
		 {{"one", json::array({{{"set", "state.clicked"}, {"to", 1}}})},
		  {"two", json::array({{{"set", "state.clicked"}, {"to", 2}}})}}},
	    {"root",
		 {{"type", "panel"},
		  {"children",
		   json::array(
	           {{{"id", "btn"}, {"type", "label"}, {"text", "Go"}, {"width", 100}, {"height", 30}, {"onClick", "one"}},
			    {{"id", "count"}, {"type", "label"}, {"text", "{{state.count}}"}, {"y", 40}},
			    {{"id", "groups"}, {"type", "row"}, {"y", 80}, {"children", std::move(list)}}})}}}};
}

/// @brief The Kanban template's JSON twin, its board filled to a number of cards (as tests/perf/screens.cpp does).
/// @param cards The cards: at least the template's own.
/// @return Its document.
json kanban(int cards) {
	const std::filesystem::path folder = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "flow" / "json";
	json document =
	    json::parse(affresco::Screen::fromApp(folder, {}, affresco::Screen::History::InMemory).documentJson());
	json& seed = document["models"]["issues"]["seed"];
	std::vector<json> originals;
	for(const json& card : seed)
		if(card["board"] == "launch") originals.push_back(card);
	if(originals.empty()) throw std::invalid_argument("the Kanban template has no cards to copy");
	for(std::size_t count = originals.size(); std::cmp_less(count, cards); ++count) {
		json card = originals[count % originals.size()];
		card["id"] = card["ref"] = "FLO-P" + std::to_string(count);
		card["status"] = count % 4;
		card["order"] = count;
		seed.push_back(std::move(card));
	}
	return document;
}

/// @brief A screen built in a headless window in verify mode, with the errors it reports.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the screen.
	std::unique_ptr<affresco::Screen> screen;                                         ///< The screen.
	std::vector<std::string> errors;                                                  ///< What it reported.
	affresco::Connection listening;                                                   ///< To its errors.

	/// @brief Builds a screen, every update of it checked against a full pass.
	/// @param document Its document.
	/// @param folder Where its files are.
	explicit Built(const json& document, const std::filesystem::path& folder = ".") {
		affresco::test::verifyNextRuntimes(true);
		screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(document.dump(), folder));
		listening = screen->onError([this](const std::string& message) { errors.push_back(message); });
		screen->build(*window);
		affresco::test::verifyNextRuntimes(false);
		window->render();
	}
	Built(const Built&) = delete;
	Built& operator=(const Built&) = delete;
	~Built() = default;

	/// @brief Applies a revision, which must apply, and renders.
	/// @param revision Its edits.
	/// @return What its update cost.
	const affresco::format::UpdateStatistics& apply(json revision) const {
		revision["author"] = "agent/x";
		revision["message"] = "m";
		std::string error;
		const bool applied = screen->applyRevision(revision.dump(), &error);
		INFO(error);
		REQUIRE(applied);
		window->render();
		return screen->lastUpdate();
	}
	/// @brief A label's text.
	/// @param id Its id.
	/// @return The text.
	[[nodiscard]] std::string text(const char* id) const {
		const auto* label = dynamic_cast<const affresco::Label*>(screen->find(id));
		REQUIRE(label != nullptr);
		return label->text();
	}
	/// @brief The ids of a panel's children.
	/// @param id Its id.
	/// @return Them, in order.
	[[nodiscard]] std::vector<std::string> childIds(const char* id) const {
		const affresco::Panel* panel = screen->find(id);
		REQUIRE(panel != nullptr);
		std::vector<std::string> ids;
		for(const auto& child : panel->children()) ids.emplace_back(child->id());
		return ids;
	}
	/// @brief Clicks a panel in its middle.
	/// @param id Its id.
	void click(const char* id) const {
		const affresco::Rect bounds = screen->find(id)->bounds();
		const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		window->render();
	}
	/// @brief A value of the state.
	/// @param key Its key.
	/// @return It.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen->stateJson()).at(key); }
};

} // namespace

TEST_CASE("revision partial: a label's text set in the Kanban template applies that label only") {
	for(const int cards : {14, 400}) {
		CAPTURE(cards);
		Built built(kanban(cards), std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "flow" / "json");
		REQUIRE(built.errors.empty());
		// what every revision brings: the version's number and the history, read by a few panels of the template
		const std::size_t review = built.apply({{"set", {{"/state/benchRevision", 1}}}}).nodesEvaluated;
		CHECK(review < 10U);
		const auto& one = built.apply({{"set", {{"#logo/text", "Flow 2"}}}});
		CHECK_FALSE(one.full);
		CHECK(one.nodesEvaluated == review + 1U);
		CHECK(one.panelsCreated == 0);
		CHECK(built.text("logo") == "Flow 2");

		// a node of the board's column template: every column applies it, and none is created
		const std::size_t columns = built.screen->find("board")->children().size();
		REQUIRE(columns > 1);
		const auto& template_ = built.apply({{"set", {{"#columnName/height", 38}}}});
		CHECK_FALSE(template_.full);
		CHECK(template_.nodesEvaluated == review + columns);
		CHECK(template_.panelsCreated == 0);
		CHECK(built.errors.empty()); // verify mode found the panels a full pass gives
	}
}

TEST_CASE("revision partial: a label's text set in a large screen applies that label only") {
	const Built built(largeScreen());
	REQUIRE(built.errors.empty());
	const auto& one = built.apply({{"set", {{"#l7_42/text", "changed"}}}});
	CHECK_FALSE(one.full);
	CHECK(std::string(one.fullReason).empty());
	CHECK(one.nodesEvaluated == 1);
	CHECK(one.panelsCreated == 0);
	CHECK(built.text("l7_42") == "changed");
	CHECK(built.text("l7_41") == "t7_41");
	CHECK(built.errors.empty());
}

TEST_CASE("revision partial: a node added, removed or moved") {
	Built built(largeScreen());
	SUBCASE("added at the end: its panel is created, no sibling is evaluated") {
		const auto& added =
		    built.apply({{"add", {{"#g3/children/-", {{"id", "new"}, {"type", "label"}, {"text", "n"}}}}}});
		CHECK_FALSE(added.full);
		CHECK(added.panelsCreated == 1);
		CHECK(added.nodesEvaluated == 1);
		CHECK(built.text("new") == "n");
		CHECK(built.childIds("g3").size() == static_cast<std::size_t>(labels) + 1U);
	}
	SUBCASE("added in the middle") {
		const auto& added =
		    built.apply({{"add", {{"#g3/children/5", {{"id", "new"}, {"type", "label"}, {"text", "n"}}}}}});
		CHECK_FALSE(added.full);
		CHECK(added.panelsCreated >= 1);
		CHECK(built.childIds("g3")[5] == "new");
	}
	SUBCASE("removed") {
		const auto& removed = built.apply({{"remove", json::array({"#l3_5"})}});
		CHECK_FALSE(removed.full);
		CHECK(removed.panelsCreated == 0);
		CHECK(built.screen->find("l3_5") == nullptr);
		CHECK(built.childIds("g3").size() == static_cast<std::size_t>(labels) - 1U);
	}
	SUBCASE("moved within its list") {
		const auto& moved = built.apply({{"move", {{"#l3_1", "#g3/children/5"}}}});
		CHECK_FALSE(moved.full);
		const std::vector<std::string> ids = built.childIds("g3");
		CHECK(ids[5] == "l3_1");
		CHECK(ids[1] == "l3_2");
	}
	SUBCASE("moved to another parent") {
		const auto& moved = built.apply({{"move", {{"#l3_1", "#g4/children/0"}}}});
		CHECK_FALSE(moved.full);
		CHECK(built.childIds("g4")[0] == "l3_1");
		CHECK(built.childIds("g3")[1] == "l3_2");
		CHECK(built.text("l3_1") == "t3_1");
	}
	CHECK(built.errors.empty());
}

TEST_CASE("revision partial: a handler replaced runs on the next click") {
	const Built built(largeScreen());
	built.click("btn");
	CHECK(built.state("clicked") == 1);
	const auto& replaced = built.apply({{"set", {{"#btn/onClick", "two"}}}});
	CHECK_FALSE(replaced.full);
	CHECK(replaced.nodesEvaluated == 1);
	built.click("btn");
	CHECK(built.state("clicked") == 2);
	CHECK(built.errors.empty());
}

TEST_CASE("revision partial: a node and a place of the state in one revision are one update") {
	Built built(largeScreen());
	const std::size_t before = built.screen->lastUpdate().updates;
	const auto& both = built.apply({{"set", {{"#l7_42/text", "changed"}, {"/state/count", 5}}}});
	CHECK_FALSE(both.full);
	CHECK(both.updates == before + 1U);
	CHECK(both.nodesEvaluated == 2);
	CHECK(built.text("l7_42") == "changed");
	CHECK(built.text("count") == "5");
	CHECK(built.errors.empty());
}

TEST_CASE("revision partial: a section the panels read without a place takes the full pass") {
	const Built built(largeScreen());
	const auto& styled = built.apply({{"set", {{"/styles/extra", {{"textColor", "#ff0000"}}}}}});
	CHECK(styled.full);
	CHECK(std::string(styled.fullReason) == "revision: styles");
	const auto& acted =
	    built.apply({{"set", {{"/actions/three", json::array({{{"set", "state.clicked"}, {"to", 3}}})}}}});
	CHECK(acted.full);
	CHECK(std::string(acted.fullReason) == "revision: actions");
	// the root changed in its own keys
	const auto& root = built.apply({{"set", {{"/root/color", "#ffffff"}}}});
	CHECK(root.full);
	CHECK(std::string(root.fullReason) == "revision: a root, screen or overlay");
	CHECK(built.errors.empty());
}

TEST_CASE("revision partial: a refused revision leaves the screen as it was") {
	Built built(largeScreen());
	const std::size_t before = built.screen->lastUpdate().updates;
	std::string error;
	CHECK_FALSE(built.screen->applyRevision(
	    json{{"author", "agent/x"}, {"message", "m"}, {"set", {{"#l7_42/text", "changed"}, {"#nope/text", "x"}}}}
	        .dump(),
	    &error));
	built.window->render();
	CHECK_FALSE(error.empty());
	CHECK(built.screen->lastUpdate().updates == before);
	CHECK(built.text("l7_42") == "t7_42");
	const auto& next = built.apply({{"set", {{"#l7_42/text", "after"}}}});
	CHECK_FALSE(next.full);
	CHECK(built.text("l7_42") == "after");
	CHECK(built.errors.empty());
}
