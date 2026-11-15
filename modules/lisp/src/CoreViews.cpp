/// @file
/// @brief The language's own functions for building views: `handler` and `or-else`, and those its forms use (`%repeat`
/// for `repeat`, `%component` for `defcomponent`, `%symbolic?` for a binding that depends on a local). A name of the
/// running app, as a view's code holds it, is a symbol: `repeat`'s item, a component's parameter. And two that forms
/// from Clojure use: `%arity` for a function of many arities, `%repeat-times` for `(repeat n x)`.

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief A value as a node's property holds it: a name of the running app as its binding, `"{{card}}"`.
/// @param machine The machine.
/// @param value The value.
/// @return The value, or the binding's text.
Value asProperty(Machine& machine, const Value& value) {
	if(value.kind() != Kind::Symbol) return value;
	return machine.heap.string("{{" + pathExpression(value.text()) + "}}");
}

/// @brief `%symbolic?`: whether one of the values is a name of the running app. @param machine The machine.
/// @param arguments The values. @return true if one is a symbol.
Value symbolic(Machine& /*machine*/, Arguments arguments) {
	return Value::boolean(std::ranges::any_of(arguments, [](const Value& v) { return v.kind() == Kind::Symbol; }));
}

/// @brief `%as-name`: a binding's text, `{{expression}}`, as a name of the running app, the symbol `(expression)`: what
/// a view is given for an argument that needs the running app (Compiler::compileLiveName()). Only while building, where
/// the program alone wrote the text.
/// @param machine The machine.
/// @param arguments The text.
/// @return The symbol.
Value asName(Machine& machine, Arguments arguments) {
	const std::string_view text =
	    arguments.size() == 1 && arguments[0].kind() == Kind::String ? arguments[0].text() : "";
	if(!machine.pure() || text.size() < 5 || !text.starts_with("{{") || !text.ends_with("}}") ||
	   text.substr(2).contains("{{"))
		throw Problem("\"%as-name\" is a view's call's own");
	return machine.heap.symbol("(" + std::string(text.substr(2, text.size() - 4)) + ")");
}

/// @brief `or-else`: the first value that isn't nil, as an expression's `??`.
/// @param machine The machine.
/// @param arguments The values, at least one.
/// @return It, or nil.
Value orElse(Machine& /*machine*/, Arguments arguments) {
	if(arguments.empty()) throw Problem("\"or-else\" takes one value or more");
	const auto found = std::ranges::find_if(arguments, [](const Value& v) { return !v.isNil(); });
	return found == arguments.end() ? Value() : *found;
}

/// @brief What a repeat goes over, as `repeat`'s `in` holds it: a path, or a binding's text.
/// @param machine The machine.
/// @param list What the build gave: a name of the running app, a binding's text, or a list.
/// @return The text.
Value repeatSource(Machine& machine, const Value& list) {
	if(list.kind() == Kind::Symbol) return machine.heap.string(pathExpression(list.text()));
	if(list.kind() == Kind::String) { // "{{state.items}}" is the path state.items, as a repeat writes it
		const std::string_view text = list.text();
		const std::string_view inside = text.size() > 4 && text.starts_with("{{") && text.ends_with("}}")
		                                    ? text.substr(2, text.size() - 4)
		                                    : std::string_view();
		const bool path = !inside.empty() && std::ranges::all_of(inside, [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
		});
		return path ? machine.heap.string(inside) : list;
	}
	return machine.heap.string("{{" + expressionOfValue(machine, list) + "}}");
}

