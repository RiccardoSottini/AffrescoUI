/// @file
/// @brief Unit tests for the format module's checker (src/Checker.hpp): what it finds and where, what it lets
/// through, the revision policy in format::Rules, and the shipped templates, which must have no findings.

#include "Checker.hpp"
#include "Expression.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::format::Finding;

namespace {

/// @brief One row of a table: a screen, and what the checker finds in it.
struct Case {
	const char* what;               ///< What the row shows.
	const char* sections;           ///< The screen's sections, without "version" (added).
	std::vector<std::string> found; ///< The findings, one line each: "<place>: <problem> (<hint>)", sorted by place.
};

/// @brief Makes a screen from its sections.
/// @param sections JSON text of an object, e.g. `"state": {...}, "root": {...}`.
/// @return The document.
json screen(const std::string& sections) { return json::parse("{\"version\": 1, " + sections + "}"); }

/// @brief Checks a document against its own state.
/// @param document The document.
/// @param nativeActions Actions the application registered.
/// @return Each finding as one line.
std::vector<std::string> lines(const json& document, const std::set<std::string, std::less<>>& nativeActions = {}) {
	const auto found = affresco::format::check(document, document.value("state", json::object()), nativeActions);
	REQUIRE(found);
	std::vector<std::string> result;
	if(!found) return result; // never: REQUIRE has stopped the test, which clang-tidy can't tell
	for(const Finding& finding : *found) result.push_back(finding.text());
	return result;
}

/// @brief Checks each row of a table.
/// @param cases The rows.
void checkAll(const std::vector<Case>& cases) {
	for(const Case& row : cases) {
		const std::string what = row.what;
		CAPTURE(what);
		const std::vector<std::string> found = lines(screen(row.sections));
		if(found != row.found)
			for(const std::string& line : found) MESSAGE("found: " << line);
		CHECK(found == row.found);
	}
}

/// @brief Reads a template from examples/assets/screens.
/// @param name E.g. "kanban".
/// @return The document.
json screenTemplate(const std::string& name) {
	const std::ifstream file(std::string(AFFRESCOUI_SCREENS_DIRECTORY) + "/" + name + ".screen.json");
	REQUIRE(file);
	std::stringstream content;
	content << file.rdbuf();
	return json::parse(content.str());
}

/// The templates in examples/assets/screens.
constexpr auto templates = std::to_array<const char*>(
    {"kanban", "smart-home", "music-player", "code-editor", "habit-tracker", "sign-in", "studio"});

} // namespace

TEST_CASE("checker: the templates have no findings") {
	for(const char* name : templates) {
		CAPTURE(name);
		const std::vector<std::string> found = lines(screenTemplate(name));
		for(const std::string& line : found) MESSAGE(line);
		CHECK(found.empty());
	}
}

TEST_CASE("checker: typos planted in the Kanban template are each found, with their place") {
	std::string text = screenTemplate("kanban").dump();
	const std::vector<std::pair<std::string, std::string>> typos = {
	    {"{{card.title}}", "{{card.titel}}"},
	    {R"("@accentSoft")", R"("@accentSofty")"},
	    {R"("onSubmit":"addCard")", R"("onSubmit":"addCrad")"},
	    {R"({"by":-1})", R"({"bye":-1})"},
	    {"to < 0 ||", "too < 0 ||"},
	    {"computed.lastColumn}}", "computed.lastColum}}"},
	    {"{{member.name}}", "{{member.nmae}}"},
	    {R"("style":"caption")", R"("style":"captoin")"}};
	for(const auto& [from, to] : typos) {
		const auto at = text.find(from);
		REQUIRE(at != std::string::npos);
		text.replace(at, from.size(), to);
	}
	const std::string card = "root.children[2].children[0].children[0].children[3].children[0]";
	const std::string header = "root.children[0].children[3].children[0]";
	CHECK(lines(json::parse(text)) ==
	      std::vector<std::string>{
	          "actions.moveCard[1].if: 'computed.lastColum' doesn't exist (did you mean 'computed.lastColumn'?)",
	          "actions.moveCard[1].if: unknown variable 'too' (did you mean 'to'?)",
	          header + ".children[0].style: unknown style 'captoin' (did you mean 'caption'?)",
	          header + ".children[1].children[0].children[1].text: 'member.nmae' doesn't exist (did you mean "
	                   "'member.name'?)",
	          "root.children[1].children[1].onSubmit: unknown action 'addCrad' (did you mean 'addCard'?)",
	          card + ".children[2].text: 'card.titel' doesn't exist (did you mean 'card.title'?)",
	          card + ".children[4].with.bye: unknown parameter 'bye' of component 'moveButton' (did you mean 'by'?)",
	          card + ".dragging.color: unknown color '@accentSofty' (did you mean '@accentSoft'?)"});
}

