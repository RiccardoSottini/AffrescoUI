/// @file
/// @brief The steps that move the user: focus, scrollTo, navigate, dismiss.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief Scrolls a panel by id into view once it has its place: deferred tasks run before the frame's layout, so a
/// panel the update just made, or one in an overlay just opened, waits for the layouts that place it.
/// @param engine The engine.
/// @param id The panel's id.
/// @param frames The frames it may still wait.
void scrollLater(Engine& engine, const std::string& id, int frames) {
	engine.scheduler.once(0.0, [&engine, id, frames] {
		if(!engine.window) return;
		Panel* panel = engine.window->root().findById(id);
		if(!panel) return;
		if(!panel->isShown() && panel->isVisible() && frames > 0) scrollLater(engine, id, frames - 1);
		else panel->scrollIntoView();
	});
}

/// @brief "focus" or "scrollTo": a panel by id, after the update and its layout, so it exists and has its place. A
/// panel that takes no focus passes it to its first focusable node.
/// @param s The step.
void focusOrScroll(const Running& s) {
	const bool focus = s.step.contains("focus");
	std::string id = text(s.value(s.step[focus ? "focus" : "scrollTo"], s.where));
	if(id.starts_with('#')) id.erase(0, 1);
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	if(!focus) {
		scrollLater(engine, id, 2);
		return;
	}
	engine.events.focusStep(std::move(id));
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
	if(s.step.contains("dismiss")) { // closes the open overlays, e.g. a menu once an item is chosen
		if(boolean(s.value("dismiss"), s.where + ".dismiss")) s.steps.engine().events.dismissAll();
		return true;
	}
	return std::nullopt;
}

} // namespace

void registerNavigationSteps(StepRegistry& registry) {
	registry.add(
	    {"navigation", {{"focus", {}}, {"scrollTo", {}}, {"navigate", {}}, {"dismiss", {}}}, runNavigationStep});
}

} // namespace affresco::format
