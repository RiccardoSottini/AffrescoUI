/// @file
/// @brief The quick pick (src/ExpressionFunctionsFuzzy.cpp, src/BuiltInComponents.cpp): fuzzy matching with its
/// highlight ranges, fuzzySort's order, key and limit, and the quickPick component's providers, recent items, arrow
/// keys and Enter.

#include "Expression.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Evaluates an expression over a state.
/// @param text The `{{...}}` text.
/// @param state The state.
/// @return The result.
json evaluate(const std::string& text, const json& state = json::object()) {
	static const json window = {{"width", 800}, {"height", 600}};
	static const json revision = 1, event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{nullptr, nullptr, json::object(), {}, {}};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate(text, context, "test");
}

/// @brief Presses and releases a key.
/// @param window The window.
/// @param key The key.
void press(affresco::Window& window, affresco::Key key) {
	window.routeKey({key, 0, affresco::Action::Press, {0}});
	window.routeKey({key, 0, affresco::Action::Release, {0}});
}

/// @brief The screen's state.
/// @param screen The screen.
/// @return It.
json state(const affresco::Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief A screen with a quick pick over files (the default provider) and commands (after ">").
constexpr const char* pickScreen = R"json({
	"format": "affrescoui-screen", "version": 1,
	"state": { "quickPick": { "open": true }, "opened": null, "ran": null,
	           "files": [{ "title": "main.cpp", "detail": "src" }, { "title": "Makefile" },
	                     { "title": "README.md" }, { "title": "mouse.cpp" }] },
	"actions": {
		"open": [{ "set": "state.opened", "to": "{{item.title}}" }],
		"save": [{ "set": "state.ran", "to": "save" }],
		"query": [{ "set": "state.quickPick.query", "to": "{{event.q}}" }],
		"show": [{ "set": "state.quickPick.open", "to": true }]
	},
	"root": { "id": "root", "children": [
		{ "id": "pick", "use": "quickPick", "with": { "providers": [
			{ "items": "{{state.files}}", "run": "open" },
			{ "prefix": ">", "items": [{ "title": "Save", "run": "save" }], "placeholder": "Run a command" }
		] } }
	] }
})json";

} // namespace

TEST_CASE("format: fuzzy matches the query's characters in order and gives the ranges to highlight") {
	const json match = evaluate("{{fuzzy('mcp', 'main.cpp')}}");
	REQUIRE(match.is_object());
	CHECK(match["ranges"] == json::parse("[[0, 1], [5, 7]]")); // m, then "cp": prefers the start of a word
	CHECK(match["parts"] == json::parse(R"([{"text": "m", "match": true}, {"text": "ain.", "match": false},
		{"text": "cp", "match": true}, {"text": "p", "match": false}])"));
	CHECK(evaluate("{{fuzzy('MAIN', 'main.cpp')}}").is_object()); // any case
	CHECK(evaluate("{{fuzzy('pcm', 'main.cpp')}}").is_null());    // out of order
	CHECK(evaluate("{{fuzzy('', 'abc').score}}") == 0);
	CHECK(evaluate("{{fuzzy('é', 'café').ranges}}") == json::parse("[[3, 4]]")); // characters, not bytes
	CHECK(evaluate("{{fuzzy('ab', 'xaxb').score < fuzzy('ab', 'xabx').score}}") == true); // together is better
}

TEST_CASE("format: fuzzySort keeps the matches, the best first, on a key, up to a limit") {
	const json files = json::parse(R"({"files": [{"name": "src/main.cpp"}, {"name": "Makefile"},
		{"name": "main.cpp"}, {"name": "notes.txt"}, {"name": 3}]})");
	const json sorted = evaluate("{{map(fuzzySort(state.files, 'main', 'name'), it.item.name)}}", files);
	CHECK(sorted == json::parse(R"(["main.cpp", "src/main.cpp"])")); // a prefix first, then the shorter
	CHECK(evaluate("{{map(fuzzySort(state.files, '', 'name'), it.index)}}", files) == json::parse("[0, 1, 2, 3]"));
	CHECK(evaluate("{{len(fuzzySort(state.files, 'm', 'name', 1))}}", files) == 1);
	CHECK(evaluate("{{fuzzySort(['beta', 'alpha'], 'a')[0].item}}") == "alpha"); // texts: the item itself
	CHECK(evaluate("{{fuzzySort(null, 'a')}}") == json::array());
	CHECK_THROWS_AS((void)evaluate("{{fuzzySort(3, 'a')}}"), affresco::DocumentError);
}

TEST_CASE("format: the quick pick filters, moves with the arrows, runs with Enter and remembers what was chosen") {
	ManualClock clock;
	const auto window = createHeadlessWindow(800, 600, clock);
	affresco::Screen screen = affresco::Screen::fromString(pickScreen);
	screen.build(*window);
	window->render();
	affresco::Panel* root = screen.find("root");
	REQUIRE(root);

	screen.runAction("query", *root, R"({"q": "mcp"})"); // main.cpp, then mouse.cpp
	window->render();
	press(*window, affresco::Key::Down);
	window->render();
	CHECK(state(screen)["quickPick"]["index"] == 1);
	press(*window, affresco::Key::Enter);
	window->render();
	json now = state(screen);
	CHECK(now["opened"] == "mouse.cpp"); // the provider's run, with the item
	CHECK(now["quickPick"]["open"] == false);
	CHECK(now["quickPick"]["query"] == "");
	CHECK(now["quickPick"]["recent"][0]["title"] == "mouse.cpp");

	screen.runAction("show", *root);
	window->render();
	press(*window, affresco::Key::Enter); // an empty query: the recent item comes first
	window->render();
	CHECK(state(screen)["opened"] == "mouse.cpp");

	screen.runAction("show", *root);
	screen.runAction("query", *root, R"({"q": ">sav"})"); // the commands' provider: the item's own run
	window->render();
	press(*window, affresco::Key::Enter);
	window->render();
	now = state(screen);
	CHECK(now["ran"] == "save");
	CHECK(now["quickPick"]["recent"].size() == 1); // only the default provider's choices are remembered
}
