/// @file
/// @brief The format's registries: node types (TypeRegistry), steps (StepRegistry) and document sections
/// (SectionRegistry), and what the runtime builds from them: the catalog, and an application's types.

#include "Headless.hpp"
#include "SectionRegistry.hpp"
#include "StepRegistry.hpp"
#include "TypeRegistry.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::json;
using namespace affresco::format;

namespace {

/// @brief A text editor of the application's own, which has the properties of "editor" too.
class NotesEditor final : public affresco::TextEditor {};

/// @brief The names of a type registry's types, in order.
/// @param registry The registry.
/// @return The names.
std::vector<std::string> typeNames(const TypeRegistry& registry) {
	std::vector<std::string> names;
	for(const NodeType* type : registry.inOrder()) names.push_back(type->name);
	return names;
}

/// @brief The error a document's section validation gives.
/// @param document The document.
/// @return The error, or "" if it is valid.
std::string sectionError(const json& document) {
	SectionRegistry sections;
	registerBuiltInSections(sections);
	try {
		sections.validate(document);
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "";
}

/// @brief A step group that recognises nothing.
/// @param step The step.
/// @return Always std::nullopt.
std::optional<bool> recognisesNothing(const Running& step) {
	static_cast<void>(step);
	return std::nullopt;
}

} // namespace

TEST_CASE("format: the built-in node types register in catalog order, each with a factory") {
	TypeRegistry registry;
	registerBuiltInTypes(registry);
	CHECK(typeNames(registry) == std::vector<std::string>{"panel", "label", "textarea", "editor", "diff", "stack",
	                                                      "row", "column", "grid", "toggle", "checkbox", "slider",
	                                                      "image", "surface", "raster", "split", "tabs", "list", "tree", "table", "path",
	                                                      "colorPicker", "gradient", "curves", "canvas", "graph", "timeline", "calendar",
	                                                      "codeInput", "carousel", "scene", "outlet"});
	for(const NodeType* type : registry.inOrder()) {
		CAPTURE(type->name);
		CHECK(type->create());
		CHECK_FALSE(type->application);
	}
	CHECK(dynamic_cast<affresco::Toggle*>(registry.create("checkbox", "root", false).get()));
	CHECK(dynamic_cast<affresco::Stack*>(registry.create("row", "root", false).get()));
	CHECK_THROWS_WITH_AS(static_cast<void>(registry.create("buton", "root.children[0]", false)),
	                     doctest::Contains("root.children[0]: unknown type 'buton'"), affresco::DocumentError);
	CHECK(registry.create("buton", "root", true)); // a placeholder
}

TEST_CASE("format: a type's property sets: its own, then the built-in widgets it is a subclass of") {
	TypeRegistry registry;
	registerBuiltInTypes(registry);
	auto own = std::make_shared<PropertySet>();
	own->properties = {{"language", ValueKind::Text}};
	own->apply = [](affresco::Panel&, const json&, const PropertyReader&, const std::string&) {};
	registry.add({"notes", "Notes", [] { return std::make_unique<NotesEditor>(); }, own, true});

	const NotesEditor notes;
	const PropertySets sets = registry.setsFor(registry.find("notes"), notes);
	REQUIRE(sets[0] == own);
	REQUIRE(sets[1]);
	CHECK(sets[1]->find("lineNumbers")); // the editor's
	CHECK(sets[2]); // then the sets that apply to every node, such as the transform properties

	const affresco::Panel panel;
	const PropertySets none = registry.setsFor(registry.find("panel"), panel);
	CHECK(none[0]); // only the transform properties: the runtime applies the rest itself
	CHECK_FALSE(none[1]);

	CHECK(registry.transitionable("color"));
	CHECK(registry.transitionable("textColor"));
	CHECK(registry.transitionable("borderColor"));
	CHECK_FALSE(registry.transitionable("radius"));
	CHECK_FALSE(registry.transitionable("language"));
}

TEST_CASE("format: a type added under a registered name replaces it, and keeps its place") {
	TypeRegistry registry;
	registerBuiltInTypes(registry);
	registry.add({"label", "Mine", [] { return std::make_unique<affresco::Panel>(); }, nullptr, true});
	CHECK(typeNames(registry)[1] == "label");
	CHECK(registry.find("label")->about == "Mine");
	CHECK(registry.find("label")->own); // an empty set, never null
	CHECK_FALSE(dynamic_cast<affresco::Label*>(registry.create("label", "root", false).get()));
}

TEST_CASE("format: the built-in steps register by group, in the order they are tried") {
	StepRegistry registry;
	registerBuiltInSteps(registry);
	std::vector<std::string> groups;
	for(const StepGroup& group : registry.groups()) groups.push_back(group.name);
	CHECK(groups == std::vector<std::string>{"state", "flow", "outside", "services", "auth", "assistant", "folder", "review",
	                                         "navigation", "time", "other", "scene", "image"});

	std::vector<std::string> control;
	for(const StepGroup& group : registry.groups())
		for(const StepType& step : group.steps)
			if(step.control) control.push_back(step.name);
	std::ranges::sort(control);
	CHECK(control == std::vector<std::string>{"accept", "ask", "call", "chooseFolder", "connect", "endPreview", "patch", "preview",
	                                          "reject", "respond", "restore", "revert", "send", "signIn", "signOut",
	                                          "undo"});

	REQUIRE(registry.find("set"));
	CHECK(registry.find("set")->keys == std::vector<std::string>{"to"});
	CHECK_FALSE(registry.find("set")->control);
	CHECK_FALSE(registry.find("accept")->listed); // the catalog lists it under "controlSteps" only
	CHECK_FALSE(registry.find("teleport"));
}

TEST_CASE("format: a step group needs a runner, and a step is registered once") {
	StepRegistry registry;
	registerBuiltInSteps(registry);
	CHECK_THROWS_AS(registry.add({"broken", {{"beep", {}}}, nullptr}), std::logic_error);
	CHECK_THROWS_AS(registry.add({"again", {{"set", {}}}, recognisesNothing}), std::logic_error);
	registry.add({"sounds", {{"beep", {"volume"}}}, recognisesNothing});
	CHECK(registry.groups().back().name == "sounds");
	CHECK(registry.find("beep")->keys == std::vector<std::string>{"volume"});
}

TEST_CASE("format: the sections validate a document, naming the place") {
	const json valid = {{"format", "affrescoui-screen"}, {"version", 2}, {"root", {{"type", "panel"}}}};
	CHECK(sectionError(valid).empty());

	json unknown = valid;
	unknown["colours"] = json::object();
	CHECK(sectionError(unknown).find("colours: unknown section") != std::string::npos);
	json included = valid;
	included["include"] = "parts.json"; // merged when the file is read, never in a live document
	CHECK(sectionError(included).find("include: unknown section") != std::string::npos);
	CHECK(sectionError({{"version", 2}}).find("missing \"root\"") != std::string::npos);
	CHECK(sectionError({{"root", json::object()}}).find("missing \"version\"") != std::string::npos);
	json future = valid;
	future["version"] = 3;
	CHECK(sectionError(future).find("version: unsupported version 3") != std::string::npos);
	json component = valid;
	component["components"] = {{"card", {{"params", json::object()}}}};
	CHECK(sectionError(component).find("components.card: a component needs a \"node\"") != std::string::npos);
	json channel = valid;
	channel["channels"] = {{"host", {{"socket", "h.sock"}, {"retry", true}}}};
	CHECK(sectionError(channel).find("channels.host.retry: unknown property") != std::string::npos);
	json persist = valid;
	persist["persist"] = {"habits"};
	CHECK(sectionError(persist).find("persist[0]: expected a path under state") != std::string::npos);
}

TEST_CASE("format: the capability sections are the ones a revision can't change") {
	SectionRegistry sections;
	registerBuiltInSections(sections);
	std::vector<std::string> capabilities;
	for(const SectionType& section : sections.sections())
		if(section.capability) capabilities.push_back(section.name);
	CHECK(capabilities ==
	      std::vector<std::string>{"channels", "services", "connections", "persist", "permissions", "images",
	                               "points"});

	const Rules rules;
	const json before = {{"format", "affrescoui-screen"}, {"version", 2}, {"root", {{"type", "panel"}}}};
	for(const std::string& section : capabilities) {
		CAPTURE(section);
		json after = before;
		after[section] = section == "persist" ? json::array({"state.x"}) : json::object();
		const std::string expected = "revisions can't change \"" + section + "\"";
		CHECK_THROWS_WITH_AS(rules.checkChange(before, after), doctest::Contains(expected.c_str()),
		                     affresco::DocumentError);
	}
}

TEST_CASE("format: the catalog is built from the registries, and lists an application's types") {
	affresco::Screen screen =
	    affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2, "root": {"type": "panel"}})");
	screen.registerType("notes", [] { return std::make_unique<NotesEditor>(); }, {"language"});
	const json catalog = json::parse(screen.catalogJson());

	CHECK(catalog["types"]["notes"]["properties"] == json::array({"language"}));
	CHECK(catalog["types"]["notes"]["about"] == "Registered by the application");
	CHECK(catalog["customTypes"] == json::array({"notes"}));
	CHECK(catalog["types"]["toggle"]["properties"] ==
	      json::array({"bind", "checked", "activeColor", "trackColor", "knobColor"}));
	CHECK(catalog["common"] == catalog["types"]["panel"]["properties"]);
	CHECK(std::ranges::find(catalog["steps"], json("set")) != catalog["steps"].end());
	CHECK(std::ranges::find(catalog["steps"], json("accept")) == catalog["steps"].end());
	CHECK(catalog["controlSteps"].size() == 15);
	CHECK(catalog["sections"].front() == "window");
	CHECK(catalog["sections"].back() == "root");
	CHECK(std::ranges::find(catalog["sections"], json("include")) != catalog["sections"].end());
}

