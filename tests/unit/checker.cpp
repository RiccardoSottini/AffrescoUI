/// @file
/// @brief Unit tests for the format module's checker (src/Checker.hpp): what it finds and where, what it lets
/// through, the revision policy in format::Rules, and the shipped templates, which must have no findings.

#include "Checker.hpp"
#include "Expression.hpp"
#include "Helpers.hpp"
#include "Templates.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

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
json screen(const std::string& sections) { return json::parse("{\"version\": 2, " + sections + "}"); }

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

/// @brief Merges one entry of an included file's section into the screen's, as a screen is loaded: the screen's own
/// entry wins, and "menus" and "strings" merge one level deeper, menu by menu and locale by locale.
/// @param entry The screen's entry (null if it has none), which receives the merge.
/// @param section The section's name.
/// @param part The included file's entry.
void mergeEntry(json& entry, const std::string& section, const json& part) {
	if(entry.is_null()) {
		entry = part;
	} else if((section == "menus" || section == "strings") && entry.is_object() && part.is_object()) {
		for(const auto& [key, value] : part.items())
			if(!entry.contains(key)) entry[key] = value;
	}
}

/// @brief Reads a template from its folder in examples.
/// @param name E.g. "kanban".
/// @return The document.
json screenTemplate(const std::string& name) {
	const std::filesystem::path directory = templateDirectory(AFFRESCOUI_EXAMPLES_DIRECTORY, name);
	const std::ifstream file(directory / "index.screen.json");
	REQUIRE(file);
	std::stringstream content;
	content << file.rdbuf();
	json document = json::parse(content.str());
	// The "include" files are merged when a screen is loaded: each section's entries, the screen's own winning;
	// "persist" lists join.
	if(const auto includes = document.find("include"); includes != document.end()) {
		for(const json& include : *includes) {
			const std::ifstream part(directory / include.get<std::string>());
			REQUIRE(part);
			std::stringstream partContent;
			partContent << part.rdbuf();
			const json sections = json::parse(partContent.str()); // a loop over a temporary's items dangles
			for(const auto& [section, entries] : sections.items())
				if(entries.is_object())
					for(const auto& [key, entry] : entries.items()) mergeEntry(document[section][key], section, entry);
				else if(section == "persist" && entries.is_array())
					for(const json& entry : entries)
						if(std::ranges::find(document[section], entry) == document[section].end())
							document[section].push_back(entry);
		}
		document.erase("include");
	}
	return document;
}

/// The templates (examples/templates.json).
constexpr auto templates = std::to_array<const char*>(
    {"kanban", "smart-home", "music-player", "code-editor", "habit-tracker", "sign-in", "launcher"});

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
	    {"{{state.toast.message}}", "{{state.toast.mesage}}"},
	    {R"("dragging":{"color":"@color.accentSoft"})", R"("dragging":{"color":"@color.accentSofty"})"},
	    {R"("onSubmit":"addCard")", R"("onSubmit":"addCrad")"},
	    {R"("use":"navItem","with":{"icon":"box","text":"Flow"})",
	     R"("use":"navItem","with":{"icno":"box","text":"Flow"})"},
	    {"to < 0 ||", "too < 0 ||"},
	    {"computed.lastColumn}}", "computed.lastColum}}"},
	    {"{{member.name}}", "{{member.nmae}}"},
	    {R"("style":"caption")", R"("style":"captoin")"}};
	for(const auto& [from, to] : typos) {
		const auto at = text.find(from);
		REQUIRE(at != std::string::npos);
		text.replace(at, from.size(), to);
	}
	const std::string drawer = "components.cardDrawer.node.children";
	CHECK(lines(json::parse(text)) ==
	      std::vector<std::string>{
	          "actions.moveCard[1].if: unknown variable 'too' (did you mean 'to'?)",
	          drawer + "[0].children[1].style: unknown style 'captoin' (did you mean 'caption'?)",
	          drawer + "[12].children[0].label: 'member.nmae' doesn't exist (did you mean 'member.name'?)",
	          drawer + "[6].children[2].when: 'computed.lastColum' doesn't exist (did you mean 'computed.lastColumn'?)",
	          "components.sidebar.node.children[3].children[2].with.icno: unknown parameter 'icno' of component 'navItem' "
	          "(did you mean 'icon'?)",
	          "components.swimlanes.node.children[0].children[3].children[0].children[1].children[0].dragging.color: "
	          "unknown color '@color.accentSofty' (did you mean '@color.accentSoft'?)",
	          "root.children[1].children[2].onSubmit: unknown action 'addCrad' (did you mean 'addCard'?)",
	          "root.children[8].with.message: 'state.toast.mesage' doesn't exist (did you mean 'state.toast.message'?)"});
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
		 R"("state": {"cards": [], "name": ""}, "root": {"text": "{{length(state.cards)}} {{state.cards.length}} {{state.name.length}} {{state.cards.first}} {{state.name.first}}"})",
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

