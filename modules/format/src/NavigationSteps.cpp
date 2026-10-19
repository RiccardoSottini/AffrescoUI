/// @file
/// @brief The steps that move the user: focus, scrollTo, navigate.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief "focus" or "scrollTo": a panel by id, after the update and its layout, so it exists and has its place.
/// @param s The step.
void focusOrScroll(const Running& s) {
	const bool focus = s.step.contains("focus");
	std::string id = text(s.value(s.step[focus ? "focus" : "scrollTo"], s.where));
	if(id.starts_with('#')) id.erase(0, 1);
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	engine.scheduler.once(0.0, [&engine, id, focus] {
		if(Panel* panel = engine.window->root().findById(id)) {
			if(focus) engine.window->setFocus(panel);
			else panel->scrollIntoView();
		}
	});
}

/// @brief "navigate": shows a screen of "screens" in the outlets, or "back".
/// @param s The step.
void navigate(const Running& s) {
	Engine& engine = s.steps.engine();
	const std::string name = text(s.value("navigate"));
	if(name == "back") {
		if(!engine.document.back()) return;
	} else {
		const json& document = engine.versions.document();
		if(!document.contains("screens") || !document["screens"].contains(name))
			fail(s.where + ".navigate", "unknown screen '" + name + "'");
		engine.document.navigate(name);
	}
	engine.document.globalsChanged();
	engine.document.markStateChanged();
}

/// @brief Runs a navigation step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runNavigationStep(const Running& s) {
	if(s.step.contains("focus") || s.step.contains("scrollTo")) {
		focusOrScroll(s);
		return true;
	}
	if(s.step.contains("navigate")) {
		navigate(s);
		return true;
	}
	return std::nullopt;
}

} // namespace

void registerNavigationSteps(StepRegistry& registry) {
	registry.add({"navigation", {{"focus", {}}, {"scrollTo", {}}, {"navigate", {}}}, runNavigationStep});
}

} // namespace affresco::format
