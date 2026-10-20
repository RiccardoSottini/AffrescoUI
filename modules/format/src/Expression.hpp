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
///
/// How it works: the parser (ExpressionParser.hpp) turns an expression into an operator tree, which is lowered once
/// into a typed form and run by a machine on small Values (ExpressionMachine.hpp, Value.hpp). The functions are rows
/// of a registry (ExpressionFunctions.hpp). JSON appears only at the edges: the state is read in place, and results
/// become JSON when evaluate() returns.

#include "affresco/core/Json.hpp"

#include <cstddef>
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
	[[nodiscard]] const json* find(std::string_view name) const {
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
	const json& state;                    ///< The live state.
	const json& window;                   ///< Window size.
	const json& revision;                 ///< The current revision.
	const json& event;                    ///< The event being handled.
	const Scope& locals;                  ///< "repeat" / "forEach" / "let" variables.
	const json& globals;                  ///< Other screen-wide variables (proposals, history, computed, theme, ...).
	const NodeLookup* nodes = nullptr;    ///< For the `nodes` variable (none: it is unknown).
	const NodeLookup* computed = nullptr; ///< For `computed.<name>`, evaluated when read (none: from globals).
};

/// @brief Evaluates templates, expressions and operators, recursing into objects and arrays.
/// @param value The value.
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @return The evaluated value.
/// @throws DocumentError If a path, expression or operator is invalid.
[[nodiscard]] json evaluate(const json& value, const Context& context, const std::string& where);

/// How many compiled expressions the cache holds: one more compile clears it, so a tree compile() returned is valid
/// only until the next compile.
inline constexpr std::size_t compileCacheLimit = 20000;

/// @brief Compiles the inside of a `{{ }}` into an operator tree (cached: each text is parsed once).
/// @param text E.g. "a > 3 ? 'big' : 'small'".
/// @param where Its place in the document, for errors.
/// @return The tree; evaluate() runs it. It lives in the cache until the cache is cleared (compileCacheLimit).
/// @throws DocumentError If it isn't a valid expression.
[[nodiscard]] const json& compile(std::string_view text, const std::string& where);

/// @brief Empties the compile cache, so that tests know how full it is (the cache clears itself when full).
void clearCompileCache() noexcept;

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

/// Positions past a list's end that a step may write to; the positions between are filled with null. Without it,
/// one write to `state.items[2000000000]` would fill the list with two billion nulls.
inline constexpr std::size_t maxListGap = 10000;

/// @brief The place in the state a JSON Pointer names, made if it is missing, ready to be written: what
/// `state[pointer]` does, except that it throws DocumentError where that would throw a JSON exception (which no event
/// handler catches), and refuses to grow a list without bound.
/// @param state The live state.
/// @param pointer The place (statePointer()).
/// @param where The step's place, for errors.
/// @return The value there: null if it was just made.
/// @throws DocumentError If a value on the way is neither an object nor a list, a key into a list isn't a position,
/// or a position is more than maxListGap past the end of its list.
[[nodiscard]] json& statePlace(json& state, const json::json_pointer& pointer, const std::string& where);

/// @brief The value at a place in the state, read without making anything, and without the JSON exceptions that
/// state.value(pointer) throws for a key into a list that isn't a position.
/// @param state The live state.
/// @param pointer The place (statePointer()).
/// @return The value, or nullptr if there is none (or the pointer can't lead to one).
[[nodiscard]] const json* findInState(const json& state, const json::json_pointer& pointer);

/// @brief Tells whether a value counts as true.
/// @param value The value.
/// @return false for false, null, 0, "", [] and {}.
[[nodiscard]] bool truthy(const json& value) noexcept;

/// @brief Tells whether a value is computed: a `{{...}}` string or a `$` operator.
/// @param value The value.
/// @return true if evaluating it may give something else.
[[nodiscard]] bool isDynamic(const json& value) noexcept;

/// @brief The names of the operators and functions, for the catalog (from the function registry).
/// @return E.g. ["$add", "$filter", ...].
[[nodiscard]] json operatorNames();

/// @brief What each function takes and gives (from the function registry), for tools that explain them. The catalog
/// leaves it out: it would double getCatalog's tokens, and the format guide lists every function's arguments.
/// @return E.g. {"clamp": "value, low, high: The value, kept between low and high", ...}.
[[nodiscard]] json functionCatalog();

/// @brief The format guide's list of functions, as SCREEN_FORMAT.md has it (from the function registry).
/// @return Markdown lines: "- functions: `len(value)`, ...", then the list functions.
[[nodiscard]] std::string functionGuide();

} // namespace affresco::format
