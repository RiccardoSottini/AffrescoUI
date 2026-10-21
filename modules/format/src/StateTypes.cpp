/// @file
/// @brief Typed state: types checked, values matched against them, and types described.

#include "StateTypes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace affresco::format {

namespace {

/// @brief Where a value is, built on the stack as matching goes down and written out only for a mismatch, so that a
/// value that matches costs no allocation.
struct Where {
	const Where* parent = nullptr;     ///< The enclosing place, or nullptr for the outermost.
	const std::string* root = nullptr; ///< The outermost place's text.
	std::string_view key;              ///< ".key" after the parent's text, if not empty.
	std::size_t index = noIndex;       ///< "[index]" after the parent's text, if set.

	static constexpr std::size_t noIndex = static_cast<std::size_t>(-1); ///< No index.

	/// @brief The place as text: "state.cards[2].title".
	/// @return It.
	[[nodiscard]] std::string text() const {
		std::string result = parent ? parent->text() : root ? *root : std::string();
		if(!key.empty()) result.append(".").append(key);
		if(index != noIndex) result += "[" + std::to_string(index) + "]";
		return result;
	}
};

/// @brief The first way a value doesn't match a type.
/// @param value The value.
/// @param type The type.
/// @param place The value's place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> matchAt(const json& value, const json& type, const Where& place);

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
	const auto it = std::ranges::find(typeNames, name, &TypeName::name); // not a pointer with MSVC's library
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
	for(const json& name : value) checkTypeNames(expect::string(name, place), place);
}

/// @brief Checks a keyword whose value is a count: a whole number of 0 or more.
/// @param value The value.
/// @param place Its place.
void checkCount(const json& value, const std::string& place) {
	if(!value.is_number_unsigned() && (!value.is_number_integer() || value.get<std::int64_t>() < 0))
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

/// @brief Checks a keyword that holds no type: enum, required, the limits and the notes.
/// @param key The keyword.
/// @param value Its value.
/// @param place Its place.
void checkValueKeyword(const std::string& key, const json& value, const std::string& place) {
	if(key == "enum" && (!value.is_array() || value.empty())) expect::fail(place, "expected a list of values");
	if(key == "required") {
		if(!value.is_array()) expect::fail(place, "expected a list of property names");
		for(const json& name : value) static_cast<void>(expect::string(name, place));
	}
	if(key == "minimum" || key == "maximum") static_cast<void>(expect::number(value, place));
	else if(key.starts_with("min") || key.starts_with("max")) checkCount(value, place);
	if((key == "description" || key == "title" || key == "note") && !value.is_string())
		expect::fail(place, "expected text");
}

void checkKeyword(const std::string& key, const json& value, const std::string& place, int depth) {
	if(std::ranges::find(keywords, key) == keywords.end())
		expect::fail(place, "unknown keyword (expected type, enum, const, properties, required, additionalProperties, "
		                    "items, minimum, maximum, minLength, maxLength, minItems, maxItems or a note: "
		                    "description, title, default, examples, note)");
	if(key == "type") checkTypeNames(value, place);
	else if(key == "properties")
		for(const auto& [name, type] : expect::object(value, place).items())
			checkTypeAt(type, std::format("{}.{}", place, name), depth + 1);
	else if(key == "additionalProperties" || key == "items") checkTypeAt(value, place, depth + 1);
	else checkValueKeyword(key, value, place);
}

/// @brief Checks what a type says about a number: "minimum" and "maximum".
/// @param value The number.
/// @param type The type.
/// @param place The value's place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> numberLimits(const json& value, const json& type, const Where& place) {
	const double number = value.get<double>();
	if(type.contains("minimum") && number < type["minimum"].get<double>())
		return Mismatch{place.text(), std::format("expected at least {}, got {}", type["minimum"].dump(), value.dump())};
	if(type.contains("maximum") && number > type["maximum"].get<double>())
		return Mismatch{place.text(), std::format("expected at most {}, got {}", type["maximum"].dump(), value.dump())};
	return std::nullopt;
}

