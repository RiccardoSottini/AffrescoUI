/// @file
/// @brief Nodes compiled once per version (CompiledNode, Binding): a node in, its slots and bindings out; and the
/// runtime that applies them, which leaves a panel alone only while nothing it reads has changed.

#include "Binding.hpp"
#include "CompiledNode.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using namespace affresco::format;

namespace {

/// @brief Compiles a node written as JSON.
/// @param text The node.
/// @return It compiled.
CompiledRef compiledFrom(const char* text) { return compileNode(std::make_shared<const json>(json::parse(text))); }

/// @brief A screen built on a headless window.
struct Built {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Builds a document.
	/// @param document The document.
	explicit Built(const json& document)
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(std::make_unique<affresco::Screen>(affresco::Screen::fromString(document.dump()))) {
		screen->build(*window);
		window->render();
	}
	/// @brief Runs an action, then the update it asks for.
	/// @param action The action.
	/// @param event Its event.
	void run(std::string_view action, std::string_view event = "{}") {
		screen->runAction(action, window->root(), event);
		window->render();
	}
};

} // namespace

TEST_CASE("compiler: each property becomes a slot, and only dynamic values are evaluated") {
	const CompiledRef node = compiledFrom(R"({
		"id": "card", "type": "label", "text": "{{card.title}}", "color": "@surface", "width": 120,
		"anchors": {"left": 0, "right": "{{gap}}"}, "hover": {"color": "@hover"},
		"onClick": {"set": "state.open", "to": true}, "bind": "state.name", "note": "a card",
		"with": {"gap": 4}, "let": {"wide": "{{gap > 2}}"}, "when": "{{card.shown}}",
		"children": [{"type": "panel"}]
	})");

	CHECK(node->type == "label");
	CHECK(node->typeIsText);
	CHECK(node->hasId);
	CHECK(node->hasStateBlocks);
	CHECK_FALSE(node->alwaysApply);
	CHECK_FALSE(node->own.contains("children"));
	REQUIRE(node->children);
	CHECK(node->children->size() == 1);

	// Every key but the scoping ones, in the order written
	std::vector<std::string> keys;
	for(const CompiledProperty& property : node->properties) keys.push_back(property.key);
	CHECK(keys ==
	      std::vector<std::string>{"id", "type", "text", "color", "width", "anchors", "hover", "onClick", "bind"});

	const auto slotOfKey = [&node](const char* key) { return node->find(key)->slot; };
	CHECK(slotOfKey("id") == Slot::Id);
	CHECK(slotOfKey("type") == Slot::Value);
	CHECK(slotOfKey("text") == Slot::Value);
	CHECK(slotOfKey("color") == Slot::Color);
	CHECK(slotOfKey("width") == Slot::Layout);
	CHECK(slotOfKey("anchors") == Slot::Layout);
	CHECK(slotOfKey("hover") == Slot::StateBlock);
	CHECK(slotOfKey("onClick") == Slot::Handler);
	CHECK(slotOfKey("bind") == Slot::Bind);

	// Only "text" and the anchors are evaluated; the rest is used as written
	CHECK(node->evaluatedCount == 2);
	CHECK(node->find("text")->evaluated == 0);
	CHECK(node->find("anchors")->evaluated == 1);
	CHECK(node->find("color")->evaluated == CompiledProperty::noValue);
	CHECK(node->find("onClick")->evaluated == CompiledProperty::noValue); // structural: never evaluated

	REQUIRE(node->with.size() == 1);
	CHECK(node->with[0].name == "gap");
	REQUIRE(node->let.size() == 1);
	CHECK(node->let[0].value.dynamic());
	REQUIRE(node->when);
	CHECK(node->when->dynamic());
	CHECK_FALSE(node->repeat);
}

TEST_CASE("compiler: a repeat, a dynamic style, and the keys that make a node apply on every update") {
	const CompiledRef node = compiledFrom(R"({
		"repeat": {"for": "tag", "in": "state.tags", "index": "i", "key": "{{tag}}"},
		"style": "{{on ? 'chipOn' : 'chip'}}",
		"overlay": {"open": "state.menuOpen"}
	})");
	REQUIRE(node->repeat);
	CHECK(node->repeat->item == "tag");
	CHECK(node->repeat->index == "i");
	REQUIRE(node->repeat->path);
	CHECK(*node->repeat->path == "{{state.tags}}"); // a path written before expressions
	CHECK(node->repeat->key);
	CHECK(node->style);
	CHECK(node->alwaysApply); // an overlay whose "open" is a state path reads the state when it is applied
	CHECK_FALSE(node->find("style"));

	CHECK(compiledFrom(R"({"bind": "{{field}}"})")->alwaysApply);
	CHECK(compiledFrom(R"({"every": {"ms": "{{period}}", "do": []}})")->alwaysApply);
	CHECK_FALSE(compiledFrom(R"({"every": {"ms": 500, "do": []}})")->alwaysApply);

	// Malformed scoping keys are left for the builder, which reports them where each instance is built
	const CompiledRef malformed = compiledFrom(R"({"repeat": {"for": 3, "in": []}, "with": 4, "type": 7})");
	CHECK_FALSE(malformed->repeat);
	CHECK(malformed->malformedWith);
	CHECK_FALSE(malformed->typeIsText);
}

