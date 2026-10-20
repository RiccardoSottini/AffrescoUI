/// @file
/// @brief format::Events: input on the panels and the window, turned into a document's handlers.

#include "Events.hpp"
#include "Engine.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace affresco::format {

/* Connections */

void Events::connect(Window& window) {
	// Bindings can use the window size
	windowHooks_.resized =
	    window.onResize.connect([this](const ResizeEvent&) { engine_.reconciler.requestReconcile(); });
	// Screen-wide shortcuts ("keys"); a focused text area keeps plain keys for typing
	windowHooks_.key = window.onKey.connect([this](const KeyEvent& event) { screenKey(event); });
	// A press outside an open overlay that closes on it
	windowHooks_.button = window.onMouseButton.connect([this](const MouseButtonEvent& event) {
		if(event.action == affresco::Action::Press) dismissOverlays(&event.position);
	});
}

// Signals are connected once; handlers read the panel's current record, so later updates need no reconnecting
void Events::hookUp(Panel& panel, Record& adopted) {
	Record::Hooks& hooks = adopted.hooks;
	Reconciler& reconciler = engine_.reconciler;
	hooks.hover = panel.onHover.connect([this, &panel, &reconciler](bool hovered) {
		Record* record = reconciler.recordOf(&panel);
		if(!record) return;
		record->hovered = hovered;
		engine_.properties.refresh(panel, *record);
		fire(panel, hovered ? "onHover" : "onLeave", {{"type", hovered ? "hover" : "leave"}});
	});
	hooks.focus = panel.onFocusChanged.connect([this, &panel, &reconciler](bool focused) {
		Record* record = reconciler.recordOf(&panel);
		if(!record) return;
		record->focused = focused;
		engine_.properties.refresh(panel, *record);
		fire(panel, focused ? "onFocus" : "onBlur", {{"type", focused ? "focus" : "blur"}});
	});
	hooks.button =
	    panel.onMouseButton.connect([this, &panel](const MouseButtonEvent& event) { mouseButton(panel, event); });
	hooks.move = panel.onMouseMove.connect([this, &panel](const MouseMoveEvent& event) { mouseMove(panel, event); });
	hooks.scroll = panel.onScroll.connect([this, &panel](const ScrollEvent& event) {
		bubble(panel, "onScroll", {{"type", "scroll"}, {"dx", event.offset.x}, {"dy", event.offset.y}});
	});
	hooks.key = panel.onKey.connect([this, &panel](const KeyEvent& event) { key(panel, event); });

	// The value the user changes: a text area's text, or a widget's (its type's ValueHooks)
	const auto changed = [this, &panel](const json& value) { valueChanged(panel, value); };
	if(auto* textArea = dynamic_cast<TextArea*>(&panel)) {
		hooks.value = textArea->onTextChanged.connect([changed](const std::string& value) { changed(value); });
	} else {
		for(const auto& set : adopted.sets) {
			if(!set || !set->value.listen) continue;
			hooks.value = set->value.listen(panel, changed);
			break;
		}
	}
	hooks.caret = hookCaret(panel);
}

Connection Events::hookCaret(Panel& panel) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return 0;
	return editor->onCaretMoved.connect([this, &panel, editor](TextEditor::Position caret) {
		Record* record = engine_.reconciler.recordOf(&panel);
		if(!record || !record->caret) return;
		try {
			const std::string where = record->path + ".caret";
			const json::json_pointer pointer =
			    format::statePointer(*record->caret, engine_.document.context(*record->locals, json::object()), where);
			statePlace(engine_.versions.state(), pointer, where) = {
			    {"line", caret.line + 1}, {"column", caret.column + 1}, {"selected", editor->selectedLength()}};
			engine_.document.markStateChanged();
			engine_.reconciler.requestReconcile();
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	});
}

void Events::unhook(Panel& panel, const Record& record) {
	const Record::Hooks& hooks = record.hooks;
	panel.onHover.disconnect(hooks.hover);
	panel.onFocusChanged.disconnect(hooks.focus);
	panel.onMouseButton.disconnect(hooks.button);
	panel.onMouseMove.disconnect(hooks.move);
	panel.onScroll.disconnect(hooks.scroll);
	panel.onKey.disconnect(hooks.key);
	if(auto* textArea = dynamic_cast<TextArea*>(&panel)) {
		textArea->onTextChanged.disconnect(hooks.value);
	} else {
		for(const auto& set : record.sets) {
			if(!set || !set->value.stopListening) continue;
			set->value.stopListening(panel, hooks.value);
			break;
		}
	}
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) editor->onCaretMoved.disconnect(hooks.caret);
}

