/// @file
/// @brief The checker's reading of values: expressions, operators and paths, the variables they see, and what
/// steps write into the state.

#include "CheckerImpl.hpp"
#include "Strings.hpp"

#include <cctype>
#include <charconv>
#include <format>

namespace affresco::format::checker {

namespace {

/// Variables every expression may read (Expression.cpp, variable()), whose contents the checker doesn't follow.
/// "window", "revision" and "event" can't be hidden by a local; the others can.
constexpr auto fixedVariables = std::to_array<std::string_view>({"window", "revision", "event"});
constexpr auto screenVariables = std::to_array<std::string_view>(
    {"nodes", "proposals", "history", "preview", "mode", "theme", "locale", "screen", "context", "commands",
	 "keybindings", "menus", "settings", "settingsList", "services", "connections", "auth"});

/// List functions whose second argument sees each item as `it` (Expression.cpp, applyToItems()).
constexpr auto itemOperators =
    std::to_array<std::string_view>({"$filter", "$map", "$count", "$sum", "$find", "$some", "$every", "$sort"});

/// Operators that give their arguments merged: lists joined, objects merged.
constexpr auto mergingOperators = std::to_array<std::string_view>({"$concat", "$merge"});

/// Operators that give a list of single values.
constexpr auto listOperators = std::to_array<std::string_view>({"$keys", "$split", "$range"});

/// @brief Reads a name in a text path, as the runtime does (letters, digits, '_' and '-').
/// @param text The path.
/// @param i Where the name starts; moved past it.
/// @return The name, or std::nullopt if there is none.
std::optional<std::string> nameAt(std::string_view text, std::size_t& i) {
	const std::size_t start = i;
	while(i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '-'))
		++i;
	if(i == start) return std::nullopt;
	return std::string(text.substr(start, i - start));
}

/// @brief Finds the ']' that closes a '[', past any brackets inside.
/// @param text The path.
/// @param open Where the '[' is.
/// @return Where its ']' is, or std::nullopt.
std::optional<std::size_t> closingBracket(std::string_view text, std::size_t open) {
	int depth = 0;
	for(std::size_t i = open; i < text.size(); ++i) {
		depth += text[i] == '[' ? 1 : text[i] == ']' ? -1 : 0;
		if(depth == 0) return i;
	}
	return std::nullopt;
}

/// @brief Tells whether a value is a `$` operator: an object with one key starting with '$'.
/// @param value The value.
/// @return true for `{"$clamp": [...]}`.
bool isOperator(const json& value) {
	return value.is_object() && value.size() == 1 && value.begin().key().starts_with('$');
}

/// @brief Tells whether a value may hold an expression: an object, a list, or text with `{{`.
/// @param value The value.
/// @return false for numbers, booleans, null and plain text, which refer to nothing.
bool mayHoldExpression(const json& value) { return value.is_structured() || (value.is_string() && isDynamic(value)); }

/// @brief Takes the place out of a load error, which a finding gives on its own.
/// @param error The error: "AffrescoUI screen: <place>: <problem>".
/// @param place The place.
/// @return The problem.
std::string problemOf(const DocumentError& error, const std::string& place) {
	std::string_view text = error.what();
	for(const std::string& prefix : {std::string("AffrescoUI screen: "), place + ": "})
		if(text.starts_with(prefix)) text.remove_prefix(prefix.size());
	return std::string(text);
}

/// @brief What a value written at the end of a path creates where nothing was.
/// @param keys The path.
/// @param i The first key not there yet.
/// @param value What is written.
/// @param change How.
/// @return The new value's shape.
Shape created(const std::vector<PathKey>& keys, std::size_t i, const Shape& value, Change change) {
	if(i == keys.size()) return change == Change::push ? Shape::array({value}) : value;
	if(keys[i].name.is_number_integer()) return Shape::array({created(keys, i + 1, value, change)});
	if(!keys[i].name.is_string()) return anything; // a key an expression gives: a map
	Shape fields = Shape::object();
	fields[keys[i].name.get<std::string>()] = created(keys, i + 1, value, change);
	return fields;
}

/// @brief Tells whether a list that had no known items has some now, anywhere in a shape.
/// @param before The shape before a write.
/// @param after The shape after it.
/// @return true if items appeared: reads of them may now find what they didn't.
bool itemsAppeared(const Shape& before, const Shape& after) {
	if(before.is_array() && after.is_array())
		return before.empty() ? !after.empty() : !after.empty() && itemsAppeared(before[0], after[0]);
	if(!before.is_object() || !after.is_object()) return false;
	return std::ranges::any_of(before.items(), [&](const auto& field) {
		const auto it = after.find(field.key());
		return it != after.end() && itemsAppeared(field.value(), *it);
	});
}

/// @brief Records a write at the end of its path.
/// @param at The shape there.
/// @param value What is written.
/// @param change How.
/// @return What was learnt.
Learning assignHere(Shape& at, const Shape& value, Change change) {
	Shape after = at;
	if(change == Change::push && at.is_array()) addItem(after, value);
	else if(change == Change::push) after = anything; // pushing onto something that isn't a list
	else mergeInto(after, value);
	if(after == at) return Learning::nothing;
	const Learning learnt = itemsAppeared(at, after) ? Learning::items : Learning::more;
	at = std::move(after);
	return learnt;
}

/// @brief The shape of what some operators give, whatever their arguments.
/// @param name The operator.
/// @return The shape, or std::nullopt for the others.
std::optional<Shape> fixedResult(std::string_view name) {
	if(among(listOperators, name)) return Shape::array({single});
	if(name == "$values") return Shape::array({anything});
	if(name == "$entries") return Shape::array({{{"key", single}, {"value", anything}}});
	if(name == "$parseJson") return anything;
	return std::nullopt;
}

/// @brief Records a write in what the state may hold.
/// @param at The shape at keys[i].
/// @param keys The path.
/// @param i The key reached.
/// @param value What is written.
/// @param change How.
/// @return What was learnt (another pass may then find more, or less).
Learning assign(Shape& at, const std::vector<PathKey>& keys, std::size_t i, const Shape& value, Change change) {
	if(at.is_null() || change == Change::none) return Learning::nothing;
	if(i == keys.size()) return assignHere(at, value, change);
	const PathKey& key = keys[i];
	if(key.name.is_string() && at.is_object()) {
		const auto it = at.find(key.name.get<std::string>());
		if(it != at.end()) return assign(*it, keys, i + 1, value, change);
		at[key.name.get<std::string>()] = created(keys, i + 1, value, change);
	} else if(!key.name.is_string() && at.is_array()) {
		if(!at.empty()) return assign(at[0], keys, i + 1, value, change);
		at.push_back(created(keys, i + 1, value, change));
		return Learning::items;
	} else {
		at = anything; // keys an expression gives (a map), or a value of another type: anything may be there
	}
	return Learning::more;
}

} // namespace

