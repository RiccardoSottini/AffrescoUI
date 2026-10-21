/// @file
/// @brief format::Events: input on the panels and the window, turned into a document's handlers.

#include "Events.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "Settings.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

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
			statePlace(engine_.versions.state(), pointer, where) = {
			    {"line", caret.line + 1}, {"column", caret.column + 1}, {"selected", editor->selectedLength()}};
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

bool Events::resolve(Panel* from, const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return false;
	const std::string name = shortcut(event);
	if(name.empty()) return false;
	const bool chording = !chord_.empty();
	if(name == "escape") {
		if(chording) { // Escape ends a chord, and does nothing else
			chord_.clear();
			engine_.document.contextChanged();
			return true;
		}
		dismissOverlays(nullptr);
	}
	const std::string press = chording ? chord_ + " " + name : name;
	const std::string prefix = press + " ";
	bool waits = false; // some key starts with this press
	const auto starts = [&](const json& keys, bool normalise) {
		if(!keys.is_object()) return;
		for(const auto& [written, handler] : keys.items())
			waits = waits || (normalise ? normalizeChord(written) : written).starts_with(prefix);
	};
	const auto finish = [&](bool ran) {
		if(!ran && waits) chord_ = press; // the press starts a chord, or goes on with one
		else chord_.clear();              // it ran, or ends a chord, which drops it
		if(chording || !chord_.empty()) engine_.document.contextChanged();
		return ran || waits || chording;
	};

	// The focused panel and its ancestors
	for(Panel* p = from; p; p = p->parent()) {
		const Record* record = engine_.reconciler.recordOf(p);
		if(!record || !record->keys.is_object()) continue;
		starts(record->keys, false);
		if(!record->keys.contains(press)) continue;
		const json handler = record->keys[press]; // a copy: the handler may revise the document
		const SharedScope locals = record->locals;
		if(runBinding(handler, *p, *locals, press, record->path + ".keys." + press)) return finish(true);
	}

	// The document's; a focused text input keeps plain keys for typing
	const json& document = engine_.versions.document();
	const bool typing = (dynamic_cast<const TextArea*>(from) || dynamic_cast<const TextEditor*>(from)) && !chording &&
	                    !event.mods.super() && !event.mods.control() && !event.mods.alt() && name != "escape";
	if(!typing && document.contains("keys") && document["keys"].is_object() && engine_.window) {
		starts(document["keys"], true);
		for(const auto& [written, handler] : document["keys"].items()) {
			if(normalizeChord(written) != press) continue;
			const json copy = handler;
			if(runBinding(copy, engine_.window->root(), *noVariables(), press, "keys." + written)) return finish(true);
		}
	}
	return finish(false);
}

bool Events::runBinding(const json& handler, Panel& owner, const Scope& locals, const std::string& press,
                        const std::string& where) {
	const json event = {{"type", "key"}, {"key", press}};
	if(!isBinding(handler) && !isBindingList(handler)) {
		engine_.runner.handle(handler, owner, event, locals, where);
		return true;
	}
	const json bindings = isBindingList(handler) ? handler : json::array({handler});
	for(std::size_t i = 0; i < bindings.size(); ++i) {
		const json& binding = bindings[i];
		const std::string at = isBindingList(handler) ? where + "[" + std::to_string(i) + "]" : where;
		try {
			if(binding.contains("when") &&
			   !truthy(engine_.document.evaluate(binding["when"], locals, event, at + ".when")))
				continue;
			const json& run = binding["do"];
			if(run.is_string() && !commandEnabled(engine_, run.get<std::string>())) continue;
			Scope scope{&locals, nullptr, json::object()};
			if(binding.contains("with") && binding["with"].is_object())
				for(const auto& [key, value] : binding["with"].items())
					scope.vars[key] = engine_.document.evaluate(value, locals, event, at + ".with." + key);
			engine_.runner.handle(run, owner, event, scope, at + ".do");
			return true;
		} catch(const DocumentError& error) {
			engine_.report(error.what());
			return true;
		}
	}
	return false;
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
	const Record* target = engine_.reconciler.recordOf(ended.over);
	if(!target) return;
	setFlag(ended.over, &Record::dragOver, false);
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
	if(hooks && hooks->dropAt && !(hooks->ownDrops && over == press_.source))
		static_cast<void>(hooks->dropAt(*over, &at));
	if(over == press_.over) return;
	if(const DragHooks* left = dragHooks(engine_.reconciler.recordOf(press_.over)); left && left->dropAt)
		static_cast<void>(left->dropAt(*press_.over, nullptr));
	setFlag(press_.over, &Record::dragOver, false);
	press_.over = over;
	setFlag(over, &Record::dragOver, true);
}

/* Keys and overlays */

void Events::key(Panel& panel, const KeyEvent& event) {
	if(event.action == affresco::Action::Release) return;

	// Enter in a text area submits
	if(auto* textArea = dynamic_cast<TextArea*>(&panel); textArea &&
	                                                     (event.key == Key::Enter || event.key == Key::KeypadEnter) &&
	                                                     event.action == affresco::Action::Press)
		fire(panel, "onSubmit", {{"type", "submit"}, {"text", textArea->text()}});

	// Keys ("keys") on the focused panel, its ancestors and the document, in one walk (the window's hook skips it)
	keyResolved_ = true;
	if(resolve(&panel, event)) return;
	const std::string name = shortcut(event);
	const bool handled = bubble(panel, "onKey",
	       {{"type", "key"},
	        {"key", keyName(event.key)},
	        {"shortcut", name},
	        {"shift", event.mods.shift()},
	        {"ctrl", event.mods.control()},
	        {"alt", event.mods.alt()},
	        {"cmd", event.mods.super()}});
	if(!handled && engine_.reconciler.recordOf(&panel)) navigate(&panel, event);
}