// Only the panels still in the window are reached: a panel the application removed itself is gone, and its record
// is never read again
void Events::disconnectAll() {
	Window* window = engine_.window;
	if(!window) return;
	window->onResize.disconnect(windowHooks_.resized);
	window->onKey.disconnect(windowHooks_.key);
	window->onMouseButton.disconnect(windowHooks_.button);
	std::vector<Panel*> pending{&window->root()};
	while(!pending.empty()) {
		Panel* panel = pending.back();
		pending.pop_back();
		if(const Record* record = engine_.reconciler.recordOf(panel)) unhook(*panel, *record);
		for(const auto& child : panel->children()) pending.push_back(child.get());
	}
}

void Events::forget(const Panel& panel) noexcept {
	if(press_.panel == &panel) press_ = {};
	if(press_.source == &panel) press_.source = nullptr;
	if(press_.over == &panel) press_.over = nullptr;
	if(lastClick_.owner == &panel) lastClick_.owner = nullptr;
}

void Events::valueChanged(Panel& panel, const json& value) {
	Record* record = engine_.reconciler.recordOf(&panel);
	if(!record) return;
	if(record->bind) {
		try {
			const std::string where = record->path + ".bind";
			const json::json_pointer pointer =
			    format::statePointer(*record->bind, engine_.document.context(*record->locals, json::object()), where);
			statePlace(engine_.versions.state(), pointer, where) = value;
			engine_.document.markStateChanged();
			engine_.reconciler.requestReconcile();
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	}
	fire(panel, "onChange", {{"type", "change"}, {"value", value}});
}

void Events::screenKey(const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return;
	if(event.key == Key::Escape) dismissOverlays(nullptr);
	const json& document = engine_.versions.document();
	if(!document.contains("keys") || !document["keys"].is_object()) return;
	const std::string name = shortcut(event);
	const Panel* focused = engine_.window->focusedPanel();
	const bool typing = dynamic_cast<const TextArea*>(focused) || dynamic_cast<const TextEditor*>(focused);
	if(name.empty() ||
	   (typing && !event.mods.super() && !event.mods.control() && !event.mods.alt() && name != "escape"))
		return;
	for(const auto& [written, handler] : document["keys"].items()) {
		if(normalizeShortcut(written) != name) continue;
		const json steps = handler;
		engine_.runner.handle(steps, engine_.window->root(), {{"type", "key"}, {"key", name}}, *noVariables(),
		                      "keys." + written);
		return;
	}
}

/* Handlers */

Panel* Events::handlerOwner(Panel* from, std::string_view handler) {
	const bool bubbles = std::ranges::find(bubbling, handler) != std::end(bubbling);
	for(Panel* panel = from; panel; panel = bubbles ? panel->parent() : nullptr) {
		const Record* record = engine_.reconciler.recordOf(panel);
		if(record && record->handlers.contains(std::string(handler))) return record->disabled ? nullptr : panel;
	}
	return nullptr;
}

void Events::fire(Panel& panel, std::string_view handler, const json& event) {
	Record* record = engine_.reconciler.recordOf(&panel);
	if(!record || record->disabled || !record->handlers.contains(std::string(handler))) return;
	const json steps = record->handlers[std::string(handler)];
	const SharedScope locals = record->locals; // the handler may rebuild the panel
	engine_.runner.handle(steps, panel, event, *locals, record->path + "." + std::string(handler));
}

json Events::pointerEvent(std::string_view type, const Panel& panel, Vec2 position) {
	const Rect& f = panel.frame();
	return {{"type", std::string(type)}, {"x", position.x},     {"y", position.y},       {"left", position.x - f.left},
	        {"top", position.y - f.top}, {"width", f.width()}, {"height", f.height()}};
}

bool Events::bubble(Panel& from, std::string_view handler, json event, const Vec2* position) {
	Panel* target = handlerOwner(&from, handler);
	if(!target) return false;
	if(position) {
		json fields = pointerEvent(event.value("type", ""), *target, *position);
		fields.update(event);
		event = std::move(fields);
	}
	fire(*target, handler, event);
	return true;
}

void Events::setFlag(Panel* panel, bool Record::* flag, bool on) {
	if(Record* record = engine_.reconciler.recordOf(panel)) {
		record->*flag = on;
		engine_.properties.refresh(*panel, *record);
	}
}

void Events::setFlagUpwards(Panel* from, bool Record::* flag, bool on) {
	for(Panel* panel = from; panel; panel = panel->parent()) {
		Record* record = engine_.reconciler.recordOf(panel);
		if(!record || record->*flag == on) continue;
		record->*flag = on;
		engine_.properties.refresh(*panel, *record);
	}
}

/* Pointer */

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
	if(engine_.reconciler.recordOf(ended.over)) {
		setFlag(ended.over, &Record::dragOver, false);
		json drop = pointerEvent("drop", *ended.over, at);
		drop["data"] = ended.data;
		fire(*ended.over, "onDrop", drop);
	}
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
			Record& source = *engine_.reconciler.recordOf(press_.source);
			press_.data = engine_.document.evaluate(source.drag.value("data", json()), *source.locals, json::object(),
			                                        source.path + ".drag");
			press_.dragging = true;
			source.dragging = true;
			engine_.properties.refresh(*press_.source, source);
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			press_.source = nullptr;
		}
	}
	if(press_.dragging && engine_.window) dragOver();

	bubble(panel, "onDrag", {{"type", "drag"}, {"dx", at.x - press_.position.x}, {"dy", at.y - press_.position.y}},
	       &at);
}