/* Values */

Shape Checker::shapeOf(const json& value, const Scope& scope, const std::string& place) {
	if(!visit()) return anything;
	if(value.is_string()) return shapeOfText(value.get_ref<const std::string&>(), scope, place);
	if(value.is_object()) {
		if(isOperator(value)) return shapeOfOperator(value.begin().key(), value.begin().value(), scope, place);
		if(value.empty()) return anything;
		Shape fields = Shape::object();
		for(const auto& [key, item] : value.items())
			fields[key] = shapeOf(item, scope, std::format("{}.{}", place, key));
		return fields;
	}
	if(value.is_array()) {
		Shape items = Shape::array();
		for(std::size_t i = 0; i < value.size(); ++i)
			addItem(items, shapeOf(value[i], scope, place + "[" + std::to_string(i) + "]"));
		return items;
	}
	return value.is_null() ? anything : single;
}

// Most values are read only for what they refer to: no shape is built for them, and a place only for what may hold
// an expression
void Checker::read(const json& value, const Scope& scope, const std::string& place) {
	if(!mayHoldExpression(value) || !visit()) return;
	if(value.is_string()) {
		(void)shapeOfText(value.get_ref<const std::string&>(), scope, place);
	} else if(isOperator(value)) {
		(void)shapeOfOperator(value.begin().key(), value.begin().value(), scope, place);
	} else if(value.is_object()) {
		for(const auto& [key, item] : value.items())
			if(mayHoldExpression(item)) read(item, scope, std::format("{}.{}", place, key));
	} else {
		for(std::size_t i = 0; i < value.size(); ++i)
			if(mayHoldExpression(value[i])) read(value[i], scope, place + "[" + std::to_string(i) + "]");
	}
}

