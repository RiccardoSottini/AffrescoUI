#pragma once

/// @file
/// @brief data::Query: which records of a collection to read, in what order, how many: simple comparisons on fields
/// (`{"genre": "jazz", "rating": {">=": 3}}`), sort keys, a limit and an offset, or a count.

#include "affresco/data/Model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace affresco::data {

/// @brief One comparison of a field with a value.
struct Condition {
	std::string field; ///< A model's field, or "id", "created", "updated".
	std::string op;    ///< "==", "!=", "<", "<=", ">", ">=", "in" (a list), "contains", "startsWith".
	json value;        ///< The value compared with (null: "has none" for == and "has one" for !=).
};

/// @brief Conditions that all hold, and groups of which one must.
struct Filter {
	std::vector<Condition> all;           ///< Each holds.
	std::vector<std::vector<Filter>> any; ///< In each group, one of the filters holds.
};

/// @brief A sort key.
struct SortKey {
	std::string field;       ///< The field.
	bool descending = false; ///< Largest first.
};

/// @brief A query of one collection.
struct Query {
	std::string from;                         ///< The collection.
	Filter where;                             ///< Which records.
	std::vector<SortKey> sort;                ///< Their order, then the order they were stored in.
	std::size_t limit = Limits::queryResults; ///< At most this many (at most Limits::queryResults).
	std::size_t offset = 0;                   ///< Skip this many first.
	bool count = false;                       ///< Give how many match instead of the records.
};

/// @brief Reads a query whose values are already evaluated:
/// `{"from": "songs", "where": {"genre": "jazz", "rating": {">=": 3}, "any": [{...}, {...}]}, "sort": ["title",
/// {"field": "year", "descending": true}], "limit": 50, "offset": 0, "count": false}`.
/// @param query The query.
/// @param models The models: the collection and its fields must be there.
/// @param where Its place, for errors.
/// @return The query.
/// @throws DataError "invalid" (or "model" for an unknown collection or field), naming the place.
[[nodiscard]] Query parseQuery(const json& query, const Models& models, const std::string& where);

/// @brief The operators a condition may use.
/// @return "==", "!=", "<", "<=", ">", ">=", "in", "contains", "startsWith".
[[nodiscard]] const std::vector<std::string>& conditionOperators();

} // namespace affresco::data
