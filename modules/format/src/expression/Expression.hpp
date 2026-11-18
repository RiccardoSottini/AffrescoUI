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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct ReadSet;
class Strings;

struct Scope;
class ScopeLayer;

/// The key under a list's place that a read of a row's number records: "/items/~i" changes when rows of "/items" are
/// numbered again after an insert or a removal before them. No JSON Pointer has it ("~" escapes only "0" and "1").
inline constexpr std::string_view indexKey = "~i";

/// @brief Where a scope's item lies (Scope::origin()): a repeat's row is a list's place and the row's number in it,
/// which an insert or a removal before it changes in place (Scope::renumber()); another item is a place alone.
struct ItemPlace {
	/// The number of an item that is no list's row.
	static constexpr std::size_t noRow = static_cast<std::size_t>(-1);

	/// An enclosing scope whose item the list lies in ("column.cards"), so the list moves with that item; null when
	/// `list` is a whole place. Lifetime: an ancestor of the scope, kept alive by its Scope::keep chain.
	const Scope* base = nullptr;
	std::string list{};                 ///< The list's place, after base's origin; the item's place for no row.
	std::string index{};                ///< The repeat's variable that holds the row's number (empty: none).
	mutable std::size_t row = noRow;    ///< The row's number in the list, or noRow.
	mutable std::string built{};        ///< The origin, as last built.
	mutable std::uint64_t revision = 0; ///< Counts the times built changed, so scopes over it see it did.
	mutable std::uint64_t baseSeen = 0; ///< base's revision when built was built.
	mutable bool upToDate = false;      ///< Whether built is right for row and base.
};

/// @brief Variables in scope: its own, then its parent's, and so on. Panels built inside a repeat share their parent's
/// scope instead of copying it: a card's scope points at its column's, which holds the column once.
struct Scope {
	const Scope* parent = nullptr;     ///< The enclosing scope, or nullptr.
	std::shared_ptr<const Scope> keep; ///< Keeps the parent alive when it is shared (null for a scope on the stack).
	/// This scope's own variables. Mutable for one writer: the reconciler refreshes a repeat's item in place when the
	/// state it was copied from changes (Reconciler::refreshItems), and the children lists give a row its new number
	/// (renumber()), for panels that share the scope as const.
	mutable json vars = json::object();
	std::string fromState; ///< A variable whose value lies in the state (a repeat's item), or empty.
	ItemPlace place;       ///< Where in the state it lies (origin()).
	/// The number of its layer when its "with" and "let" are tracked (ScopeLayers): a read of one of its variables
	/// records "/~s/<layer>/<name>" (Reads.hpp, places::scopes), which the layer marks when the value changes. 0: none.
	std::uint32_t layer = 0;
	const ScopeLayer* tracking = nullptr; ///< The layer when there is one: which variables it tracks.
	std::shared_ptr<void> hold{};         ///< Gives the layer back when the scope dies (null: no layer).