/// @brief A node with a property added after its type and its id: a list `(type :id x ...)` or a map.
/// @param machine The machine.
/// @param node The node.
/// @param key The property, which it must not have.
/// @param value Its value.
/// @return The node.
Value withProperty(Machine& machine, const Value& node, std::string_view key, const Value& value) {
	const auto has = [&](std::size_t at) {
		return node.kind() == Kind::Map ? node.keyAt(at) == key
		                                : node.at(at).kind() == Kind::Keyword && node.at(at).text() == key;
	};
	for(std::size_t at = node.kind() == Kind::Map ? 0 : 1; at < node.count(); ++at)
		if(has(at)) throw Problem("repeat's node has a " + std::string(key) + " of its own");
	machine.spend(node.count());
	if(node.kind() == Kind::Map) {
		std::vector<MapEntry> members;
		std::size_t at = 0;
		for(; at < node.count() && (node.keyAt(at) == "type" || node.keyAt(at) == "id"); ++at)
			members.push_back({node.keyAt(at), node.valueAt(at)});
		members.push_back({machine.heap.keep(key), value});
		for(; at < node.count(); ++at) members.push_back({node.keyAt(at), node.valueAt(at)});
		return machine.heap.map(std::move(members));
	}
	std::vector<Value> items = {node.at(0)};
	std::size_t at = 1;
	if(node.count() > 2 && node.at(1).kind() == Kind::Keyword && node.at(1).text() == "id") {
		items.push_back(node.at(1));
		items.push_back(node.at(2));
		at = 3;
	}
	items.push_back(machine.heap.keyword(key));
	items.push_back(value);
	for(; at < node.count(); ++at) items.push_back(node.at(at));
	return machine.heap.sequence(Kind::List, std::move(items), node.place());
}

/// @brief `%repeat`: a node repeated over a list, its `repeat` after its `type` and `id`.
/// @param machine The machine.
/// @param arguments What it goes over, the item's name, the index's name or nil, and `[node key]`.
/// @return The node.
Value repeatNode(Machine& machine, Arguments arguments) {
	if(arguments.size() != 4 || arguments[3].kind() != Kind::Vector || arguments[3].count() != 2)
		throw Problem("\"%repeat\" is repeat's own");
	const Value node = arguments[3].at(0);
	if(node.kind() != Kind::Map && (node.kind() != Kind::List || node.count() == 0))
		throw Problem("repeat's body gives one node, and gave " + std::string(describeKind(node)));
	std::vector<MapEntry> repeat = {{"for", arguments[1]}, {"in", repeatSource(machine, arguments[0])}};
	if(!arguments[2].isNil()) repeat.push_back({"index", arguments[2]});
	if(const Value key = arguments[3].at(1); !key.isNil()) repeat.push_back({"key", asProperty(machine, key)});
	return withProperty(machine, node, "repeat", machine.heap.map(std::move(repeat)));
}

/// @brief Whether a component's `:runsActions` names one of its parameters, as a word, a keyword or a text.
/// @param runs The `:runsActions`.
/// @param params The parameters with their defaults.
/// @return true if it does.
bool namesParameter(const Value& runs, const Value& params) {
	if(runs.kind() != Kind::Symbol && runs.kind() != Kind::String && runs.kind() != Kind::Keyword) return false;
	bool named = false;
	for(std::size_t key = 0; key < params.count(); ++key) named = named || params.keyAt(key) == runs.text();
	return named;
}

/// @brief `%component`: what `defcomponent` builds.
/// @param machine The machine.
/// @param arguments Its docstring, its parameters with their defaults, its `:runsActions` (nil, a parameter's name or
/// true), and its node.
/// @return `{note, params, runsActions, node}`.
Value component(Machine& machine, Arguments arguments) {
	if(arguments.size() != 4 || arguments[0].kind() != Kind::String || arguments[1].kind() != Kind::Map)
		throw Problem("\"%component\" is defcomponent's own");
	if(arguments[3].kind() != Kind::Map && arguments[3].kind() != Kind::List)
		throw Problem("a component's body gives one node, and gave " + std::string(describeKind(arguments[3])));
	const Value& runs = arguments[2];
	const bool named = namesParameter(runs, arguments[1]);
	if(!runs.isNil() && !named && (runs.kind() != Kind::Boolean || !runs.truthy()))
		throw Problem(":runsActions names one of the component's parameters, or is true");
	std::vector<MapEntry> members;
	if(!arguments[0].text().empty()) members.push_back({"note", arguments[0]});
	members.push_back({"params", arguments[1]});
	if(named) members.push_back({"runsActions", machine.heap.string(runs.text())});
	else if(!runs.isNil()) members.push_back({"runsActions", runs});
	members.push_back({"node", arguments[3]});
	return machine.heap.map(std::move(members));
}