/* Focus */

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
	const auto visit = [&](this const auto& self, Panel& panel) -> void {
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
		for(const auto& child : panel.children()) self(*child);
	};
	visit(scope);
	positive.insert(positive.end(), result.begin(), result.end());
	return positive;
}

Panel& Events::modalScope() {
	for(auto it = modals_.rbegin(); it != modals_.rend(); ++it)
		if(it->overlay->isShown()) return *it->overlay;
	return engine_.window->root();
}

bool Events::keyClick(Panel& panel) {
	if(!handlerOwner(&panel, "onClick")) return false;
	return bubble(panel, "onClick", {{"type", "click"}, {"keyboard", true}});
}

bool Events::navigate(Panel* from, const KeyEvent& event) {
	if(event.action == affresco::Action::Release || !engine_.window) return false;
	const Mods mods = event.mods;
	const bool input = dynamic_cast<const TextArea*>(from) || dynamic_cast<const TextEditor*>(from);
	const bool plain = !mods.super() && !mods.control() && !mods.alt();
	Window& window = *engine_.window;

	// Tab and Shift+Tab, in the open modal overlay if any (a code editor keeps Tab for indenting)
	if(event.key == Key::Tab && plain && !dynamic_cast<const TextEditor*>(from)) {
		const std::vector<Panel*> order = focusables(modalScope(), false);
		if(order.empty()) return false;
		Panel* at = from;
		auto it = order.end();
		for(; at && it == order.end(); at = at->parent()) it = std::ranges::find(order, at);
		const std::size_t n = order.size();
		std::size_t next = mods.shift() ? n - 1 : 0;
		if(it != order.end()) {
			const auto here = static_cast<std::size_t>(it - order.begin());
			next = mods.shift() ? (here + n - 1) % n : (here + 1) % n;
		}
		window.setFocus(order[next]);
		return true;
	}
	if(input) return false; // a text input keeps the other keys for typing

	// Alt+letter: the mnemonic's node, in the open modal overlay if any
	const char letter = event.key >= Key::A && event.key <= Key::Z
	                        ? static_cast<char>('a' + (static_cast<int>(event.key) - static_cast<int>(Key::A)))
	                        : '\0';
	const auto clickMnemonic = [&](Panel& scope) {
		for(Panel* panel : focusables(scope, true))
			if(engine_.reconciler.recordOf(panel)->mnemonic == letter) return keyClick(*panel);
		return false;
	};
	if(letter && mods.alt() && !mods.super() && !mods.control()) { // anywhere in it, lists too
		Panel& scope = modalScope();
		const auto find = [&](this const auto& self, Panel& panel) -> Panel* {
			const Record* record = engine_.reconciler.recordOf(&panel);
			if(!panel.isShown() || (record && record->disabled) || (record && &panel != &scope && record->modal))
				return nullptr;
			if(record && &panel != &scope && record->mnemonic == letter) return &panel;
			for(const auto& child : panel.children())
				if(Panel* found = self(*child)) return found;
			return nullptr;
		};
		Panel* found = find(scope);
		return found && keyClick(*found);
	}

	if(!from || !plain || mods.shift()) return false;
	const Record* own = engine_.reconciler.recordOf(from);
	if(own && own->focusable && (event.key == Key::Enter || event.key == Key::KeypadEnter || event.key == Key::Space))
		return keyClick(*from);

	// Inside the nearest "arrows" node: the arrows, Home, End and its mnemonics
	Panel* list = from;
	const Record* listRecord = nullptr;
	for(; list; list = list->parent()) {
		listRecord = engine_.reconciler.recordOf(list);
		if(listRecord && !listRecord->arrows.empty()) break;
	}
	if(!list) return false;
	if(letter) return clickMnemonic(*list);
	const std::string& axis = listRecord->arrows;
	const bool vertical = axis != "horizontal", horizontal = axis != "vertical";
	int step = 0;
	if((vertical && event.key == Key::Down) || (horizontal && event.key == Key::Right)) step = 1;
	else if((vertical && event.key == Key::Up) || (horizontal && event.key == Key::Left)) step = -1;
	else if(event.key != Key::Home && event.key != Key::End) return false;

	const std::vector<Panel*> items = focusables(*list, true);
	if(items.empty()) return false;
	const auto n = static_cast<long>(items.size());
	long here = -1;
	for(Panel* at = from; at && here < 0; at = at->parent())
		if(const auto it = std::ranges::find(items, at); it != items.end()) here = it - items.begin();
	long next = event.key == Key::Home ? 0 : event.key == Key::End ? n - 1 : here < 0 ? (step > 0 ? 0 : n - 1)
	                                                                        : (here + step + n) % n;
	window.setFocus(items[static_cast<std::size_t>(next)]);
	return true;
}

/// @brief Tells whether a point is on a shown panel inside another, an overlay such as a submenu being one.
/// @param panel The outer panel.
/// @param position The point.
/// @return true if it is.
static bool insideShown(const Panel& panel, Vec2 position) {
	if(!panel.isShown()) return false;
	if(panel.bounds().contains(position)) return true;
	return std::ranges::any_of(panel.children(), [&](const auto& child) { return insideShown(*child, position); });
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
