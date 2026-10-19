#pragma once

/// @file
/// @brief What the keys of a node mean (structure, handlers, state blocks), and the scope and easing helpers the
/// runtime's subsystems share.

#include "affresco/core/Json.hpp"

#include "Expression.hpp"

#include <cctype>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

using expect::boolean;
using expect::fail;
using expect::integer;
using expect::number;
using expect::object;
using expect::string;

/// @brief Reads a text property; numbers and booleans from bindings are shown as text.
/// @param value The value.
/// @return The text.
inline std::string text(const json& value) { return toText(value); }

/// @brief Tells whether a node key is an event handler: "on" and a capital letter ("onClick", "onDrop").
/// @param key The key.
/// @return true for handlers.
inline bool handlerKey(const std::string& key) {
	return key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
}

/// Event handlers a node may have.
inline constexpr std::string_view handlerNames[] = {
    "onClick", "onDoubleClick", "onContextMenu", "onPress", "onRelease", "onDrag",   "onDrop", "onHover", "onLeave",
    "onFocus", "onBlur",        "onScroll",      "onKey",   "onChange",  "onSubmit", "onShow", "onHide"};

/// Handlers that bubble: when a node has none, its nearest ancestor with one handles the event.
inline constexpr std::string_view bubbling[] = {"onClick",   "onDoubleClick", "onContextMenu", "onPress",
                                                "onRelease", "onDrag",        "onScroll",      "onKey"};

/// Visual states, in order: later ones win when several apply.
inline constexpr std::string_view stateBlocks[] = {"hover", "focused", "dragging", "dragOver", "pressed"};

/// @brief Tells whether a node key describes structure or behaviour, so it is never evaluated or applied as a property.
/// @param key The key.
/// @return true for children, repeat, when, let, handlers, ...
inline bool structural(const std::string& key) {
	return key == "children" || key == "repeat" || key == "when" || key == "let" || key == "check" || key == "use" ||
	       key == "with" || key == "bind" || key == "caret" || key == "keys" || key == "every" || key == "drag" ||
	       key == "note" || key == "slot" || handlerKey(key);
}

/// @brief Eases an animation's progress.
/// @param easing "linear", "in", "out" or "inOut".
/// @param t Progress, 0 to 1.
/// @return The eased progress.
inline double ease(const std::string& easing, double t) {
	if(easing == "in") return t * t * t;
	if(easing == "out") return 1.0 - std::pow(1.0 - t, 3.0);
	if(easing == "inOut") return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
	return t;
}

/// @brief The scope with no variables.
/// @return It (shared, never changed).
inline const SharedScope& noVariables() {
	static const SharedScope none = std::make_shared<Scope>();
	return none;
}

/// @brief A copy of a scope chain as one shared scope, for work that runs later (timers).
/// @param scope The scope.
/// @return The copy.
inline SharedScope snapshot(const Scope& scope) {
	std::vector<const Scope*> chain;
	for(const Scope* s = &scope; s; s = s->parent) chain.push_back(s);
	json vars = json::object();
	for(auto it = chain.rbegin(); it != chain.rend(); ++it) vars.update((*it)->vars); // inner scopes win
	return extend(nullptr, std::move(vars));
}

/// @brief An empty JSON object, shared, for values that are often absent.
/// @return It (never changed).
inline const json& emptyObject() {
	static const json none = json::object();
	return none;
}

} // namespace affresco::format