TEST_CASE("checker: the images a screen edits, the image step and the raster node") {
	const std::string images = R"("images": {"avatar": {"width": 256, "height": 256, "into": "state.avatar"},
	        "banner": {"width": 600, "height": 200}}, )";
	checkAll({
	    {"an image's snapshot may be read at its into path",
	     R"("images": {"avatar": {"width": 256, "height": 256, "into": "state.avatar"}},
	        "root": {"text": "{{state.avatar.width}} {{state.avatr}}"})",
	     {"root.text: 'state.avatr' doesn't exist (did you mean 'state.avatar'?)"}},
	    {"a step's operation, the keys of its with, and the image it names",
	     (images + R"("actions": {"crop": [{"image": "crop", "of": "avatar", "with": {"x": 0, "y": 0, "widht": 9}},
	        {"image": "zom", "of": "avatar"}, {"image": "fit", "of": "avtar"}, {"image": "fit"},
	        {"image": "{{state.operation}}", "of": "banner"}]}, "state": {"operation": "fit"}, "root": {})")
	         .c_str(),
	     {"actions.crop[0].with.widht: 'crop' doesn't read 'widht' (did you mean 'width'?)",
	      "actions.crop[1].image: unknown image operation 'zom' (did you mean 'zoom'?)",
	      "actions.crop[2].of: unknown image 'avtar' (did you mean 'avatar'?)",
	      "actions.crop[3]: \"of\" names the image when a screen declares more than one"}},
	    {"a step needs an images section; with one image, of is optional",
	     R"("actions": {"fit": [{"image": "fit"}]}, "root": {})",
	     {"actions.fit[0]: an \"image\" step needs an \"images\" section"}},
	    {"a raster node names a declared image",
	     (images + R"("root": {"children": [{"type": "raster", "image": "avatar"}, {"type": "raster", "image": "baner"}]})")
	         .c_str(),
	     {"root.children[1].image: unknown image 'baner' (did you mean 'banner'?)"}},
	});
}

TEST_CASE("checker: the sounds a screen plays and the audio step") {
	const std::string sounds = R"("audio": {"song": {"file": "song.mp3", "into": "state.song"},
	        "beep": {"tone": {"frequency": 880, "seconds": 0.2}}}, )";
	checkAll({
	    {"a sound's state may be read at its into path",
	     R"("audio": {"song": {"file": "song.mp3", "into": "state.song"}},
	        "root": {"text": "{{state.song.position}} {{state.sng}}"})",
	     {"root.text: 'state.sng' doesn't exist (did you mean 'state.song'?)"}},
	    {"a step's operation and the sound it names",
	     (sounds + R"("actions": {"go": [{"audio": "play", "of": "song"}, {"audio": "paly", "of": "song"},
	        {"audio": "stop", "of": "sng"}, {"audio": "pause"}, {"audio": "master", "to": 0.5}]}, "root": {})")
	         .c_str(),
	     {"actions.go[1].audio: unknown audio operation 'paly' (did you mean 'play'?)",
	      "actions.go[2].of: unknown sound 'sng' (did you mean 'song'?)",
	      "actions.go[3]: \"of\" names the sound when a screen declares more than one"}},
	    {"a step needs an audio section",
	     R"("actions": {"go": [{"audio": "play"}]}, "root": {})",
	     {"actions.go[0]: an \"audio\" step needs an \"audio\" section"}},
	});
}