/// @brief Checks what a type says about the size of text ("minLength", "maxLength") or a list ("minItems",
/// "maxItems").
/// @param value The text or the list.
/// @param type The type.
/// @param place The value's place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> sizeLimits(const json& value, const json& type, const Where& place) {
	const bool text = value.is_string();
	const std::size_t size = text ? characters(value.get_ref<const std::string&>()) : value.size();
	const char* unit = text ? "characters" : "items";
	const char* fewest = text ? "minLength" : "minItems";
	const char* most = text ? "maxLength" : "maxItems";
	if(type.contains(fewest) && size < type[fewest].get<std::size_t>())
		return Mismatch{place.text(), std::format("expected at least {} {}, got {}", type[fewest].dump(), unit, size)};
	if(type.contains(most) && size > type[most].get<std::size_t>())
		return Mismatch{place.text(), std::format("expected at most {} {}, got {}", type[most].dump(), unit, size)};
	return std::nullopt;
}

/// @brief Checks what a type says about numbers, text and lists.
/// @param value The value.
/// @param type The type.
/// @param place The value's place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> limits(const json& value, const json& type, const Where& place) {
	if(value.is_number()) return numberLimits(value, type, place);
	if(value.is_string() || value.is_array()) return sizeLimits(value, type, place);
	return std::nullopt;
}

/// @brief Matches an object's properties.
/// @param value The object.
/// @param type Its type.
/// @param place Its place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> objectMismatch(const json& value, const json& type, const Where& place) {
	if(const auto required = type.find("required"); required != type.end())
		for(const json& name : *required)
			if(!value.contains(name.get<std::string>()))
				return Mismatch{place.text(), "missing '" + name.get<std::string>() + "'"};
	for(const auto& [key, item] : value.items()) {
		const Where at{&place, nullptr, key};
		const json* inside = typeInside(type, &key);
		if(inside) {
			if(auto found = matchAt(item, *inside, at)) return found;
		} else if(const auto others = type.find("additionalProperties"); others != type.end() && *others == false) {
			std::string names;
			const auto properties = type.find("properties"); // a temporary's items() would outlive it (GCC 13)
			if(properties != type.end() && properties->is_object())
				for(const auto& [name, property] : properties->items()) names += (names.empty() ? "" : ", ") + name;
			return Mismatch{at.text(), "not one of its properties (" + (names.empty() ? std::string("none") : names) + ")"};
		}
	}
	return std::nullopt;
}

} // namespace

Kinds kindOf(const json& value) {
	switch(value.type()) {
	case json::value_t::null:
		return bit(Kind::Null);
	case json::value_t::boolean:
		return bit(Kind::Boolean);
	case json::value_t::number_integer:
	case json::value_t::number_unsigned:
		return bit(Kind::Integer);
	case json::value_t::number_float: {
		const double number = value.get<double>();
		return std::isfinite(number) && std::trunc(number) == number ? bit(Kind::Integer) : bit(Kind::Fraction);
	}
	case json::value_t::string:
		return bit(Kind::Text);
	case json::value_t::array:
		return bit(Kind::List);
	default:
		return bit(Kind::Object);
	}
}

