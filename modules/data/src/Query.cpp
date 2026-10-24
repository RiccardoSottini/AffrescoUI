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
			if(!value.is_array() || value.empty()) invalid(place + ".any", "expected a list of filters");
			std::vector<Filter> group;
			for(std::size_t i = 0; i < value.size(); ++i)
				group.push_back(parseFilter(value[i], model, std::format("{}.any[{}]", place, i), count, depth + 1));
			filter.any.push_back(std::move(group));
			continue;
		}
		expectField(model, field, place + "." + field);
		const auto add = [&](std::string op, json compared) {
			if(++count > Limits::conditions) invalid(place, std::format("at most {} conditions", Limits::conditions));
			const auto& ops = conditionOperators();
			if(std::ranges::find(ops, op) == ops.end())
				invalid(place + "." + field, "unknown comparison '" + op +
				                                 "' (==, !=, <, <=, >, >=, in, contains, "
				                                 "startsWith)");
			if(compared.is_structured() && op != "in") invalid(place + "." + field, "expected a single value");
			if(op == "in" && !compared.is_array()) invalid(place + "." + field, "\"in\" takes a list");
			if(op == "in" && compared.size() > 1000) invalid(place + "." + field, "\"in\" takes at most 1000 values");
			if((op == "contains" || op == "startsWith") && !compared.is_string() &&
			   !(op == "contains" && model.fields.contains(field) && model.fields.at(field).type == FieldType::List))
				invalid(place + "." + field, "\"" + op + "\" takes a text");
			filter.all.push_back({field, std::move(op), std::move(compared)});
		};
		const bool comparisons = value.is_object() && !value.empty();
		if(!comparisons) {
			add("==", value);
			continue;
		}
		for(const auto& [op, compared] : value.items()) add(op, compared);
	}
	return filter;
}

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
	if(!query.is_object()) invalid(where, "expected {\"from\", \"where\", \"sort\", \"limit\", \"offset\", \"count\"}");
	for(const auto& [key, value] : query.items())
		if(key != "from" && key != "where" && key != "sort" && key != "limit" && key != "offset" && key != "count" &&
		   key != "note")
			invalid(where + "." + key, "unknown key");
	Query read;
	const json from = query.value("from", json());
	if(!from.is_string()) invalid(where + ".from", "expected a collection's name");
	read.from = from.get<std::string>();
	const auto model = models.find(read.from);
	if(model == models.end())
		throw DataError("model", where + ".from: no collection '" + read.from + "' in \"models\"");
	std::size_t count = 0;
	if(query.contains("where") && !query["where"].is_null())
		read.where = parseFilter(query["where"], model->second, where + ".where", count, 0);
	if(query.contains("sort") && !query["sort"].is_null()) {
		const json& sort = query["sort"];
		const json keys = sort.is_array() ? sort : json::array({sort});
		if(keys.size() > 8) invalid(where + ".sort", "at most 8 sort keys");
		for(std::size_t i = 0; i < keys.size(); ++i)
			read.sort.push_back(parseSortKey(keys[i], model->second, std::format("{}.sort[{}]", where, i)));
	}
	read.limit =
	    std::min(countOr(query.value("limit", json()), Limits::queryResults, where + ".limit"), Limits::queryResults);
	read.offset = countOr(query.value("offset", json()), 0, where + ".offset");
	if(query.contains("count")) read.count = expect::boolean(query["count"], where + ".count");
	return read;
}

} // namespace affresco::data