Shape Checker::shapeOfText(std::string_view text, const Scope& scope, const std::string& place) {
	if(!text.contains("{{")) return single;
	const Placeholders found = placeholders(text);
	if(found.unclosed) report(place, "unclosed '{{'");
	Shape result = single;
	for(const std::string_view expression : found.expressions) {
		Shape shape = shapeOfExpression(expression, scope, place);
		if(found.whole) result = std::move(shape);
	}
	return result;
}

Shape Checker::shapeOfExpression(std::string_view text, const Scope& scope, const std::string& place) {
	const json* tree = nullptr;
	try {
		tree = &compile(text, place);
	} catch(const DocumentError& error) {
		report(place, problemOf(error, place));
		return anything;
	}
	// The tree lives in the compile cache, which a later compile may clear (past compileCacheLimit expressions). A "$path"
	// compiles its index expressions, but it is always a whole tree and its text is copied first. Reading a computed
	// value compiles that value's expression while this tree is still read, so such a tree is copied
	if(text.contains("computed")) {
		const json copy = *tree;
		return shapeOf(copy, scope, place);
	}
	return shapeOf(*tree, scope, place);
}

Shape Checker::shapeOfOperator(const std::string& name, const json& raw, const Scope& scope, const std::string& place) {
	if(name == "$value") return shapeOfData(raw);
	if(name == "$equals" || name == "$notEquals") checkComparison(raw, place);
	if(name == "$get") return shapeOfGet(raw, scope, place);
	if(name == "$t") // a translation: its key and parameters against "strings" (Strings.hpp)
		if(const std::string problem = checkTranslation(document_.value("strings", json()), raw); !problem.empty())
			report(place, problem);
	if(name == "$path") return raw.is_string() ? shapeOfPath(raw.get<std::string>(), scope, place) : anything; // copied
	if(name == "$object") {
		if(!raw.is_object() || raw.empty()) return anything;
		Shape fields = Shape::object();
		for(const auto& [key, value] : raw.items()) fields[key] = shapeOf(value, scope, place);
		return fields;
	}
	if(among(choiceOperators, name)) return shapeOfChoice(name, raw, scope, place);
	if(among(itemOperators, name)) return shapeOfItems(name, raw, scope, place);
	return shapeOfResult(name, raw, scope, place);
}

Shape Checker::shapeOfResult(const std::string& name, const json& raw, const Scope& scope, const std::string& place) {
	// One value, or a list of arguments: these are merged (a finding inside names the operator's place)
	Shape arguments = raw.is_array() ? Shape::array() : shapeOf(raw, scope, place);
	if(raw.is_array())
		for(const json& argument : raw) addItem(arguments, shapeOf(argument, scope, place));
	if(name == "$reverse") return arguments; // its one argument, a list
	if(name == "$slice") return raw.is_array() && !raw.empty() ? shapeOf(raw[0], scope, place) : anything;
	if(among(mergingOperators, name)) return itemOf(arguments);
	return fixedResult(name).value_or(single);
}

Shape Checker::shapeOfChoice(const std::string& name, const json& raw, const Scope& scope, const std::string& place) {
	if(!raw.is_array() || (name == "$if" && raw.size() != 3)) {
		read(raw, scope, place);
		return anything;
	}
	if(name == "$if") {
		read(raw[0], scope, place);
		return merge(shapeOf(raw[1], scope, place), shapeOf(raw[2], scope, place));
	}
	Shape result;
	for(std::size_t i = 0; i < raw.size(); ++i) {
		Shape shape = shapeOf(raw[i], scope, place);
		result = i == 0 ? std::move(shape) : merge(result, shape);
	}
	return result;
}

