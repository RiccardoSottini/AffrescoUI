/// @file
/// @brief Checking documents against a JSON Schema (Schema.hpp).

#include "Schema.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace affresco::cli {

namespace {

/// "$ref"s inside each other before a check stops: a schema that refers to itself without end.
constexpr int maxReferences = 512;

/// The longest string a pattern is matched against: std::regex recurses on each character.
constexpr std::size_t maxPatternInput = 100000;

/// @brief A member's place.
/// @param place The object's place.
/// @param key The member's key.
/// @return "place.key", or "key" at the top.
std::string memberPlace(const std::string& place, const std::string& key) { return place.empty() ? key : place + "." + key; }

/// @brief Tells whether a value is of a JSON Schema type.
/// @param value The value.
/// @param type "object", "array", "string", "number", "integer", "boolean" or "null".
/// @return true if it is.
bool isType(const json& value, const std::string& type) {
	if(type == "integer") return value.is_number_integer() || (value.is_number_float() && std::trunc(value.get<double>()) == value.get<double>());
	if(type == "number") return value.is_number();
	if(type == "object") return value.is_object();
	if(type == "array") return value.is_array();
	if(type == "string") return value.is_string();
	if(type == "boolean") return value.is_boolean();
	return type == "null" && value.is_null();
}

/// @brief Writes a value short, for a problem's text.
/// @param value The value.
/// @return Its JSON, cut at 40 characters.
std::string shortly(const json& value) {
	std::string text = value.dump();
	return text.size() > 40 ? text.substr(0, 37) + "..." : text;
}

/// @brief Records a problem, if they are wanted.
/// @param problems Where (may be nullptr).
/// @param place Where in the document.
/// @param problem What.
/// @return false: the value doesn't match.
bool report(std::vector<SchemaProblem>* problems, const std::string& place, std::string problem) {
	if(problems) problems->push_back({place, std::move(problem)});
	return false;
}

/// @brief Checks a number against minimum, maximum, exclusiveMinimum and exclusiveMaximum.
/// @param schema The subschema.
/// @param number The number.
/// @param place Its place.
/// @param problems Receives what doesn't match (may be nullptr).
/// @return true if it is in range.
bool inRange(const json& schema, double number, const std::string& place, std::vector<SchemaProblem>* problems) {
	bool ok = true;
	auto bound = [&](const char* key, auto outside, const char* words) {
		if(const auto it = schema.find(key); it != schema.end() && it->is_number() && outside(number, it->get<double>()))
			ok = report(problems, place, std::string("expected a number ") + words + " " + it->dump());
	};
	bound("minimum", [](double n, double b) { return n < b; }, "of at least");
	bound("maximum", [](double n, double b) { return n > b; }, "of at most");
	bound("exclusiveMinimum", [](double n, double b) { return n <= b; }, "greater than");
	bound("exclusiveMaximum", [](double n, double b) { return n >= b; }, "less than");
	return ok;
}

/// @brief Checks a count (of characters, items or members) against a minimum and a maximum keyword.
/// @param schema The subschema.
/// @param count The count.
/// @param keys The minimum and maximum keywords.
/// @param what What is counted, for the problem.
/// @param place The value's place.
/// @param problems Receives what doesn't match (may be nullptr).
/// @return true if it is in range.
bool countInRange(const json& schema, std::size_t count, std::pair<const char*, const char*> keys, const char* what,
                  const std::string& place, std::vector<SchemaProblem>* problems) {
	const auto low = schema.find(keys.first), high = schema.find(keys.second);
	if(low != schema.end() && low->is_number_unsigned() && count < low->get<std::size_t>())
		return report(problems, place, "expected at least " + low->dump() + " " + what);
	if(high != schema.end() && high->is_number_unsigned() && count > high->get<std::size_t>())
		return report(problems, place, "expected at most " + high->dump() + " " + what);
	return true;
}

} // namespace

Schema::Schema(json schema) : schema_(std::move(schema)) {}