TEST_CASE("checker: state paths") {
	checkAll({
	    {"a missing key",
		 R"("state": {"cards": []}, "root": {"text": "{{state.cardz}}"})",
		 {"root.text: 'state.cardz' doesn't exist (did you mean 'state.cards'?)"}},
	    {"a missing key inside an object, with what it has",
		 R"("state": {"user": {"name": "Ada", "email": ""}}, "root": {"text": "{{state.user.phone}}"})",
		 {"root.text: 'state.user.phone' doesn't exist (state.user has name, email)"}},
	    {"through an index an expression gives",
		 R"("state": {"cards": [{"title": "a"}], "current": 0}, "root": {"text": "{{state.cards[state.current].titel}}"})",
		 {"root.text: 'state.cards[state.current].titel' doesn't exist (did you mean "
		  "'state.cards[state.current].title'?)"}},
	    {"a list and a single value have no keys, only a length",
		 R"("state": {"cards": [], "name": ""}, "root": {"text": "{{len(state.cards)}} {{state.cards.length}} {{state.name.length}} {{state.cards.first}} {{state.name.first}}"})",
		 {"root.text: 'state.cards.first' doesn't exist (state.cards is a list)",
		  "root.text: 'state.name.first' doesn't exist (state.name is a single value)"}},
	    {"a key a step creates may be read",
		 R"("actions": {"start": [{"set": "state.draft", "to": ""}, {"set": "state.user.name", "to": "Ada"}]},
	        "root": {"text": "{{state.draft}} {{state.user.name}} {{state.user.email}}"})",
		 {"root.text: 'state.user.email' doesn't exist (state.user has name)"}},
	    {"what a step pushes describes a list's items",
		 R"("state": {"todos": []}, "actions": {"add": {"push": "state.todos", "value": {"title": "{{state.draft}}", "done": false}}},
	        "root": {"children": [{"repeat": {"for": "todo", "in": "state.todos"}, "text": "{{todo.title}} {{todo.don}}"}]})",
		 {"actions.add.value.title: 'state.draft' doesn't exist (state has todos)",
		  "root.children[0].text: 'todo.don' doesn't exist (did you mean 'todo.done'?)"}},
	    {"bindings and an editor's caret create what they write",
		 R"("root": {"children": [{"type": "textarea", "bind": "state.query"}, {"type": "editor", "caret": "state.caret"},
	        {"text": "{{state.query}} {{state.caret.line}} {{state.caret.lin}}"}]})",
		 {"root.children[2].text: 'state.caret.lin' doesn't exist (did you mean 'state.caret.line'?)"}},
	    {"computed values exist by name, with their own shape",
		 R"("state": {"cards": [{"title": "a"}]}, "computed": {"first": "{{state.cards[0]}}"},
	        "root": {"text": "{{computed.first.title}} {{computed.frist}} {{computed.first.titel}}"})",
		 {"root.text: 'computed.first.titel' doesn't exist (did you mean 'computed.first.title'?)",
		  "root.text: 'computed.frist' doesn't exist (did you mean 'computed.first'?)"}},
	    {"text paths: watch, forEach and overlays (with dismiss, an overlay writes its path, like a bind)",
		 R"("state": {"open": false, "items": []},
	        "watch": {"state.opne": "go"}, "actions": {"go": {"forEach": "state.itmes", "do": []}},
	        "root": {"children": [{"overlay": {"open": "state.opn"}}, {"overlay": {"open": "state.menu", "dismiss": "outside"}}]})",
		 {"actions.go.forEach: 'state.itmes' doesn't exist (did you mean 'state.items'?)",
		  "root.children[0].overlay.open: 'state.opn' doesn't exist (did you mean 'state.open'?)",
		  "watch.state.opne: 'state.opne' doesn't exist (did you mean 'state.open'?)"}},
	});
}