Shape Checker::shapeOfItems(const std::string& name, const json& raw, const Scope& scope, const std::string& place) {
	if(!raw.is_array() || raw.empty()) {
		read(raw, scope, place);
		return anything;
	}
	Shape list = shapeOf(raw[0], scope, place); // not const: returned, so moved
	const Shape item = itemOf(list);
	const Scope each{&scope, nullptr, {{"it", item}, {"index", single}}, {}, {}};
	const Shape result = raw.size() > 1 ? shapeOf(raw[1], each, place) : item;
	for(std::size_t i = 2; i < raw.size(); ++i) read(raw[i], scope, place); // "desc"
	if(name == "$filter" || name == "$sort") return list;
	if(name == "$map") return Shape::array({result});
	return name == "$find" ? item : single;
}

/* Paths */

Shape Checker::shapeOfGet(const json& raw, const Scope& scope, const std::string& place) {
	if(!raw.is_array() || raw.empty()) return anything;
	std::vector<PathKey> keys;
	for(std::size_t i = 1; i < raw.size(); ++i) {
		if(raw[i].is_string() || raw[i].is_number_integer()) keys.push_back({raw[i], nullptr});
		else keys.push_back({nullptr, raw[i]});
	}
	if(raw[0].is_string()) return follow(raw[0].get<std::string>(), keys, scope, place);
	return walkKeys(shapeOf(raw[0], scope, place), "(...)", keys, 0, scope, place); // e.g. (a ?? b).name
}

Shape Checker::shapeOfPath(std::string_view text, const Scope& scope, const std::string& place) {
	std::optional<std::vector<PathKey>> keys = parsePath(text, place);
	if(!keys) return anything;
	const std::string root = keys->front().name.get<std::string>();
	keys->erase(keys->begin());
	return follow(root, *keys, scope, place);
}

Shape Checker::follow(const std::string& root, const std::vector<PathKey>& keys, const Scope& scope,
                      const std::string& place) {
	if(root == "computed") { // computed.<name>: its expression's shape
		if(keys.empty() || !keys.front().name.is_string()) return walkKeys(anything, root, keys, 0, scope, place);
		const std::string name = keys.front().name.get<std::string>();
		if(definition("computed", name))
			return walkKeys(computedShape(name), "computed." + name, keys, 1, scope, place);
		report(place, "'computed." + name + "' doesn't exist",
		       hint([&] { return hintFor(name, definedNames("computed"), "computed.", "computed has"); }));
		return walkKeys(anything, root, keys, 1, scope, place);
	}
	const Shape* shape = variable(root, scope);
	if(!shape) {
		report(place, "unknown variable '" + root + "'",
		       hint([&] { return hintFor(root, visibleNames(scope), "", ""); }));
		return walkKeys(anything, root, keys, 0, scope, place);
	}
	return walkKeys(*shape, root, keys, 0, scope, place);
}

Shape Checker::walkKeys(const Shape& start, std::string described, const std::vector<PathKey>& keys, std::size_t first,
                        const Scope& scope, const std::string& place) {
	const Shape* shape = &start;
	for(std::size_t i = first; i < keys.size(); ++i) {
		const PathKey& key = keys[i];
		if(key.name.is_string()) {
			shape = &member(*shape, described, key.name.get_ref<const std::string&>(), place);
			described += "." + key.name.get<std::string>();
			continue;
		}
		// An index, or a key an expression gives: an item of a list, or anything in a map
		if(!key.expression.is_null()) read(key.expression, scope, place);
		described += "[" + (key.name.is_null() ? describe(key.expression) : key.name.dump()) + "]";
		shape = &itemOf(*shape);
	}
	return *shape;
}

const Shape& Checker::member(const Shape& shape, const std::string& described, const std::string& name,
                             const std::string& place) {
	if(shape.is_object()) {
		if(const auto it = shape.find(name); it != shape.end()) return *it;
		report(place, "'" + described + "." + name + "' doesn't exist",
		       hint([&] { return hintFor(name, keysOf(shape), described + ".", described + " has"); }));
		return anything;
	}
	if(shape.is_null()) return anything;
	if(name == "length") return single;
	report(place, "'" + described + "." + name + "' doesn't exist",
	       described + (shape.is_array() ? " is a list" : " is a single value"));
	return anything;
}

/* Variables */

const Shape* Checker::variable(const std::string& name, const Scope& scope) const {
	if(name == "state") return &state_;
	if(among(fixedVariables, name)) return &anything;
	if(const json* local = scope.find(name)) return local;
	if(among(screenVariables, name) || scope.find(std::string(openScope))) return &anything;
	return nullptr;
}

