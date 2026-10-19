#pragma once

/// @file
/// @brief Checks a document against a JSON Schema: the part of draft 2020-12 that schema/affrescoui-screen.schema.json
/// uses, so that `affrescoui check` needs no other library.

#include <affresco/core/Json.hpp>

#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace affresco::cli {

/// @brief A place where a document doesn't match its schema.
struct SchemaProblem {
	std::string place;   ///< Where, as errors name places: "root.children[2].width" ("" for the document).
	std::string problem; ///< What: "expected one of: \"start\", \"center\", \"end\"".
};

/// @brief A JSON Schema, ready to check documents against.
///
/// It reads: `$ref` (to "#/..." in the same schema), `type`, `enum`, `const`, `properties`, `required`,
/// `additionalProperties`, `patternProperties`, `propertyNames`, `unevaluatedProperties`, `items`, `minItems`,
/// `maxItems`, `minProperties`, `maxProperties`, `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`,
/// `minLength`, `maxLength`, `pattern`, `allOf`, `anyOf`, `oneOf`, `not`, `if`/`then`/`else`. Other keywords
/// (`description`, `$defs`, ...) are ignored. A schema using more must not be trusted to this class.
class Schema {
public:
	/// @brief A schema.
	/// @param schema The schema document.
	explicit Schema(json schema);

	/// @brief Checks a document.
	/// @param document The document.
	/// @return Where it doesn't match, each place once; empty if it matches.
	[[nodiscard]] std::vector<SchemaProblem> check(const json& document);

private:
	/// @brief Checks a value against a subschema.
	/// @param schema The subschema.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr: only whether it matches is wanted).
	/// @param evaluated Receives the object members some keyword evaluated, for unevaluatedProperties.
	/// @return true if it matches.
	bool matches(const json& schema, const json& value, const std::string& place, std::vector<SchemaProblem>* problems,
	             std::set<std::string>& evaluated);

	/// @brief Checks the keywords about one kind of value (strings, numbers, arrays, objects).
	/// @param schema The subschema.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the object members evaluated.
	/// @return true if it matches.
	bool matchesKind(const json& schema, const json& value, const std::string& place,
	                 std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks a string against minLength, maxLength and pattern.
	/// @param schema The subschema.
	/// @param value The string.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @return true if it matches.
	bool matchesText(const json& schema, const json& value, const std::string& place,
	                 std::vector<SchemaProblem>* problems);

	/// @brief Checks an object's members.
	/// @param schema The subschema.
	/// @param value The object.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the members evaluated.
	/// @return true if they match.
	bool matchesMembers(const json& schema, const json& value, const std::string& place,
	                    std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks one member against properties, patternProperties, propertyNames and additionalProperties.
	/// @param schema The object's subschema.
	/// @param key The member's key.
	/// @param member Its value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the key if a keyword evaluated it.
	/// @return true if it matches.
	bool matchesMember(const json& schema, const std::string& key, const json& member, const std::string& place,
	                   std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks the combining keywords: allOf, anyOf, oneOf, not, if/then/else, then unevaluatedProperties.
	/// @param schema The subschema.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the members the subschemas that matched evaluated.
	/// @return true if it matches.
	bool matchesCombined(const json& schema, const json& value, const std::string& place,
	                     std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks anyOf or oneOf. When no option matches, the problems of the nearest are reported.
	/// @param options The subschemas.
	/// @param onlyOne true for oneOf: exactly one must match.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the members the options that matched evaluated.
	/// @return true if it matches.
	bool matchesOptions(const json& options, bool onlyOne, const json& value, const std::string& place,
	                    std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks not, and if with then or else.
	/// @param schema The subschema.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated Receives the members a condition that held and its branch evaluated.
	/// @return true if it matches.
	bool matchesConditions(const json& schema, const json& value, const std::string& place,
	                       std::vector<SchemaProblem>* problems, std::set<std::string>& evaluated);

	/// @brief Checks the members no other keyword evaluated against unevaluatedProperties.
	/// @param schema The subschema.
	/// @param value The value.
	/// @param place Its place.
	/// @param problems Receives what doesn't match (may be nullptr).
	/// @param evaluated The members evaluated so far.
	/// @return true if they match.
	bool matchesUnevaluated(const json& schema, const json& value, const std::string& place,
	                        std::vector<SchemaProblem>* problems, const std::set<std::string>& evaluated);

	/// @brief Finds what a "$ref" points to.
	/// @param reference "#/$defs/...".
	/// @return The subschema.
	/// @throws std::runtime_error If it isn't in this schema.
	const json& resolve(const std::string& reference) const;

	/// @brief A pattern, compiled once.
	/// @param pattern The pattern (ECMAScript).
	/// @return The regular expression.
	const std::regex& compiled(const std::string& pattern);

	json schema_;                               ///< The schema document.
	std::map<std::string, std::regex> regexes_; ///< Patterns compiled so far.
	int depth_ = 0;                             ///< How many "$ref"s deep the check is (a loop must stop).
};

} // namespace affresco::cli