TEST_CASE("checker: variables in scope") {
	checkAll({
	    {"an unknown variable",
		 R"("root": {"text": "{{stat.x}} {{nodes.side.width}} {{event.x}} {{window.width}}"})",
		 {"root.text: unknown variable 'stat' (did you mean 'state'?)"}},
	    {"repeat, with its index; let and with in order",
		 R"("state": {"cards": [{"title": "a"}]},
	        "root": {"children": [{"repeat": {"for": "card", "in": "state.cards", "index": "i"},
	          "with": {"big": "{{card.title}}"}, "let": {"first": "{{i == 0}}", "label": "{{big}}{{first}}"},
	          "text": "{{label}} {{item}}"}]})",
		 {"root.children[0].text: unknown variable 'item'"}},
	    {"a variable from one node isn't seen by its sibling",
		 R"("state": {"cards": []},
	        "root": {"children": [{"repeat": {"for": "card", "in": "state.cards"}}, {"text": "{{card}}"}]})",
		 {"root.children[1].text: unknown variable 'card'"}},
	    {"check gives value, valid and error",
		 R"("state": {"email": ""},
	        "root": {"children": [{"type": "textarea", "bind": "state.email", "check": {"rule": "{{len(value) > 3}}"},
	          "children": [{"text": "{{valid ? '' : error}}"}]}]})",
		 {}},
	    {"list functions see it and index",
		 R"("state": {"cards": [{"done": true}]},
	        "root": {"text": "{{count(state.cards, it.done)}} {{len(filter(state.cards, it.don))}}"})",
		 {"root.text: 'it.don' doesn't exist (did you mean 'it.done'?)"}},
	    {"forEach, run with, and an action that reads its caller's variables",
		 R"("state": {"cards": [{"title": "a"}]}, "actions": {"rename": [{"set": "state.last", "to": "{{card.title}} {{to}}"}]},
	        "root": {"onClick": [{"forEach": "state.cards", "as": "card", "do": [{"run": "rename", "with": {"to": "x"}}]},
	                             {"run": "rename"}]})",
		 {"actions.rename[0].to: unknown variable 'card'", "actions.rename[0].to: unknown variable 'to'"}},
	    {"a native call's error",
		 R"("root": {"onClick": {"call": "pick", "into": "state.file", "onError": [{"log": "{{error}} {{state.file.name}}"}]}})",
		 {}},
	});
}