	/// @brief Where in the state (or under a place outside it, Reads.hpp, `places`) the item of fromState lies now,
	/// as a JSON Pointer: the list's place, its base's origin first, then the row's number. Built when asked, and
	/// again only after renumber() or a base's change.
	/// @return The place; empty for a scope with no such item.
	[[nodiscard]] const std::string& origin() const {
		bool rebuild = !place.upToDate;
		if(place.base) {
			static_cast<void>(place.base->origin()); // brings the base up to date first
			rebuild = rebuild || place.baseSeen != place.base->place.revision;
		}
		if(rebuild) {
			place.built = place.base ? place.base->place.built : std::string();
			place.built += place.list;
			if(place.row != ItemPlace::noRow) {
				place.built += '/';
				place.built += std::to_string(place.row);
			}
			place.baseSeen = place.base ? place.base->place.revision : 0U;
			place.upToDate = true;
			++place.revision;
		}
		return place.built;
	}
	/// @brief The place of the list a row lies in, as a JSON Pointer.
	/// @return It; empty for a scope that isn't a row.
	[[nodiscard]] std::string listPlace() const {
		if(place.row == ItemPlace::noRow) return {};
		return place.base ? place.base->origin() + place.list : place.list;
	}
	/// @brief Gives a row its new number after an insert or a removal before it: its index variable and its origin.
	/// @param row The number.
	void renumber(std::size_t row) const {
		if(row == place.row) return;
		place.row = row;
		place.upToDate = false;
		if(!place.index.empty()) vars[place.index] = row;
	}
	/// @brief Finds a variable here or in an enclosing scope (the innermost wins).
	/// @param name The variable.
	/// @return Its value, or nullptr.
	[[nodiscard]] const json* find(std::string_view name) const {
		for(const Scope* scope = this; scope; scope = scope->parent)
			if(const auto it = scope->vars.find(name); it != scope->vars.end()) return &*it;
		return nullptr;
	}
	/// @brief The scope that defines a variable, if it is a repeat's item over a list of the state (the innermost
	/// definition decides).
	/// @param name The variable.
	/// @return The scope, or nullptr.
	[[nodiscard]] const Scope* itemScope(std::string_view name) const {
		for(const Scope* scope = this; scope; scope = scope->parent)
			if(scope->vars.contains(name)) return scope->fromState == name ? scope : nullptr;
		return nullptr;
	}
	/// @brief Where in the state a variable's value lies, if it is a repeat's item over a list of the state: the
	/// scope that defines it decides (the innermost).
	/// @param name The variable.
	/// @return The place, as a JSON Pointer, or nullptr.
	[[nodiscard]] const std::string* originOf(std::string_view name) const {
		const Scope* scope = itemScope(name);
		return scope ? &scope->origin() : nullptr;
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

/// @brief A shared scope with a repeat's variables on top of another, its item lying in the state (or under a place
/// outside it).
/// @param parent The enclosing scope.
/// @param vars The new variables.
/// @param fromState The item's variable.
/// @param place Where the item lies.
/// @return The scope.
[[nodiscard]] inline SharedScope extend(SharedScope parent, json vars, std::string fromState, ItemPlace place) {
	auto scope = std::make_shared<Scope>();
	scope->parent = parent.get();
	scope->keep = std::move(parent);
	scope->vars = std::move(vars);
	scope->fromState = std::move(fromState);
	scope->place = std::move(place);
	return scope;
}

/// @brief The work one event or one update may still do (SECURITY.md, "Limits"). Each step run, each operation of an
/// expression evaluated and each byte a step writes into the state spends from it; when one runs out, what is
/// running stops with an error that names its place.
struct WorkBudget {
	std::size_t steps = 0;      ///< Steps it may still run.
	std::size_t operations = 0; ///< Expression operations it may still evaluate (a list function's item is one).
	std::size_t bytes = 0;      ///< Bytes it may still write into the state.
};

inline constexpr std::size_t stepsPerEvent = 100000;        ///< The steps one event or update may run.
inline constexpr std::size_t operationsPerEvent = 10000000; ///< The expression operations it may evaluate.
inline constexpr std::size_t bytesPerEvent = 64u << 20;     ///< The bytes its steps may write into the state: 64 MiB.

/// @brief Reads a built panel's geometry by id, for the `nodes` variable: `{x, y, width, height, visible}`, or null.
using NodeLookup = std::function<json(std::string_view id)>;

/// @brief Gives a screen-wide variable built when it is read (`context`, `commands`, ...): its value, or null if the
/// name is none of them. The value stays valid for the whole evaluation.
using VariableLookup = std::function<const json*(std::string_view name)>;

/// @brief Calls a function of the document's program for a value being read (the expression function `lisp`): its name,
/// its arguments, the variables and event around it, and its place for errors. It can only read (format::Programs).
using ProgramCall =
    std::function<json(const std::string&, std::vector<json>, const Scope&, const json&, const std::string&)>;

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
	WorkBudget* budget = nullptr;         ///< What the event or update may still do (none: unlimited, for tools).
	ReadSet* reads = nullptr;             ///< Gets the places of the state read, for fine-grained updates (or none).
	const NodeLookup* models = nullptr;   ///< A data model's row by its place, "/~m/files/3" (Models.hpp), or none.
	const VariableLookup* variables = nullptr; ///< Variables built when read (none: only the others).
	const Strings* strings = nullptr; ///< The translations `t()` reads, and the locale (none: English, no keys).
	/// `computed.<name>` read in place, valid for the whole evaluation (none: copied from `computed`), so a large
	/// computed list isn't copied by every value that reads one item of it.
	const VariableLookup* computedValues = nullptr;
	const ProgramCall* program = nullptr; ///< For the function `lisp` (none: there is no program to call).
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

/// @brief Reads the value at a path as lookup() does, recording the same reads, without copying a value that lies in
/// place: a value of the state, the event or a variable is given where it lies, valid while that JSON is (for the
/// state, until it next changes, as Value::fromJson()).
/// @param path E.g. "state.threads[0].title".
/// @param context What paths can see.
/// @param where Its place in the document, for errors.
/// @param made Gets the value when it lies nowhere: a number, or what reading it made.
/// @return Where it lies, or nullptr when it is in @p made.
/// @throws DocumentError If the path is malformed.
[[nodiscard]] const json* lookupInPlace(std::string_view path, const Context& context, const std::string& where,
                                        json& made);

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
/// @return Markdown lines: "- functions: `length(value)`, ...", then the list functions.
[[nodiscard]] std::string functionGuide();

} // namespace affresco::format
