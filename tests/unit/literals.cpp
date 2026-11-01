/// @file
/// @brief The text a person reads: finding what a document writes in place, messages that show it as written, and
/// titles outside the nodes following the language.

#include "Checker.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Literals.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief The texts a document writes in place, through the vocabulary of its own screen.
/// @param text The document.
/// @return What was found.
std::vector<format::LiteralText> literalsIn(const std::string& text) {
	const affresco::Screen screen = affresco::Screen::fromString(text);
	const json document = json::parse(text);
	const json strings = document.value("strings", json::object());
	return format::literalTexts(document, format::textVocabulary(json::parse(screen.catalogJson()), document),
	                            strings.value("keep", json::array()));
}

/// @brief The places of what was found.
/// @param found What was found.
/// @return The pointers, sorted.
std::vector<std::string> placesOf(const std::vector<format::LiteralText>& found) {
	std::vector<std::string> places;
	places.reserve(found.size());
	for(const format::LiteralText& literal : found) places.push_back(literal.pointer);
	std::ranges::sort(places);
	return places;
}

} // namespace

TEST_CASE("literals: what a person reads is found, and what they don't isn't") {
	const auto found = literalsIn(R"json({
		"format": "affrescoui-screen", "version": 2,
		"window": { "title": "My app" },
		"actions": { "go": { "title": "Go", "do": [] }, "plain": [{ "set": "state.note", "to": "not looked at" }] },
		"state": { "note": "the state's" },
		"root": { "id": "root", "children": [
			{ "id": "a", "type": "label", "text": "Hello", "tooltip": "Says hello" },
			{ "id": "b", "type": "label", "text": "+" },
			{ "id": "c", "type": "label", "text": "{{state.note}}" },
			{ "id": "d", "type": "label", "text": "@tokenName" },
			{ "id": "e", "type": "editor", "text": "code, not text" },
			{ "id": "f", "type": "textarea", "text": "typed", "placeholder": "Type here" }
		] } })json");
	CHECK(placesOf(found) == std::vector<std::string>{"/actions/go/title", "/root/children/0/text",
	                                                  "/root/children/0/tooltip", "/root/children/5/placeholder",
	                                                  "/window/title"});
}

TEST_CASE("literals: a key comes from where the text is, so rewording keeps it") {
	const std::string text = R"json({ "format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "id": "title", "type": "label", "text": "Hello" }] } })json";
	const auto found = literalsIn(text);
	REQUIRE(found.size() == 1);
	CHECK(found[0].key == "title.text");
	CHECK(literalsIn(text.substr(0, text.find("Hello")) + "Hi there" + text.substr(text.find("Hello") + 5))[0].key ==
	      "title.text");
}

TEST_CASE("literals: a component's text is what reaches a property a person reads, even through another component") {
	const auto found = literalsIn(R"json({ "format": "affrescoui-screen", "version": 2,
		"components": {
			"inner": { "params": { "shown": "" }, "node": { "id": "innerLabel", "type": "label", "text": "{{shown}}" } },
			"outer": { "params": { "words": "", "key": "" }, "node": { "id": "outerPanel", "children": [
				{ "id": "use", "use": "inner", "with": { "shown": "{{words}}" } },
				{ "id": "keyed", "type": "label", "text": "{{t(key)}}" }
			] } }
		},
		"root": { "id": "root", "children": [
			{ "id": "one", "use": "outer", "with": { "words": "Shown", "key": "not.text" } }
		] } })json");
	CHECK(placesOf(found) == std::vector<std::string>{"/root/children/0/with/words"});
}

TEST_CASE("literals: text around an expression is told apart, and what strings.keep lists stays") {
	const auto found = literalsIn(R"json({ "format": "affrescoui-screen", "version": 2,
		"strings": { "keep": ["Forge"] },
		"window": { "title": "Forge" },
		"root": { "id": "root", "children": [{ "id": "a", "type": "label", "text": "Saved {{state.name}}" }] } })json");
	REQUIRE(found.size() == 1);
	CHECK(found[0].built);
}

TEST_CASE("literals: a text in another script counts, and symbols, icons and emoji don't") {
	const auto found = literalsIn(R"json({ "format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [
			{ "id": "a", "type": "label", "text": "مرحبا" },
			{ "id": "b", "type": "label", "text": "−" },
			{ "id": "c", "type": "label", "text": "" },
			{ "id": "d", "type": "label", "text": "😀" },
			{ "id": "e", "type": "label", "text": "42" }
		] } })json");
	CHECK(placesOf(found) == std::vector<std::string>{"/root/children/0/text"});
}