TEST_CASE("checker: references to what a section defines") {
	checkAll({
	    {"actions: handlers, run, and names an expression computes (skipped)",
		 R"("actions": {"save": []}, "keys": {"mod+s": "sav"},
	        "root": {"onClick": [{"run": "svae"}, {"run": "{{state.which}}"}], "onHover": "save"})",
		 {"keys.mod+s: unknown action 'sav' (did you mean 'save'?)",
		  "root.onClick[0].run: unknown action 'svae' (did you mean 'save'?)",
		  "root.onClick[1].run: 'state.which' doesn't exist (state has none)"}},
	    {"styles: one, a list, and the names an expression may choose",
		 R"("styles": {"chip": {"color": "@paper"}, "chipOn": {"style": "chp"}}, "colors": {"paper": "#fff"},
	        "root": {"children": [{"style": ["chip", "chap"]}, {"style": "{{state.on ? 'chipOn' : 'chipOff'}}"}]})",
		 {"root.children[0].style: unknown style 'chap' (did you mean 'chip'?)",
		  "root.children[1].style: 'state.on' doesn't exist (state has none)",
		  "root.children[1].style: unknown style 'chipOff' (did you mean 'chipOn'?)",
		  "styles.chipOn.style: unknown style 'chp' (did you mean 'chip'?)"}},
	    {"fonts and colours, in visual states too; built names are skipped",
		 R"("fonts": {"small": {"file": "a.ttf", "size": 12}}, "colors": {"ink": "#000", "inkSoft": "#888"},
	        "state": {"tag": "ink"},
	        "root": {"type": "label", "font": "smal", "textColor": "{{state.tag == 'ink' ? '@ink' : '@inc'}}",
	                 "hover": {"color": "@inkSof"}, "color": "@{{state.tag}}Soft"})",
		 {"root.font: unknown font 'smal' (did you mean 'small'?)",
		  "root.hover.color: unknown color '@inkSof' (did you mean '@inkSoft'?)",
		  "root.textColor: unknown color '@inc' (did you mean '@ink'?)"}},
	    {"screens: navigate, but not back",
		 R"("screens": {"home": {}, "settings": {}},
	        "root": {"onClick": [{"navigate": "setings"}, {"navigate": "back"}]})",
		 {"root.onClick[0].navigate: unknown screen 'setings' (did you mean 'settings'?)"}},
	    {"components and their parameters",
		 R"("components": {"chip": {"params": {"label": ""}, "node": {"text": "{{label}}"}}},
	        "root": {"children": [{"use": "chip", "with": {"label": "a", "lable": "b"}}, {"use": "chips"}]})",
		 {"root.children[0].with.lable: unknown parameter 'lable' of component 'chip' (did you mean 'label'?)",
		  "root.children[1].use: unknown component 'chips' (did you mean 'chip'?)"}},
	    {"steps it doesn't know",
		 R"("root": {"onClick": [{"sett": "state.x", "to": 1}, {"note": "fine"}]})",
		 {"root.onClick[0]: unknown step (expected set, push, remove, toggle, increment, if, forEach, while, run, "
		  "call, emit, send, connect, patch, focus, scrollTo, navigate, after, debounce, animate, copy, log, stop, "
		  "note, accept, reject, preview, endPreview, restore, revert or undo)"}},
	    {"expressions that don't parse, and writes outside the state",
		 R"("root": {"text": "{{1 +}}", "onClick": {"set": "card.title", "to": 1}})",
		 {"root.onClick.set: can only change values under 'state', not 'card.title'",
		  "root.text: in {{1 +}}: expected a value (at column 4)"}},
	});
}

TEST_CASE("checker: findings inside styles and components name them, and see the variables of their use") {
	checkAll({
	    {"a style reads its node's variables; its typo is named in the style",
		 R"("state": {"cards": [{"title": "a"}]}, "styles": {"cardText": {"type": "label", "text": "{{card.titel}}"}},
	        "root": {"children": [{"repeat": {"for": "card", "in": "state.cards"}, "style": "cardText"}]})",
		 {"styles.cardText.text: 'card.titel' doesn't exist (did you mean 'card.title'?)"}},
	    {"a component sees its parameters and its caller's variables; a slot gets the use's children",
		 R"("state": {"cards": [{"title": "a"}]},
	        "components": {"frame": {"params": {"label": ""}, "node": {"text": "{{label}} {{c}}", "children": [{"slot": true}]}}},
	        "root": {"children": [{"repeat": {"for": "card", "in": "state.cards", "index": "c"}, "use": "frame",
	          "with": {"label": "{{card.title}}"}, "children": [{"text": "{{label}} {{card.titel}}"}]}]})",
		 {"root.children[0].children[0].text: 'card.titel' doesn't exist (did you mean 'card.title'?)"}},
	    {"a component no node uses is checked with every variable allowed",
		 R"("components": {"badge": {"node": {"text": "{{whatever}} {{state.nope}}", "color": "@nope"}}})",
		 {"components.badge.node.color: unknown color '@nope' (defined: none)",
		  "components.badge.node.text: 'state.nope' doesn't exist (state has none)"}},
	});
}

