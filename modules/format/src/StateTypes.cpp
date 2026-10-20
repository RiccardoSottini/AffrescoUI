/// @file
/// @brief Typed state: types checked, values matched against them, and types described.

#include "StateTypes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace affresco::format {

namespace {

/// @brief A name "type" may give, with its kinds.
struct TypeName {
	std::string_view name; ///< E.g. "integer".
	Kinds kinds;           ///< What it allows.
	std::string_view says; ///< How a description says it.
};

/// The names "type" may give.
constexpr auto typeNames = std::to_array<TypeName>({
    {"string", bit(Kind::Text), "text"},
    {"number", numberKinds, "number"},
    {"integer", bit(Kind::Integer), "whole number"},
    {"boolean", bit(Kind::Boolean), "boolean"},
    {"object", bit(Kind::Object), "object"},
    {"array", bit(Kind::List), "list"},
    {"null", bit(Kind::Null), "null"},
});

/// The keywords a type may have.
constexpr auto keywords = std::to_array<std::string_view>(
    {"type", "enum", "const", "properties", "required", "additionalProperties", "items", "minimum", "maximum",
     "minLength", "maxLength", "minItems", "maxItems", "description", "title", "default", "examples", "note"});

/// @brief Finds a name "type" may give.
/// @param name The name.
/// @return It, or nullptr.
const TypeName* typeNamed(std::string_view name) noexcept {
	const auto it = std::ranges::find(typeNames, name, &TypeName::name);
	return it == typeNames.end() ? nullptr : &*it;
}

/// @brief A value, short, for a message.
/// @param value The value.
/// @return Its JSON, cut at 40 characters.
std::string shortly(const json& value) {
	std::string text = value.dump();
	if(text.size() > 40) text = text.substr(0, 37) + "...";
	return text;
}

/// @brief Counts the characters of UTF-8 text, as JSON Schema's lengths do.
/// @param text The text.
/// @return Its code points.
std::size_t characters(const std::string& text) noexcept {
	return static_cast<std::size_t>(
	    std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; }));
}

/// @brief Checks "type": a name, or a list of names.
/// @param value The value.
/// @param place Its place.
void checkTypeNames(const json& value, const std::string& place) {
	if(value.is_string()) {
		if(!typeNamed(value.get_ref<const std::string&>()))
			expect::fail(place, "unknown type (expected string, number, integer, boolean, object, array or null)");
		return;
	}
	if(!value.is_array() || value.empty()) expect::fail(place, "expected a type's name, or a list of them");
	for(std::size_t i = 0; i < value.size(); ++i) checkTypeNames(expect::string(value[i], place), place);
}

/// @brief Checks a keyword whose value is a count: a whole number of 0 or more.
/// @param value The value.
/// @param place Its place.
void checkCount(const json& value, const std::string& place) {
	if(!value.is_number_unsigned() && !(value.is_number_integer() && value.get<std::int64_t>() >= 0))
		expect::fail(place, "expected a whole number of 0 or more");
}

/// @brief Checks one keyword of a type.
/// @param key The keyword.
/// @param value Its value.
/// @param place Its place.
/// @param depth Types inside types so far.
void checkKeyword(const std::string& key, const json& value, const std::string& place, int depth);

/// @brief Checks a type, counting how deep it is.
/// @param type The type.
/// @param place Its place.
/// @param depth Types inside types so far.
void checkTypeAt(const json& type, const std::string& place, int depth) {
	if(depth > maxTypeDepth) expect::fail(place, "types nested too deeply");
	if(type.is_boolean()) return; // true: anything; false: nothing
	for(const auto& [key, value] : expect::object(type, place).items())
		checkKeyword(key, value, std::format("{}.{}", place, key), depth);
}

void checkKeyword(const std::string& key, const json& value, const std::string& place, int depth) {
	if(std::ranges::find(keywords, key) == keywords.end())
		expect::fail(place, "unknown keyword (expected type, enum, const, properties, required, additionalProperties, "
		                    "items, minimum, maximum, minLength, maxLength, minItems, maxItems or a note: "
		                    "description, title, default, examples, note)");
	if(key == "type") checkTypeNames(value, place);
	else if(key == "enum" && (!value.is_array() || value.empty())) expect::fail(place, "expected a list of values");
	else if(key == "properties")
		for(const auto& [name, type] : expect::object(value, place).items())
			checkTypeAt(type, std::format("{}.{}", place, name), depth + 1);
	else if(key == "required" && !value.is_array()) expect::fail(place, "expected a list of property names");
	else if(key == "required")
		for(const json& name : value) static_cast<void>(expect::string(name, place));
	else if(key == "additionalProperties" || key == "items") checkTypeAt(value, place, depth + 1);
	else if(key == "minimum" || key == "maximum") expect::number(value, place);
	else if(key.starts_with("min") || key.starts_with("max")) checkCount(value, place);
	else if((key == "description" || key == "title" || key == "note") && !value.is_string())
		expect::fail(place, "expected text");
}

