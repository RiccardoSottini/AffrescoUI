/// @file
/// @brief Every event and every update has a work budget (steps, expression operations, bytes written into the
/// state), and revisions, documents and states have sizes they can't pass (SECURITY.md, "Limits").

#include "Expression.hpp"
#include "Headless.hpp"
#include "Limits.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Collects the errors the library logs while it lives.
class Errors {
public:
	/// @brief Starts collecting.
	Errors() {
		affresco::setLogSink([this](affresco::LogLevel level, std::string_view message) {
			if(level == affresco::LogLevel::Error) lines.emplace_back(message);
		});
	}
	/// @brief Gives the log back to stderr.
	~Errors() { affresco::setLogSink(nullptr); }
	Errors(const Errors&) = delete;
	Errors& operator=(const Errors&) = delete;
	Errors(Errors&&) = delete;
	Errors& operator=(Errors&&) = delete;

	std::vector<std::string> lines; ///< The errors, oldest first.
};

/// @brief A screen with actions, built on a headless window.
struct Built {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Builds a screen.
	/// @param actions Its "actions".
	/// @param root Its root's children.
	explicit Built(const char* actions, const char* root = "[]")
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(std::make_unique<affresco::Screen>(affresco::Screen::fromString(
	          std::string(R"({"version": 2, "state": {"count": 0, "text": "ab"}, "actions": )") + actions +
	          R"(, "root": {"id": "root", "children": )" + root + "}}"))) {
		screen->build(*window);
	}
	/// @brief Runs an action and the update it asks for.
	/// @param action The action.
	void run(std::string_view action) const {
		screen->runAction(action, window->root());
		window->render();
	}
	/// @brief A state value.
	/// @param key Its key.
	/// @return The value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen->stateJson())[key]; }
};

} // namespace

TEST_CASE("budget: an event that runs too many steps stops, says where, and the screen goes on") {
	const Errors errors;
	const Built built(R"({
		"spin": [{"while": "{{true}}", "do": [{"forEach": "{{range(100)}}", "do": [{"increment": "state.count"}]}]}],
		"once": [{"set": "state.text", "to": "done"}]
	})");
	built.run("spin");
	REQUIRE(errors.lines.size() == 1);
	CHECK(errors.lines[0].find("actions.spin[0]") != std::string::npos);
	CHECK(errors.lines[0].find("100000 steps, its work budget") != std::string::npos);
	// The budget counts steps, not time: the loop ran, then stopped before the limit.
	CHECK(built.state("count").get<int>() > 0);
	CHECK(built.state("count").get<int>() < static_cast<int>(affresco::format::stepsPerEvent));

	built.run("once"); // the next event has a budget of its own
	CHECK(built.state("text") == "done");
	CHECK(errors.lines.size() == 1);
}

// Found by fuzzing (tests/fuzz, actions): a forEach with no "do" inside a while ran turns that spent nothing
TEST_CASE("budget: a loop's turns spend from it, even with nothing to do") {
	const Errors errors;
	// 10000 turns of a 10000-item list: 10^8 turns that ran no step, before each turn spent one.
	const Built built(R"({
		"fill": [{"set": "state.items", "to": "{{range(10000)}}"}],
		"spin": [{"while": "{{true}}", "do": [{"forEach": "state.items"}]}]
	})");
	built.run("fill");
	built.run("spin");
	REQUIRE(errors.lines.size() == 1);
	CHECK(errors.lines[0].find("actions.spin[0].do[0].forEach") != std::string::npos);
	CHECK(errors.lines[0].find("100000 steps, its work budget") != std::string::npos);
}

TEST_CASE("budget: an expression that does too much stops at its place") {
	const Errors errors;
	const Built built(R"({"heavy": [{"set": "state.count", "to": "{{sum(range(10000), sum(range(10000), it))}}"}]})");
	built.run("heavy");
	REQUIRE(errors.lines.size() == 1);
	CHECK(errors.lines[0].find("actions.heavy[0].to") != std::string::npos);
	CHECK(errors.lines[0].find("10000000 operations, its work budget") != std::string::npos);
	CHECK(built.state("count") == 0);
}