TEST_CASE("checker: what can't be known is never reported") {
	checkAll({
	    {"events, native results, maps and nulls",
		 R"("state": {"drafts": {}, "picked": null},
	        "actions": {"pick": {"call": "pickFile", "into": "state.file"}},
	        "root": {"onDrop": {"set": "state.last", "to": "{{event.data.whatever}}"},
	                 "text": "{{state.file.path}} {{state.drafts.x.y}} {{state.picked.name}} {{state.last.z}}"})",
		 {}},
	    {"a value whose type a step changes",
		 R"("state": {"selected": ""},
	        "actions": {"choose": {"set": "state.selected", "to": {"name": "a"}}}, "root": {"text": "{{state.selected.name}}"})",
		 {}},
	    {"an action nothing runs (a channel's, a native caller's) may read any variable",
		 R"("actions": {"host:committed": [{"set": "state.message", "to": "{{event.message}} {{caller}}"}]})",
		 {}},
	});
}

TEST_CASE("checker: native actions are known by name") {
	const json document = screen(R"("root": {"onClick": "quit"})");
	CHECK(lines(document) == std::vector<std::string>{"root.onClick: unknown action 'quit' (defined: none)"});
	CHECK(lines(document, {"quit"}).empty());
}

TEST_CASE("checker: limits keep a hostile document cheap") {
	// A component that uses itself, and an action that runs itself: each is checked once
	const json recursive = screen(R"("components": {"tree": {"node": {"children": [{"use": "tree"}]}}},
		"actions": {"again": [{"run": "again"}]}, "root": {"children": [{"use": "tree"}], "onClick": "again"})");
	CHECK(lines(recursive).empty());

	// Styles that use each other, and a style an expression chooses that chooses itself
	const json styles = screen(R"("styles": {"a": {"style": "b"}, "b": {"style": "a"}, "c": {"style": "{{'c'}}"}},
		"root": {"children": [{"style": "a"}, {"style": "{{'c'}}"}]})");
	CHECK(lines(styles).empty());

	// Components nested a thousand deep stop at the nesting limit instead of overflowing the stack
	json components = json::object();
	for(int i = 0; i < 1000; ++i)
		components["c" + std::to_string(i)] = {
		    {"node", {{"children", json::array({{{"use", "c" + std::to_string(i + 1)}}})}}}};
	components["c1000"] = {{"node", json::object()}};
	json chain = {{"version", 1}, {"components", components}, {"root", {{"children", json::array({{{"use", "c0"}}})}}}};
	CHECK(affresco::format::check(chain, json::object()));

	// Over a million values: too large to check, so nothing is claimed
	json wide = {{"version", 1}, {"root", {{"text", json::array()}}}};
	wide["root"]["text"].get_ref<json::array_t&>().resize(1100000, json::array()); // numbers cost no visit: lists do
	CHECK_FALSE(affresco::format::check(wide, json::object()));

	// A long name with a typo: hints compare names of a sensible length only, not a square of a hundred thousand
	const std::string longName(100000, 'a');
	const json longTypo = {{"version", 1},
	                       {"state", {{longName, 1}}},
	                       {"root", {{"text", "{{state." + longName + "b}}"}}}};
	const auto found = affresco::format::check(longTypo, longTypo["state"]);
	REQUIRE(found);
	CHECK((found && found->size() == 1));
}

TEST_CASE("checker: computed values that read each other are read safely") {
	// One computed value's shape is still being read while an index reads another, which adds it to the shapes known
	checkAll({{"an index that reads another computed value",
	           R"("computed": {"a": "{{computed.b[computed.c].x}}", "b": "{{[{x: 1}]}}", "c": "{{0}}"},
	            "root": {"text": "{{computed.a}}"})",
	           {}}});

	// Reading a computed value compiles its expression while the reader's is still being walked. A full compile cache
	// is cleared: filled to nine short of its limit, it is cleared at the tenth compile, inside a chain of twenty (the
	// address sanitizer finds a reader's expression read after it was freed). Filling the cache directly keeps the
	// document small: a computed value is found by name in a list, so 40,000 of them took minutes under sanitizers
	affresco::format::clearCompileCache();
	for(std::size_t i = 0; i + 9 < affresco::format::compileCacheLimit; ++i)
		(void)affresco::format::compile("fill + " + std::to_string(i), "test");
	json computed = json::object();
	for(int link = 0; link < 20; ++link) {
		const std::string next = "c" + std::to_string(link + 1);
		computed["c" + std::to_string(link)] = link < 19 ? "{{computed." + next + " + 1}}" : "{{0}}";
	}
	CHECK(lines({{"version", 1}, {"computed", computed}, {"root", {{"text", "{{computed.c0}}"}}}}).empty());
}

