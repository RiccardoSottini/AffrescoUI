/// @file
/// @brief format::Events: input on the panels and the window, turned into a document's handlers: the connections,
/// the values widgets report, and the handlers and their bubbling.

#include "Events.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "Settings.hpp"

#include "affresco/render/Controls.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief Runs a handler and counts it for the usage summaries (VERSION_CONTROL.md, "Usage"): the node's id as
/// written, the handler and the action it names, whether it reported an error, and the screen it led to. Never what
/// the event or the state hold.
/// @param engine The runtime; its usage summaries are on.
/// @param record The panel's record (the handler may rebuild the panel, so it is read first).
/// @param panel The panel.
/// @param handler E.g. "onClick".
/// @param event The event.
/// @param steps The handler.
/// @param locals Its variables.
void runCounted(Engine& engine, const Record& record, Panel& panel, std::string_view handler, const json& event,
                const json& steps, const Scope& locals) {
	const json id = record.node ? record.node->own.value("id", json()) : json();
	const std::string action = std::string(handler) + (steps.is_string() ? ":" + steps.get<std::string>() : "");
	const std::string screen = engine.document.screen();
	bool failed = false;
	const Connection watch = engine.owner.onError.connect([&failed](const std::string&) { failed = true; });
	engine.runner.handle(steps, panel, event, locals, record.path + "." + std::string(handler));
	engine.owner.onError.disconnect(watch);
	const std::int64_t now = Usage::now();
	if(id.is_string()) engine.owner.usage.ran(id.get<std::string>(), action, failed, now);
	if(engine.document.screen() != screen) engine.owner.usage.visited(engine.document.screen(), now);
}

} // namespace

/* Connections */

void Events::connect(Window& window) {
	// Bindings can use the window size
	windowHooks_.resized =
	    window.onResize.connect([this](const ResizeEvent&) { engine_.reconciler.requestReconcile(); });
	// Screen-wide shortcuts ("keys"); a focused text area keeps plain keys for typing
	windowHooks_.key = window.onKey.connect([this](const KeyEvent& event) {
		if(!keyResolved_ && engine_.window) {
			Panel* focused = engine_.window->focusedPanel();
			if(!resolve(focused, event)) navigate(focused, event);
		}
		keyResolved_ = false;
		// Bindings can use window.capsLock
		if(event.mods.capsLock() != capsLock_) {
			capsLock_ = event.mods.capsLock();
			engine_.reconciler.requestReconcile();
		}
	});
	// The display's scale or the system's colour scheme changed: bindings can use them
	windowHooks_.display =
	    window.onDisplay.connect([this](const DisplayEvent&) { engine_.reconciler.requestReconcile(); });
	// Files dropped from the OS: the node under the pointer with "onDrop" (or an ancestor) gets their paths
	windowHooks_.drop = window.onDrop.connect([this](const DropEvent& event) {
		if(!engine_.window) return;
		Panel* over = engine_.window->panelAt(event.position);
		if(over) bubble(*over, "onDrop", {{"type", "drop"}, {"files", event.paths}}, &event.position);
	});
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
		engine_.document.contextChanged();
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
	for(const auto& set : adopted.sets) {
		if(!set || !set->events.listen) continue;
		hooks.events = set->events.listen(
		    panel, [this, &panel](std::string_view handler, const json& event) { fire(panel, handler, event); });
		break;
	}
	hooks.caret = hookCaret(panel);
	hooks.link = hookLink(panel);
}

Connection Events::hookLink(Panel& panel) {
	auto* label = dynamic_cast<Label*>(&panel);
	if(!label) return 0;
	return label->onLink.connect(
	    [this, &panel](const std::string& link) { fire(panel, "onLink", {{"type", "link"}, {"link", link}}); });
}

Connection Events::hookCaret(Panel& panel) {
	auto* editor = dynamic_cast<TextEditor*>(&panel);
	if(!editor) return 0;
	return editor->onCaretMoved.connect([this, &panel, editor](TextEditor::Position caret) {
		Record* record = engine_.reconciler.recordOf(&panel);
		if(!record || !record->caret) return;
		try {
			const DocumentContext::BudgetScope budget(engine_.document); // the path may hold expressions
			const std::string where = record->path + ".caret";
			const json::json_pointer pointer =
			    format::statePointer(*record->caret, engine_.document.context(*record->locals, json::object()), where);
			json written = {
			    {"line", caret.line + 1}, {"column", caret.column + 1}, {"selected", editor->selectedLength()}};
			if(editor->finding()) written["matches"] = editor->matchCount();
			statePlace(engine_.versions.state(), pointer, where) = std::move(written);
			engine_.document.markStateChanged(pointer);
			engine_.reconciler.requestStateUpdate();
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
	for(const auto& set : record.sets) {
		if(!set || !set->events.stopListening) continue;
		set->events.stopListening(panel, hooks.events);
		break;
	}
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) editor->onCaretMoved.disconnect(hooks.caret);
	if(auto* label = dynamic_cast<Label*>(&panel)) label->onLink.disconnect(hooks.link);
}

// Only the panels still in the window are reached: a panel the application removed itself is gone, and its record
// is never read again
void Events::disconnectAll() {
	Window* window = engine_.window;
	if(!window) return;
	window->onResize.disconnect(windowHooks_.resized);
	window->onKey.disconnect(windowHooks_.key);
	window->onMouseButton.disconnect(windowHooks_.button);
	window->onDisplay.disconnect(windowHooks_.display);
	window->onDrop.disconnect(windowHooks_.drop);
	hideMenuBar(engine_);
	window->setFocusRing(std::nullopt);
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
	std::erase_if(modals_, [&panel](const Modal& open) { return open.overlay == &panel; });
	for(Modal& open : modals_)
		if(open.previous == &panel) open.previous = nullptr;
	if(focusLater_.panel == &panel || focusLater_.scope == &panel) focusLater_.cancelled = true;
}

void Events::valueChanged(Panel& panel, const json& value) {
	Record* record = engine_.reconciler.recordOf(&panel);
	if(!record) return;
	if(record->bind) {
		try {
			const DocumentContext::BudgetScope budget(engine_.document);
			const std::string where = record->path + ".bind";
			const json& document = engine_.versions.document();
			const std::optional<json::json_pointer> setting = settingPointer(document, *record->bind, where);
			if(setting) checkSettingValue(document, setting->back(), &value, where); // "bind": "settings.x"
			const json::json_pointer pointer =
			    setting ? *setting
				        : format::statePointer(*record->bind, engine_.document.context(*record->locals, json::object()),
				                               where);
			statePlace(engine_.versions.state(), pointer, where) = value;
			engine_.document.markStateChanged(pointer);
			engine_.reconciler.requestStateUpdate();
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	}
	fire(panel, "onChange", {{"type", "change"}, {"value", value}});
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
	if(engine_.owner.usage.enabled()) return runCounted(engine_, *record, panel, handler, event, steps, *locals);
	engine_.runner.handle(steps, panel, event, *locals, record->path + "." + std::string(handler));
}

json Events::pointerEvent(std::string_view type, const Panel& panel, Vec2 position) {
	const Rect& f = panel.frame();
	return {{"type", std::string(type)}, {"x", position.x},    {"y", position.y},     {"left", position.x - f.left},
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

} // namespace affresco::format
