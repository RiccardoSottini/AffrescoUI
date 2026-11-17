#pragma once

/// @file
/// @brief What the tests of features that hold programs share: an app written as a program, with a status bar point,
/// an action it hands features and a hook point, running on a headless window; and a feature's record.

#include "LispApp.hpp"

#include <affresco/format/Runtime.hpp>

#include <memory>
#include <string>
#include <vector>

namespace featureapp {

using affresco::json;
using lispapp::ManualClock;
using lispapp::Repository;
using lispapp::Runtime;
using lispapp::WithoutTheScan;

/// @brief The word count's definitions: its state, a view placed in the status bar that runs its command, the command
/// and the function it calls.
/// @param flip The function's body.
/// @return The definitions, by address.
inline json wordCount(const std::string& flip = R"((toggle "state.wordCount.characters"))") {
	return {{"state.wordCount", R"((defstate wordCount {:characters false :later 0} "The word count's state."))"},
	        {"wordCount-flip", R"((defn wordCount-flip "Flips it." [] )" + flip + ")"},
	        {"wordCount-later", R"((defn wordCount-later "Runs later." [] (set "state.wordCount.later" 1)))"},
	        {"wordCount-label",
	         R"((defview wordCount-label "The label." [] `(label#wordCount-label :text "count" :onClick wordCount.toggle)))"},
	        {"wordCount.toggle", R"((defcommand wordCount.toggle "Toggles." :title "Toggle" [] (wordCount-flip)))"},
	        {"part.wordCount-views",
	         R"((defpart wordCount-views "Where its views go." :views {:statusBar.right {:wordCount-label ~(wordCount-label)}}))"}};
}

/// @brief A feature's record holding a program, placed in the status bar, handed the app's `editor.save` as `save`.
/// @param definitions Its definitions, by address, in one file, `feature.lisp`.
/// @return The record, enabled.
inline json record(const json& definitions = wordCount()) {
	return {{"release", "1.0.0"},
	        {"source", "revision"},
	        {"enabled", true},
	        {"order", 0},
	        {"requires", {{"points", {"statusBar.right"}}}},
	        {"needs", {{"save", {{"kind", "action"}}}}},
	        {"bindings", {{"save", {{"export", "editor.save"}}}}},
	        {"provides", {{"program", {{"feature.lisp", definitions}}}}}};
}

/// @brief The app: a program with a hook point and the person's own command `host.go`, a status bar point that lists
/// the hook and receives the export `editor.save`, the action `save` and the state key `secret`.
/// @param own More definitions of the app's own file, by address.
/// @return The document, with no feature.
inline json app(const json& own = json::object()) {
	json document = json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"program": { "app.lisp": { "after-save": "(defhook after-save \"Runs after a save.\")" } },
		"state": { "text": "one two three", "secret": "kept" },
		"features": {},
		"points": { "statusBar.right": { "slot": "toolbar", "receives": ["editor.save"], "hooks": ["after-save"] } },
		"exports": { "editor.save": { "type": "string", "kind": "action", "value": "{{'save'}}" } },
		"actions": { "save": [{ "set": "state.text", "to": "" }] },
		"root": { "type": "row", "id": "status-bar", "point": "statusBar.right", "children": [
			{ "type": "label", "id": "status-folder", "text": "folder" }
		] }
	})json");
	for(const auto& [name, text] : own.items()) document["program"]["app.lisp"][name] = text;
	return document;
}

/// @brief An app on a headless window; the document is the screen file, so what it holds is the app's own.
struct App {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	WithoutTheScan rules;                     ///< What revisions are judged by.
	Repository repository;                    ///< The history.
	std::unique_ptr<Runtime> runtime;         ///< The running screen.
	int hits = 0;                             ///< How many times the application's function ran.
	std::vector<std::string> errors;          ///< What was reported.

	/// @brief Builds the app.
	/// @param document The document.
	/// @param scan Whether revisions are scanned for new ways to reach control steps.
	explicit App(const json& document, bool scan = true)
	    : window(lispapp::createHeadlessWindow(400, 300, clock)), rules(scan), repository(rules, document),
	      runtime(std::make_unique<Runtime>(repository, "")) {
		runtime->onCall("hit", [this](const json&) {
			++hits;
			return json();
		});
		runtime->onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime->build(*window);
		window->render();
	}

	/// @brief Clicks a panel, the person's own input.
	/// @param id Its id.
	void click(const std::string& id) const {
		const affresco::Panel* panel = runtime->find(id);
		REQUIRE(panel != nullptr);
		const affresco::Rect& bounds = panel->bounds();
		const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		settle();
	}

	/// @brief Runs an action as the screen's own (a key's, a menu's).
	/// @param name The action.
	void run(const std::string& name) const {
		runtime->runAction(name, window->root(), json::object());
		settle();
	}

	/// @brief Lets what was deferred from an event happen.
	void settle() const {
		window->render();
		window->render();
	}

	/// @brief Moves the clock on, with frames: timers fire by themselves.
	/// @param ms How long.
	void wait(double ms) {
		clock.advance(ms);
		settle();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const {
		return repository.state().contains(key) ? repository.state()[key] : json();
	}

	/// @brief Commits a revision, which must apply.
	/// @param revision The revision.
	void commit(const json& revision) {
		const std::optional<std::string> error = repository.commit(revision);
		INFO(error.value_or(""));
		REQUIRE_FALSE(error.has_value());
	}

	/// @brief Sets a value at a path, by an author, which must apply.
	/// @param path The path.
	/// @param value The value.
	/// @param author Who.
	void set(const std::string& path, const json& value, const std::string& author = "agent/x") {
		commit({{"author", author}, {"message", "m"}, {"set", {{path, value}}}});
	}

	/// @brief Tells whether a reason was reported holding a text.
	/// @param text The text.
	/// @return true if it was.
	[[nodiscard]] bool reported(const std::string& text) const {
		return std::ranges::any_of(errors, [&](const std::string& error) { return error.contains(text); });
	}
};

} // namespace featureapp