/// @brief `handler`: a handler that calls a function of the program with these arguments, `{:lisp "name" :with [...]}`;
/// a name of the running app is given as its value then (`(handler select-card c i)` in a repeat).
/// @param machine The machine.
/// @param arguments The function, then its arguments.
/// @return The step.
Value callStep(Machine& machine, Arguments arguments) {
	if(arguments.empty() || arguments[0].kind() != Kind::Function)
		throw Problem("\"handler\" takes a function of the program, then its arguments");
	const std::string& name = static_cast<const ClosureObject*>(arguments[0].pointer())->code->name;
	const Definition* definition = name.empty() ? nullptr : machine.definition(name);
	if(!definition || (definition->kind != "defn" && definition->kind != "defcommand"))
		throw Problem("\"handler\" takes a function a defn or a defcommand of the program defines");
	std::vector<Value> with;
	with.reserve(arguments.size() - 1);
	for(const Value& argument : arguments.subspan(1)) with.push_back(asProperty(machine, argument));
	std::vector<MapEntry> step = {{"lisp", machine.heap.string(name)},
	                              {"with", machine.heap.sequence(Kind::Vector, std::move(with))}};
	return machine.heap.map(std::move(step));
}

/// @brief `%arity`: which clause of a function of many arities takes its arguments (Compiler::arities()).
/// @param machine The machine.
/// @param arguments The arguments, a vector; each clause's count, a vector of n, or -(n + 1) for n then '&'; the
/// function's name, or nil.
/// @return The clause's index.
Value arity(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 3 || arguments[0].kind() != Kind::Vector || arguments[1].kind() != Kind::Vector)
		throw Problem("\"%arity\" is a function's own");
	const auto given = static_cast<std::int64_t>(arguments[0].count());
	const Value counts = arguments[1];
	std::vector<std::int64_t> fixed;
	std::optional<std::pair<std::size_t, std::int64_t>> rest; // the '&' clause, and how many it takes at least
	for(std::size_t at = 0; at < counts.count(); ++at) {
		const std::int64_t count = wholeNumber(counts.at(at)).value_or(0);
		if(count == given) return Value::integer(static_cast<std::int64_t>(at));
		if(count >= 0) fixed.push_back(count);
		else rest = {at, -count - 1};
	}
	if(rest && given >= rest->second) return Value::integer(static_cast<std::int64_t>(rest->first));
	std::ranges::sort(fixed);
	std::vector<std::string> ways;
	ways.reserve(fixed.size() + 1);
	for(const std::int64_t count : fixed) ways.push_back(std::to_string(count));
	if(rest) ways.push_back("at least " + std::to_string(rest->second));
	std::string taken;
	for(std::size_t at = 0; at < ways.size(); ++at)
		taken += (at == 0 ? "" : at + 1 == ways.size() ? " or " : ", ") + ways[at];
	const std::string who =
	    arguments[2].kind() == Kind::String ? "\"" + std::string(arguments[2].text()) + "\"" : "this function";
	throw Problem(who + " takes " + taken + " arguments, and was given " + std::to_string(given));
}

/// @brief `%repeat-times`: Clojure's `(repeat n x)`, a vector of n x.
/// @param machine The machine.
/// @param arguments The count, a whole number, 0 or more; the value.
/// @return The vector.
Value repeatTimes(Machine& machine, Arguments arguments) {
	const std::optional<std::int64_t> times = arguments.size() == 2 ? wholeNumber(arguments[0]) : std::nullopt;
	if(!times || *times < 0) throw Problem("\"repeat\" takes a count, a whole number 0 or more, then a value");
	const auto size = static_cast<std::size_t>(*times);
	machine.spend(size);
	machine.heap.charge(size * sizeof(Value));
	return machine.heap.sequence(Kind::Vector, std::vector<Value>(size, arguments[1]));
}

} // namespace

void defineViews(Machine& machine) {
	machine.defineNative("%symbolic?", symbolic);
	machine.defineNative("%as-name", asName);
	machine.defineNative("%repeat", repeatNode);
	machine.defineNative("%component", component);
	machine.defineNative("handler", callStep);
	machine.defineNative("or-else", orElse);
	machine.defineNative("%arity", arity);
	machine.defineNative("%repeat-times", repeatTimes);
}

} // namespace affresco::lisp
