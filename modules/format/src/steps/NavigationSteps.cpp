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

/// @brief "focus" or "scrollTo": a panel by id (or a path of ids), after the update and its layout, so it exists and
/// has its place.
/// @param s The step.
void focusOrScroll(const Running& s) {
	const bool focus = s.step.contains("focus");
	std::string path = text(s.value(s.step[focus ? "focus" : "scrollTo"], s.where));
	Engine& engine = s.steps.engine();
	if(!engine.window) return;
	engine.scheduler.once(0.0, [&engine, path, focus] {
		if(Panel* panel = findByPath(engine.window->root(), path)) {
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