/// @brief Checks what a type says about numbers, text and lists.
/// @param value The value.
/// @param type The type.
/// @param place The value's place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> limits(const json& value, const json& type, const std::string& place) {
	if(value.is_number()) {
		const double number = value.get<double>();
		if(type.contains("minimum") && number < type["minimum"].get<double>())
			return Mismatch{place, std::format("expected at least {}, got {}", type["minimum"].dump(), value.dump())};
		if(type.contains("maximum") && number > type["maximum"].get<double>())
			return Mismatch{place, std::format("expected at most {}, got {}", type["maximum"].dump(), value.dump())};
	}
	const std::size_t size = value.is_string() ? characters(value.get_ref<const std::string&>())
	                         : value.is_array() ? value.size()
	                                            : 0;
	const char* unit = value.is_string() ? "characters" : "items";
	const char* fewest = value.is_string() ? "minLength" : "minItems";
	const char* most = value.is_string() ? "maxLength" : "maxItems";
	if((value.is_string() || value.is_array()) && type.contains(fewest) && size < type[fewest].get<std::size_t>())
		return Mismatch{place, std::format("expected at least {} {}, got {}", type[fewest].dump(), unit, size)};
	if((value.is_string() || value.is_array()) && type.contains(most) && size > type[most].get<std::size_t>())
		return Mismatch{place, std::format("expected at most {} {}, got {}", type[most].dump(), unit, size)};
	return std::nullopt;
}

/// @brief Matches an object's properties.
/// @param value The object.
/// @param type Its type.
/// @param place Its place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> objectMismatch(const json& value, const json& type, const std::string& place) {
	if(const auto required = type.find("required"); required != type.end())
		for(const json& name : *required)
			if(!value.contains(name.get<std::string>()))
				return Mismatch{place, "missing '" + name.get<std::string>() + "'"};
	for(const auto& [key, item] : value.items()) {
		const std::string at = std::format("{}.{}", place, key);
		const json* inside = typeInside(type, &key);
		if(inside) {
			if(auto found = mismatch(item, *inside, at)) return found;
		} else if(type.value("additionalProperties", json(true)) == false) {
			std::string names;
			for(const auto& [name, property] : type.value("properties", json::object()).items())
				names += (names.empty() ? "" : ", ") + name;
			return Mismatch{at, "not one of its properties (" + (names.empty() ? std::string("none") : names) + ")"};
		}
	}
	return std::nullopt;
}

} // namespace

Kinds kindOf(const json& value) noexcept {
	switch(value.type()) {
	case json::value_t::null: return bit(Kind::Null);
	case json::value_t::boolean: return bit(Kind::Boolean);
	case json::value_t::number_integer:
	case json::value_t::number_unsigned: return bit(Kind::Integer);
	case json::value_t::number_float: {
		const double number = value.get<double>();
		return std::isfinite(number) && std::trunc(number) == number ? bit(Kind::Integer) : bit(Kind::Fraction);
	}
	case json::value_t::string: return bit(Kind::Text);
	case json::value_t::array: return bit(Kind::List);
	default: return bit(Kind::Object);
	}
}

Kinds allowedKinds(const json& type) noexcept {
	if(type.is_boolean()) return type.get<bool>() ? anyKind : 0;
	if(!type.is_object()) return anyKind;
	Kinds kinds = 0;
	if(const auto names = type.find("type"); names != type.end()) {
		for(const json& name : names->is_array() ? *names : json::array({*names}))
			if(const TypeName* found = name.is_string() ? typeNamed(name.get_ref<const std::string&>()) : nullptr)
				kinds = static_cast<Kinds>(kinds | found->kinds);
		return kinds;
	}
	const auto values = type.find("enum");
	if(values != type.end() && values->is_array())
		for(const json& value : *values) kinds = static_cast<Kinds>(kinds | kindOf(value));
	else if(const auto only = type.find("const"); only != type.end()) kinds = kindOf(*only);
	else kinds = anyKind;
	return kinds;
}

