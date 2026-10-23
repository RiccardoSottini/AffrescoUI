/// @file
/// @brief The checker's reading of typed state ("stateTypes"): what the state may hold, the kinds of value a
/// document's values may give, and the writes, bindings and comparisons that don't fit a declared type.

#include "CheckerImpl.hpp"

#include "StateTypes.hpp"

#include <format>

namespace affresco::format::checker {

namespace {

/// Operators that give true or false.
constexpr auto booleanOperators = std::to_array<std::string_view>(
    {"$equals", "$notEquals", "$less", "$greater", "$lessOrEqual", "$greaterOrEqual", "$not"});

/// Operators that give a number.
constexpr auto numberOperators = std::to_array<std::string_view>({"$subtract", "$multiply", "$divide", "$mod"});

/// @brief Tells whether a value as written holds an expression anywhere: a `{{ }}` or a `$` operator.
/// @param value The value.
/// @param depth Values inside values so far.
/// @return false for a value that is used as it is.
bool holdsExpression(const json& value, int depth = 0) {
	if(value.is_string()) return isDynamic(value);
	if(!value.is_structured()) return false;
	if(depth > maxDepth || isDynamic(value)) return true;
	return std::ranges::any_of(value, [depth](const json& item) { return holdsExpression(item, depth + 1); });
}

/// @brief The kinds of value a widget writes into the state it binds, by node type.
/// @param type The node's "type".
/// @return Them; anyKind for a type the checker doesn't know (an application's).
Kinds writtenByWidget(std::string_view type) noexcept {
	if(type == "toggle" || type == "checkbox") return bit(Kind::Boolean);
	if(type == "slider") return numberKinds;
	if(type == "textarea" || type == "editor") return bit(Kind::Text);
	if(type == "canvas") return bit(Kind::Object); // its view: camera, selection, guides
	return anyKind;
}

/// @brief What a widget is called in a message.
/// @param type The node's "type".
/// @return E.g. "a slider".
std::string widgetName(std::string_view type) {
	return std::string(type == "editor" || type == "textarea" ? "an " : "a ") + std::string(type);
}

/// @brief The setting a path names: "settings.editor.fontSize" → "editor.fontSize".
/// @param keys The path's keys, the variable first.
/// @return The setting's key, or std::nullopt if the path isn't under "settings" or has an index.
std::optional<std::string> settingOf(const std::vector<PathKey>& keys) {
	if(keys.size() < 2 || keys.front().name != "settings") return std::nullopt;
	std::string key;
	for(std::size_t i = 1; i < keys.size(); ++i) {
		if(!keys[i].name.is_string()) return std::nullopt;
		key += (i > 1 ? "." : "") + keys[i].name.get<std::string>();
	}
	return key;
}

/// @brief The state key a path starts with, for a type's place.
/// @param keys The path's keys, "state" first.
/// @return "stateTypes.<key>".
std::string typePlace(const std::vector<PathKey>& keys) {
	if(const auto setting = settingOf(keys)) return "settings." + *setting;
	return "stateTypes." + (keys.size() > 1 && keys[1].name.is_string() ? keys[1].name.get<std::string>() : "");
}

/// @brief A path as written, for a message: "state.cards[...].title".
/// @param keys The path's keys, the variable first.
/// @return The path.
std::string written(const std::vector<PathKey>& keys) {
	std::string text = keys.front().name.get<std::string>();
	for(std::size_t i = 1; i < keys.size(); ++i)
		text += keys[i].name.is_string()
		            ? "." + keys[i].name.get<std::string>()
					: "[" + (keys[i].name.is_null() ? std::string("...") : keys[i].name.dump()) + "]";
	return text;
}

/// @brief The keys of a compiled path into the state.
/// @param path A tree: `{"$get": ["state", key, ...]}`.
/// @return Its keys, "state" first (a key an expression gives is null); std::nullopt if it isn't a state path.
std::optional<std::vector<PathKey>> stateKeys(const json& path) {
	if(!path.is_object() || path.size() != 1 || !path.contains("$get")) return std::nullopt;
	const json& raw = path["$get"];
	if(!raw.is_array() || raw.empty() || raw[0] != "state") return std::nullopt;
	std::vector<PathKey> keys;
	for(const json& key : raw) keys.push_back({key.is_string() || key.is_number_integer() ? key : json(), nullptr});
	return keys;
}

/// @brief A literal of a compiled expression: `{"$value": ...}`, a number or a boolean.
/// @param tree The tree.
/// @return The value; null if it isn't one.
json literalOf(const json& tree) {
	if(tree.is_object() && tree.size() == 1 && tree.contains("$value")) return tree["$value"];
	return tree.is_primitive() ? tree : json();
}

} // namespace

Shape shapeOfType(const json& type, int depth) {
	if(!type.is_object() || depth > maxTypeDepth) return anything;
	const Kinds kinds = allowedKinds(type);
	if(kinds == bit(Kind::List)) return Shape::array({shapeOfType(type.value("items", json(true)), depth + 1)});
	if(kinds != bit(Kind::Object)) return kinds == anyKind || (kinds & bit(Kind::Null)) == kinds ? anything : single;
	// Its properties, unless it says other keys come too (a map, "additionalProperties": true or a type): then any
	// key may be read. Left out, the checker reads only the properties declared (and those the state holds), so a
	// typo is found; matching a value still lets other keys in, as JSON Schema does.
	const auto properties = type.find("properties");
	const auto others = type.find("additionalProperties");
	if(properties == type.end() || !properties->is_object() || (others != type.end() && *others != false))
		return anything;
	Shape fields = Shape::object();
	for(const auto& [key, inside] : properties->items()) // all different: each goes at the end, without looking
		fields.get_ref<Shape::object_t&>().emplace_back(key, shapeOfType(inside, depth + 1));
	return fields;
}

void Checker::learnTypes() {
	const auto types = document_.find("stateTypes");
	if(types == document_.end() || !types->is_object()) return;
	types_ = &*types;
	Shape declared = Shape::object();
	for(const auto& [key, type] : types->items())
		declared.get_ref<Shape::object_t&>().emplace_back(key, shapeOfType(type, 0));
	mergeInto(state_, declared); // what the state holds and what its types let it hold
}

const json* Checker::typeOf(const std::vector<PathKey>& keys) const {
	if(const auto setting = settingOf(keys)) {
		const auto settings = document_.find("settings");
		if(settings == document_.end() || !settings->is_object()) return nullptr;
		const auto it = settings->find(*setting);
		return it == settings->end() ? nullptr : &*it;
	}
	if(!types_ || keys.size() < 2 || keys.front().name != "state" || !keys[1].name.is_string()) return nullptr;
	const auto it = types_->find(keys[1].name.get_ref<const std::string&>());
	const json* type = it == types_->end() ? nullptr : &*it;
	for(std::size_t i = 2; type && i < keys.size(); ++i)
		type = typeInside(*type, keys[i].name.is_string() ? &keys[i].name.get_ref<const std::string&>() : nullptr);
	return type;
}

Kinds Checker::kindsOf(const json& value) const {
	if(value.is_string()) {
		const Placeholders found = placeholders(value.get_ref<const std::string&>());
		if(!found.whole) return bit(Kind::Text); // text with values inserted
		try {
			return kindsOfTree(compile(found.expressions.front(), "stateTypes"));
		} catch(const DocumentError&) {
			return anyKind; // reported where the value is read
		}
	}
	if(value.is_object() && value.size() == 1 && value.begin().key().starts_with('$')) return kindsOfTree(value);
	return kindOf(value);
}

Kinds Checker::kindsOfTree(const json& tree) const {
	if(!tree.is_object() || tree.size() != 1 || !tree.begin().key().starts_with('$')) return kindOf(tree);
	const auto only = tree.begin(); // named: references into a temporary iterator would dangle
	const std::string& name = only.key();
	const json& raw = only.value();
	if(name == "$value") return kindOf(raw);
	if(among(booleanOperators, name)) return bit(Kind::Boolean);
	if(among(numberOperators, name)) return numberKinds;
	if(name == "$add") return static_cast<Kinds>(numberKinds | bit(Kind::Text));
	if(const auto keys = stateKeys(tree)) {
		const json* type = typeOf(*keys);
		return type ? allowedKinds(*type) : anyKind;
	}
	return kindsOfChoice(name, raw);
}

Kinds Checker::kindsOfChoice(std::string_view name, const json& raw) const {
	const bool choice = (name == "$if" && raw.size() == 3) || name == "$and" || name == "$or" || name == "$coalesce";
	if(!choice || !raw.is_array()) return anyKind;
	Kinds kinds = 0;
	for(std::size_t i = name == "$if" ? 1 : 0; i < raw.size(); ++i)
		kinds = static_cast<Kinds>(kinds | kindsOfTree(raw[i]));
	return kinds;
}

void Checker::checkWrite(std::string_view kind, const json& step, const std::string& place) {
	const json& target = step[std::string(kind)];
	if(!target.is_string() || isDynamic(target)) return; // typeOf() knows "stateTypes" and "settings"
	const std::optional<std::vector<PathKey>> parsed = parsePath(target.get_ref<const std::string&>(), place);
	if(!parsed) return;
	const std::vector<PathKey>& keys = *parsed;
	const json* type = typeOf(keys);
	if(!type) return;
	const std::string where = std::format("{}.{}", place, kind);
	if(kind == "set" && step.contains("to")) checkValue(*type, step["to"], keys, place + ".to", "");
	else if(kind == "toggle") expectKinds(*type, bit(Kind::Boolean), keys, where, "a toggle writes");
	else if(kind == "increment" || kind == "animate") expectKinds(*type, numberKinds, keys, where, "it writes");
	else if(kind == "push") {
		expectKinds(*type, bit(Kind::List), keys, where, "a push needs");
		if(const json* item = typeInside(*type, nullptr); item && step.contains("value"))
			checkValue(*item, step["value"], keys, place + ".value", "[...]");
	}
}

void Checker::checkValue(const json& type, const json& value, const std::vector<PathKey>& keys,
                         const std::string& place, std::string_view inside) {
	if(holdsExpression(value)) return expectKinds(type, kindsOf(value), keys, place, "this writes");
	// A value as it is: matched in full, its enum and limits too
	if(const auto problem = typeMismatch(value, type, written(keys) + std::string(inside)))
		report(place, problem->text(), typePlace(keys));
}

void Checker::expectKinds(const json& type, Kinds kinds, const std::vector<PathKey>& keys, const std::string& place,
                          std::string_view what) {
	if((kinds & allowedKinds(type)) != 0) return;
	report(place,
	       std::format("'{}' takes {}, but {} {}", written(keys), describeType(type), what, describeKinds(kinds)),
	       typePlace(keys));
}

void Checker::checkBind(const Prepared& node, const json& target, const std::string& place) {
	if(!target.is_string() || isDynamic(target)) return;
	const json* typeName = node.value("type");
	const std::string type = typeName && typeName->is_string() ? typeName->get<std::string>() : "panel";
	const Kinds kinds = writtenByWidget(type);
	if(kinds == anyKind) return;
	const std::optional<std::vector<PathKey>> parsed = parsePath(target.get_ref<const std::string&>(), place);
	if(!parsed) return;
	if(const json* declared = typeOf(*parsed))
		expectKinds(*declared, kinds, *parsed, place, widgetName(type) + " writes");
}

void Checker::checkComparison(const json& raw, const std::string& place) {
	if(!types_ || !raw.is_array() || raw.size() != 2) return;
	for(std::size_t side = 0; side < 2; ++side) {
		const std::optional<std::vector<PathKey>> keys = stateKeys(raw[side]);
		const json literal = literalOf(raw[1 - side]);
		if(!keys || literal.is_null()) continue;
		const json* type = typeOf(*keys);
		if(!type || (!type->contains("enum") && !type->contains("const")) || !typeMismatch(literal, *type, ""))
			continue;
		report(place, std::format("'{}' is never {}: it takes {}", written(*keys), literal.dump(), describeType(*type)),
		       typePlace(*keys));
	}
}

} // namespace affresco::format::checker
