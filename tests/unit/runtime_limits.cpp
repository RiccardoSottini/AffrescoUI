/// @file
/// @brief Unit tests for what a revision can't make the runtime do: recurse without end (computed values reading
/// computed values, a component or a screen inside itself), build as many panels as nested components and repeats
/// multiply to, throw an exception no event handler catches (a step writing through a value that isn't an object or a
/// list), or fill a list with billions of nulls.

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
	"version": 2,
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

/// @brief The messages of the errors a peer heard.
/// @param peer The peer.
/// @return Their messages, oldest first.
std::vector<std::string> errorsOf(const affresco::test::MemoryChannel& peer) {
	std::vector<std::string> errors;
	for(const json& message : peer.sent)
		if(message.value("type", "") == "error") errors.push_back(message.value("message", ""));
	return errors;
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
	CHECK((findings && findings->empty()));
}

// Before, components and repeats inside each other built as many panels as they multiplied to: 2^25, or 100 million
TEST_CASE("format: a screen builds at most maxPanels panels, however its components and repeats multiply") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	screen.build(*window);
	window->render();

	// A component that uses another twice, 18 levels down: half a million panels
	json components = {{"c0", {{"node", {{"type", "label"}, {"text", "x"}}}}}};
	for(int i = 1; i <= 18; ++i) {
		const std::string inner = "c" + std::to_string(i - 1);
		components["c" + std::to_string(i)] = {
		    {"node", {{"type", "panel"}, {"children", {{{"use", inner}}, {{"use", inner}}}}}}};
	}
	const affresco::RevisionResult doubled = screen.submitRevision(revision(json::array({
	    {{"op", "add"}, {"path", "/components"}, {"value", components}},
	    {{"op", "add"}, {"path", "/root/children/-"}, {"value", {{"id", "doubled"}, {"use", "c18"}}}},
	})));
	CHECK(doubled.status == RevisionStatus::Rejected);
	CHECK(doubled.error.ends_with("more than 250000 panels (components or repeats inside each other multiply)"));
	CHECK(screen.find("doubled") == nullptr);

	// A repeat inside another: 300,000 labels
	const affresco::RevisionResult repeated = screen.submitRevision(revision(json::parse(R"([
		{"op": "add", "path": "/root/children/-", "value": {"id": "rows", "type": "panel", "children": [
			{"type": "panel", "repeat": {"in": "{{range(1000)}}"}, "children": [
				{"type": "label", "repeat": {"in": "{{range(300)}}"}, "text": "x"}]}]}}
	])")));
	CHECK(repeated.status == RevisionStatus::Rejected);
	CHECK(repeated.error.find("more than 250000 panels") != std::string::npos);

	// Fewer is built
	const affresco::RevisionResult fewer = screen.submitRevision(revision(json::parse(R"([
		{"op": "add", "path": "/root/children/-", "value": {"id": "rows", "type": "panel", "children": [
			{"type": "panel", "repeat": {"in": "{{range(100)}}"}, "children": [
				{"type": "label", "repeat": {"in": "{{range(100)}}"}, "text": "x"}]}]}}
	])")));
	CHECK(fewer.status == RevisionStatus::Applied);
	REQUIRE(screen.find("rows") != nullptr);
	CHECK(screen.find("rows")->children().size() == 100);
}

// Before, the reconciler built the panels of a component inside itself without end, and overflowed the stack
TEST_CASE("format: a component or a screen inside itself is refused, not built without end") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	screen.build(*window);
	window->render();

	const affresco::RevisionResult component = screen.submitRevision(revision(json::parse(R"([
		{"op": "add", "path": "/components", "value": {"loop": {"node": {"type": "panel", "children": [{"use": "loop"}]}}}},
		{"op": "add", "path": "/root/children/-", "value": {"id": "looping", "use": "loop"}}
	])")));
	CHECK(component.status == RevisionStatus::Rejected);
	CHECK(component.error.ends_with("nodes nested too deeply (more than 256 levels: a component or a screen inside "
	                                "itself?)"));

	const affresco::RevisionResult outlet = screen.submitRevision(revision(json::parse(R"([
		{"op": "add", "path": "/screens", "value": {"home": {"id": "home", "children": [{"id": "inner", "type": "outlet"}]}}},
		{"op": "add", "path": "/root/children/-", "value": {"id": "shown", "type": "outlet"}}
	])")));
	CHECK(outlet.status == RevisionStatus::Rejected);
	CHECK(outlet.error.find("nodes nested too deeply") != std::string::npos);

	window->render(); // the screen as it was
	CHECK(screen.revision() == 0);
	CHECK(screen.find("out"));
}

// Before, the JSON exception went out of the step, through the window's input routing, and ended the program
TEST_CASE("format: a step writing through what a revision made a number reports an error instead of throwing") {
	affresco::test::MemoryChannel peer; // before the screen: its session uses it until it goes
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	screen.connect(peer, "watcher");
	affresco::Panel sender;
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});

	REQUIRE(screen.submitRevision(revision({{{"op", "replace"}, {"path", "/state/form"}, {"value", 5}}})).status ==
	        RevisionStatus::Applied);
	CHECK_NOTHROW(screen.runAction("save", sender));
	CHECK_NOTHROW(screen.runAction("flip", sender));
	REQUIRE(
	    screen.submitRevision(revision({{{"op", "replace"}, {"path", "/state/items"}, {"value", {1, 2}}}})).status ==
	    RevisionStatus::Applied);
	CHECK_NOTHROW(screen.runAction("forget", sender));
	CHECK(errorsOf(peer) == std::vector<std::string>{
	                            "AffrescoUI screen: actions.save[0]: can't write 'name' inside a number of the state",
	                            "AffrescoUI screen: actions.flip[0]: can't write 'name' inside a number of the state",
	                            "AffrescoUI screen: actions.forget[0]: to remove an item of a list, name the list and "
	                            "the item's \"at\""});
	CHECK(json::parse(screen.stateJson())["form"] == 5);
	CHECK(json::parse(screen.stateJson())["items"] == json({1, 2}));
	affresco::setLogSink(nullptr);
}