std::string describeKinds(Kinds kinds) {
	if(kinds == anyKind) return "anything";
	static constexpr std::array<std::pair<Kinds, std::string_view>, 7> names = {{
	    {numberKinds, "a number"},
	    {bit(Kind::Integer), "a whole number"},
	    {bit(Kind::Fraction), "a number"},
	    {bit(Kind::Text), "text"},
	    {bit(Kind::Boolean), "true or false"},
	    {bit(Kind::List), "a list"},
	    {bit(Kind::Object), "an object"},
	}};
	std::string result;
	Kinds left = kinds;
	for(const auto& [bits, name] : names)
		if((left & bits) == bits) {
			result += (result.empty() ? "" : " or ") + std::string(name);
			left = static_cast<Kinds>(left & ~bits);
		}
	if(left & bit(Kind::Null)) result += (result.empty() ? "" : " or ") + std::string("null");
	return result.empty() ? "nothing" : result;
}

void checkType(const json& type, const std::string& place) { checkTypeAt(type, place, 0); }

void checkStateTypes(const json& section, const std::string& name) {
	for(const auto& [key, type] : expect::object(section, name).items()) checkType(type, std::format("{}.{}", name, key));
}

std::optional<Mismatch> mismatch(const json& value, const json& type, const std::string& place) {
	if(type.is_boolean()) {
		if(type.get<bool>()) return std::nullopt;
		return Mismatch{place, "nothing may be here"};
	}
	if(const auto values = type.find("enum"); values != type.end() && std::ranges::find(*values, value) == values->end()) {
		std::string list;
		for(std::size_t i = 0; i < values->size() && i < 8; ++i) list += (i ? ", " : "") + (*values)[i].dump();
		if(values->size() > 8) list += ", ...";
		return Mismatch{place, "expected one of " + list + ", got " + shortly(value)};
	}
	if(const auto only = type.find("const"); only != type.end() && *only != value)
		return Mismatch{place, "expected " + shortly(*only) + ", got " + shortly(value)};
	if(type.contains("type") && (kindOf(value) & allowedKinds(type)) == 0)
		return Mismatch{place, "expected " + describeKinds(allowedKinds(type)) + ", got " + shortly(value)};
	if(auto found = limits(value, type, place)) return found;
	if(value.is_object()) return objectMismatch(value, type, place);
	if(value.is_array())
		if(const auto items = type.find("items"); items != type.end())
			for(std::size_t i = 0; i < value.size(); ++i)
				if(auto found = mismatch(value[i], *items, place + "[" + std::to_string(i) + "]")) return found;
	return std::nullopt;
}

std::vector<Mismatch> stateMismatches(const json& document, const json& state) {
	std::vector<Mismatch> found;
	const auto types = document.find("stateTypes");
	if(types == document.end() || !types->is_object() || !state.is_object()) return found;
	for(const auto& [key, type] : types->items())
		if(const auto value = state.find(key); value != state.end())
			if(auto problem = mismatch(*value, type, "state." + key)) found.push_back(std::move(*problem));
	return found;
}

const json* typeInside(const json& type, const std::string* key) noexcept {
	if(!type.is_object()) return nullptr;
	if(key) {
		if(const auto properties = type.find("properties"); properties != type.end() && properties->is_object())
			if(const auto it = properties->find(*key); it != properties->end()) return &*it;
	} else if(const auto items = type.find("items"); items != type.end()) {
		return &*items;
	}
	const auto other = type.find("additionalProperties");
	return other != type.end() && other->is_object() ? &*other : nullptr;
}

std::string describeType(const json& type, int depth) {
	if(type.is_boolean()) return type.get<bool>() ? "anything" : "nothing";
	if(!type.is_object()) return "anything";
	if(const auto values = type.find("enum"); values != type.end() && values->is_array()) {
		std::string text;
		for(std::size_t i = 0; i < values->size() && i < 8; ++i) {
			const json& value = (*values)[i];
			text += (i ? " | " : "") + (value.is_string() ? "'" + value.get<std::string>() + "'" : value.dump());
		}
		return values->size() > 8 ? text + " | ..." : text;
	}
	if(const auto only = type.find("const"); only != type.end()) return only->dump();
	const json names = type.value("type", json());
	std::string text;
	for(const json& name : names.is_array() ? names : json::array({names})) {
		const TypeName* found = name.is_string() ? typeNamed(name.get_ref<const std::string&>()) : nullptr;
		if(!found) continue;
		std::string said(found->says);
		if(found->name == "array" && type.contains("items"))
			said = depth >= 3 ? "list of ..." : "list of " + describeType(type["items"], depth + 1);
		else if(found->name == "object" && type.contains("properties") && type["properties"].is_object()) {
			if(depth >= 3) said = "{...}";
			else {
				said = "{";
				for(const auto& [key, inside] : type["properties"].items())
					said += (said.size() > 1 ? ", " : "") + key + ": " + describeType(inside, depth + 1);
				said += "}";
			}
		}
		text += (text.empty() ? "" : " | ") + said;
	}
	return text.empty() ? "anything" : text;
}

} // namespace affresco::format