void Events::dragOver() {
	Panel* over = nullptr;
	for(Panel* p = engine_.window->hoveredPanel(); p; p = p->parent())
		if(const Record* r = engine_.reconciler.recordOf(p); r && r->handlers.contains("onDrop") && !r->disabled) {
			over = p;
			break;
		}
	if(over == press_.over) return;
	setFlag(press_.over, &Record::dragOver, false);
	press_.over = over;
	setFlag(over, &Record::dragOver, true);
}

/* Keys and overlays */

void Events::key(Panel& panel, const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return;

	// Enter in a text area submits
	if(auto* textArea = dynamic_cast<TextArea*>(&panel);
	   textArea && (event.key == Key::Enter || event.key == Key::KeypadEnter) && event.action == affresco::Action::Press)
		fire(panel, "onSubmit", {{"type", "submit"}, {"text", textArea->text()}});

	// Shortcuts on the focused panel or its ancestors
	const std::string name = shortcut(event);
	if(!name.empty()) {
		for(Panel* p = &panel; p; p = p->parent()) {
			const Record* record = engine_.reconciler.recordOf(p);
			if(!record || !record->keys.is_object() || !record->keys.contains(name)) continue;
			const json handler = record->keys[name];
			const SharedScope locals = record->locals;
			engine_.runner.handle(handler, *p, {{"type", "key"}, {"key", name}}, *locals,
			                      record->path + ".keys." + name);
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

void Events::dismissOverlays(const Vec2* position) {
	bool closed = false;
	for(auto& [panel, record] : engine_.reconciler.records()) {
		if(!record.dismissOutside || !record.overlayOpen || !panel->isShown()) continue;
		if(position) {
			if(panel->bounds().contains(*position)) continue;
			const Panel* anchor =
			    record.overlayAnchor.empty() ? panel->parent() : engine_.window->root().findById(record.overlayAnchor);
			// The anchor toggles it itself; the root holds every point, so it is no anchor
			if(anchor && anchor != &engine_.window->root() && anchor->bounds().contains(*position)) continue;
		}
		try {
			const std::string where = record.path + ".overlay.open";
			const json::json_pointer pointer = format::statePointer(
			    *record.overlayOpen, engine_.document.context(*record.locals, json::object()), where);
			statePlace(engine_.versions.state(), pointer, where) = false;
			closed = true;
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	}
	if(closed) {
		engine_.document.markStateChanged();
		engine_.reconciler.requestReconcile();
	}
}

} // namespace affresco::format