// Before, state.items[2000000000] filled the list with two billion nulls (gigabytes) on the user's click
TEST_CASE("format: a step writes at most maxListGap positions past a list's end") {
	affresco::test::MemoryChannel peer;
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	screen.connect(peer, "watcher");
	affresco::Panel sender;
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});

	const auto select = [&](const json& position) {
		REQUIRE(screen.submitRevision(revision({{{"op", "replace"}, {"path", "/state/selected"}, {"value", position}}}))
		            .status == RevisionStatus::Applied);
		screen.runAction("pick", sender);
		return json::parse(screen.stateJson())["items"];
	};
	CHECK(select(2) == json({nullptr, nullptr, true})); // a small gap is filled with null, as before
	CHECK(select(3 + 10000).size() == 10004);
	CHECK(select(2000000000).size() == 10004);
	CHECK(select(-1).size() == 10004);
	CHECK(errorsOf(peer) == std::vector<std::string>{
	                            "AffrescoUI screen: actions.pick[0]: position 2000000000 is more than 10000 past the "
	                            "end of a list of 10004",
	                            "AffrescoUI screen: actions.pick[0]: position 18446744073709551615 is more than "
	                            "10000 past the end of a list of 10004"});
	affresco::setLogSink(nullptr);
}

// Before, two integers whose sum doesn't fit in 64 bits overflowed (undefined behaviour, which UBSan reports)
TEST_CASE("format: increment gives a double when the integers' sum doesn't fit in one") {
	affresco::Screen screen = affresco::Screen::fromString(stateScreen);
	affresco::Panel sender;
	REQUIRE(screen.submitRevision(revision({{{"op", "replace"}, {"path", "/state/by"}, {"value", 2}}})).status ==
	        RevisionStatus::Applied);
	screen.runAction("count", sender);
	CHECK(json::parse(screen.stateJson())["count"] == 3);

	const std::int64_t largest = std::numeric_limits<std::int64_t>::max();
	REQUIRE(screen.submitRevision(revision({{{"op", "replace"}, {"path", "/state/by"}, {"value", largest}}})).status ==
	        RevisionStatus::Applied);
	screen.runAction("count", sender);
	const json count = json::parse(screen.stateJson())["count"];
	CHECK(count.is_number_float());
	CHECK(count.get<double>() == doctest::Approx(static_cast<double>(largest)));
}
