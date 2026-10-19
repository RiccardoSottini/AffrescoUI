#pragma once

/// @file
/// @brief Values in screen documents: data lookups, templates, expressions and operators.
///
/// - `"{{state.threads[state.current].title}}"`: the value at a path (any JSON type)
/// - `"Clicked {{state.clicks}} times"`: a string with values interpolated
/// - `"{{tag == state.filter ? '@topBar' : '@chip'}}"`: an expression, JS-style (compiled once, then cached)
/// - `{ "$clamp": ["{{event.x}}", 180, 520] }`: an operator (one key starting with '$'); expressions compile to these
///
/// Paths start with a variable: state, event, window, revision, proposals, history, preview, mode, computed, theme,
/// screen, nodes, or a name given by "repeat", "forEach", "let", "with" or a component's params.

#include "affresco/core/Json.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief Variables in scope: its own, then its parent's, and so on. Panels built inside a repeat share their parent's
/// scope instead of copying it: a card's scope points at its column's, which holds the column once.
struct Scope {
	const Scope* parent = nullptr;     ///< The enclosing scope, or nullptr.
	std::shared_ptr<const Scope> keep; ///< Keeps the parent alive when it is shared (null for a scope on the stack).
	json vars = json::object();        ///< This scope's own variables.

	/// @brief Finds a variable here or in an enclosing scope (the innermost wins).
	/// @param name The variable.
	/// @return Its value, or nullptr.
	[[nodiscard]] const json* find(const std::string& name) const {
		for(const Scope* scope = this; scope; scope = scope->parent)
			if(const auto it = scope->vars.find(name); it != scope->vars.end()) return &*it;
		return nullptr;
	}
};

/// @brief A scope that panels and timers can keep.
using SharedScope = std::shared_ptr<const Scope>;

/// @brief A shared scope with more variables on top of another.
/// @param parent The enclosing scope.
/// @param vars The new variables.
/// @return The scope.
[[nodiscard]] inline SharedScope extend(SharedScope parent, json vars) {
	auto scope = std::make_shared<Scope>();
	scope->parent = parent.get();
	scope->keep = std::move(parent);
	scope->vars = std::move(vars);
	return scope;
}

/// @brief Reads a built panel's geometry by id, for the `nodes` variable: `{x, y, width, height, visible}`, or null.
using NodeLookup = std::function<json(std::string_view id)>;

/// @brief What a path can see while a value is evaluated.
struct Context {
	const json& state;               ///< The live state.
	const json& window;              ///< Window size.
	const json& revision;            ///< The current revision.
	const json& event;               ///< The event being handled.
	const Scope& locals;             ///< "repeat" / "forEach" / "let" variables.
	const json& globals;             ///< Other screen-wide variables (proposals, history, computed, theme, ...).
	const NodeLookup* nodes = nullptr; ///< For the `nodes` variable (none: it is unknown).
	const NodeLookup* computed = nullptr; ///< For `computed.<name>`, evaluated when read (none: from globals).
};

/// @brief Evaluates templates, expressions and operators, recursing into objects and arrays.
/// @param value The value.
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The evaluated value.
/// @throws DocumentError If a path, expression or operator is invalid.
[[nodiscard]] json evaluate(const json& value, const Context& context, const std::string& where);

/// @brief Compiles the inside of a `{{ }}` into an operator tree (cached: each text is parsed once).
/// @param text E.g. "a > 3 ? 'big' : 'small'".
/// @param where Its place in the document, for errors.
/// @return The tree; evaluate() runs it.
/// @throws DocumentError If it isn't a valid expression.
[[nodiscard]] const json& compile(std::string_view text, const std::string& where);

/// @brief The `{{ }}` placeholders of a string, found as evaluate() finds them.
struct Placeholders {
	std::vector<std::string_view> expressions; ///< The text inside each, in order (views into the string).
	bool whole = false;                        ///< One placeholder and nothing else: the value keeps its JSON type.
	bool unclosed = false;                     ///< A `{{` has no `}}` (the expressions before it are listed).
};

/// @brief Finds the placeholders of a string, for tools that read expressions without evaluating them (Checker).
/// @param text The string.
/// @return Its placeholders.
[[nodiscard]] Placeholders placeholders(std::string_view text);

/// @brief Reads the value at a path.
/// @param path E.g. "state.threads[0].title".
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The value, or null if it doesn't exist.
/// @throws DocumentError If the path is malformed.
[[nodiscard]] json lookup(std::string_view path, const Context& context, const std::string& where);

/// @brief Resolves a writable location.
/// @param path A path starting with "state".
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The JSON Pointer inside the state.
/// @throws DocumentError If the path doesn't start with "state" or is malformed.
[[nodiscard]] json::json_pointer statePointer(std::string_view path, const Context& context, const std::string& where);

/// @brief Tells whether a value counts as true.
/// @param value The value.
/// @return false for false, null, 0, "", [] and {}.
[[nodiscard]] bool truthy(const json& value) noexcept;

/// @brief Tells whether a value is computed: a `{{...}}` string or a `$` operator.
/// @param value The value.
/// @return true if evaluating it may give something else.
[[nodiscard]] bool isDynamic(const json& value) noexcept;

/// @brief The names of the operators and functions, for the catalog.
/// @return E.g. ["$add", "$filter", ...].
[[nodiscard]] json operatorNames();

} // namespace affresco::format