TEST_CASE("compiler: a binding gives what evaluate() gives, with the same errors at the same places") {
	const json state = {{"count", 3}, {"items", {1, 2, 3}}};
	const json empty = json::object();
	const Scope locals{nullptr, nullptr, {{"name", "Ada"}}};
	const Context context{state, empty, empty, empty, locals, empty};

	const std::vector<json> values = {
	    json(7),
	    json("plain"),
	    json("{{state.count * 2}}"),
	    json("Hello {{name}}, {{len(state.items)}} items"),
	    json::parse(R"({"$add": ["{{state.count}}", 1]})"),
	    json::parse(R"({"left": 4, "right": "{{state.count}}", "list": ["{{name}}", 2]})"),
	    json::parse(R"([1, {"a": "{{name}}"}])"),
	};
	for(const json& value : values) {
		const Binding binding(value);
		CHECK(binding.evaluate(context, "root", "text") == evaluate(value, context, "root.text"));
	}
	CHECK_FALSE(Binding(json::parse(R"({"left": 4})")).dynamic());
	CHECK(Binding(json("{{name}}")).dynamic());

	const json broken = "{{state.count +}}";
	CHECK_THROWS_WITH_AS(static_cast<void>(Binding(broken).evaluate(context, "root", "text")),
	                     doctest::Contains("root.text"), affresco::DocumentError);
	const json unknown = "{{nobody.here}}";
	CHECK_THROWS_WITH_AS(static_cast<void>(Binding(unknown).evaluate(context, "root", "with", "title")),
	                     doctest::Contains("root.with.title"), affresco::DocumentError);
}

TEST_CASE("compiler: an unchanged panel is left alone, and applied again when its values or the theme change") {
	Built built(json::parse(R"({
		"format": "affrescoui-screen", "version": 1,
		"state": {"count": 0, "dark": false},
		"colors": {"surface": {"light": "#ffffff", "dark": "#000000"}},
		"theme": "{{state.dark ? 'dark' : 'light'}}",
		"actions": {"count": [{"increment": "state.count"}], "darken": [{"set": "state.dark", "to": true}]},
		"root": {"id": "root", "children": [
			{"id": "box", "width": 40, "height": 40, "color": "@surface"},
			{"id": "counter", "type": "label", "text": "{{state.count}}"}
		]}
	})"));
	auto& counter = built.screen->get<affresco::Label>("counter");
	affresco::Panel& box = built.screen->get("box");
	CHECK(counter.text() == "0");
	CHECK(box.color() == affresco::Color::hex(0xffffff));

	built.run("count");
	CHECK(counter.text() == "1");
	CHECK(box.color() == affresco::Color::hex(0xffffff));

	// The box's values are the same, but "@surface" is another colour in the dark theme
	built.run("darken");
	CHECK(box.color() == affresco::Color::hex(0x000000));
}

TEST_CASE("compiler: a revision compiles the new version, and the panels follow it") {
	Built built(json::parse(R"({
		"format": "affrescoui-screen", "version": 1, "state": {"title": "Tasks"},
		"root": {"id": "root", "children": [
			{"id": "title", "type": "label", "text": "{{state.title}}", "width": 80, "height": 20}
		]}
	})"));
	auto& title = built.screen->get<affresco::Label>("title");
	CHECK(title.text() == "Tasks");
	CHECK(title.frame().width() == 80.0f);

	REQUIRE(built.screen->applyRevision(R"({"set": {"#title/width": 120, "#title/text": "Done: {{state.title}}"}})"));
	built.window->render();
	CHECK(title.text() == "Done: Tasks");
	CHECK(title.frame().width() == 120.0f);
}

TEST_CASE("compiler: a property that fails to apply fails again at the next update, as before") {
	std::vector<std::string> errors;
	affresco::setLogSink([&errors](affresco::LogLevel level, std::string_view message) {
		if(level == affresco::LogLevel::Error) errors.emplace_back(message);
	});
	{
		Built built(json::parse(R"({
			"format": "affrescoui-screen", "version": 1, "state": {"color": "#ff0000", "count": 0},
			"actions": {"spoil": [{"set": "state.color", "to": "#zz0000"}], "count": [{"increment": "state.count"}]},
			"root": {"id": "root", "children": [{"id": "box", "width": 10, "height": 10, "color": "{{state.color}}"}]}
		})"));
		built.run("spoil");
		built.run("count"); // the same values, still not applied: the error comes again
	}
	affresco::setLogSink(nullptr);
	REQUIRE(errors.size() == 2);
	CHECK(errors[1].find("root.children[0].color") != std::string::npos);
}