TEST_CASE("checker: paths nested past the expression limit are findings, not a crash") {
	std::string deep;
	for(int i = 0; i < 20000; ++i) deep += "state.items[";
	deep += "0" + std::string(20000, ']');
	json document = screen(R"("state": {"items": [0]}, "actions": {"go": {"set": "", "to": 1}}, "root": {})");
	document["root"]["text"] = "{{" + deep + "}}";
	document["actions"]["go"]["set"] = deep;
	const std::vector<std::string> found = lines(document);
	REQUIRE(found.size() == 3); // the text, the step's target, and the action that holds it
	for(const std::string& line : found) CHECK(line.find("nested too deeply") != std::string::npos);
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
	        "root": {"children": [{"type": "textarea", "bind": "state.email", "check": {"rule": "{{length(value) > 3}}"},
	          "children": [{"text": "{{valid ? '' : error}}"}]}]})",
		 {}},
	    {"list functions see it and index",
		 R"("state": {"cards": [{"done": true}]},
	        "root": {"text": "{{count(state.cards, it.done)}} {{length(filter(state.cards, it.don))}}"})",
		 {"root.text: 'it.don' doesn't exist (did you mean 'it.done'?)"}},
	    {"folds see acc too, and only folds",
		 R"("state": {"days": [1, 0]},
	        "root": {"text": "{{reduce(state.days, acc + it, 0)}} {{scan(state.days, map([it], acc), 0)}} {{acc}}"})",
		 {"root.text: unknown variable 'acc'"}},
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
		  "call, emit, send, connect, patch, focus, scrollTo, navigate, dismiss, after, debounce, animate, copy, log, "
		  "stop, "
		  "note, accept, reject, preview, endPreview, restore, revert, undo, invoke, subscribe, respond, ask, "
		  "signIn, signOut, chooseFolder, audio, attach, saveFile, openFile, export, openWindow, closeWindow or "
		  "fullscreen)"}},
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
	    {"a node of many properties, and its style, are checked where the property that wins is written",
		 R"("state": {"title": ""}, "styles": {"wide": {"text": "{{state.nope}}", "color": "{{state.titel}}",
	        "a0": 1, "a1": 1, "a2": 1, "a3": 1, "a4": 1, "a5": 1, "a6": 1, "a7": 1}}, "root": {"style": "wide", "text": "{{state.tilte}}", "b0": 1, "b1": 1, "b2": 1, "b3": 1, "b4": 1, "b5": 1, "b6": 1, "b7": 1})",
		 {"root.text: 'state.tilte' doesn't exist (did you mean 'state.title'?)",
		  "styles.wide.color: 'state.titel' doesn't exist (did you mean 'state.title'?)",
		  "styles.wide.text: 'state.nope' doesn't exist (state has title)"}},
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
	const json chain = {
	    {"version", 2}, {"components", components}, {"root", {{"children", json::array({{{"use", "c0"}}})}}}};
	CHECK(affresco::format::check(chain, json::object()));

	// Over a million values: too large to check, so nothing is claimed
	json wide = {{"version", 2}, {"root", {{"text", json::array()}}}};
	wide["root"]["text"].get_ref<json::array_t&>().resize(1100000, json::array()); // numbers cost no visit: lists do
	CHECK_FALSE(affresco::format::check(wide, json::object()));

	// A long name with a typo: hints compare names of a sensible length only, not a square of a hundred thousand
	const std::string longName(100000, 'a');
	const json longTypo = {{"version", 2},
	                       {"state", {{longName, 1}}},
	                       {"root", {{"text", "{{state." + longName + "b}}"}}}};
	const auto found = affresco::format::check(longTypo, longTypo["state"]);
	REQUIRE(found);
	CHECK((found && found->size() == 1));
}

