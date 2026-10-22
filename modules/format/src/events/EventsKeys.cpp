/// @file
/// @brief format::Events, the keys: bindings and chords ("keys"), "onKey", and moving the focus with the keyboard.

#include "Events.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "Shortcuts.hpp"

#include "affresco/render/ColorPicker.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/ListView.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace affresco::format {

namespace {

/// @brief Runs a key handler's short form ("ctrl+s": "save"), unless it names a disabled command: then the key falls
/// through, as it does past a disabled binding.
/// @param engine The runtime.
/// @param handler The handler.
/// @param owner The panel it runs on.
/// @param event The key event.
/// @param locals The local variables.
/// @param where Its place in the document.
/// @return true if it ran.
bool runShortForm(Engine& engine, const json& handler, Panel& owner, const json& event, const Scope& locals,
                  const std::string& where) {
	if(handler.is_string() && !commandEnabled(engine, handler.get<std::string>())) return false;
	engine.runner.handle(handler, owner, event, locals, where);
	return true;
}

/// @brief Whether a focused list, table, tree or slider, or an item inside an "arrows" node, moves with this key, which
/// it then keeps from the screen's "keys" (as a text input keeps the keys it types).
/// @param reconciler The records, to find an "arrows" node above the panel.
/// @param panel The focused panel, or nullptr.
/// @param event The key.
/// @return true if it does.
bool movesWith(Reconciler& reconciler, const Panel* panel, const KeyEvent& event) {
	if(event.mods.alt() || event.mods.control() || event.mods.super()) return false;
	const bool vertical = event.key == Key::Up || event.key == Key::Down || event.key == Key::PageUp ||
	                      event.key == Key::PageDown || event.key == Key::Home || event.key == Key::End;
	const bool horizontal = event.key == Key::Left || event.key == Key::Right;
	if(dynamic_cast<const ListView*>(panel)) return vertical;
	if(dynamic_cast<const Slider*>(panel)) return vertical || horizontal;
	for(const Panel* p = panel; p; p = p->parent()) {
		const Record* record = reconciler.recordOf(p);
		if(!record || record->arrows.empty()) continue;
		const bool along = record->arrows == "vertical" ? event.key == Key::Up || event.key == Key::Down
		                   : record->arrows == "horizontal"
		                       ? horizontal
		                       : horizontal || event.key == Key::Up || event.key == Key::Down;
		return along || event.key == Key::Home || event.key == Key::End;
	}
	return false;
}

} // namespace

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
	if(!typing && !movesWith(engine_.reconciler, from, event) && document.contains("keys") &&
	   document["keys"].is_object() && engine_.window) {
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
	if(!isBinding(handler) && !isBindingList(handler)) return runShortForm(engine_, handler, owner, event, locals, where);
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
			Scope scope{&locals, nullptr, json::object(), {}, {}};
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

	// a colour picker that took Tab moved to its next part, and keeps the focus
	if(event.key == Key::Tab && !keyResolved_ && dynamic_cast<const ColorPicker*>(from)) return false;
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
		const auto find = [&](const auto& self, Panel& panel) -> Panel* { // takes itself to recurse, as visit does
			const Record* record = engine_.reconciler.recordOf(&panel);
			if(!panel.isShown() || (record && record->disabled) || (record && &panel != &scope && record->modal))
				return nullptr;
			if(record && &panel != &scope && record->mnemonic == letter) return &panel;
			for(const auto& child : panel.children())
				if(Panel* found = self(self, *child)) return found;
			return nullptr;
		};
		Panel* found = find(find, scope);
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
	const auto n = static_cast<std::ptrdiff_t>(items.size());
	std::ptrdiff_t here = -1;
	for(Panel* at = from; at && here < 0; at = at->parent())
		if(const auto it = std::ranges::find(items, at); it != items.end()) here = it - items.begin();
	const std::ptrdiff_t next = event.key == Key::Home ? 0
	                            : event.key == Key::End ? n - 1
	                            : here < 0              ? (step > 0 ? 0 : n - 1)
	                                                    : (here + step + n) % n;
	window.setFocus(items[static_cast<std::size_t>(next)]);
	return true;
}

} // namespace affresco::format