std::vector<std::string> Checker::visibleNames(const Scope& scope) {
	std::vector<std::string> names = {"state", "computed"};
	names.insert(names.end(), fixedVariables.begin(), fixedVariables.end());
	names.insert(names.end(), screenVariables.begin(), screenVariables.end());
	for(const Scope* layer = &scope; layer; layer = layer->parent)
		for(const auto& [name, shape] : layer->vars.items()) names.push_back(name);
	return names;
}

std::string Checker::variableNames(const Scope& scope) {
	std::set<std::string> names;
	for(const Scope* layer = &scope; layer; layer = layer->parent)
		for(const auto& [name, shape] : layer->vars.items()) names.insert(name);
	std::string joined;
	for(const std::string& name : names) joined += name + ",";
	return joined;
}

const Shape& Checker::computedShape(const std::string& name) {
	if(const auto it = computedShapes_.find(name); it != computedShapes_.end()) return it->second;
	if(std::ranges::find(computing_, name) != computing_.end()) return anything; // a circle: the runtime reports it
	if(computing_.size() >= maxComputedDepth) return anything;                   // too deep: the runtime reports it
	computing_.push_back(name);
	const Scope none;
	Shape shape = shapeOf(document_.at("computed").at(name), none, "computed." + name);
	computing_.pop_back();
	return computedShapes_[name] = std::move(shape);
}

/* Text paths and writes */

std::optional<std::vector<PathKey>> Checker::parsePath(std::string_view text, const std::string& place) {
	std::vector<PathKey> keys;
	std::size_t i = 0;
	std::optional<std::string> name = nameAt(text, i);
	bool valid = name.has_value();
	if(valid) keys.push_back({*name, nullptr});
	while(valid && i < text.size()) {
		if(text[i] == '.') {
			name = nameAt(text, ++i);
			valid = name.has_value();
			if(valid) keys.push_back({*name, nullptr});
		} else if(const auto end = text[i] == '[' ? closingBracket(text, i) : std::nullopt) {
			keys.push_back(parseIndex(text.substr(i + 1, *end - i - 1), place)); // [3], [state.current], [c + 1]
			i = *end + 1;
		} else {
			valid = false;
		}
	}
	if(!valid) {
		report(place, "invalid path '" + std::string(text) + "'");
		return std::nullopt;
	}
	return keys;
}

PathKey Checker::parseIndex(std::string_view inner, const std::string& place) {
	PathKey key{nullptr, nullptr}; // neither: read as anything
	std::int64_t index = 0;
	if(const auto [end, error] = std::from_chars(inner.data(), inner.data() + inner.size(), index);
	   error == std::errc() && end == inner.data() + inner.size()) {
		key.name = index;
		return key;
	}
	try {
		key.expression = compile(inner, place);
	} catch(const DocumentError& failure) {
		report(place, problemOf(failure, place));
	}
	return key;
}

void Checker::write(const json& target, const Shape& value, Change change, const Scope& scope,
                    const std::string& place) {
	if(isDynamic(target)) read(target, scope, place); // a computed place: where it writes isn't known
	if(!target.is_string() || isDynamic(target)) return;
	const std::optional<std::vector<PathKey>> keys = parsePath(target.get_ref<const std::string&>(), place);
	if(!keys) return;
	for(const PathKey& key : *keys)
		if(!key.expression.is_null()) read(key.expression, scope, place);
	if(keys->front().name == "settings" && document_.contains("settings")) { // a setting: runStateStep() writes it
		std::string key;
		for(std::size_t i = 1; i < keys->size(); ++i)
			key += (i > 1 ? "." : "") + ((*keys)[i].name.is_string() ? (*keys)[i].name.get<std::string>() : "[...]");
		if(!document_["settings"].contains(key))
			report(place, "there is no setting '" + key + "' in \"settings\"");
		return;
	}
	if(keys->front().name != "state") {
		report(place, "can only change values under 'state', not '" + target.get<std::string>() + "'");
		return;
	}
	learnt_ = std::max(learnt_, assign(state_, *keys, 1, value, change));
}

} // namespace affresco::format::checker
