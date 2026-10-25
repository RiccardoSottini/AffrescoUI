/// @file
/// @brief The steps that move the user: focus, scrollTo, navigate, dismiss.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief Scrolls a panel by id into view once it has its place: deferred tasks run before the frame's layout, so a
/// panel the update just made, or one in an overlay just opened, waits for the layouts that place it.
/// @param engine The engine.
/// @param path The panel's id, or a path of ids (Events::findByPath()).
/// @param frames The frames it may still wait.
void scrollLater(Engine& engine, const std::string& path, int frames) {
	engine.scheduler.once(0.0, [&engine, path, frames] {
		if(!engine.window) return;
		Panel* panel = Events::findByPath(engine.window->root(), path);
		if(!panel) return;
		if(!panel->isShown() && panel->isVisible() && frames > 0) scrollLater(engine, path, frames - 1);
		else panel->scrollIntoView();
	});
}

/// @brief "focus" or "scrollTo": a panel by id, or by a path of ids ("#form #input": the input inside form), after
/// the update and its layout, so it exists and has its place. A panel that takes no focus passes it to its first
/// focusable node.
/// @param s The step.
void focusOrScroll(const Running& s) {
	const bool focus = s.step.contains("focus");
	std::string path = text(s.value(s.step[focus ? "focus" : "scrollTo"], s.where));
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	if(!focus) {
		scrollLater(engine, path, 2);
		return;
	}
	engine.events.focusStep(std::move(path));
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
