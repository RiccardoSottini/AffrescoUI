#include "RuntimeImpl.hpp"

namespace affresco::format {

/* Events */

Panel* Runtime::Impl::handlerOwner(Panel* from, std::string_view handler) {
	const bool bubbles = std::ranges::find(bubbling, handler) != std::end(bubbling);
	for(Panel* panel = from; panel; panel = bubbles ? panel->parent() : nullptr) {
		const Record* record = recordOf(panel);
		if(record && record->handlers.contains(std::string(handler))) return record->disabled ? nullptr : panel;
	}
	return nullptr;
}

void Runtime::Impl::fire(Panel& panel, std::string_view handler, json event) {
	Record* record = recordOf(&panel);
	if(!record || record->disabled || !record->handlers.contains(std::string(handler))) return;
	const json steps = record->handlers[std::string(handler)];
	const SharedScope locals = record->locals; // the handler may rebuild the panel
	handle(steps, panel, event, *locals, record->path + "." + std::string(handler));
}

json Runtime::Impl::pointerEvent(std::string_view type, const Panel& panel, Vec2 position) {
	const Rect& f = panel.frame();
	return {{"type", std::string(type)}, {"x", position.x},     {"y", position.y},       {"left", position.x - f.left},
	        {"top", position.y - f.top}, {"width", f.width()}, {"height", f.height()}};
}

bool Runtime::Impl::bubble(Panel& from, std::string_view handler, json event, const Vec2* position) {
	Panel* target = handlerOwner(&from, handler);
	if(!target) return false;
	if(position) {
		json fields = pointerEvent(event.value("type", ""), *target, *position);
		fields.update(event);
		event = std::move(fields);
	}
	fire(*target, handler, std::move(event));
	return true;
}

void Runtime::Impl::setFlagUpwards(Panel* from, bool Record::* flag, bool on) {
	for(Panel* panel = from; panel; panel = panel->parent()) {
		Record* record = recordOf(panel);
		if(!record || record->*flag == on) continue;
		record->*flag = on;
		refresh(*panel, *record);
	}
}

void Runtime::Impl::mouseButton(Panel& panel, const MouseButtonEvent& event) {
	const Vec2 at = event.position;

	if(event.button == MouseButton::Right) {
		if(event.action == affresco::Action::Release && panel.bounds().contains(at))
			bubble(panel, "onContextMenu", {{"type", "contextMenu"}}, &at);
		return;
	}
	if(event.button != MouseButton::Left) return;

	if(event.action == affresco::Action::Press) {
		press = {};
		press.panel = &panel;
		press.position = at;
		for(Panel* p = &panel; p; p = p->parent())
			if(const Record* r = recordOf(p); r && !r->drag.is_null()) {
				if(!r->disabled) press.source = p;
				break;
			}
		setFlagUpwards(&panel, &Record::down, true);
		bubble(panel, "onPress", {{"type", "press"}}, &at);
		return;
	}

	// Release
	setFlagUpwards(&panel, &Record::down, false);
	const Press ended = std::exchange(press, {});
	if(ended.dragging) {
		if(Record* source = recordOf(ended.source)) {
			source->dragging = false;
			refresh(*ended.source, *source);
		}
		if(Record* target = recordOf(ended.over)) {
			target->dragOver = false;
			refresh(*ended.over, *target);
			json drop = pointerEvent("drop", *ended.over, at);
			drop["data"] = ended.data;
			fire(*ended.over, "onDrop", std::move(drop));
		}
		return; // a drag is not a click
	}
	if(!panel.bounds().contains(at)) return;

	bubble(panel, "onRelease", {{"type", "release"}}, &at);

	// A toggle or slider keeps its click (like a native control): it changed its value
	const Record* own = recordOf(&panel);
	if((dynamic_cast<Toggle*>(&panel) || dynamic_cast<Slider*>(&panel)) && (!own || !own->handlers.contains("onClick")))
		return;
	Panel* clicked = handlerOwner(&panel, "onClick");
	if(clicked) bubble(panel, "onClick", {{"type", "click"}}, &at);

	// The second click on the same panel soon after the first
	const double time = now();
	Panel* handler = clicked ? clicked : handlerOwner(&panel, "onDoubleClick");
	if(handler && lastClick.owner == handler && time - lastClick.time < doubleClickSeconds) {
		bubble(panel, "onDoubleClick", {{"type", "doubleClick"}}, &at);
		lastClick = {};
	} else {
		lastClick = {handler, time};
	}
}

void Runtime::Impl::mouseMove(Panel& panel, const MouseMoveEvent& event) {
	if(press.panel != &panel) return;
	const Vec2 at = event.position;

	if(press.source && !press.dragging &&
	   std::hypot(at.x - press.position.x, at.y - press.position.y) > dragThreshold) {
		try {
			Record& source = *recordOf(press.source);
			press.data = evaluate(source.drag.value("data", json()), *source.locals, json::object(), source.path + ".drag");
			press.dragging = true;
			source.dragging = true;
			refresh(*press.source, source);
		} catch(const DocumentError& error) {
			report(error.what());
			press.source = nullptr;
		}
	}
	if(press.dragging && window) {
		Panel* over = nullptr;
		for(Panel* p = window->hoveredPanel(); p; p = p->parent())
			if(const Record* r = recordOf(p); r && r->handlers.contains("onDrop") && !r->disabled) {
				over = p;
				break;
			}
		if(over != press.over) {
			if(Record* old = recordOf(press.over)) {
				old->dragOver = false;
				refresh(*press.over, *old);
			}
			press.over = over;
			if(Record* target = recordOf(over)) {
				target->dragOver = true;
				refresh(*over, *target);
			}
		}
	}

	bubble(panel, "onDrag",
	       {{"type", "drag"}, {"dx", at.x - press.position.x}, {"dy", at.y - press.position.y}}, &at);
}

void Runtime::Impl::key(Panel& panel, const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return;

	// Enter in a text area submits
	if(auto* textArea = dynamic_cast<TextArea*>(&panel);
	   textArea && (event.key == Key::Enter || event.key == Key::KeypadEnter) && event.action == affresco::Action::Press)
		fire(panel, "onSubmit", {{"type", "submit"}, {"text", textArea->text()}});

	// Shortcuts on the focused panel or its ancestors
	const std::string name = shortcut(event);
	if(!name.empty()) {
		for(Panel* p = &panel; p; p = p->parent()) {
			const Record* record = recordOf(p);
			if(!record || !record->keys.is_object() || !record->keys.contains(name)) continue;
			const json handler = record->keys[name];
			const SharedScope locals = record->locals;
			handle(handler, *p, {{"type", "key"}, {"key", name}}, *locals, record->path + ".keys." + name);
			return;
		}
	}
	bubble(panel, "onKey",
	       {{"type", "key"},
	        {"key", keyName(event.key)},
	        {"shortcut", name},
	        {"shift", event.mods.shift()},
	        {"ctrl", event.mods.control()},
	        {"alt", event.mods.alt()},
	        {"cmd", event.mods.super()}});
}

void Runtime::Impl::dismissOverlays(const Vec2* position) {
	bool closed = false;
	for(auto& [panel, record] : records) {
		if(!record.dismissOutside || !record.overlayOpen || !panel->isShown()) continue;
		if(position) {
			if(panel->bounds().contains(*position)) continue;
			const Panel* anchor = record.overlayAnchor.empty() ? panel->parent()
			                                                   : window->root().findById(record.overlayAnchor);
			if(anchor && anchor->bounds().contains(*position)) continue; // the anchor toggles it itself
		}
		try {
			versions.state()[format::statePointer(*record.overlayOpen, context(*record.locals, json::object()),
			                                      record.path + ".overlay.open")] = false;
			closed = true;
		} catch(const DocumentError& error) {
			report(error.what());
		}
	}
	if(closed) {
		changed();
		requestReconcile();
	}
}

} // namespace affresco::format