TEST_CASE("budget: an event can't write more than its bytes into the state") {
	const Errors errors;
	const Built built(R"({"grow": [{"while": "{{true}}", "do": [{"set": "state.text", "to": "{{state.text + state.text}}"}]}]})");
	built.run("grow");
	REQUIRE(errors.lines.size() == 1);
	CHECK(errors.lines[0].find("actions.grow[0].do[0].to") != std::string::npos);
	CHECK(errors.lines[0].find("64 MiB into the state") != std::string::npos);
	CHECK(built.state("text").get<std::string>().size() <= affresco::format::bytesPerEvent);
}

TEST_CASE("budget: an update has a budget too, so a binding can't freeze the screen") {
	const Errors errors;
	CHECK_THROWS_WITH_AS(
	    Built("{}", R"([{"id": "slow", "type": "label", "text": "{{sum(range(10000), sum(range(10000), it))}}"}])"),
	    doctest::Contains("root.children[0].text"), affresco::DocumentError);
}

TEST_CASE("budget: a drag's data has a budget too, so dragging can't freeze the screen") {
	const Errors errors;
	const Built built("{}", R"([{"id": "source", "width": 100, "height": 100,
		"drag": {"data": "{{sum(range(10000), sum(range(10000), it))}}"}}])");
	built.window->render();
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {10.0f, 10.0f}});
	built.window->routeMouseMove({60.0f, 60.0f});
	REQUIRE(errors.lines.size() == 1);
	CHECK(errors.lines[0].find("root.children[0].drag") != std::string::npos);
	CHECK(errors.lines[0].find("10000000 operations, its work budget") != std::string::npos);
}

TEST_CASE("budget: the place a caret is written to has a budget too") {
	const Errors errors;
	const Built built("{}", R"([{"id": "field", "type": "editor", "width": 200, "height": 30,
		"caret": "state[sum(range(10000), sum(range(10000), it)) > 0 ? 'text' : 'count']"}])");
	built.window->render();
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {10.0f, 10.0f}});
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {10.0f, 10.0f}});
	built.window->routeChar({U'x'});
	REQUIRE(!errors.lines.empty());
	CHECK(errors.lines[0].find("root.children[0].caret") != std::string::npos);
	CHECK(errors.lines[0].find("10000000 operations, its work budget") != std::string::npos);
	CHECK(built.state("text") == "ab");
}

TEST_CASE("limits: a revision, a document and a state have sizes they can't pass") {
	using namespace affresco::format;
	const Rules rules;

	json many = json::array();
	for(std::size_t i = 0; i <= maxRevisionOperations; ++i) many.push_back({{"op", "test"}, {"path", ""}});
	CHECK_THROWS_WITH_AS(rules.checkRevision(many), doctest::Contains("100001 operations"), affresco::DocumentError);
	const json large = json::array({{{"op", "add"}, {"path", "/state/x"}, {"value", std::string(maxRevisionBytes, 'a')}}});
	CHECK_THROWS_WITH_AS(rules.checkRevision(large), doctest::Contains("16 MiB"), affresco::DocumentError);
	CHECK_NOTHROW(rules.checkRevision(json::array({{{"op", "add"}, {"path", "/state/x"}, {"value", 1}}})));

	json document = {{"version", 2}, {"root", {{"id", "root"}}}, {"state", json::object()}};
	CHECK_NOTHROW(rules.validate(document));
	document["note"] = std::string(maxDocumentBytes, 'a');
	CHECK_THROWS_WITH_AS(rules.validate(document), doctest::Contains("larger than 32 MiB"), affresco::DocumentError);
	document.erase("note");
	document["state"]["big"] = std::string(maxStateBytes, 'a');
	CHECK_THROWS_WITH_AS(rules.validate(document), doctest::Contains("state: larger than 32 MiB"),
	                     affresco::DocumentError);

	const json before = {{"version", 2}, {"root", {{"id", "root"}}}, {"state", {{"big", ""}}}};
	json after = before;
	after["state"]["big"] = std::string(maxStateBytes, 'a');
	CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains("the state would be larger"),
	                     affresco::DocumentError);
}

TEST_CASE("limits: a value's size is measured as its JSON") {
	using affresco::format::approximateBytes;
	const json value = json::parse(R"({"a": [1, 22, "xyz"], "b": null, "c": false, "d": 1.5})");
	CHECK(approximateBytes(value) == value.dump().size());
	CHECK(approximateBytes(json(std::string(100, 'a')), 10) > 10);
}
