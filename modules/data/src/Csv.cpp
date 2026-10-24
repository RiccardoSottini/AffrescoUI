/// @file
/// @brief CSV import (Csv.hpp).

#include "affresco/data/Csv.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <map>

namespace affresco::data {

namespace {

/// @brief Lower case, for matching headers with fields.
/// @param text The text.
/// @return It in lower case (ASCII), without spaces, '_' and '-'.
std::string folded(std::string_view text) {
	std::string result;
	for(const char c : text)
		if(c != ' ' && c != '_' && c != '-')
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	return result;
}

/// @brief A cell converted to a field's type, when the mapping gives no expression.
/// @param field The field.
/// @param cell The cell's text (not empty).
/// @return The value; the text itself when it doesn't read as the type (the model check then names the problem).
json converted(const Field& field, const std::string& cell) {
	switch(field.type) {
	case FieldType::Number:
	case FieldType::Integer:
	case FieldType::DateTime: {
		double number = 0;
		const auto [end, error] = std::from_chars(cell.data(), cell.data() + cell.size(), number);
		if(error != std::errc() || end != cell.data() + cell.size() || !std::isfinite(number)) return cell;
		if(field.type != FieldType::Number && std::trunc(number) == number && std::abs(number) < 9.0e15)
			return static_cast<std::int64_t>(number);
		return number;
	}
	case FieldType::Boolean: {
		const std::string word = folded(cell);
		if(word == "true" || word == "yes" || word == "1") return true;
		if(word == "false" || word == "no" || word == "0") return false;
		return cell;
	}
	case FieldType::List: {
		json items = json::array();
		std::size_t start = 0;
		while(start <= cell.size()) {
			const std::size_t end = std::min(cell.find(';', start), cell.size());
			std::string item = cell.substr(start, end - start);
			while(!item.empty() && item.front() == ' ') item.erase(item.begin());
			while(!item.empty() && item.back() == ' ') item.pop_back();
			if(!item.empty()) items.push_back(converted(*field.of, item));
			start = end + 1;
		}
		return items;
	}
	default:
		return cell;
	}
}

/// @brief How each field is read from a row.
struct ColumnRule {
	std::size_t column = 0; ///< The column.
	std::string expression; ///< A conversion, or "".
};

/// @brief The rules of a mapping, checked against the header and the model.
/// @param options The import.
/// @param header The header row.
/// @param model The model.
/// @return field -> rule.
std::map<std::string, ColumnRule> rulesOf(const CsvImport& options, const std::vector<std::string>& header,
                                          const Model& model) {
	const auto columnOf = [&](const std::string& name, const std::string& where) {
		for(std::size_t i = 0; i < header.size(); ++i)
			if(header[i] == name || folded(header[i]) == folded(name)) return i;
		throw DataError("invalid", where + ": the CSV has no column '" + name + "'");
	};
	std::map<std::string, ColumnRule> rules;
	if(options.columns.is_null()) {
		for(std::size_t i = 0; i < header.size(); ++i)
			for(const std::string& field : model.order)
				if(folded(field) == folded(header[i])) rules[field] = {i, ""};
		if(rules.empty())
			throw DataError("invalid", "importCsv: no column of the CSV names a field of '" + model.name + "'");
		return rules;
	}
	if(!options.columns.is_object()) throw DataError("invalid", "importCsv.columns: expected field -> column");
	for(const auto& [field, rule] : options.columns.items()) {
		const std::string where = "importCsv.columns." + field;
		if(!model.fields.contains(field)) throw DataError("model", where + ": '" + model.name + "' has no such field");
		if(rule.is_string()) {
			rules[field] = {columnOf(rule.get<std::string>(), where), ""};
		} else if(rule.is_object() && rule.contains("column") && rule["column"].is_string()) {
			rules[field] = {columnOf(rule["column"].get<std::string>(), where),
			                rule.contains("with") && rule["with"].is_string() ? rule["with"].get<std::string>() : ""};
		} else {
			throw DataError("invalid", where + ": expected a column's header, or {\"column\", \"with\"}");
		}
	}
	return rules;
}

} // namespace

std::vector<std::vector<std::string>> parseCsv(std::string_view text, char separator) {
	if(text.size() > Limits::csvBytes) throw DataError("limit", "a CSV file is at most 16 MiB");
	if(text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
	std::vector<std::vector<std::string>> rows;
	std::vector<std::string> row;
	std::string cell;
	bool quoted = false, any = false;
	const auto endCell = [&] {
		if(row.size() >= Limits::csvColumns)
			throw DataError("limit", std::format("a CSV row has at most {} columns", Limits::csvColumns));
		row.push_back(std::move(cell));
		cell.clear();
	};
	const auto endRow = [&] {
		endCell();
		if(!(row.size() == 1 && row[0].empty())) rows.push_back(std::move(row)); // blank lines
		row.clear();
		if(rows.size() > Limits::csvRows + 1)
			throw DataError("limit", std::format("a CSV file has at most {} rows", Limits::csvRows));
	};
	for(std::size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		any = true;
		if(quoted) {
			if(c != '"') cell.push_back(c);
			else if(i + 1 < text.size() && text[i + 1] == '"') cell.push_back(text[++i]);
			else quoted = false;
		} else if(c == '"' && cell.empty()) {
			quoted = true;
		} else if(c == separator) {
			endCell();
		} else if(c == '\n' || c == '\r') {
			if(c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
			endRow();
		} else {
			cell.push_back(c);
		}
	}
	if(quoted) throw DataError("invalid", "the CSV has a quote that never closes");
	if(any && (!cell.empty() || !row.empty())) endRow();
	return rows;
}

json importCsv(Store& store, std::string_view text, const CsvImport& options, const Evaluator& evaluate) {
	const auto model = store.models().find(options.collection);
	if(model == store.models().end())
		throw DataError("model", "importCsv: no collection '" + options.collection + "' in \"models\"");
	const std::vector<std::vector<std::string>> rows = parseCsv(text, options.separator);
	json report = {{"imported", 0}, {"failed", 0}, {"failures", json::array()}, {"ids", json::array()}};
	if(rows.empty()) return report;
	const std::vector<std::string>& header = rows.front();
	const std::map<std::string, ColumnRule> rules = rulesOf(options, header, model->second);
	{
		const Store::Batch batch(store); // one step of undo
		for(std::size_t r = 1; r < rows.size(); ++r) {
			const std::vector<std::string>& row = rows[r];
			try {
				json byHeader = json::object();
				for(std::size_t i = 0; i < header.size() && i < row.size(); ++i) byHeader[header[i]] = row[i];
				json fields = json::object();
				for(const auto& [field, rule] : rules) {
					const std::string cell = rule.column < row.size() ? row[rule.column] : std::string();
					if(!rule.expression.empty()) {
						if(!evaluate) throw DataError("invalid", "a conversion needs the screen format's expressions");
						json value = evaluate(rule.expression, {{"value", cell}, {"row", byHeader}});
						if(!value.is_null()) fields[field] = std::move(value);
					} else if(!cell.empty()) {
						fields[field] = converted(model->second.fields.at(field), cell);
					}
				}
				report["ids"].push_back(store.insert(options.collection, fields, options.author).id);
				report["imported"] = report["imported"].get<std::size_t>() + 1;
			} catch(const DocumentError& error) {
				report["failed"] = report["failed"].get<std::size_t>() + 1;
				if(report["failures"].size() < Limits::failuresReported)
					report["failures"].push_back({{"row", r + 1}, {"error", error.what()}});
			}
		}
	}
	if(options.atomic && report["failed"].get<std::size_t>() > 0 && report["imported"].get<std::size_t>() > 0) {
		store.undo(); // the batch's group: every row it imported
		report["imported"] = 0;
		report["ids"] = json::array();
	}
	return report;
}

} // namespace affresco::data
