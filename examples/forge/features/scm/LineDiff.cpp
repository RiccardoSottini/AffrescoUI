/// @file
/// @brief Forge's source control: a bounded line diff and the gutter marks made from it (LineDiff.hpp).

#include "LineDiff.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace forge {

namespace {

using affresco::json;

/// @brief Splits a text into lines, without their `\n` or a `\r` before it; a last empty line is left out.
/// @param text The text.
/// @return The lines.
std::vector<std::string_view> splitLines(std::string_view text) {
	std::vector<std::string_view> lines;
	while(!text.empty()) {
		const std::size_t end = std::min(text.find('\n'), text.size());
		std::string_view line = text.substr(0, end);
		if(!line.empty() && line.back() == '\r') line.remove_suffix(1);
		lines.push_back(line);
		text.remove_prefix(std::min(end + 1, text.size()));
	}
	return lines;
}

/// @brief The kinds of rows, in the order a changed block shows them.
enum class Row : std::uint8_t { Same, Removed, Added };

/// @brief Matches the changed middles of two texts: a longest common subsequence, or none past the limit.
/// @param before The middle before.
/// @param after The middle after.
/// @param matched The longest middle it matches.
/// @return The rows, in order.
std::vector<Row> matchMiddle(const std::vector<std::string_view>& before, const std::vector<std::string_view>& after,
                             std::size_t matched) {
	const std::size_t rows = before.size(), columns = after.size();
	std::vector<Row> result;
	if(rows > matched || columns > matched) {
		result.assign(rows, Row::Removed);
		result.insert(result.end(), columns, Row::Added);
		return result;
	}
	std::vector<std::uint16_t> common((rows + 1) * (columns + 1), 0); // common[i][j]: of before[i..] and after[j..]
	const auto at = [columns](std::size_t i, std::size_t j) { return i * (columns + 1) + j; };
	for(std::size_t i = rows; i-- > 0;)
		for(std::size_t j = columns; j-- > 0;)
			common[at(i, j)] = before[i] == after[j]
			                       ? static_cast<std::uint16_t>(common[at(i + 1, j + 1)] + 1)
			                       : std::max(common[at(i + 1, j)], common[at(i, j + 1)]);
	std::size_t i = 0, j = 0;
	while(i < rows || j < columns) {
		if(i < rows && j < columns && before[i] == after[j]) {
			result.push_back(Row::Same);
			++i, ++j;
		} else if(j < columns && (i == rows || common[at(i, j + 1)] >= common[at(i + 1, j)])) {
			result.push_back(Row::Added);
			++j;
		} else {
			result.push_back(Row::Removed);
			++i;
		}
	}
	return result;
}

/// @brief Puts a block's removed rows before its added ones, as diff views show them.
/// @param rows The rows.
void removedFirst(std::vector<Row>& rows) {
	for(auto start = rows.begin(); start != rows.end();) {
		auto end = std::find(start, rows.end(), Row::Same);
		std::stable_partition(start, end, [](Row row) { return row == Row::Removed; });
		start = end == rows.end() ? end : end + 1;
	}
}

/// @brief Every row of the two texts: the common start, the matched middle, the common end.
/// @param before The lines before.
/// @param after The lines after.
/// @param matched The longest middle matched one by one.
/// @return The rows.
std::vector<Row> allRows(const std::vector<std::string_view>& before, const std::vector<std::string_view>& after,
                         std::size_t matched) {
	std::size_t start = 0, end = 0;
	while(start < before.size() && start < after.size() && before[start] == after[start]) ++start;
	while(end < before.size() - start && end < after.size() - start &&
	      before[before.size() - 1 - end] == after[after.size() - 1 - end])
		++end;
	std::vector<Row> rows(start, Row::Same);
	const auto head = static_cast<std::ptrdiff_t>(start), tail = static_cast<std::ptrdiff_t>(end);
	std::vector<Row> middle =
	    matchMiddle({before.begin() + head, before.end() - tail}, {after.begin() + head, after.end() - tail}, matched);
	removedFirst(middle);
	rows.insert(rows.end(), middle.begin(), middle.end());
	rows.insert(rows.end(), end, Row::Same);
	return rows;
}

} // namespace

json diffLines(std::string_view original, std::string_view modified, const DiffLimits& limits) {
	const std::vector<std::string_view> before = splitLines(original), after = splitLines(modified);
	const std::vector<Row> all = allRows(before, after, limits.matched);
	json rows = json::array();
	std::size_t line = 0, other = 0, added = 0, removed = 0;
	for(const Row row : all) {
		const std::size_t from = row == Row::Added ? 0 : ++line, to = row == Row::Removed ? 0 : ++other;
		added += row == Row::Added ? 1 : 0;
		removed += row == Row::Removed ? 1 : 0;
		if(rows.size() == limits.rows) continue;
		const char* kind = row == Row::Same ? "same" : row == Row::Removed ? "removed" : "added";
		const std::string_view text = row == Row::Removed ? before[from - 1] : after[to - 1];
		rows.push_back({{"kind", kind}, {"original", from}, {"modified", to}, {"text", std::string(text)}});
	}
	return {{"rows", std::move(rows)}, {"added", added}, {"removed", removed}, {"truncated", all.size() > limits.rows}};
}

json gutterMarks(const json& diff) {
	json marks = json::array();
	const json& rows = diff.at("rows");
	std::size_t line = 0; // the modified text's last line so far
	for(std::size_t i = 0; i < rows.size();) {
		if(rows[i].at("kind") == "same") {
			line = rows[i].at("modified").get<std::size_t>();
			++i;
			continue;
		}
		std::size_t removed = 0, added = 0;
		for(; i < rows.size() && rows[i].at("kind") != "same"; ++i) (rows[i].at("kind") == "added" ? added : removed)++;
		const char* kind = added == 0 ? "deleted" : removed == 0 ? "added" : "modified";
		const char* color = added == 0 ? "#f85149" : removed == 0 ? "#2ea043" : "#0078d4";
		// a deletion marks the line after it, or the last line if nothing follows
		const std::size_t first = added == 0 && i == rows.size() ? std::max<std::size_t>(line, 1) : line + 1;
		const std::size_t last = added == 0 ? first : line + added;
		marks.push_back({{"line", first}, {"endLine", last}, {"kind", "gutter"}, {"color", color}, {"text", kind}});
		line += added;
	}
	return marks;
}

} // namespace forge