TEST_CASE("checker: a finding a change moves is not new; one it adds is") {
	const Finding typo{"root.children[0].text", "'state.cardz' doesn't exist", "did you mean 'state.cards'?"};
	Finding moved = typo;
	moved.place = "root.children[1].text";
	moved.hint = "state has cards, title"; // hints change with the document: they don't count
	const Finding other{"root.children[2].text", "unknown variable 'x'", ""};

	CHECK(affresco::format::introduced({typo}, {typo}).empty());
	CHECK(affresco::format::introduced({typo}, {moved}).empty());
	CHECK(affresco::format::introduced({typo}, {moved, typo}) == std::vector<Finding>{moved}); // a second one
	CHECK(affresco::format::introduced({typo}, {typo, other}) == std::vector<Finding>{other});
	CHECK(affresco::format::introduced({typo, other}, {}).empty());
}

TEST_CASE("rules: a revision that brings in a finding is rejected; one that leaves an old finding is not") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, screen(R"("state": {"cards": [{"title": "a"}]}, "root": {"children": [
	        {"id": "title", "type": "label", "text": "{{state.cards[0].title}}"},
	        {"id": "old", "type": "label", "text": "{{state.olde}}"}]})"));

	const affresco::RevisionResult typo =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "{{state.cards[0].titel}}"}}}});
	CHECK(typo.status == RevisionStatus::Rejected);
	CHECK(typo.error == "AffrescoUI screen: revision: the checker found: "
	                    "root.children[0].text: 'state.cards[0].titel' doesn't exist (did you mean "
	                    "'state.cards[0].title'?)");
	CHECK(repository.head().number == 0);

	// The old finding stays, and moves to another place: it doesn't block a change elsewhere
	const affresco::RevisionResult elsewhere = repository.submit(
	    {{"author", "agent"}, {"add", {{"/root/children/0", {{"type", "label"}, {"text", "Cards"}}}}}});
	CHECK(elsewhere.status == RevisionStatus::Applied);

	// A number too large for its type is rejected with its place (it used to escape as std::out_of_range, and the host
	// ended)
	const affresco::RevisionResult huge =
	    repository.submit({{"author", "agent"}, {"set", {{"#title/text", "{{99999999999999999999}}"}}}});
	CHECK(huge.status == RevisionStatus::Rejected);
	CHECK(huge.error.find("is too large") != std::string::npos);

	// A key a revision adds to the live state may be read in the same revision
	const affresco::RevisionResult added = repository.submit(
	    {{"author", "agent"}, {"set", {{"/state/filter", "all"}, {"#title/text", "{{state.filter}}"}}}});
	CHECK(added.status == RevisionStatus::Applied);
}

TEST_CASE("rules: findings lists one line per finding, with its place") {
	const affresco::format::Rules rules;
	const json document = screen(R"("root": {"onClick": "quit", "text": "{{state.x}}"})");
	CHECK(rules.findings(document, json::object()) ==
	      std::vector<std::string>{"root.onClick: unknown action 'quit' (defined: none)",
	                               "root.text: 'state.x' doesn't exist (state has none)"});
	affresco::format::Rules withNative;
	withNative.addNativeAction("quit");
	CHECK(withNative.findings(document, json::object()).size() == 1);
}

TEST_CASE("checker: checking a template again gives the same findings") {
	// Speed is measured by the benchmarks (7.1), not here: a unit test can't hold a time on every build and machine.
	// A second check reads the compile cache the first one filled, and must find the same
	for(const char* name : templates) {
		CAPTURE(std::string(name));
		const json document = screenTemplate(name);
		const auto first = affresco::format::check(document, document["state"]);
		const auto again = affresco::format::check(document, document["state"]);
		REQUIRE(first);
		REQUIRE(again);
		CHECK(first == again); // both have a value: equal optionals hold equal findings
	}
}
