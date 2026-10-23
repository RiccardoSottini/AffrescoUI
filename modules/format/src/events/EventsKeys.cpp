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
#include <format>
#include <optional>
#include <string>
#include <string_view>
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

/// @brief Tells whether a key moves along a vertical list: the up and down arrows, page up and down, Home and End.
/// @param key The key.
/// @return true if it does.
bool movesVertically(Key key) {
	return key == Key::Up || key == Key::Down || key == Key::PageUp || key == Key::PageDown || key == Key::Home ||
	       key == Key::End;
}

/// @brief Tells whether a key is the left or the right arrow.
/// @param key The key.
/// @return true if it is.
bool isHorizontal(Key key) { return key == Key::Left || key == Key::Right; }

/// @brief Tells whether an arrow key goes along an "arrows" node's axis.
/// @param arrows Its "arrows": "vertical", "horizontal", or both ways.
/// @param key The key.
/// @return true if it does.
bool alongArrows(std::string_view arrows, Key key) {
	const bool vertical = key == Key::Up || key == Key::Down;
	if(arrows == "vertical") return vertical;
	if(arrows == "horizontal") return isHorizontal(key);
	return vertical || isHorizontal(key);
}

/// @brief Whether a focused list, table, tree or slider, or an item inside an "arrows" node, moves with this key, which
/// it then keeps from the screen's "keys" (as a text input keeps the keys it types).
/// @param reconciler The records, to find an "arrows" node above the panel.
/// @param panel The focused panel, or nullptr.
/// @param event The key.
/// @return true if it does.
bool movesWith(Reconciler& reconciler, const Panel* panel, const KeyEvent& event) {
	if(event.mods.alt() || event.mods.control() || event.mods.super()) return false;
	if(dynamic_cast<const ListView*>(panel)) return movesVertically(event.key);
	if(dynamic_cast<const Slider*>(panel)) return movesVertically(event.key) || isHorizontal(event.key);
	for(const Panel* p = panel; p; p = p->parent()) {
		const Record* record = reconciler.recordOf(p);
		if(!record || record->arrows.empty()) continue;
		return alongArrows(record->arrows, event.key) || event.key == Key::Home || event.key == Key::End;
	}
	return false;
}

/// @brief Tells whether a panel is a text input, which keeps the keys it types.
/// @param panel The panel, or nullptr.
/// @return true if it is.
bool isTextInput(const Panel* panel) {
	return dynamic_cast<const TextArea*>(panel) || dynamic_cast<const TextEditor*>(panel);
}

/// @brief Tells whether a focused text input types a key, which the document's "keys" then leave to it: a plain key,
/// outside a chord, that isn't Escape.
/// @param from The focused panel, or nullptr.
/// @param event The key.
/// @param name The key's shortcut name.
/// @param chording Whether a chord is in progress.
/// @return true if it types it.
bool typesKey(const Panel* from, const KeyEvent& event, std::string_view name, bool chording) {
	return isTextInput(from) && !chording && !event.mods.super() && !event.mods.control() && !event.mods.alt() &&
	       name != "escape";
}

/// @brief Tells whether a key of a "keys" object starts with a press, so the press may begin or go on with a chord.
/// @param keys The "keys" object (anything else has none).
/// @param prefix The press and a space.
/// @param normalise Whether the keys are as written (the document's) rather than normalised (the records').
/// @return true if one does.
bool startsChord(const json& keys, std::string_view prefix, bool normalise) {
	if(!keys.is_object()) return false;
	return std::ranges::any_of(keys.items(), [&](const auto& entry) {
		return (normalise ? normalizeChord(entry.key()) : entry.key()).starts_with(prefix);
	});
}

/// @brief The letter of a key from A to Z, for mnemonics.
/// @param key The key.
/// @return 'a' to 'z', or '\0' for any other key.
char letterOf(Key key) {
	if(key < Key::A || key > Key::Z) return '\0';
	return static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(Key::A)));
}