// The application's TextEditor subclass takes the editor's properties, and its own go to its apply function only
// when they change
TEST_CASE("format: an application's type gets the built-in widget's properties and its own") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "language": "c++", "numbers": true },
		"root": { "type": "panel", "children": [
			{ "type": "notes", "id": "notes", "lineNumbers": "{{state.numbers}}", "language": "{{state.language}}" }
		] }
	})json");
	std::vector<std::string> applied;
	screen.registerType(
	    "notes", [] { return std::make_unique<NotesEditor>(); }, {"language"},
	    [&](affresco::Panel&, const std::string& properties) { applied.push_back(properties); });
	screen.build(*window);
	window->render();
	auto* notes = dynamic_cast<NotesEditor*>(screen.find("notes"));
	REQUIRE(notes);
	CHECK(applied == std::vector<std::string>{R"({"language":"c++"})"});

	REQUIRE(screen.applyRevision(R"({"set": {"/state/numbers": false}})"));
	window->render();
	CHECK(applied.size() == 1); // its own values didn't change
	REQUIRE(screen.applyRevision(R"({"set": {"/state/language": "rust"}})"));
	window->render();
	CHECK(applied.back() == R"({"language":"rust"})");
}

// Its values may name colours ("@view"), which a theme switch changes though the values don't: it is applied again
TEST_CASE("format: an application's type is applied again when the theme changes") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 2, "theme": "{{state.theme}}",
		"state": { "theme": "dark", "other": 1 },
		"root": { "type": "panel", "children": [{ "type": "notes", "id": "notes", "language": "c++" }] }
	})json");
	std::vector<std::string> applied;
	screen.registerType(
	    "notes", [] { return std::make_unique<NotesEditor>(); }, {"language"},
	    [&](affresco::Panel&, const std::string& properties) { applied.push_back(properties); });
	screen.build(*window);
	window->render();
	REQUIRE(applied.size() == 1);
	REQUIRE(screen.applyRevision(R"({"set": {"/state/other": 2}})"));
	window->render();
	CHECK(applied.size() == 1); // another revision: the same values, the same look
	REQUIRE(screen.applyRevision(R"({"set": {"/state/theme": "light"}})"));
	window->render();
	CHECK(applied.size() == 2);
}

