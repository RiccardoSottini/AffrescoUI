/// @file
/// @brief format::Events, the pointer: presses, releases, clicks and double clicks, drag and drop.

#include "Events.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco::format {

namespace {

/// @brief The drag hooks of a panel's widget, if it has them.
/// @param record The panel's record, or nullptr.
/// @return Its hooks, or nullptr.
const DragHooks* dragHooks(const Record* record) {
	if(!record) return nullptr;
	for(const auto& set : record->sets)
		if(set && (set->drag.data || set->drag.dropAt)) return &set->drag;
	return nullptr;
}

} // namespace

void Events::mouseButton(Panel& panel, const MouseButtonEvent& event) {
	const Vec2 at = event.position;
	if(event.button == MouseButton::Right) {
		if(event.action == affresco::Action::Release && panel.bounds().contains(at))
			bubble(panel, "onContextMenu", {{"type", "contextMenu"}}, &at);
		return;
	}
	if(event.button != MouseButton::Left) return;
	if(event.action == affresco::Action::Press) pressed(panel, at);
	else released(panel, at);
}

void Events::pressed(Panel& panel, Vec2 at) {
	press_ = {};
	press_.panel = &panel;
	press_.position = at;
	for(Panel* p = &panel; p; p = p->parent())
		if(const Record* r = engine_.reconciler.recordOf(p); r && !r->drag.is_null()) {
			if(!r->disabled) press_.source = p;
			break;
		}
	setFlagUpwards(&panel, &Record::down, true);
	bubble(panel, "onPress", {{"type", "press"}}, &at);
}

void Events::dropped(const Press& ended, Vec2 at) {
	setFlag(ended.source, &Record::dragging, false);
	const Record* target = engine_.reconciler.recordOf(ended.over);
	if(!target) return;
	setFlag(ended.over, &Record::dragOver, false);
	if(engine_.window) at = ended.over->toLocal(engine_.window->pointer()).value_or(at); // in the target's space
	json drop = pointerEvent("drop", *ended.over, at);
	if(const DragHooks* hooks = dragHooks(target); hooks && hooks->dropAt) {
		const bool own = hooks->ownDrops && ended.over == ended.source; // e.g. a tab strip reorders itself
		if(!own) drop.update(hooks->dropAt(*ended.over, &at));
		hooks->dropAt(*ended.over, nullptr);
		if(own) return;
	}
	drop["data"] = ended.data;
	fire(*ended.over, "onDrop", drop);
}

void Events::released(Panel& panel, Vec2 at) {
	setFlagUpwards(&panel, &Record::down, false);
	const Press ended = std::exchange(press_, {});
	if(ended.dragging) {
		dropped(ended, at);
		return; // a drag is not a click
	}
	if(!panel.bounds().contains(at)) return;

	bubble(panel, "onRelease", {{"type", "release"}}, &at);

	// A toggle or slider keeps its click (like a native control): it changed its value
	const Record* own = engine_.reconciler.recordOf(&panel);
	const PropertySets sets = own ? own->sets : engine_.types.setsFor(nullptr, panel);
	if(std::ranges::any_of(sets, [](const auto& set) { return set && set->keepsClicks; }) &&
	   (!own || !own->handlers.contains("onClick")))
		return;
	Panel* clicked = handlerOwner(&panel, "onClick");
	if(clicked) bubble(panel, "onClick", {{"type", "click"}}, &at);

	// The second click on the same panel soon after the first
	const double time = engine_.now();
	Panel* handler = clicked ? clicked : handlerOwner(&panel, "onDoubleClick");
	if(handler && lastClick_.owner == handler && time - lastClick_.time < doubleClickSeconds) {
		bubble(panel, "onDoubleClick", {{"type", "doubleClick"}}, &at);
		lastClick_ = {};
	} else {
		lastClick_ = {handler, time};
	}
}

void Events::mouseMove(Panel& panel, const MouseMoveEvent& event) {
	if(press_.panel != &panel) return;
	const Vec2 at = event.position;

	if(press_.source && !press_.dragging &&
	   std::hypot(at.x - press_.position.x, at.y - press_.position.y) > dragThreshold) {
		try {
			const DocumentContext::BudgetScope budget(engine_.document); // the data is evaluated only now
			Record& source = *engine_.reconciler.recordOf(press_.source);
			const DragHooks* hooks = dragHooks(&source);
			if(source.drag.contains("data") || !hooks || !hooks->data)
				press_.data = engine_.document.evaluate(source.drag.value("data", json()), *source.locals,
				                                        json::object(), source.path + ".drag");
			else // the widget says what is under the press, e.g. a tab
				press_.data = hooks->data(*press_.source, press_.position);
			if(press_.data.is_null() && hooks && hooks->data) {
				press_.source = nullptr; // nothing to drag there
			} else {
				press_.dragging = true;
				source.dragging = true;
				engine_.properties.refresh(*press_.source, source);
			}
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			press_.source = nullptr;
		}
	}
	if(press_.dragging && engine_.window) dragOver(at);

	bubble(panel, "onDrag", {{"type", "drag"}, {"dx", at.x - press_.position.x}, {"dy", at.y - press_.position.y}},
	       &at);
}

void Events::dragOver(Vec2 at) {
	Panel* over = nullptr;
	for(Panel* p = engine_.window->hoveredPanel(); p; p = p->parent())
		if(const Record* r = engine_.reconciler.recordOf(p); r && r->handlers.contains("onDrop") && !r->disabled) {
			over = p;
			break;
		}
	// A widget target shows where the drop would go (one that handles drops on itself already does)
	const DragHooks* hooks = dragHooks(engine_.reconciler.recordOf(over));
	if(hooks && hooks->dropAt && (!hooks->ownDrops || over != press_.source)) {
		const Vec2 there = over->toLocal(engine_.window->pointer()).value_or(at); // in the target's space
		static_cast<void>(hooks->dropAt(*over, &there));
	}
	if(over == press_.over) return;
	if(const DragHooks* left = dragHooks(engine_.reconciler.recordOf(press_.over)); left && left->dropAt)
		static_cast<void>(left->dropAt(*press_.over, nullptr));
	setFlag(press_.over, &Record::dragOver, false);
	press_.over = over;
	setFlag(over, &Record::dragOver, true);
}

} // namespace affresco::format