Kinds allowedKinds(const json& type) {
	if(type.is_boolean()) return type.get<bool>() ? anyKind : 0;
	if(!type.is_object()) return anyKind;
	Kinds kinds = 0;
	if(const auto names = type.find("type"); names != type.end()) {
		const auto add = [&kinds](const json& name) {
			if(const TypeName* found = name.is_string() ? typeNamed(name.get_ref<const std::string&>()) : nullptr)
				kinds = static_cast<Kinds>(kinds | found->kinds);
		};
		if(names->is_array()) std::ranges::for_each(*names, add);
		else add(*names); // one name, read where it is: matching a large state allocates nothing
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
	for(const auto& [key, type] : expect::object(section, name).items())
		checkType(type, std::format("{}.{}", name, key));
}

namespace {

/// @brief Matches what a type says the value is: one of its "enum", its "const", one of its "type"s.
/// @param value The value.
/// @param type The type, an object.
/// @param place The value's place.
/// @return The mismatch, or std::nullopt.
std::optional<Mismatch> valueMismatch(const json& value, const json& type, const Where& place) {
	if(const auto values = type.find("enum");
	   values != type.end() && std::ranges::find(*values, value) == values->end()) {
		std::string list;
		for(std::size_t i = 0; i < values->size() && i < 8; ++i) list += (i ? ", " : "") + (*values)[i].dump();
		if(values->size() > 8) list += ", ...";
		return Mismatch{place.text(), "expected one of " + list + ", got " + shortly(value)};
	}
	if(const auto only = type.find("const"); only != type.end() && *only != value)
		return Mismatch{place.text(), "expected " + shortly(*only) + ", got " + shortly(value)};
	if(type.contains("type") && (kindOf(value) & allowedKinds(type)) == 0)
		return Mismatch{place.text(), "expected " + describeKinds(allowedKinds(type)) + ", got " + shortly(value)};
	return std::nullopt;
}

/// @brief Matches a list's items.
/// @param value The list.
/// @param type Its type.
/// @param place Its place.
/// @return The first mismatch, or std::nullopt.
std::optional<Mismatch> itemsMismatch(const json& value, const json& type, const Where& place) {
	const auto items = type.find("items");
	if(items == type.end()) return std::nullopt;
	for(std::size_t i = 0; i < value.size(); ++i)
		if(auto found = matchAt(value[i], *items, Where{&place, nullptr, {}, i})) return found;
	return std::nullopt;
}

/// @brief Describes an "enum": its values, joined.
/// @param values The values.
/// @return E.g. "'all' | 'bug' | 3".
std::string describeValues(const json& values) {
	std::string text;
	for(std::size_t i = 0; i < values.size() && i < 8; ++i) {
		const json& value = values[i];
		text += (i ? " | " : "") + (value.is_string() ? "'" + value.get<std::string>() + "'" : value.dump());
	}
	return values.size() > 8 ? text + " | ..." : text;
}

/// @brief Describes an object's properties: "{title: text, done: boolean}".
/// @param properties The properties.
/// @param depth Types inside types so far.
/// @return The description.
std::string describeProperties(const json& properties, int depth) {
	if(depth >= 3) return "{...}";
	std::string text = "{";
	for(const auto& [key, inside] : properties.items())
		text += (text.size() > 1 ? ", " : "") + key + ": " + describeType(inside, depth + 1);
	return text + '}';
}

/// @brief Describes one of a type's "type" names, with its items or properties.
/// @param name The name.
/// @param type The type.
/// @param depth Types inside types so far.
/// @return The description, or "" for a name that isn't one.
std::string describeName(const json& name, const json& type, int depth) {
	const TypeName* found = name.is_string() ? typeNamed(name.get_ref<const std::string&>()) : nullptr;
	if(!found) return "";
	if(found->name == "array" && type.contains("items"))
		return depth >= 3 ? "list of ..." : "list of " + describeType(type["items"], depth + 1);
	if(found->name == "object" && type.contains("properties") && type["properties"].is_object())
		return describeProperties(type["properties"], depth);
	return std::string(found->says);
}

std::optional<Mismatch> matchAt(const json& value, const json& type, const Where& place) {
	if(type.is_boolean()) {
		if(type.get<bool>()) return std::nullopt;
		return Mismatch{place.text(), "nothing may be here"};
	}
	if(auto found = valueMismatch(value, type, place)) return found;
	if(auto found = limits(value, type, place)) return found;
	if(value.is_object()) return objectMismatch(value, type, place);
	if(value.is_array()) return itemsMismatch(value, type, place);
	return std::nullopt;
}

} // namespace

std::optional<Mismatch> typeMismatch(const json& value, const json& type, const std::string& place) {
	return matchAt(value, type, Where{nullptr, &place, {}, Where::noIndex});
}

std::vector<Mismatch> stateMismatches(const json& document, const json& state) {
	std::vector<Mismatch> found;
	const auto types = document.find("stateTypes");
	if(types == document.end() || !types->is_object() || !state.is_object()) return found;
	for(const auto& [key, type] : types->items())
		if(const auto value = state.find(key); value != state.end())
			if(auto problem = typeMismatch(*value, type, "state." + key)) found.push_back(std::move(*problem));
	return found;
}

const json* typeInside(const json& type, const std::string* key) {
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
	if(const auto values = type.find("enum"); values != type.end() && values->is_array())
		return describeValues(*values);
	if(const auto only = type.find("const"); only != type.end()) return only->dump();
	const json names = type.value("type", json());
	std::string text;
	for(const json& name : names.is_array() ? names : json::array({names}))
		if(const std::string said = describeName(name, type, depth); !said.empty())
			text += (text.empty() ? "" : " | ") + said;
	return text.empty() ? "anything" : text;
}

} // namespace affresco::format