// Every node's events are a fixed list; an application's type may list events of its own ("onType"), which then
// go to its apply function like its other properties, and an unknown event still fails
TEST_CASE("format: an application's type gets the events it lists as its own") {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 1,
		"actions": { "typed": [{ "set": "state.typed", "to": true }] },
		"root": { "type": "panel", "children": [
			{ "type": "notes", "id": "notes", "onType": "typed", "onClick": "typed" }
		] }
	})json");
	std::vector<std::string> applied;
	screen.registerType(
	    "notes", [] { return std::make_unique<NotesEditor>(); }, {"onType"},
	    [&](affresco::Panel&, const std::string& properties) { applied.push_back(properties); });
	screen.build(*window);
	window->render();
	REQUIRE(screen.find("notes"));
	CHECK(applied == std::vector<std::string>{R"({"onType":"typed"})"}); // onClick stays every node's event

	affresco::Screen unknown = affresco::Screen::fromString(R"json({
		"format": "affrescoui-screen", "version": 1,
		"root": { "type": "panel", "children": [{ "type": "notes", "onPopupKey": "typed" }] }
	})json");
	unknown.registerType(
	    "notes", [] { return std::make_unique<NotesEditor>(); }, {"onType"}, [](auto&, const auto&) {});
	CHECK_THROWS_WITH(unknown.build(*window), doctest::Contains("onPopupKey: unknown event"));
}
