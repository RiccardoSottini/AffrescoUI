#pragma once

/// @file
/// @brief data::importCsv: rows of a CSV text (RFC 4180) imported into a collection, each column matched to a field
/// (by its header, or by a mapping with conversions), each row checked against the model, the ones that fail
/// reported.

#include "affresco/data/Store.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::data {

/// @brief Reads a CSV text: fields separated by a comma (or another separator), quoted with '"' ("" is a quote
/// inside), rows ended by LF or CRLF. A UTF-8 byte order mark at the start is skipped.
/// @param text The text, at most Limits::csvBytes.
/// @param separator The separator: ',' (the default), ';' or '\t'.
/// @return The rows, the header first.
/// @throws DataError "limit" past the limits, "invalid" for an unclosed quote.
[[nodiscard]] std::vector<std::vector<std::string>> parseCsv(std::string_view text, char separator = ',');

/// @brief How a CSV text is imported.
struct CsvImport {
	std::string collection; ///< Into which collection.
	/// `field -> "Header"`, or `field -> {"column": "Header", "with": "{{number(value)}}"}` (the expression sees
	/// `value`, the cell's text, and `row`, the row by header). Null: each header names a field (case aside).
	json columns;
	char separator = ','; ///< The separator.
	bool atomic = false;  ///< All the rows or none: one that fails imports nothing.
	std::string author;   ///< Who.
};

/// @brief Imports a CSV text into a collection, as one step of undo: each row becomes a record, its cells converted
/// to the fields' types (numbers, integers, true/false/yes/no/1/0, lists separated by ';', empty cells as no value).
/// @param store The store.
/// @param text The CSV text.
/// @param options How.
/// @param evaluate For the mapping's expressions (may be empty if it has none).
/// @return `{"imported": n, "failed": n, "failures": [{"row": 3, "error": "..."}], "ids": [...]}`.
/// @throws DataError For a malformed text or mapping (no row is imported then).
json importCsv(Store& store, std::string_view text, const CsvImport& options, const Evaluator& evaluate);

} // namespace affresco::data