/// @brief The step a key takes in an "arrows" node.
/// @param axis Its "arrows": "vertical", "horizontal", or both ways.
/// @param key The key.
/// @return 1 forward, -1 back, 0 for Home and End, or std::nullopt for a key that doesn't move.
std::optional<int> arrowStep(std::string_view axis, Key key) {
	const bool vertical = axis != "horizontal", horizontal = axis != "vertical";
	if((vertical && key == Key::Down) || (horizontal && key == Key::Right)) return 1;
	if((vertical && key == Key::Up) || (horizontal && key == Key::Left)) return -1;
	if(key == Key::Home || key == Key::End) return 0;
	return std::nullopt;
}

/// @brief Where a panel, or its nearest ancestor in the list, is in a list of panels.
/// @param panels The list.
/// @param from The panel, or nullptr.
/// @return Its index, or std::nullopt if neither it nor an ancestor is there.
std::optional<std::size_t> positionIn(const std::vector<Panel*>& panels, const Panel* from) {
	for(const Panel* at = from; at; at = at->parent())
		if(const auto it = std::ranges::find(panels, at); it != panels.end())
			return static_cast<std::size_t>(it - panels.begin());
	return std::nullopt;
}

/// @brief Tells whether a command key is held: Cmd, or Ctrl.
/// @param mods The modifiers.
/// @return true if one is.
bool commandHeld(Mods mods) { return mods.super() || mods.control(); }

/// @brief Tells whether Tab moves the focus: without Cmd, Ctrl or Alt, and not in a code editor (it indents).
/// @param from The focused panel, or nullptr.
/// @param mods The modifiers.
/// @return true if it does.
bool tabMovesFocus(const Panel* from, Mods mods) {
	return !commandHeld(mods) && !mods.alt() && !dynamic_cast<const TextEditor*>(from);
}

/// @brief Tells whether a key clicks a focusable node: Enter, the keypad's Enter or Space.
/// @param key The key.
/// @return true if it does.
bool clicksWith(Key key) { return key == Key::Enter || key == Key::KeypadEnter || key == Key::Space; }

/// @brief Finds the node a mnemonic letter clicks: shown and not disabled, in a scope but not in a modal overlay
/// nested in it, lists included.
/// @param reconciler The records.
/// @param scope The scope.
/// @param panel The panel to look in (the scope, then its descendants).
/// @param letter The letter.
/// @return The first such node in document order, or nullptr.
Panel* findMnemonic(Reconciler& reconciler, const Panel& scope, Panel& panel, char letter) {
	const Record* record = reconciler.recordOf(&panel);
	const bool inner = record && &panel != &scope;
	if(!panel.isShown() || (record && record->disabled) || (inner && record->modal)) return nullptr;
	if(inner && record->mnemonic == letter) return &panel;
	for(const auto& child : panel.children())
		if(Panel* found = findMnemonic(reconciler, scope, *child, letter)) return found;
	return nullptr;
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
			if(engine_.window) engine_.window->takeKey();
			return true;
		}
		dismissOverlays(nullptr);
	}
	const std::string press = chording ? chord_ + " " + name : name;
	bool waits = false; // some key starts with this press
	// The focused panel and its ancestors, then the document's; a focused text input keeps plain keys for typing
	const bool ran = runPanelKeys(from, press, waits) ||
	                 (!typesKey(from, event, name, chording) && !movesWith(engine_.reconciler, from, event) &&
	                  runDocumentKeys(press, waits));
	if(!ran && waits) chord_ = press; // the press starts a chord, or goes on with one
	else chord_.clear();              // it ran, or ends a chord, which drops it
	if(chording || !chord_.empty()) engine_.document.contextChanged();
	// A press that ran, starts or ends a chord is the bindings': the focused panel doesn't edit or type with it too
	const bool taken = ran || waits || chording;
	if(taken && engine_.window) engine_.window->takeKey();
	return taken;
}

bool Events::runPanelKeys(Panel* from, const std::string& press, bool& waits) {
	for(Panel* p = from; p; p = p->parent()) {
		const Record* record = engine_.reconciler.recordOf(p);
		if(!record || !record->keys.is_object()) continue;
		waits = waits || startsChord(record->keys, press + " ", false);
		if(!record->keys.contains(press)) continue;
		const json handler = record->keys[press]; // a copy: the handler may revise the document
		const SharedScope locals = record->locals;
		if(runBinding(handler, *p, *locals, press, record->path + ".keys." + press)) return true;
	}
	return false;
}

