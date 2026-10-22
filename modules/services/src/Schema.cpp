/// @file
/// @brief checkSchema(): types, enums, objects, lists, lengths and ranges.

#include "affresco/services/Schema.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::services {

namespace {

/// @brief 2^63: a double converts to a 64-bit integer only below it (and from its negative), else undefined.
constexpr double int64Limit = 9223372036854775808.0;

/// @brief Whether a number is whole and fits a 64-bit integer, tested without converting it.
/// @param number The number.
/// @return Whether it is.
bool isWhole(double number) noexcept {
	return number >= -int64Limit && number < int64Limit && std::trunc(number) == number;
}

/// @brief Whether a value has a type JSON Schema names.
/// @param value The value.
/// @param type The name.
/// @return Whether it does ("integer" takes whole numbers that fit 64 bits only; "number" takes any number).
bool hasType(const json& value, const std::string& type) {
	if(type == "object") return value.is_object();
	if(type == "array") return value.is_array();
	if(type == "string") return value.is_string();
	if(type == "boolean") return value.is_boolean();
	if(type == "null") return value.is_null();
	if(type == "number") return value.is_number();
	if(type == "integer") return value.is_number_integer() || (value.is_number_float() && isWhole(value.get<double>()));
	return true;
}

/// @brief Checks `type` and `enum`.
/// @param schema The schema.
/// @param value The value.
/// @param path Its place.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> checkKind(const json& schema, const json& value, const std::string& path) {
	if(const auto type = schema.find("type"); type != schema.end()) {
		const json types = type->is_array() ? *type : json::array({*type});
		const bool matches =
		    std::ranges::any_of(types, [&](const json& name) { return name.is_string() && hasType(value, name); });
		if(!matches) return path + ": must be " + (type->is_string() ? "a value of type " + type->get<std::string>() : "one of the types " + type->dump());
	}
	if(const auto choices = schema.find("enum"); choices != schema.end() && choices->is_array())
		if(std::ranges::find(*choices, value) == choices->end()) return path + ": must be one of " + choices->dump();
	return std::nullopt;
}

/// @brief A schema's numeric keyword.
/// @param schema The schema.
/// @param key The keyword, e.g. "maxLength".
/// @return Its number, or std::nullopt if it has none.
std::optional<double> bound(const json& schema, const char* key) {
	const auto it = schema.find(key);
	return it != schema.end() && it->is_number() ? std::optional(it->get<double>()) : std::nullopt;
}

/// @brief A length bound for a message, cut to a whole number: "64".
/// @param bound The bound.
/// @return Its text.
std::string whole(double bound) {
	return bound >= -int64Limit && bound < int64Limit ? std::to_string(static_cast<long long>(bound))
	                                                  : json(bound).dump();
}

/// @brief Checks the bounds: lengths of text, ranges of numbers, sizes of lists.
/// @param schema The schema.
/// @param value The value.
/// @param path Its place.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> checkBounds(const json& schema, const json& value, const std::string& path) {
	if(value.is_string()) {
		const auto size = static_cast<double>(value.get_ref<const std::string&>().size());
		if(const auto most = bound(schema, "maxLength"); most && size > *most)
			return path + ": at most " + whole(*most) + " bytes";
		if(const auto least = bound(schema, "minLength"); least && size < *least)
			return path + ": at least " + whole(*least) + " bytes";
	}
	if(value.is_number()) {
		const double number = value.get<double>();
		if(const auto most = bound(schema, "maximum"); most && number > *most)
			return path + ": at most " + json(*most).dump();
		if(const auto least = bound(schema, "minimum"); least && number < *least)
			return path + ": at least " + json(*least).dump();
	}
	if(const auto most = bound(schema, "maxItems");
	   most && value.is_array() && static_cast<double>(value.size()) > *most)
		return path + ": at most " + whole(*most) + " items";
	return std::nullopt;
}

/// @brief Checks that an object has the members a schema's `required` lists.
/// @param schema The schema.
/// @param value The object.
/// @param path Its place.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> checkRequired(const json& schema, const json& value, const std::string& path) {
	const auto required = schema.find("required");
	if(required == schema.end() || !required->is_array()) return std::nullopt;
	for(const json& name : *required)
		if(name.is_string() && !value.contains(name.get<std::string>()))
			return path + ": needs \"" + name.get<std::string>() + "\"";
	return std::nullopt;
}

/// @brief Checks one member against its `properties` schema, else against `additionalProperties`.
/// @param schema The object's schema.
/// @param key The member's key.
/// @param member The member.
/// @param place The member's place.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> checkMember(const json& schema, const std::string& key, const json& member,
                                       const std::string& place) {
	if(const auto properties = schema.find("properties");
	   properties != schema.end() && properties->is_object() && properties->contains(key))
		return checkSchema((*properties)[key], member, place);
	const auto additional = schema.find("additionalProperties");
	if(additional == schema.end()) return std::nullopt;
	if(additional->is_boolean() && !additional->get<bool>()) return place + ": unknown key";
	if(additional->is_object()) return checkSchema(*additional, member, place);
	return std::nullopt;
}

/// @brief Checks an object's members: `required`, `properties`, `additionalProperties`.
/// @param schema The schema.
/// @param value The object.
/// @param path Its place.
/// @return What is wrong, or std::nullopt.
std::optional<std::string> checkMembers(const json& schema, const json& value, const std::string& path) {
	if(auto problem = checkRequired(schema, value, path)) return problem;
	for(const auto& [key, member] : value.items())
		if(auto problem = checkMember(schema, key, member, path + "." + key)) return problem;
	return std::nullopt;
}

} // namespace

std::optional<std::string> checkSchema(const json& schema, const json& value, const std::string& path) {
	if(!schema.is_object()) return std::nullopt;
	if(auto problem = checkKind(schema, value, path)) return problem;
	if(auto problem = checkBounds(schema, value, path)) return problem;
	if(value.is_object()) return checkMembers(schema, value, path);
	if(value.is_array())
		if(const auto items = schema.find("items"); items != schema.end())
			for(std::size_t index = 0; index < value.size(); ++index)
				if(auto problem = checkSchema(*items, value[index], path + "[" + std::to_string(index) + "]"))
					return problem;
	return std::nullopt;
}

} // namespace affresco::services