TEST_CASE("literals: a text moved to a message shows exactly as it was written") {
	for(const std::string text : {"Plain", "Don't stop", "A {brace} here", "''", "'{'", "a } b", "100% # {x}, 'y'"}) {
		CAPTURE(text);
		const json document = {{"format", "affrescoui-screen"},
		                       {"version", 2},
		                       {"strings", {{"en", {{"k", format::messageOfText(text)}}}}},
		                       {"state", {{"shown", ""}}},
		                       {"actions", {{"look", json::array({{{"set", "state.shown"}, {"to", "{{t('k')}}"}}})}}},
		                       {"root", {{"id", "root"}}}};
		const affresco::detail::ManualClock clock;
		const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
		affresco::Screen screen = affresco::Screen::fromString(document.dump());
		screen.build(*window);
		screen.runAction("look", *screen.find("root"));
		CHECK(json::parse(screen.stateJson())["shown"] == text);
	}
}

TEST_CASE("declared text: a title with a key follows the language, and plain text shows as written") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"window": { "title": "{{t('app')}}" },
		"strings": { "default": "en", "locale": "{{state.locale}}",
			"en": { "app": "Tracker", "save": "Save", "file": "File", "font": "Font size" },
			"fr": { "app": "Suivi", "save": "Enregistrer", "file": "Fichier", "font": "Taille" } },
		"state": { "locale": "en", "seen": null },
		"settings": { "editor.size": { "type": "number", "default": 12, "title": "{{t('font')}}" } },
		"menus": { "top": { "saveItem": { "title": "{{t('save')}}", "run": "save" } } },
		"actions": {
			"save": { "title": "{{t('save')}}", "category": "{{t('file')}}", "do": [] },
			"open": { "title": "Open", "category": "File", "do": [] },
			"look": [{ "set": "state.seen", "to": {
				"commands": "{{commands}}", "menus": "{{menus}}", "settings": "{{settingsList}}" } }],
			"french": [{ "set": "state.locale", "to": "fr" }]
		},
		"root": { "id": "root" } })json");
	screen.build(*window);
	const auto seen = [&screen] {
		screen.runAction("look", *screen.find("root"));
		return json::parse(screen.stateJson())["seen"];
	};
	const auto titled = [](const json& commands, const std::string& name) {
		for(const json& command : commands)
			if(command["name"] == name) return command;
		return json();
	};
	CHECK(window->title() == "Tracker");
	CHECK(titled(seen()["commands"], "save")["title"] == "Save");
	CHECK(titled(seen()["commands"], "open")["title"] == "Open");

	screen.runAction("french", *screen.find("root"));
	affresco::detail::renderWithoutPresenting(*window); // an update chooses the locale
	const json french = seen();
	CHECK(window->title() == "Suivi");
	CHECK(titled(french["commands"], "save")["title"] == "Enregistrer");
	CHECK(titled(french["commands"], "save")["category"] == "Fichier");
	CHECK(titled(french["commands"], "open")["title"] == "Open");
	CHECK(french["menus"]["top"][0]["title"] == "Enregistrer");
	CHECK(french["settings"][0]["title"] == "Taille");
}

TEST_CASE("declared text: the checker reads a title's key, as it reads a node's") {
	const json document = json::parse(R"json({ "format": "affrescoui-screen", "version": 2,
		"strings": { "en": { "known": "Known" } },
		"window": { "title": "{{t('noWindow')}}" },
		"menus": { "top": { "item": { "title": "{{t('noMenu')}}", "run": "go" } } },
		"settings": { "a.b": { "type": "number", "default": 1, "title": "{{t('noSetting')}}" } },
		"actions": { "go": { "title": "{{t('known')}}", "category": "{{t('noCategory')}}", "do": [] } },
		"root": { "id": "root" } })json");
	const auto found = format::check(document, json::object(), {});
	REQUIRE(found);
	if(!found) return;
	std::string text;
	for(const auto& finding : *found) text += finding.text() + '\n';
	CAPTURE(text);
	for(const char* key : {"noWindow", "noMenu", "noSetting", "noCategory"}) CHECK(text.contains(key));
	CHECK_FALSE(text.contains("'known'"));
}

TEST_CASE("strings: its section takes \"required\" and \"keep\", and refuses them in the wrong shape") {
	const auto load = [](const std::string& section) {
		return affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2, "strings": )" + section +
		                                    R"(, "root": {"id": "root"}})");
	};
	CHECK_NOTHROW(static_cast<void>(load(R"({"required": true, "keep": ["Brand"], "en": {"a": "b"}})")));
	CHECK_THROWS_AS(static_cast<void>(load(R"({"required": "yes"})")), affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(load(R"({"keep": "Brand"})")), affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(load(R"({"keep": [1]})")), affresco::DocumentError);
}
