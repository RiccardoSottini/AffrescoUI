/// @file
/// @brief Unit tests for what a revision can't make the runtime do: recurse without end (computed values reading
/// computed values, a component or a screen inside itself), throw an exception no event handler catches (a step
/// writing through a value that isn't an object or a list), or fill a list with billions of nulls.

#include "Checker.hpp"
#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <limits>
#include <string>

using affresco::json;
using affresco::RevisionStatus;

namespace {

/// @brief A screen with a label that shows a computed value, and actions that write to the state.
constexpr const char* stateScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"state": { "n": 1, "form": { "name": "" }, "items": [], "selected": 0, "count": 1, "by": 1 },
	"computed": { "c0": "{{state.n}}" },
	"actions": {
		"save": [{ "set": "state.form.name", "to": "x" }],
		"pick": [{ "set": "state.items[state.selected]", "to": true }],
		"forget": [{ "remove": "state.items.0" }],
		"count": [{ "increment": "state.count", "by": "{{state.by}}" }],
		"flip": [{ "toggle": "state.form.name.on" }]
	},
	"root": { "type": "panel", "id": "main", "children": [
		{ "type": "label", "id": "out", "text": "{{computed.c0}}" }
	] }
})json";

/// @brief computed values c0 to c<links>, each reading the next, the last reading state.n.
/// @param links How many read another.
/// @return The "computed" section.
json computedChain(int links) {
	json computed = json::object();
	for(int i = 0; i < links; ++i) computed["c" + std::to_string(i)] = "{{computed.c" + std::to_string(i + 1) + "}}";
	computed["c" + std::to_string(links)] = "{{state.n}}";
	return computed;
}

/// @brief A revision from the agent.
/// @param patch Its JSON Patch.
/// @return The revision, as text.
std::string revision(const json& patch) {
	return json{{"author", "agent"}, {"message", "test"}, {"patch", patch}}.dump();
}

} // namespace

// Before, each link was a level of recursion in the checker and in the runtime, and a revision of 20,000 links
// overflowed the stack (SIGSEGV), in Checker::computedShape first
TEST_CASE("format: computed values may read each other 32 levels deep, and a longer chain is refused") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	screen.build(*window);
	window->render();

	const auto chain = [&](int links) {
		return screen.submitRevision(
		    revision({{{"op", "replace"}, {"path", "/computed"}, {"value", computedChain(links)}}}));
	};
	REQUIRE(chain(31).status == RevisionStatus::Applied); // c0 to c31: 32 computed values, one inside the other
	window->render();
	const auto* out = dynamic_cast<const affresco::Label*>(screen.find("out"));
	REQUIRE(out);
	CHECK(out->text() == "1");

	const affresco::RevisionResult deeper = chain(32);
	CHECK(deeper.status == RevisionStatus::Rejected);
	CHECK(deeper.error.find("computed values read each other too deeply (more than 32 levels)") != std::string::npos);
	const affresco::RevisionResult longest = chain(20000);
	CHECK(longest.status == RevisionStatus::Rejected);

	// The checker alone stops too, without reporting it (the runtime does)
	json document = json::parse(stateScreen);
	document["computed"] = computedChain(20000);
	const auto findings = affresco::format::check(document, document["state"]);
	REQUIRE(findings);
	CHECK(findings->empty());
}
