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

/// @brief A cell read as a number of a field: a whole one for an integer or a time.
/// @param field The field.
/// @param cell The cell's text.
/// @return The number, or the text itself if it isn't one.
json numberOf(const Field& field, const std::string& cell) {
	double number = 0;
	const auto [end, error] = std::from_chars(cell.data(), cell.data() + cell.size(), number);
	if(error != std::errc() || end != cell.data() + cell.size() || !std::isfinite(number)) return cell;
	const bool whole = field.type != FieldType::Number && std::trunc(number) == number && std::abs(number) < 9.0e15;
	return whole ? json(static_cast<std::int64_t>(number)) : json(number);
}

/// @brief A cell read as true or false: true, yes or 1, false, no or 0, in any case.
/// @param cell The cell's text.
/// @return The boolean, or the text itself if it isn't one.
json booleanOf(const std::string& cell) {
	const std::string word = folded(cell);
	if(word == "true" || word == "yes" || word == "1") return true;
	if(word == "false" || word == "no" || word == "0") return false;
	return cell;
}

json converted(const Field& field, const std::string& cell);

/// @brief A cell read as a list: items separated by ';', spaces around them trimmed, empty ones left out.
/// @param field The list field.
/// @param cell The cell's text.
/// @return The list.
json listOf(const Field& field, const std::string& cell) {
	json items = json::array();
	for(std::size_t start = 0; start <= cell.size();) {
		const std::size_t end = std::min(cell.find(';', start), cell.size());
		const std::size_t first = cell.find_first_not_of(' ', start);
		const std::size_t last = cell.find_last_not_of(' ', end == 0 ? 0 : end - 1);
		if(first < end && last != std::string::npos && last >= first)
			items.push_back(converted(*field.of, cell.substr(first, last - first + 1)));
		start = end + 1;
	}
	return items;
}

/// @brief A cell converted to a field's type, when the mapping gives no expression.
/// @param field The field.
/// @param cell The cell's text (not empty).
/// @return The value; the text itself when it doesn't read as the type (the model check then names the problem).
json converted(const Field& field, const std::string& cell) {
	switch(field.type) {
	case FieldType::Number:
	case FieldType::Integer:
	case FieldType::DateTime:
		return numberOf(field, cell);
	case FieldType::Boolean:
		return booleanOf(cell);
	case FieldType::List:
		return listOf(field, cell);
	default:
		return cell;
	}
}

/// @brief How each field is read from a row.
struct ColumnRule {
	std::size_t column = 0; ///< The column.
	std::string expression; ///< A conversion, or "".
};

/// @brief The rules when no mapping is given: each column whose header names a field (case, spaces, '_' and '-'
/// aside) fills it.
/// @param header The header row.
/// @param model The model.
/// @return field -> rule.
std::map<std::string, ColumnRule> rulesByHeader(const std::vector<std::string>& header, const Model& model) {
	std::map<std::string, ColumnRule> rules;
	for(std::size_t i = 0; i < header.size(); ++i)
		for(const std::string& field : model.order)
			if(folded(field) == folded(header[i])) rules[field] = {i, ""};
	if(rules.empty())
		throw DataError("invalid", "importCsv: no column of the CSV names a field of '" + model.name + "'");
	return rules;
}

/// @brief The column a mapping names.
/// @param header The header row.
/// @param name The column's header as the mapping writes it.
/// @param where The mapping's place.
/// @return Its position.
std::size_t columnOf(const std::vector<std::string>& header, const std::string& name, const std::string& where) {
	for(std::size_t i = 0; i < header.size(); ++i)
		if(header[i] == name || folded(header[i]) == folded(name)) return i;
	throw DataError("invalid", where + ": the CSV has no column '" + name + "'");
}

/// @brief The rules of a mapping, checked against the header and the model.
/// @param options The import.
/// @param header The header row.
/// @param model The model.
/// @return field -> rule.
std::map<std::string, ColumnRule> rulesOf(const CsvImport& options, const std::vector<std::string>& header,
                                          const Model& model) {
	if(options.columns.is_null()) return rulesByHeader(header, model);
	if(!options.columns.is_object()) throw DataError("invalid", "importCsv.columns: expected field -> column");
	std::map<std::string, ColumnRule> rules;
	for(const auto& [field, rule] : options.columns.items()) {
		const std::string where = "importCsv.columns." + field;
		if(!model.fields.contains(field)) throw DataError("model", where + ": '" + model.name + "' has no such field");
		const json& column = rule.is_object() ? rule.value("column", json()) : rule;
		if(!column.is_string())
			throw DataError("invalid", where + ": expected a column's header, or {\"column\", \"with\"}");
		const json with = rule.is_object() ? rule.value("with", json()) : json();
		rules[field] = {columnOf(header, column.get<std::string>(), where),
		                with.is_string() ? with.get<std::string>() : ""};
	}
	return rules;
}

/// @brief Reads CSV text a character at a time.
class CsvReader {
public:
	/// @brief A reader.
	/// @param separator The separator.
	explicit CsvReader(char separator) : separator_(separator) {}