std::vector<SchemaProblem> Schema::check(const json& document) {
	std::vector<SchemaProblem> problems;
	std::set<std::string> evaluated;
	depth_ = 0;
	matches(schema_, document, "", &problems, evaluated);
	std::ranges::sort(problems, {}, [](const SchemaProblem& p) { return std::tie(p.place, p.problem); });
	const auto [first, last] = std::ranges::unique(
	    problems, [](const SchemaProblem& a, const SchemaProblem& b) { return a.place == b.place && a.problem == b.problem; });
	problems.erase(first, last);
	return problems;
}

const json& Schema::resolve(const std::string& reference) const {
	if(!reference.starts_with('#')) throw std::runtime_error("schema: only references inside it are read: " + reference);
	const json::json_pointer pointer(reference.substr(1));
	if(!schema_.contains(pointer)) throw std::runtime_error("schema: nothing at " + reference);
	return schema_.at(pointer);
}

const std::regex& Schema::compiled(const std::string& pattern) {
	auto it = regexes_.find(pattern);
	if(it == regexes_.end()) it = regexes_.emplace(pattern, std::regex(pattern, std::regex::ECMAScript)).first;
	return it->second;
}

bool Schema::matches(const json& schema, const json& value, const std::string& place,
                     std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated) {
	if(schema.is_boolean()) return schema.get<bool>() || report(problems, place, "not allowed here");
	if(!schema.is_object()) return true;

	bool ok = true;
	if(const auto reference = schema.find("$ref"); reference != schema.end()) {
		if(++depth_ > maxReferences) throw std::runtime_error("schema: references nested too deeply");
		ok = matches(resolve(reference->get<std::string>()), value, place, problems, evaluated);
		--depth_;
	}
	if(const auto type = schema.find("type"); type != schema.end()) {
		const json types = type->is_array() ? *type : json::array({*type});
		if(std::ranges::none_of(types, [&](const json& t) { return isType(value, t.get<std::string>()); }))
			return report(problems, place, "expected " + (types.size() == 1 ? "a value of type " : std::string("one of the types ")) + type->dump() + ", got " + shortly(value));
	}
	if(const auto allowed = schema.find("enum"); allowed != schema.end() && std::ranges::find(*allowed, value) == allowed->end())
		ok = report(problems, place, "expected one of " + shortly(*allowed) + ", got " + shortly(value));
	if(const auto constant = schema.find("const"); constant != schema.end() && *constant != value)
		ok = report(problems, place, "expected " + shortly(*constant) + ", got " + shortly(value));
	ok = matchesKind(schema, value, place, problems, evaluated) && ok;
	return matchesCombined(schema, value, place, problems, evaluated) && ok;
}

bool Schema::matchesKind(const json& schema, const json& value, const std::string& place,
                         std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated) {
	bool ok = true;
	if(value.is_number()) ok = inRange(schema, value.get<double>(), place, problems);
	if(value.is_string()) {
		const auto& text = value.get_ref<const std::string&>();
		ok = countInRange(schema, text.size(), {"minLength", "maxLength"}, "characters", place, problems) && ok;
		if(const auto pattern = schema.find("pattern"); pattern != schema.end()) {
			if(text.size() > maxPatternInput) ok = report(problems, place, "too long to check against its pattern");
			else if(!std::regex_search(text, compiled(pattern->get<std::string>())))
				ok = report(problems, place, "expected text matching " + pattern->dump() + ", got " + shortly(value));
		}
	}
	if(value.is_array()) {
		ok = countInRange(schema, value.size(), {"minItems", "maxItems"}, "items", place, problems) && ok;
		if(const auto items = schema.find("items"); items != schema.end())
			for(std::size_t i = 0; i < value.size(); ++i) {
				std::set<std::string> inner;
				ok = matches(*items, value[i], place + "[" + std::to_string(i) + "]", problems, inner) && ok;
			}
	}
	if(value.is_object()) ok = matchesMembers(schema, value, place, problems, evaluated) && ok;
	return ok;
}

