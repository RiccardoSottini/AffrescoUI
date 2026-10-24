/// @file
/// @brief Reading queries (Query.hpp).

#include "affresco/data/Query.hpp"

#include <algorithm>
#include <format>

namespace affresco::data {

namespace {

/// @brief Fails with an invalid query.
/// @param where The place.
/// @param message What is wrong.
[[noreturn]] void invalid(const std::string& where, const std::string& message) {
	throw DataError("invalid", where + ": " + message);
}

/// @brief Checks that a field can be compared or sorted by.
/// @param model The collection's model.
/// @param field The field.
/// @param where Its place.
void expectField(const Model& model, const std::string& field, const std::string& where) {
	if(isSystemField(field)) return;
	if(!model.fields.contains(field))
		throw DataError("model", where + ": '" + model.name + "' has no field '" + field + "'");
	const FieldType type = model.fields.at(field).type;
	if(type == FieldType::Object || type == FieldType::Map) invalid(where, "an object or map field can't be compared");
}

/// @brief Reads one comparison of a field.
/// @param model The collection's model.
/// @param field The field.
/// @param op The operator.
/// @param compared The value compared with.
/// @param place The field's place.
/// @return The condition.
Condition conditionOf(const Model& model, const std::string& field, std::string op, json compared,
                      const std::string& place) {
	if(!std::ranges::contains(conditionOperators(), op))
		invalid(place, "unknown comparison '" + op + "' (==, !=, <, <=, >, >=, in, contains, startsWith)");
	const bool in = op == "in";
	if(compared.is_structured() && !in) invalid(place, "expected a single value");
	if(in && (!compared.is_array() || compared.size() > 1000)) invalid(place, "\"in\" takes a list of at most 1000");
	const bool list = model.fields.contains(field) && model.fields.at(field).type == FieldType::List;
	const bool textual = op == "startsWith" || (op == "contains" && !list);
	if(textual && !compared.is_string()) invalid(place, "\"" + op + "\" takes a text");
	return {field, std::move(op), std::move(compared)};
}

Filter parseFilter(const json& where, const Model& model, const std::string& place, std::size_t& count, int depth);

/// @brief Reads a filter's "any": groups of filters, one of which must hold.
/// @param value The list.
/// @param model The collection's model.
/// @param place Its place.
/// @param count Conditions so far.
/// @param depth Filters inside filters so far.
/// @return The group.
std::vector<Filter> parseAny(const json& value, const Model& model, const std::string& place, std::size_t& count,
                             int depth) {
	if(!value.is_array() || value.empty()) invalid(place, "expected a list of filters");
	std::vector<Filter> group;
	for(std::size_t i = 0; i < value.size(); ++i)
		group.push_back(parseFilter(value[i], model, std::format("{}[{}]", place, i), count, depth + 1));
	return group;
}

/// @brief Reads a filter.
/// @param where The filter: field -> value or {op: value}, and "any": [filters].
/// @param model The collection's model.
/// @param place Its place.
/// @param count Conditions so far.
/// @param depth Filters inside filters so far.
/// @return The filter.
Filter parseFilter(const json& where, const Model& model, const std::string& place, std::size_t& count, int depth) {
	if(depth > 4) invalid(place, "\"any\" nests at most 4 deep");
	if(!where.is_object()) invalid(place, "expected an object: field -> value, or field -> {\"op\": value}");
	Filter filter;
	for(const auto& [field, value] : where.items()) {
		if(field == "any") {
			filter.any.push_back(parseAny(value, model, place + ".any", count, depth));
			continue;
		}
		expectField(model, field, place + "." + field);
		const json comparisons = value.is_object() && !value.empty() ? value : json{{"==", value}};
		for(const auto& [op, compared] : comparisons.items()) {
			if(++count > Limits::conditions) invalid(place, std::format("at most {} conditions", Limits::conditions));
			filter.all.push_back(conditionOf(model, field, op, compared, place + "." + field));
		}
	}
	return filter;
}

/// @brief Reads a query's "sort": one key or a list of them.
/// @param sort The value.
/// @param model The collection's model.
/// @param where Its place.
/// @return The keys.
std::vector<SortKey> parseSort(const json& sort, const Model& model, const std::string& where);

/// @brief Reads a sort key.
/// @param key "title", or {"field": "title", "descending": true}.
/// @param model The collection's model.
/// @param where Its place.
/// @return The key.
SortKey parseSortKey(const json& key, const Model& model, const std::string& where) {
	SortKey sort;
	if(key.is_string()) {
		sort.field = key.get<std::string>();
	} else if(key.is_object() && key.contains("field") && key["field"].is_string()) {
		sort.field = key["field"].get<std::string>();
		if(key.contains("descending")) sort.descending = expect::boolean(key["descending"], where + ".descending");
	} else {
		invalid(where, "expected a field's name, or {\"field\", \"descending\"}");
	}
	expectField(model, sort.field, where);
	return sort;
}

std::vector<SortKey> parseSort(const json& sort, const Model& model, const std::string& where) {
	const json keys = sort.is_array() ? sort : json::array({sort});
	if(keys.size() > 8) invalid(where, "at most 8 sort keys");
	std::vector<SortKey> read;
	for(std::size_t i = 0; i < keys.size(); ++i)
		read.push_back(parseSortKey(keys[i], model, std::format("{}[{}]", where, i)));
	return read;
}

/// @brief Reads a count that may be null (the default).
/// @param value The value.
/// @param fallback The default.
/// @param where Its place.
/// @return The count.
std::size_t countOr(const json& value, std::size_t fallback, const std::string& where) {
	if(value.is_null()) return fallback;
	if(!value.is_number() || value.get<double>() < 0) invalid(where, "expected a whole number from 0");
	return static_cast<std::size_t>(std::min(value.get<double>(), 1e15));
}

} // namespace

const std::vector<std::string>& conditionOperators() {
	static const std::vector<std::string> ops = {"==", "!=", "<", "<=", ">", ">=", "in", "contains", "startsWith"};
	return ops;
}

Query parseQuery(const json& query, const Models& models, const std::string& where) {
	static constexpr std::array<std::string_view, 7> keys = {"from",   "where", "sort", "limit",
	                                                         "offset", "count", "note"};
	if(!query.is_object()) invalid(where, "expected {\"from\", \"where\", \"sort\", \"limit\", \"offset\", \"count\"}");
	for(const auto& [key, value] : query.items())
		if(!std::ranges::contains(keys, key)) invalid(where + "." + key, "unknown key");
	Query read;
	const json from = query.value("from", json());
	if(!from.is_string()) invalid(where + ".from", "expected a collection's name");
	read.from = from.get<std::string>();
	const auto model = models.find(read.from);
	if(model == models.end())
		throw DataError("model", where + ".from: no collection '" + read.from + "' in \"models\"");
	std::size_t count = 0;
	if(const json filter = query.value("where", json()); !filter.is_null())
		read.where = parseFilter(filter, model->second, where + ".where", count, 0);
	if(const json sort = query.value("sort", json()); !sort.is_null())
		read.sort = parseSort(sort, model->second, where + ".sort");
	read.limit =
	    std::min(countOr(query.value("limit", json()), Limits::queryResults, where + ".limit"), Limits::queryResults);
	read.offset = countOr(query.value("offset", json()), 0, where + ".offset");
	if(query.contains("count")) read.count = expect::boolean(query["count"], where + ".count");
	return read;
}

} // namespace affresco::data
