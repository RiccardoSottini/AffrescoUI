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
	return anyKind;
}

/// @brief What a widget is called in a message.
/// @param type The node's "type".
/// @return E.g. "a slider".
std::string widgetName(std::string_view type) {
	return std::string(type == "editor" || type == "textarea" ? "an " : "a ") + std::string(type);
}

/// @brief The state key a path starts with, for a type's place.
/// @param keys The path's keys, "state" first.
/// @return "stateTypes.<key>".
std::string typePlace(const std::vector<PathKey>& keys) {
	return "stateTypes." + (keys.size() > 1 && keys[1].name.is_string() ? keys[1].name.get<std::string>() : "");
}

/// @brief A path as written, for a message: "state.cards[...].title".
/// @param keys The path's keys, the variable first.
/// @return The path.
std::string written(const std::vector<PathKey>& keys) {
	std::string text = keys.front().name.get<std::string>();
	for(std::size_t i = 1; i < keys.size(); ++i)
		text += keys[i].name.is_string() ? "." + keys[i].name.get<std::string>()
		                                 : "[" + (keys[i].name.is_null() ? std::string("...") : keys[i].name.dump()) + "]";
	return text;
}

} // namespace

Shape shapeOfType(const json& type, int depth) {
	if(!type.is_object() || depth > maxTypeDepth) return anything;
	const Kinds kinds = allowedKinds(type);
	if(kinds == bit(Kind::List)) return Shape::array({shapeOfType(type.value("items", json(true)), depth + 1)});
	if(kinds != bit(Kind::Object)) return kinds == anyKind || (kinds & bit(Kind::Null)) == kinds ? anything : single;
	// Its properties, unless it lets others in too (a map): then any key may be read
	const auto properties = type.find("properties");
	if(properties == type.end() || !properties->is_object() || type.value("additionalProperties", json(false)) != false)
		return anything;
	Shape fields = Shape::object();
	for(const auto& [key, inside] : properties->items()) fields[key] = shapeOfType(inside, depth + 1);
	return fields;
}

void Checker::learnTypes() {
	const auto types = document_.find("stateTypes");
	if(types == document_.end() || !types->is_object()) return;
	types_ = &*types;
	for(const auto& [key, type] : types->items()) {
		const Shape declared = shapeOfType(type, 0);
		if(const auto it = state_.find(key); it != state_.end()) mergeInto(*it, declared);
		else state_[key] = declared;
	}
}

const json* Checker::typeOf(const std::vector<PathKey>& keys) const {
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
	const std::string& name = tree.begin().key();
	const json& raw = tree.begin().value();
	if(name == "$value") return kindOf(raw);
	if(among(booleanOperators, name)) return bit(Kind::Boolean);
	if(among(numberOperators, name)) return numberKinds;
	if(name == "$add") return static_cast<Kinds>(numberKinds | bit(Kind::Text));
	if(name == "$get" && raw.is_array() && !raw.empty() && raw[0] == "state") {
		std::vector<PathKey> keys;
		for(const json& key : raw) keys.push_back({key.is_string() || key.is_number_integer() ? key : json(), nullptr});
		const json* type = typeOf(keys);
		return type ? allowedKinds(*type) : anyKind;
	}
	if((name == "$if" && raw.is_array() && raw.size() == 3) ||
	   ((name == "$and" || name == "$or" || name == "$coalesce") && raw.is_array())) {
		Kinds kinds = 0;
		for(std::size_t i = name == "$if" ? 1 : 0; i < raw.size(); ++i)
			kinds = static_cast<Kinds>(kinds | kindsOfTree(raw[i]));
		return kinds;
	}
	return anyKind;
}

void Checker::checkWrite(std::string_view kind, const json& step, const std::string& place) {
	const json& target = step[std::string(kind)];
	if(!types_ || !target.is_string() || isDynamic(target)) return;
	const std::optional<std::vector<PathKey>> keys = parsePath(target.get_ref<const std::string&>(), place);
	const json* type = keys ? typeOf(*keys) : nullptr;
	if(!type) return;
	const std::string where = std::format("{}.{}", place, kind);
	if(kind == "set" && step.contains("to")) {
		const json& value = step["to"];
		if(!holdsExpression(value)) { // a value as it is: matched in full, enums and limits too
			if(const auto problem = mismatch(value, *type, written(*keys)))
				report(place + ".to", problem->text(), typePlace(*keys));
			return;
		}
		return expectKinds(*type, kindsOf(value), *keys, place + ".to", "this writes");
	}
	if(kind == "toggle") return expectKinds(*type, bit(Kind::Boolean), *keys, where, "a toggle writes");
	if(kind == "increment" || kind == "animate")
		return expectKinds(*type, numberKinds, *keys, where, "it writes");
	if(kind == "push") {
		expectKinds(*type, bit(Kind::List), *keys, where, "a push needs");
		const json* item = typeInside(*type, nullptr);
		if(item && step.contains("value") && !holdsExpression(step["value"]))
			if(const auto problem = mismatch(step["value"], *item, written(*keys) + "[...]"))
				report(place + ".value", problem->text(), typePlace(*keys));
	}
}

void Checker::expectKinds(const json& type, Kinds kinds, const std::vector<PathKey>& keys, const std::string& place,
                          std::string_view what) {
	if((kinds & allowedKinds(type)) != 0) return;
	report(place, std::format("'{}' takes {}, but {} {}", written(keys), describeType(type), what, describeKinds(kinds)),
	       typePlace(keys));
}

void Checker::checkBind(const Prepared& node, const json& target, const std::string& place) {
	if(!types_ || !target.is_string() || isDynamic(target)) return;
	const json* typeName = node.value("type");
	const std::string type = typeName && typeName->is_string() ? typeName->get<std::string>() : "panel";
	const Kinds kinds = writtenByWidget(type);
	if(kinds == anyKind) return;
	const std::optional<std::vector<PathKey>> keys = parsePath(target.get_ref<const std::string&>(), place);
	if(const json* declared = keys ? typeOf(*keys) : nullptr)
		expectKinds(*declared, kinds, *keys, place, widgetName(type) + " writes");
}

void Checker::checkComparison(const json& raw, const std::string& place) {
	if(!types_ || !raw.is_array() || raw.size() != 2) return;
	for(std::size_t side = 0; side < 2; ++side) {
		const json& path = raw[side];
		const json& other = raw[1 - side];
		if(!path.is_object() || !path.contains("$get") || !path["$get"].is_array() || path["$get"].empty() ||
		   path["$get"][0] != "state")
			continue;
		const json literal = other.is_object() && other.contains("$value") && other.size() == 1 ? other["$value"]
		                     : other.is_primitive() && !other.is_null()                    ? other
		                                                                                   : json();
		if(literal.is_null()) continue;
		std::vector<PathKey> keys;
		for(const json& key : path["$get"]) keys.push_back({key.is_string() || key.is_number_integer() ? key : json(), nullptr});
		const json* type = typeOf(keys);
		if(type && (type->contains("enum") || type->contains("const")) && mismatch(literal, *type, "").has_value())
			report(place, std::format("'{}' is never {}: it takes {}", written(keys), literal.dump(), describeType(*type)),
			       typePlace(keys));
	}
}

} // namespace affresco::format::checker