namespace {

/// Keys in each wide object, and items in each long list, of the tests below: enough that looking through those
/// before each one costs seconds.
constexpr int wideCount = 100000;

/// @brief Writes an item many times, each with its number in place of every `$`.
/// @param item JSON text with `$` in it.
/// @param count How many.
/// @param open Written before the first: "{" or "[".
/// @param close Written after the last: "}" or "]".
/// @return The text.
std::string repeated(const std::string& item, int count, char open, char close) {
	std::string text(1, open);
	for(int i = 0; i < count; ++i) {
		std::string one = item;
		for(std::size_t at = one.find('$'); at != std::string::npos; at = one.find('$', at))
			one.replace(at, 1, std::to_string(i));
		text += (i ? "," : "") + one;
	}
	return text + close;
}

/// @brief One object of wideCount keys, at its place in a document.
struct Wide {
	const char* pointer; ///< Its place, as a JSON pointer.
	const char* entry;   ///< Each key and its value, `"<key>", <value>`, `$` its number.
};

/// @brief A row of a table: a wide document.
struct WideCase {
	const char* what;          ///< What the row shows.
	std::string sections;      ///< The screen's sections, without "version", with null where an object goes.
	std::vector<Wide> objects; ///< The objects.
};

/// @brief Checks wide documents, which must be checked (not too large) and have no findings.
/// @param cases The table.
void checkWide(const std::vector<WideCase>& cases) {
	for(const WideCase& row : cases) {
		CAPTURE(row.what);
		json document = screen(row.sections);
		for(const Wide& wide : row.objects) { // read as text, a JSON object would find each key among those before it
			json& object = document[json::json_pointer(wide.pointer)] = json::object();
			for(json& entry : json::parse(repeated(std::string("[") + wide.entry + "]", wideCount, '[', ']')))
				object.get_ref<json::object_t&>().emplace_back(entry[0].get<std::string>(), std::move(entry[1]));
		}
		const auto found = affresco::format::check(document, document.value("state", json::object()));
		REQUIRE(found);
		CHECK(found->empty());
	}
}

/// The screen's root, for documents whose sections are what is checked.
const std::string panel = R"("root": {"type": "panel", "id": "main"})";

} // namespace

// Sections and a node's properties were found by name in a list, once for each name: a hundred thousand actions,
// components or properties took seconds to check, and each revision is checked
TEST_CASE("checker: many sections and properties are checked at the cost of their number" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	checkWide({
	    {"actions",
		 R"("state": {"p": 0}, "actions": null, )" + panel,
		 {{"/actions", R"("a$", [{"set": "state.p", "to": $}])"}}},
	    {"styles", R"("styles": null, )" + panel, {{"/styles", R"("s$", {"padding": $})"}}},
	    {"components", R"("components": null, )" + panel, {{"/components", R"("c$", {"node": {"type": "panel"}})"}}},
	    {"computed values",
		 R"("state": {"p": 1}, "computed": null, )" + panel,
		 {{"/computed", R"("c$", "{{state.p + $}}")"}}},
	    {"a node's properties", R"("root": null)", {{"/root", R"("x$", $)"}}},
	});
}

// The shape of an object, of what a list's items hold or of what the state's types let it hold was built a key at a
// time, each found among those before it: a hundred thousand keys took from seconds to a minute to check
TEST_CASE("checker: the shapes of wide values are built at the cost of their size" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	checkWide({
	    {"items of many keys",
		 R"("state": {"rows": [null, null]}, )" + panel,
		 {{"/state/rows/0", R"("k$", $)"}, {"/state/rows/1", R"("j$", $)"}}},
	    {"items of one key each",
		 R"("state": {"rows": )" + repeated(R"({"k$": 1})", wideCount, '[', ']') + "}, " + panel,
		 {}},
	    {"state types",
		 R"("state": {}, "stateTypes": null, )" + panel,
		 {{"/stateTypes", R"("k$", {"type": "number"})"}}},
	    {"an object set onto another",
		 R"("state": {"o": null}, "actions": {"go": [{"set": "state.o", "to": null}]}, )" + panel,
		 {{"/state/o", R"("k$", $)"}, {"/actions/go/0/to", R"("k$", [$])"}}},
	    {"$object", R"("root": {"type": "text", "text": {"$object": null}})", {{"/root/text/$object", R"("k$", $)"}}},
	});
}