bool Events::runDocumentKeys(const std::string& press, bool& waits) {
	const json& document = engine_.versions.document();
	if(!document.contains("keys") || !document["keys"].is_object() || !engine_.window) return false;
	waits = waits || startsChord(document["keys"], press + " ", true);
	// NOLINTNEXTLINE(readability-use-anyofallof): runs the bindings until one handles the press
	for(const auto& [written, handler] : document["keys"].items()) {
		if(normalizeChord(written) != press) continue;
		const json copy = handler;
		if(runBinding(copy, engine_.window->root(), *noVariables(), press, "keys." + written)) return true;
	}
	return false;
}

bool Events::runBinding(const json& handler, Panel& owner, const Scope& locals, const std::string& press,
                        const std::string& where) {
	const json event = {{"type", "key"}, {"key", press}};
	if(!isBinding(handler) && !isBindingList(handler))
		return runShortForm(engine_, handler, owner, event, locals, where);
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
					scope.vars[key] =
					    engine_.document.evaluate(value, locals, event, std::format("{}.with.{}", at, key));
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
	if(const auto* textArea = dynamic_cast<const TextArea*>(&panel);
	   textArea && (event.key == Key::Enter || event.key == Key::KeypadEnter) &&
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
	if(event.key == Key::Tab) {
		// a colour picker that took Tab moved to its next part, and keeps the focus
		if(!keyResolved_ && dynamic_cast<const ColorPicker*>(from)) return false;
		// Tab and Shift+Tab, in the open modal overlay if any
		if(tabMovesFocus(from, mods)) return tabFocus(from, mods.shift());
	}
	if(isTextInput(from)) return false; // a text input keeps the other keys for typing
	const char letter = letterOf(event.key);
	if(letter && mods.alt() && !commandHeld(mods)) return pressMnemonic(letter);
	if(!from || commandHeld(mods) || mods.alt() || mods.shift()) return false;
	return plainKey(*from, event.key, letter);
}

bool Events::pressMnemonic(char letter) {
	Panel& scope = modalScope();
	Panel* found = findMnemonic(engine_.reconciler, scope, scope, letter);
	return found && keyClick(*found);
}

bool Events::plainKey(Panel& from, Key key, char letter) {
	const Record* own = engine_.reconciler.recordOf(&from);
	if(own && own->focusable && clicksWith(key)) return keyClick(from);
	return moveInList(from, key, letter);
}

bool Events::tabFocus(Panel* from, bool back) {
	const std::vector<Panel*> order = focusables(modalScope(), false);
	if(order.empty()) return false;
	const std::size_t n = order.size();
	const std::optional<std::size_t> here = positionIn(order, from);
	std::size_t next = back ? n - 1 : 0; // nothing focused in it yet: the last or the first
	if(here) next = back ? (*here + n - 1) % n : (*here + 1) % n;
	engine_.window->setFocus(order[next]);
	return true;
}

bool Events::moveInList(Panel& from, Key key, char letter) {
	// The nearest "arrows" node: the arrows, Home, End and its mnemonics
	Panel* list = &from;
	const Record* listRecord = nullptr;
	for(; list; list = list->parent()) {
		listRecord = engine_.reconciler.recordOf(list);
		if(listRecord && !listRecord->arrows.empty()) break;
	}
	if(!list) return false;
	if(letter) return clickMnemonic(*list, letter);
	const std::optional<int> step = arrowStep(listRecord->arrows, key);
	if(!step) return false;

	const std::vector<Panel*> items = focusables(*list, true);
	if(items.empty()) return false;
	const std::size_t n = items.size();
	const std::optional<std::size_t> here = positionIn(items, &from);
	std::size_t next = 0; // Home, or Down and Right with nothing focused in it yet
	if(key == Key::End || (!here && *step < 0)) next = n - 1;
	else if(here && *step != 0) next = (*here + (*step > 0 ? 1 : n - 1)) % n;
	engine_.window->setFocus(items[next]);
	return true;
}

bool Events::clickMnemonic(Panel& list, char letter) {
	for(Panel* panel : focusables(list, true))
		if(engine_.reconciler.recordOf(panel)->mnemonic == letter) return keyClick(*panel);
	return false;
}

} // namespace affresco::format
