/// @file
/// @brief Unit tests for what a partial update of a large screen visits: the readers of the places it changed, found
/// without walking the panels (src/updates/PlaceIndex.hpp).

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// Sections, groups in each, keyed rows in each group: 100 x 10 x 25 rows of two panels each, 50,000 panels.
constexpr int sections = 100;
constexpr int groups = 10;
constexpr int rows = 25;
/// Places of the state nothing reads.
constexpr int spare = 400;

/// @brief The screen: sections of groups of keyed rows, each row a panel and a label, and spare places of the state.
/// @return Its document.
json largeScreen() {
	json state = json::object();
	json& list = state["sections"] = json::array();
	for(int s = 0; s < sections; ++s) {
		json section = {{"title", "s" + std::to_string(s)}, {"groups", json::array()}};
		for(int g = 0; g < groups; ++g) {
			json group = {{"rows", json::array()}};
			for(int r = 0; r < rows; ++r) group["rows"].push_back({{"id", r}, {"text", "t" + std::to_string(r)}});
			section["groups"].push_back(std::move(group));
		}
		list.push_back(std::move(section));
	}
	for(int p = 0; p < spare; ++p) state["spare"]["p" + std::to_string(p)] = p;
	const json row = {{"type", "row"},
	                  {"repeat", {{"for", "row"}, {"in", "group.rows"}, {"key", "{{row.id}}"}}},
	                  {"children", json::array({{{"type", "label"}, {"text", "{{row.text}}"}}})}};
	const json group = {
	    {"type", "column"}, {"repeat", {{"for", "group"}, {"in", "section.groups"}}}, {"children", json::array({row})}};
	const json section = {{"type", "column"},
	                      {"repeat", {{"for", "section"}, {"in", "state.sections"}}},
	                      {"children", json::array({{{"type", "label"}, {"text", "{{section.title}}"}}, group})}};
	return {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"state", std::move(state)},
	    {"actions",
		 {{"text", json::array({{{"set", "state.sections[event.s].groups[event.g].rows[event.r].text"},
		                         {"to", "{{event.v}}"}}})},
		  {"title", json::array({{{"set", "state.sections[event.s].title"}, {"to", "{{event.v}}"}}})},
		  {"spare", json::array({{{"increment", "state.spare.p7"}}})},
		  {"textAndRemove",
		   json::array({{{"set", "state.sections[event.s].groups[event.g].rows[event.r].text"}, {"to", "{{event.v}}"}},
		                {{"remove", "state.sections[event.s].groups[event.g].rows"}, {"at", "{{event.at}}"}}})}}},
	    {"root", {{"type", "column"}, {"children", json::array({section})}}}};
}

/// @brief The large screen, built, with the errors it reports.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository{rules, largeScreen()};                       ///< The document.
	affresco::format::Runtime runtime{repository, "."};                               ///< The panels.
	std::vector<std::string> errors;                                                  ///< What it reported.
	affresco::Connection listening =
	    runtime.onError.connect([this](const std::string& message) { errors.push_back(message); });

	/// @brief Builds the screen.
	Built() {
		runtime.build(*window);
		window->render();
	}
	Built(const Built&) = delete;
	Built& operator=(const Built&) = delete;
	~Built() { runtime.onError.disconnect(listening); }

	/// @brief Runs an action and the update it queues.
	/// @param action The action.
	/// @param event Its event.
	/// @return What the update cost.
	const affresco::format::UpdateStatistics& run(const char* action, const json& event) {
		runtime.runAction(action, window->root(), event);
		window->render();
		return runtime.lastUpdate();
	}
	/// @brief The text of a row's label.
	/// @param s Its section.
	/// @param g Its group.
	/// @param r Its row.
	/// @return The text.
	[[nodiscard]] std::string text(int s, int g, int r) const {
		const affresco::Panel& section = *window->root().children()[static_cast<std::size_t>(s)];
		const affresco::Panel& group = *section.children()[static_cast<std::size_t>(g) + 1];
		const affresco::Panel& row = *group.children()[static_cast<std::size_t>(r)];
		return dynamic_cast<const affresco::Label&>(*row.children()[0]).text();
	}
};

} // namespace

TEST_CASE("update index: a change of a large screen visits only the panels that read it") {
	Built built;
	REQUIRE(built.errors.empty());

	const auto& one = built.run("text", {{"s", 42}, {"g", 3}, {"r", 17}, {"v", "changed"}});
	CHECK_FALSE(one.full);
	CHECK(one.panelsVisited == 1); // the row's label: nothing else reads its text
	CHECK(one.panelsApplied == 1);
	CHECK(built.text(42, 3, 17) == "changed");

	const auto& none = built.run("spare", json::object());
	CHECK_FALSE(none.full);
	CHECK(none.panelsVisited == 0);

	const auto& title = built.run("title", {{"s", 9}, {"v", "new"}});
	CHECK_FALSE(title.full);
	CHECK(title.panelsVisited == 1);
	CHECK(built.errors.empty());
}

TEST_CASE("update index: a reader whose parent's list is built again is updated once, or destroyed") {
	affresco::test::verifyNextRuntimes(true);
	Built built;
	affresco::test::verifyNextRuntimes(false);

	// The row changed, and one before it removed: the list keeps it by its key, and builds it again or keeps it
	const auto& shifted = built.run("textAndRemove", {{"s", 5}, {"g", 2}, {"r", 10}, {"v", "kept"}, {"at", 3}});
	CHECK_FALSE(shifted.full);
	CHECK(shifted.panelsApplied <= static_cast<std::size_t>(2 * rows)); // that group's rows, at most
	CHECK(built.text(5, 2, 9) == "kept");

	// The row changed and removed: its label's reader is never run
	const auto& gone = built.run("textAndRemove", {{"s", 6}, {"g", 1}, {"r", 4}, {"v", "gone"}, {"at", 4}});
	CHECK_FALSE(gone.full);
	CHECK(gone.panelsApplied <= static_cast<std::size_t>(2 * rows));
	CHECK(built.text(6, 1, 4) == "t5");
	CHECK(built.errors.empty()); // verify mode found the panels a full pass gives
}