// A path deeper than the runtime writes overflowed the stack (the checker followed it a level at a time), and so did a
// step that puts what a place holds inside itself, once for each such step
TEST_CASE("checker: a place too deep for the state is not followed" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	const auto writing = [](int levels) {
		std::string path = "state";
		for(int i = 0; i < levels; ++i) path += ".a";
		return lines(screen(R"("state": {}, "actions": {"go": [{"set": ")" + path +
		                    R"(", "to": 1}, {"set": "state.b", "to": "{{state.a}}"}]}, "root": {"type": "panel"})"));
	};
	CHECK(writing(200).empty()); // the deepest place the runtime writes
	const std::vector<std::string> unwritten = {"actions.go[1].to: 'state.a' doesn't exist (did you mean 'state.b'?)"};
	CHECK(writing(201) == unwritten); // the runtime refuses it, so it writes nothing
	CHECK(writing(100000) == unwritten);

	const std::string wraps = repeated(R"({"set": "state.a", "to": {"x": "{{state.a}}"}})", 20000, '[', ']');
	CHECK(
	    lines(screen(R"("state": {"a": 1}, "actions": {"go": )" + wraps + R"(}, "root": {"type": "panel"})")).empty());
}

// Each step that writes the state into the state doubled what the checker copied: forty of them took more memory than
// a computer has
TEST_CASE("checker: a document that copies too much is too large to check" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	const std::string copies = repeated(R"({"set": "state.x$", "to": "{{state}}"})", 40, '[', ']');
	const json document =
	    screen(R"("state": {"s": 1}, "actions": {"go": )" + copies + R"(}, "root": {"type": "panel"})");
	CHECK_FALSE(affresco::format::check(document, document["state"]));
	const std::string few = repeated(R"({"set": "state.x$", "to": "{{state}}"})", 10, '[', ']');
	CHECK(lines(screen(R"("state": {"s": 1}, "actions": {"go": )" + few + R"(}, "root": {"type": "panel"})")).empty());
}

// Before, every finding compared its name with every name there is: 8,000 typos against a state of 8,000 keys took
// 5.8 seconds to refuse, and a revision may hold far more of both
TEST_CASE("checker: only the first findings of a pass get a hint, each against the first thousand names") {
	json state = json::object();
	for(int i = 0; i < 2000; ++i) state["key" + std::to_string(i)] = i;
	json labels = json::array();
	for(int i = 0; i < 200; ++i) labels.push_back({{"text", "{{state.kez" + std::to_string(i) + "}}"}});
	const json document = {{"version", 2}, {"state", state}, {"root", {{"children", labels}}}};

	const auto found = affresco::format::check(document, state);
	REQUIRE(found);
	REQUIRE(found->size() == 200); // every finding is there, each once
	const auto hinted =
	    std::ranges::count_if(*found, [](const Finding& finding) { return finding.hint.starts_with("did you mean"); });
	CHECK(hinted == 50);
	CHECK((*found)[0].text() == "root.children[0].text: 'state.kez0' doesn't exist (did you mean 'state.key0'?)");

	// A name past the first thousand isn't compared
	json many = json::object();
	for(int i = 0; i < 1000; ++i) many["k" + std::to_string(i)] = i;
	many["special"] = 1;
	const json late = {{"version", 2}, {"state", many}, {"root", {{"text", "{{state.specail}}"}}}};
	CHECK(lines(late).front().starts_with("root.text: 'state.specail' doesn't exist (state has k0, k1, k2,"));
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
	CHECK(lines({{"version", 2}, {"computed", computed}, {"root", {{"text", "{{computed.c0}}"}}}}).empty());
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

// NOLINTEND(bugprone-unchecked-optional-access)