bool Schema::matchesMembers(const json& schema, const json& value, const std::string& place,
                            std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated) {
	bool ok = countInRange(schema, value.size(), {"minProperties", "maxProperties"}, "members", place, problems);
	for(const json& name : schema.value("required", json::array()))
		if(name.is_string() && !value.contains(name.get<std::string>()))
			ok = report(problems, place, "missing \"" + name.get<std::string>() + "\"");

	const json none = json::object();
	const json& properties = schema.contains("properties") ? schema["properties"] : none;
	const json& patterns = schema.contains("patternProperties") ? schema["patternProperties"] : none;
	for(const auto& [key, member] : value.items()) {
		const std::string at = memberPlace(place, key);
		std::set<std::string> inner;
		bool known = false;
		if(const auto it = properties.find(key); it != properties.end()) {
			known = true;
			ok = matches(*it, member, at, problems, inner) && ok;
		}
		for(const auto& [pattern, subschema] : patterns.items())
			if(std::regex_search(key, compiled(pattern))) {
				known = true;
				ok = matches(subschema, member, at, problems, inner) && ok;
			}
		if(const auto names = schema.find("propertyNames"); names != schema.end())
			ok = matches(*names, json(key), at, problems, inner) && ok;
		if(known) evaluated.insert(key);
		else if(const auto additional = schema.find("additionalProperties"); additional != schema.end()) {
			evaluated.insert(key);
			if(additional->is_boolean() && !additional->get<bool>()) ok = report(problems, at, "unknown property");
			else ok = matches(*additional, member, at, problems, inner) && ok;
		}
	}
	return ok;
}

bool Schema::matchesCombined(const json& schema, const json& value, const std::string& place,
                             std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated) {
	bool ok = true;
	for(const json& part : schema.value("allOf", json::array())) ok = matches(part, value, place, problems, evaluated) && ok;

	for(const char* key : {"anyOf", "oneOf"}) {
		const auto options = schema.find(key);
		if(options == schema.end()) continue;
		std::size_t matched = 0;
		std::vector<SchemaProblem> closest; // the problems of the option that came nearest, to report
		for(const json& option : *options) {
			std::vector<SchemaProblem> found;
			std::set<std::string> inner;
			if(matches(option, value, place, &found, inner)) {
				++matched;
				evaluated.insert(inner.begin(), inner.end());
			} else if(closest.empty() || found.size() < closest.size()) {
				closest = std::move(found);
			}
		}
		if(matched == 0) {
			if(problems) problems->insert(problems->end(), closest.begin(), closest.end());
			ok = false;
		} else if(key == std::string_view("oneOf") && matched > 1) {
			ok = report(problems, place, "matches more than one of the forms \"oneOf\" allows");
		}
	}

	if(const auto negated = schema.find("not"); negated != schema.end()) {
		std::set<std::string> inner;
		if(matches(*negated, value, place, nullptr, inner)) ok = report(problems, place, "not allowed here: " + shortly(value));
	}
	if(const auto condition = schema.find("if"); condition != schema.end()) {
		std::set<std::string> inner;
		const bool holds = matches(*condition, value, place, nullptr, inner);
		if(holds) evaluated.insert(inner.begin(), inner.end());
		if(const auto branch = schema.find(holds ? "then" : "else"); branch != schema.end())
			ok = matches(*branch, value, place, problems, evaluated) && ok;
	}

	if(const auto rest = schema.find("unevaluatedProperties"); rest != schema.end() && value.is_object())
		for(const auto& [key, member] : value.items()) {
			if(evaluated.contains(key)) continue;
			std::set<std::string> inner;
			if(*rest == false) ok = report(problems, memberPlace(place, key), "unknown property");
			else ok = matches(*rest, member, memberPlace(place, key), problems, inner) && ok;
		}
	return ok;
}

} // namespace affresco::cli
