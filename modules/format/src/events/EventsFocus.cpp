/// @file
/// @brief format::Events, the focus and overlays: modal overlays, the tab order, and overlays closed by a click
/// outside.

#include "Events.hpp"
#include "Engine.hpp"

#include "affresco/render/Controls.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief Tells whether a point is on a shown panel inside another, an overlay such as a submenu being one.
/// @param panel The outer panel.
/// @param position The point.
/// @return true if it is.
bool insideShown(const Panel& panel, Vec2 position) {
	if(!panel.isShown()) return false;
	if(panel.bounds().contains(position)) return true;
	return std::ranges::any_of(panel.children(), [&](const auto& child) { return insideShown(*child, position); });
}

} // namespace

void Events::modal(Panel& overlay, bool open) {
	Window* window = engine_.window;
	if(!window) return;
	if(open) {
		Panel* focused = window->focusedPanel();
		modals_.push_back({&overlay, engine_.reconciler.recordOf(focused) ? focused : nullptr});
		focusLater(nullptr, &overlay);
		return;
	}
	const auto it = std::ranges::find(modals_, &overlay, &Modal::overlay);
	if(it == modals_.end()) return;
	Panel* previous = it->previous;
	modals_.erase(it);
	bool inside = focusLater_.pending && focusLater_.scope == &overlay;
	for(Panel* p = window->focusedPanel(); p && !inside; p = p->parent()) inside = p == &overlay;
	if(inside) focusLater(previous, nullptr);
}

void Events::focusLater(Panel* panel, Panel* scope, int frames) {
	focusLater_.panel = panel;
	focusLater_.scope = scope;
	focusLater_.cancelled = false;
	if(std::exchange(focusLater_.pending, true)) return; // one is scheduled already, and does this instead
	engine_.scheduler.once(0.0, [this, frames] {
		const auto target = std::exchange(focusLater_, {});
		if(target.cancelled || !engine_.window) return;
		// Deferred tasks run before the frame's layout, which is what shows an overlay just opened
		if(target.scope && !target.scope->isShown() && target.scope->isVisible() && frames > 0) {
			focusLater(target.panel, target.scope, frames - 1);
			return;
		}
		Panel* focus = target.panel;
		if(target.scope) {
			const std::vector<Panel*> inside = focusables(*target.scope, false);
			focus = inside.empty() ? target.scope : inside.front();
		}
		engine_.window->setFocus(focus);
	});
}

std::vector<Panel*> Events::focusables(Panel& scope, bool list) {
	std::vector<Panel*> result;
	std::vector<Panel*> positive; // a "tabIndex" over 0
	const auto add = [&](Panel* panel, const Record& record) {
		if(record.tabIndex <= 0 || list) {
			result.push_back(panel);
			return;
		}
		const auto after = std::ranges::upper_bound(positive, record.tabIndex, {}, [this](Panel* p) {
			return engine_.reconciler.recordOf(p)->tabIndex;
		});
		positive.insert(after, panel);
	};
	// The lambda takes itself to recurse: GCC 13 has no explicit object parameter ("this auto")
	const auto visit = [&](const auto& self, Panel& panel) -> void {
		if(!panel.isShown()) return;
		const Record* record = engine_.reconciler.recordOf(&panel);
		if(record && record->disabled) return;
		if(record && &panel != &scope) {
			if(list ? !record->arrows.empty() : record->modal) return;
			if(!list && !record->arrows.empty()) { // Tab stops once in a list: on its focused item, else its first
				const std::vector<Panel*> items = focusables(panel, true);
				Panel* focused = engine_.window ? engine_.window->focusedPanel() : nullptr;
				const auto it = std::ranges::find(items, focused);
				if(!items.empty()) add(it != items.end() ? *it : items.front(), *engine_.reconciler.recordOf(*items.begin()));
				return;
			}
		}
		const bool input = dynamic_cast<const TextArea*>(&panel) || dynamic_cast<const TextEditor*>(&panel);
		if(record && &panel != &scope && record->tabIndex >= 0 && (record->focusable || input)) add(&panel, *record);
		for(const auto& child : panel.children()) self(self, *child);
	};
	visit(visit, scope);
	positive.insert(positive.end(), result.begin(), result.end());
	return positive;
}

Panel& Events::modalScope() {
	for(auto it = modals_.rbegin(); it != modals_.rend(); ++it)
		if(it->overlay->isShown()) return *it->overlay;
	return engine_.window->root();
}

// Without an "anchor", the parent is the anchor; the root holds every point, so it is no anchor
bool Events::clickKeepsOpen(const Panel& panel, const Record& record, Vec2 position) const {
	if(insideShown(panel, position)) return true; // a submenu open from it counts as inside
	Panel& root = engine_.window->root();
	const Panel* anchor = record.overlayAnchor.empty() ? panel.parent() : root.findById(record.overlayAnchor);
	return anchor && anchor != &root && anchor->bounds().contains(position);
}

void Events::dismissOverlays(const Vec2* position) {
	const DocumentContext::BudgetScope budget(engine_.document); // one for all the overlays' paths
	bool closed = false;
	std::vector<json::json_pointer> changed; // marked after the loop: no record is touched while it runs
	for(auto& [panel, record] : engine_.reconciler.records()) {
		if(!record.dismissOutside || !record.overlayOpen || !panel->isShown()) continue;
		if(position && clickKeepsOpen(*panel, record, *position)) continue;
		try {
			const std::string where = record.path + ".overlay.open";
			const json::json_pointer pointer = format::statePointer(
			    *record.overlayOpen, engine_.document.context(*record.locals, json::object()), where);
			statePlace(engine_.versions.state(), pointer, where) = false;
			changed.push_back(pointer);
			closed = true;
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	}
	if(closed) {
		for(const json::json_pointer& pointer : changed) engine_.document.markStateChanged(pointer);
		engine_.reconciler.requestStateUpdate();
	}
}

} // namespace affresco::format
