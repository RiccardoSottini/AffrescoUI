/// @file
/// @brief The steps that move the user: focus, scrollTo, navigate, dismiss.

#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief The panel a path of ids names: "#a #b" is the "b" inside "a", as in CSS, so that a component's inner node
/// can be told apart between its uses.
/// @param root Where the search starts.
/// @param path Ids separated by spaces, each with or without its "#".
/// @return The panel, or null when one of the ids isn't found.
Panel* findByPath(Panel& root, std::string_view path) {
	Panel* panel = &root;
	for(std::size_t at = 0; panel && at < path.size();) {
		const std::size_t start = path.find_first_not_of(' ', at);
		if(start == std::string_view::npos) break;
		const std::size_t end = std::min(path.find(' ', start), path.size());
		std::string_view id = path.substr(start, end - start);
		if(id.starts_with('#')) id.remove_prefix(1);
		panel = panel->findById(id);
		at = end;
	}
	return panel == &root ? nullptr : panel;
}

/// @brief Scrolls a panel by id (or a path of ids) into view once it has its place: deferred tasks run before the
/// frame's layout, so a panel the update just made, or one in an overlay just opened, waits for the layouts that place
/// it.
/// @param engine The engine.
/// @param path The panel's id, or a path of ids.
/// @param frames The frames it may still wait.
void scrollLater(Engine& engine, const std::string& path, int frames) {
	engine.scheduler.once(0.0, [&engine, path, frames] {
		if(!engine.window) return;
		Panel* panel = findByPath(engine.window->root(), path);
		if(!panel) return;
		if(!panel->isShown() && panel->isVisible() && frames > 0) scrollLater(engine, path, frames - 1);
		else panel->scrollIntoView();
	});
}

/// @brief "focus" or "scrollTo": a panel by id (or a path of ids), after the update and its layout, so it exists and
/// has its place. A panel that takes no focus passes it to its first focusable node.
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
	engine.scheduler.once(0.0, [&engine, path] {
		if(Panel* panel = findByPath(engine.window->root(), path)) engine.events.focus(*panel);
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
