/// @file
/// @brief format::Events, the focus and overlays: modal overlays, the tab order, and overlays closed by a click
/// outside.

#include "Events.hpp"
#include "Engine.hpp"

#include "affresco/render/Controls.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <ranges>
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

/// @brief The tab order being collected inside a scope: the stops with a positive "tabIndex", sorted by it, then the
/// rest in document order.
struct TabOrder {
	Reconciler& reconciler;       ///< The records.
	const Panel* focused;         ///< The focused panel, or nullptr: Tab stops on a list's focused item.
	bool list;                    ///< An "arrows" scope: document order only.
	std::vector<Panel*> positive; ///< The stops with a positive "tabIndex", sorted by it.
	std::vector<Panel*> rest;     ///< The others, in document order.

	/// @brief Adds a stop.
	/// @param panel The stop.
	/// @param tabIndex The "tabIndex" that places it.
	void add(Panel* panel, int tabIndex) {
		if(tabIndex <= 0 || list) {
			rest.push_back(panel);
			return;
		}
		const auto after = std::ranges::upper_bound(positive, tabIndex, {},
		                                            [this](Panel* p) { return reconciler.recordOf(p)->tabIndex; });
		positive.insert(after, panel);
	}
};

std::vector<Panel*> focusablesIn(Reconciler& reconciler, const Panel* focused, Panel& scope, bool list);

/// @brief Tells whether a panel is a tab stop: focusable or a text input, with a "tabIndex" of 0 or more.
/// @param panel The panel.
/// @param record Its record.
/// @return true if it is.
bool tabStop(const Panel& panel, const Record& record) {
	const bool input = dynamic_cast<const TextArea*>(&panel) || dynamic_cast<const TextEditor*>(&panel);
	return record.tabIndex >= 0 && (record.focusable || input);
}

/// @brief Adds the one stop Tab makes in an "arrows" node: its focused item, else its first, placed by the first
/// item's "tabIndex".
/// @param order The tab order.
/// @param list The "arrows" node.
void addListStop(TabOrder& order, Panel& list) {
	const std::vector<Panel*> items = focusablesIn(order.reconciler, order.focused, list, true);
	if(items.empty()) return;
	const auto it = std::ranges::find(items, order.focused);
	order.add(it != items.end() ? *it : items.front(), order.reconciler.recordOf(items.front())->tabIndex);
}

/// @brief Collects the stops in a panel and its descendants: shown, not disabled, skipping nested modal overlays
/// (for Tab) or nested "arrows" nodes (for a list).
/// @param scope The scope, which is no stop itself.
/// @param panel The panel to look in.
/// @param order The tab order.
void collect(const Panel& scope, Panel& panel, TabOrder& order) {
	if(!panel.isShown()) return;
	const Record* record = order.reconciler.recordOf(&panel);
	if(record && record->disabled) return;
	if(record && &panel != &scope) {
		if(order.list ? !record->arrows.empty() : record->modal) return;
		if(!record->arrows.empty()) { // Tab stops once in a list
			addListStop(order, panel);
			return;
		}
		if(tabStop(panel, *record)) order.add(&panel, record->tabIndex);
	}
	for(const auto& child : panel.children()) collect(scope, *child, order);
}

/// @brief The panels the focus can move to inside a scope, in tab order (Events::focusables).
/// @param reconciler The records.
/// @param focused The focused panel, or nullptr.
/// @param scope The scope.
/// @param list true for an "arrows" scope, false for Tab.
/// @return Them.
std::vector<Panel*> focusablesIn(Reconciler& reconciler, const Panel* focused, Panel& scope, bool list) {
	TabOrder order{reconciler, focused, list, {}, {}};
	collect(scope, scope, order);
	order.positive.insert(order.positive.end(), order.rest.begin(), order.rest.end());
	return std::move(order.positive);
}

/// @brief Tells whether a panel is a scope or inside it.
/// @param panel The panel, or nullptr.
/// @param scope The scope.
/// @return true if it is.
bool within(const Panel* panel, const Panel& scope) {
	for(; panel; panel = panel->parent())
		if(panel == &scope) return true;
	return false;
}

} // namespace

void Events::modal(Panel& overlay, bool open) {
	const Window* window = engine_.window;
	if(!window) return;
	if(open) {
		Panel* focused = window->focusedPanel();
		modals_.push_back({&overlay, engine_.reconciler.recordOf(focused) ? focused : nullptr});
		// A "focus" already scheduled inside it, by the steps that opened it, wins over its first focusable node
		const Panel* scheduled = focusLater_.panel ? focusLater_.panel : focusLater_.scope;
		if(!focusLater_.pending || focusLater_.cancelled || !within(scheduled, overlay)) focusLater(nullptr, &overlay);
		return;
	}
	const auto it = std::ranges::find(modals_, &overlay, &Modal::overlay);
	if(it == modals_.end()) return;
	Panel* previous = it->previous;
	modals_.erase(it);
	const bool inside =
	    (focusLater_.pending && focusLater_.scope == &overlay) || within(window->focusedPanel(), overlay);
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
		if(target.scope && within(engine_.window->focusedPanel(), *target.scope)) return; // already inside: kept
		if(target.scope) {
			const std::vector<Panel*> inside = focusables(*target.scope, false);
			focus = inside.empty() ? target.scope : inside.front();
		}
		engine_.window->setFocus(focus);
	});
}

void Events::focus(Panel& panel) {
	if(focusLater_.pending) focusLater_.cancelled = true; // a later "focus" wins over one scheduled before
	const Record* record = engine_.reconciler.recordOf(&panel);
	if(engine_.window && record && !tabStop(panel, *record) && within(engine_.window->focusedPanel(), panel)) return;
	if(!engine_.window || !record || tabStop(panel, *record)) {
		if(engine_.window) engine_.window->setFocus(&panel);
		return;
	}
	if(const std::vector<Panel*> inside = focusables(panel, false); !inside.empty()) {
		engine_.window->setFocus(inside.front());
	} else if(!panel.isShown()) {
		focusLater(nullptr, &panel); // not laid out yet (an overlay just opened): its first focusable node then
	} else {
		engine_.window->setFocus(&panel);
	}
}

std::vector<Panel*> Events::focusables(Panel& scope, bool list) {
	return focusablesIn(engine_.reconciler, engine_.window ? engine_.window->focusedPanel() : nullptr, scope, list);
}

Panel& Events::modalScope() {
	for(const Modal& modal : std::views::reverse(modals_))
		if(modal.overlay->isShown()) return *modal.overlay;
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