	/// @brief Reads the whole text.
	/// @param text The text, its byte order mark taken off.
	/// @return The rows.
	std::vector<std::vector<std::string>> read(std::string_view text) {
		for(std::size_t i = 0; i < text.size(); ++i) {
			const char c = text[i];
			const bool next = i + 1 < text.size();
			if(quoted_) quotedCharacter(c, next && text[i + 1] == '"', i);
			else if(c == '"' && cell_.empty()) quoted_ = true;
			else if(c == separator_) endCell();
			else if(c == '\n' || c == '\r') {
				if(c == '\r' && next && text[i + 1] == '\n') ++i;
				endRow();
			} else cell_.push_back(c);
		}
		if(quoted_) throw DataError("invalid", "the CSV has a quote that never closes");
		if(!cell_.empty() || !row_.empty()) endRow();
		return std::move(rows_);
	}

private:
	/// @brief A character inside quotes: "" is a quote, a lone quote ends them.
	/// @param c The character.
	/// @param quoteNext Whether a quote follows it.
	/// @param i Its position, moved past a doubled quote.
	void quotedCharacter(char c, bool quoteNext, std::size_t& i) {
		if(c != '"') cell_.push_back(c);
		else if(quoteNext) {
			cell_.push_back('"');
			++i;
		} else quoted_ = false;
	}
	/// @brief Ends a cell.
	void endCell() {
		if(row_.size() >= Limits::csvColumns)
			throw DataError("limit", std::format("a CSV row has at most {} columns", Limits::csvColumns));
		row_.push_back(std::move(cell_));
		cell_.clear();
	}
	/// @brief Ends a row; a blank line is none.
	void endRow() {
		endCell();
		if(!(row_.size() == 1 && row_[0].empty())) rows_.push_back(std::move(row_));
		row_.clear();
		if(rows_.size() > Limits::csvRows + 1)
			throw DataError("limit", std::format("a CSV file has at most {} rows", Limits::csvRows));
	}

	char separator_;                             ///< The separator.
	std::vector<std::vector<std::string>> rows_; ///< The rows so far.
	std::vector<std::string> row_;               ///< The row being read.
	std::string cell_;                           ///< The cell being read.
	bool quoted_ = false;                        ///< Inside quotes.
};

/// @brief A row's fields, by the rules.
/// @param row The row.
/// @param header The header.
/// @param rules The rules.
/// @param model The model.
/// @param evaluate For the rules' conversions.
/// @return The fields.
json rowFields(const std::vector<std::string>& row, const std::vector<std::string>& header,
               const std::map<std::string, ColumnRule>& rules, const Model& model, const Evaluator& evaluate) {
	json byHeader = json::object();
	for(std::size_t i = 0; i < header.size() && i < row.size(); ++i) byHeader[header[i]] = row[i];
	json fields = json::object();
	for(const auto& [field, rule] : rules) {
		const std::string cell = rule.column < row.size() ? row[rule.column] : std::string();
		if(rule.expression.empty()) {
			if(!cell.empty()) fields[field] = converted(model.fields.at(field), cell);
			continue;
		}
		if(!evaluate) throw DataError("invalid", "a conversion needs the screen format's expressions");
		json value = evaluate(rule.expression, {{"value", cell}, {"row", byHeader}});
		if(!value.is_null()) fields[field] = std::move(value);
	}
	return fields;
}

} // namespace

std::vector<std::vector<std::string>> parseCsv(std::string_view text, char separator) {
	if(text.size() > Limits::csvBytes) throw DataError("limit", "a CSV file is at most 16 MiB");
	if(text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
	return CsvReader(separator).read(text);
}

json importCsv(Store& store, std::string_view text, const CsvImport& options, const Evaluator& evaluate) {
	const auto model = store.models().find(options.collection);
	if(model == store.models().end())
		throw DataError("model", "importCsv: no collection '" + options.collection + "' in \"models\"");
	const std::vector<std::vector<std::string>> rows = parseCsv(text, options.separator);
	json report = {{"imported", 0}, {"failed", 0}, {"failures", json::array()}, {"ids", json::array()}};
	if(rows.empty()) return report;
	const std::map<std::string, ColumnRule> rules = rulesOf(options, rows.front(), model->second);
	std::size_t imported = 0, failed = 0;
	{
		const Store::Batch batch(store); // one step of undo
		for(std::size_t r = 1; r < rows.size(); ++r) try {
				const json fields = rowFields(rows[r], rows.front(), rules, model->second, evaluate);
				report["ids"].push_back(store.insert(options.collection, fields, options.author).id);
				++imported;
			} catch(const DocumentError& error) {
				if(++failed <= Limits::failuresReported)
					report["failures"].push_back({{"row", r + 1}, {"error", error.what()}});
			}
	}
	if(options.atomic && failed > 0 && imported > 0) {
		store.undo(); // the batch's group: every row it imported
		imported = 0;
		report["ids"] = json::array();
	}
	report["imported"] = imported;
	report["failed"] = failed;
	return report;
}

} // namespace affresco::data
