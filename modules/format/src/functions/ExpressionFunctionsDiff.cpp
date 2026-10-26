/// @file
/// @brief Expression functions for diffs and JSON: diffLines, atPointer, applyPatch and formatJson.
///
/// diffLines() gives what a diff view draws: hunks with their unified-diff headers, numbered lines and, for a split
/// view, side-by-side rows. atPointer() and applyPatch() read and change JSON as RFC 6901 and RFC 6902 say, so a view
/// can show a value before and after a patch; formatJson() writes a value as indented lines to compare.

#include "ExpressionFunctions.hpp"

#include <affresco/core/TextDiff.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format::detail {
namespace {

/// Most lines of each text diffLines() reads (the rest is left out, and the result says it is truncated).
constexpr std::size_t mostLines = 200'000;
/// Most unchanged lines around a change.
constexpr std::int64_t mostContext = 10'000;
/// Most lines diffLines() can be asked to give.
constexpr std::int64_t mostLimit = 100'000;
/// The longest pointer atPointer() reads.
constexpr std::size_t longestPointer = 4096;
/// Most operations of a patch.
constexpr std::size_t mostOperations = 10'000;
/// Most values the copy operations of one patch may copy, in all (a copy of a copy doubles).
constexpr std::size_t mostCopied = std::size_t{1} << 22;

/// @brief What diffLines() is asked for.
struct DiffOptions {
	std::int64_t context = 3;      ///< Unchanged lines shown around a change; -1 for the whole text.
	bool ignoreWhitespace = false; ///< Lines that differ only in whitespace are the same.
	bool split = false;            ///< Also side-by-side rows.
	std::size_t limit = 5'000;     ///< Most lines given.
};

/// @brief One line of a diff: kept, deleted or inserted, with its number in each text.
struct DiffLine {
	DiffEdit::Kind kind = DiffEdit::Kind::Equal; ///< What happened to it.
	std::size_t old = 0;                         ///< Its number in the original, from 1 (0: not in it).
	std::size_t now = 0;                         ///< Its number in the modified text, from 1 (0: not in it).
	std::string_view oldText;                    ///< Its text in the original.
	std::string_view newText;                    ///< Its text in the modified text.
};

/// @brief A hunk: lines [begin, end) of the diff, and the unchanged lines left out before it.
struct Hunk {
	std::size_t begin = 0;  ///< Its first line.
	std::size_t end = 0;    ///< After its last line.
	std::size_t hidden = 0; ///< Unchanged lines between it and the hunk before (or the start).
};

/// @brief Reads diffLines()'s options.
/// @param call The call; its third argument, if any, is the options.
/// @return They.
/// @throws DocumentError If they are not an object.
[[nodiscard]] DiffOptions diffOptions(const Call& call) {
	DiffOptions options;
	if(call.size() < 3 || call[2].isNull()) return options;
	if(!call[2].isObject()) call.fail("expected options: { context, ignoreWhitespace, split, limit }");
	const Value& given = call[2];
	if(const std::optional<Value> context = given.member("context"); context && context->isNumber())
		options.context = std::clamp<std::int64_t>(saturated(context->number()), -1, mostContext);
	if(const std::optional<Value> ignore = given.member("ignoreWhitespace"); ignore)
		options.ignoreWhitespace = ignore->boolean();
	if(const std::optional<Value> split = given.member("split"); split) options.split = split->boolean();
	if(const std::optional<Value> limit = given.member("limit"); limit && limit->isNumber())
		options.limit = static_cast<std::size_t>(std::clamp<std::int64_t>(saturated(limit->number()), 0, mostLimit));
	return options;
}

/// @brief The lines of a text as a diff counts them: none for "", and no empty line after a final "\n".
/// @param text The text.
/// @param truncated Set if the text has more than mostLines lines (the rest is left out).
/// @return Views into the text.
[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view text, bool& truncated) {
	if(text.empty()) return {};
	std::size_t at = 0;
	for(std::size_t count = 0; count < mostLines && at != std::string_view::npos; ++count) {
		at = text.find('\n', at);
		if(at != std::string_view::npos) ++at;
	}
	if(at != std::string_view::npos && at < text.size()) {
		text = text.substr(0, at);
		truncated = true;
	}
	std::vector<std::string_view> lines = splitLines(text);
	if(text.ends_with('\n')) lines.pop_back();
	return lines;
}

/// @brief A line without its whitespace, to compare lines that differ only in it.
/// @param line The line.
/// @return Its other characters.
[[nodiscard]] std::string withoutWhitespace(std::string_view line) {
	std::string kept;
	kept.reserve(line.size());
	for(const char c : line)
		if(std::isspace(static_cast<unsigned char>(c)) == 0) kept.push_back(c);
	return kept;
}

/// @brief The edit script between two lists of lines, comparing them whole or without their whitespace.
/// @param original The original's lines.
/// @param modified The modified text's lines.
/// @param ignoreWhitespace Whether lines that differ only in whitespace are the same.
/// @return The runs of the script.
[[nodiscard]] std::vector<DiffEdit> editsOf(std::span<const std::string_view> original,
                                            std::span<const std::string_view> modified, bool ignoreWhitespace) {
	if(!ignoreWhitespace) return diffLines(original, modified);
	const auto keys = [](std::span<const std::string_view> lines) {
		std::vector<std::string> out;
		out.reserve(lines.size());
		for(const std::string_view line : lines) out.push_back(withoutWhitespace(line));
		return out;
	};
	const std::vector<std::string> a = keys(original), b = keys(modified);
	const std::vector<std::string_view> aViews(a.begin(), a.end()), bViews(b.begin(), b.end());
	return diffLines(aViews, bViews);
}

/// @brief Lists every line of an edit script, numbered in both texts.
/// @param edits The script.
/// @param original The original's lines.
/// @param modified The modified text's lines.
/// @return The lines, in order.
[[nodiscard]] std::vector<DiffLine> linesOfEdits(std::span<const DiffEdit> edits,
                                                 std::span<const std::string_view> original,
                                                 std::span<const std::string_view> modified) {
	std::vector<DiffLine> lines;
	for(const DiffEdit& edit : edits)
		for(std::size_t i = 0; i < edit.count; ++i) {
			const std::size_t o = edit.original + i, m = edit.modified + i;
			if(edit.kind == DiffEdit::Kind::Equal) lines.push_back({edit.kind, o + 1, m + 1, original[o], modified[m]});
			else if(edit.kind == DiffEdit::Kind::Delete) lines.push_back({edit.kind, o + 1, 0, original[o], {}});
			else lines.push_back({edit.kind, 0, m + 1, {}, modified[m]});
		}
	return lines;
}

/// @brief Groups the changed lines with the unchanged lines around them into hunks; hunks that touch are one.
/// @param lines The diff's lines.
/// @param context Unchanged lines kept around a change; -1 for all of them.
/// @return The hunks, in order.
[[nodiscard]] std::vector<Hunk> hunksOf(std::span<const DiffLine> lines, std::int64_t context) {
	const std::size_t around = context < 0 ? lines.size() : static_cast<std::size_t>(context);
	std::vector<Hunk> hunks;
	for(std::size_t i = 0; i < lines.size(); ++i) {
		if(lines[i].kind == DiffEdit::Kind::Equal) continue;
		std::size_t last = i;
		while(last + 1 < lines.size() && lines[last + 1].kind != DiffEdit::Kind::Equal) ++last;
		const std::size_t begin = i > around ? i - around : 0, end = std::min(lines.size(), last + 1 + around);
		if(!hunks.empty() && begin <= hunks.back().end) hunks.back().end = end;
		else hunks.push_back({begin, end, begin - (hunks.empty() ? 0 : hunks.back().end)});
		i = last;
	}
	return hunks;
}

/// @brief A range of a unified-diff header: "start,count", or "start" alone for one line.
/// @param start Its first line (for no lines: the line before them).
/// @param count Its lines.
/// @return The text.
[[nodiscard]] std::string headerRange(std::size_t start, std::size_t count) {
	return count == 1 ? std::to_string(start) : std::to_string(start) + "," + std::to_string(count);
}

/// @brief A line number, or null for a line not in that text.
/// @param number The number, 0 for none.
/// @return The value.
[[nodiscard]] Value lineNumber(std::size_t number) {
	return number == 0 ? Value() : Value::fromInteger(static_cast<std::int64_t>(number));
}

/// @brief One line of a hunk, as the unified view draws it: { kind, old, new, text }.
/// @param line The line.
/// @return The object.
[[nodiscard]] Value unifiedLine(const DiffLine& line) {
	const bool deleted = line.kind == DiffEdit::Kind::Delete;
	const char* kind = line.kind == DiffEdit::Kind::Equal ? "context" : deleted ? "delete" : "insert";
	std::vector<Value::Field> fields;
	fields.push_back({"kind", Value::fromText(kind)});
	fields.push_back({"old", lineNumber(line.old)});
	fields.push_back({"new", lineNumber(line.now)});
	fields.push_back({"text", Value::fromText(deleted ? line.oldText : line.newText)});
	return Value::fromObject(std::move(fields));
}

/// @brief One side of a split row: { n, text, kind }.
/// @param number The line's number in that side's text.
/// @param text Its text.
/// @param kind "context", "delete" or "insert".
/// @return The object.
[[nodiscard]] Value side(std::size_t number, std::string_view text, const char* kind) {
	std::vector<Value::Field> fields;
	fields.push_back({"n", lineNumber(number)});
	fields.push_back({"text", Value::fromText(text)});
	fields.push_back({"kind", Value::fromText(kind)});
	return Value::fromObject(std::move(fields));
}

/// @brief A split row: { left, right }, either null where that side has no line.
/// @param left The original's side.
/// @param right The modified text's side.
/// @return The object.
[[nodiscard]] Value row(Value left, Value right) {
	std::vector<Value::Field> fields;
	fields.push_back({"left", std::move(left)});
	fields.push_back({"right", std::move(right)});
	return Value::fromObject(std::move(fields));
}

/// @brief The side-by-side rows of a hunk's lines: kept lines on both sides, deletions paired with the insertions
/// after them.
/// @param lines The lines.
/// @return The rows.
[[nodiscard]] Value splitRows(std::span<const DiffLine> lines) {
	std::vector<Value> rows;
	for(std::size_t i = 0; i < lines.size();) {
		if(lines[i].kind == DiffEdit::Kind::Equal) {
			rows.push_back(
			    row(side(lines[i].old, lines[i].oldText, "context"), side(lines[i].now, lines[i].newText, "context")));
			++i;
			continue;
		}
		std::size_t inserts = i;
		while(inserts < lines.size() && lines[inserts].kind == DiffEdit::Kind::Delete) ++inserts;
		std::size_t end = inserts;
		while(end < lines.size() && lines[end].kind == DiffEdit::Kind::Insert) ++end;
		const std::size_t deleted = inserts - i, inserted = end - inserts;
		for(std::size_t k = 0; k < std::max(deleted, inserted); ++k) {
			const DiffLine* left = k < deleted ? &lines[i + k] : nullptr;
			const DiffLine* right = k < inserted ? &lines[inserts + k] : nullptr;
			rows.push_back(row(left ? side(left->old, left->oldText, "delete") : Value(),
			                   right ? side(right->now, right->newText, "insert") : Value()));
		}
		i = end;
	}
	return Value::fromList(std::move(rows));
}

/// @brief Where a hunk starts and how long it is in each text, counted from the lines before it.
struct HunkRanges {
	std::size_t oldStart = 0, oldLines = 0, newStart = 0, newLines = 0; ///< As the header writes them.
};

/// @brief Measures a hunk.
/// @param lines The hunk's lines.
/// @param oldBefore The original's lines before it.
/// @param newBefore The modified text's lines before it.
/// @return Its ranges, a zero-length range starting at the line before it, as unified diffs write it.
[[nodiscard]] HunkRanges rangesOf(std::span<const DiffLine> lines, std::size_t oldBefore, std::size_t newBefore) {
	HunkRanges ranges;
	for(const DiffLine& line : lines) {
		ranges.oldLines += line.old != 0 ? 1 : 0;
		ranges.newLines += line.now != 0 ? 1 : 0;
	}
	ranges.oldStart = oldBefore + (ranges.oldLines > 0 ? 1 : 0);
	ranges.newStart = newBefore + (ranges.newLines > 0 ? 1 : 0);
	return ranges;
}

/// @brief A hunk as diffLines() gives it.
/// @param ranges Its ranges.
/// @param hidden The unchanged lines left out before it.
/// @param lines The lines given (all of the hunk's, or the first ones when the limit is reached).
/// @param split Whether to add its side-by-side rows.
/// @return { header, oldStart, oldLines, newStart, newLines, hiddenBefore, lines, rows? }.
[[nodiscard]] Value hunkValue(const HunkRanges& ranges, std::size_t hidden, std::span<const DiffLine> lines,
                              bool split) {
	const auto integer = [](std::size_t n) { return Value::fromInteger(static_cast<std::int64_t>(n)); };
	std::vector<Value> unified;
	unified.reserve(lines.size());
	for(const DiffLine& line : lines) unified.push_back(unifiedLine(line));
	std::vector<Value::Field> fields;
	fields.push_back({"header", Value::fromText("@@ -" + headerRange(ranges.oldStart, ranges.oldLines) + " +" +
	                                            headerRange(ranges.newStart, ranges.newLines) + " @@")});
	fields.push_back({"oldStart", integer(ranges.oldStart)});
	fields.push_back({"oldLines", integer(ranges.oldLines)});
	fields.push_back({"newStart", integer(ranges.newStart)});
	fields.push_back({"newLines", integer(ranges.newLines)});
	fields.push_back({"hiddenBefore", integer(hidden)});
	fields.push_back({"lines", Value::fromList(std::move(unified))});
	if(split) fields.push_back({"rows", splitRows(lines)});
	return Value::fromObject(std::move(fields));
}

/// @brief Counts a diff's lines of each kind.
/// @param lines Its lines.
/// @return { added, removed, same }, as fields.
[[nodiscard]] std::vector<Value::Field> countsOf(std::span<const DiffLine> lines) {
	std::int64_t added = 0, removed = 0, same = 0;
	for(const DiffLine& line : lines) {
		if(line.kind == DiffEdit::Kind::Insert) ++added;
		else if(line.kind == DiffEdit::Kind::Delete) ++removed;
		else ++same;
	}
	std::vector<Value::Field> fields;
	fields.push_back({"added", Value::fromInteger(added)});
	fields.push_back({"removed", Value::fromInteger(removed)});
	fields.push_back({"same", Value::fromInteger(same)});
	return fields;
}

/// @brief diffLines(original, modified, options?): the hunks of a line diff, with numbered lines.
/// @param call original and modified (texts; anything else as its text) and the options.
/// @return { hunks, added, removed, same, hiddenAfter, truncated }.
Value diffLinesOf(const Call& call) {
	const DiffOptions options = diffOptions(call);
	const std::string originalText = call[0].toText(), modifiedText = call[1].toText();
	bool truncated = false;
	const std::vector<std::string_view> original = linesOf(originalText, truncated);
	const std::vector<std::string_view> modified = linesOf(modifiedText, truncated);
	const std::vector<DiffLine> lines =
	    linesOfEdits(editsOf(original, modified, options.ignoreWhitespace), original, modified);
	std::vector<Value> hunks;
	std::size_t cursor = 0, oldBefore = 0, newBefore = 0, given = 0, lastEnd = 0;
	for(const Hunk& hunk : hunksOf(lines, options.context)) {
		if(given >= options.limit) {
			truncated = true;
			break;
		}
		for(; cursor < hunk.begin; ++cursor) {
			oldBefore += lines[cursor].old != 0 ? 1U : 0U;
			newBefore += lines[cursor].now != 0 ? 1U : 0U;
		}
		const std::span<const DiffLine> all(lines.data() + hunk.begin, hunk.end - hunk.begin);
		const std::span<const DiffLine> shown = all.first(std::min(all.size(), options.limit - given));
		truncated = truncated || shown.size() < all.size();
		given += shown.size();
		hunks.push_back(hunkValue(rangesOf(all, oldBefore, newBefore), hunk.hidden, shown, options.split));
		lastEnd = hunk.end;
	}
	std::vector<Value::Field> fields;
	fields.push_back({"hunks", Value::fromList(std::move(hunks))});
	for(Value::Field& count : countsOf(lines)) fields.push_back(std::move(count));
	fields.push_back({"hiddenAfter", Value::fromInteger(static_cast<std::int64_t>(lines.size() - lastEnd))});
	fields.push_back({"truncated", Value::fromBoolean(truncated)});
	return Value::fromObject(std::move(fields));
}

/// @brief A JSON Pointer's token, with ~1 read as "/" and ~0 as "~".
/// @param token The token as written.
/// @return It unescaped.
[[nodiscard]] std::string unescapedToken(std::string_view token) {
	std::string out;
	out.reserve(token.size());
	for(std::size_t i = 0; i < token.size(); ++i) {
		if(token[i] == '~' && i + 1 < token.size() && (token[i + 1] == '0' || token[i + 1] == '1'))
			out.push_back(token[++i] == '0' ? '~' : '/');
		else out.push_back(token[i]);
	}
	return out;
}

/// @brief Steps from a value to one of its parts by a pointer's token.
/// @param value An object or a list.
/// @param token A key, or an index written without leading zeros.
/// @return The part, or std::nullopt if there is none.
[[nodiscard]] std::optional<Value> partOf(const Value& value, const std::string& token) {
	if(value.isObject()) return value.member(token);
	if(!value.isList() || token.empty() || (token.size() > 1 && token[0] == '0')) return std::nullopt;
	std::size_t index = 0;
	const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), index);
	if(error != std::errc() || end != token.data() + token.size() || index >= value.size()) return std::nullopt;
	return value.item(index);
}

/// @brief atPointer(value, pointer): the part of a value a JSON Pointer names ("/nodes/0/id"), "" for all of it.
/// @param call The value and the pointer.
/// @return The part, or null if there is none.
Value atPointer(const Call& call) {
	const std::string pointer = call[1].toText();
	if(pointer.size() > longestPointer || (!pointer.empty() && pointer[0] != '/')) return {};
	Value at = call[0];
	std::string_view rest = pointer;
	while(!rest.empty()) {
		rest.remove_prefix(1); // the "/"
		const std::size_t slash = rest.find('/');
		std::optional<Value> part = partOf(at, unescapedToken(rest.substr(0, slash)));
		if(!part) return {};
		at = std::move(*part);
		rest = slash == std::string_view::npos ? std::string_view() : rest.substr(slash);
	}
	return at;
}

/// @brief Counts the values in a JSON value (itself and everything in it), stopping past a number.
/// @param value The value.
/// @param most Where to stop counting.
/// @return The count, or a number past most.
[[nodiscard]] std::size_t valuesIn(const json& value, std::size_t most) {
	std::size_t count = 0;
	std::vector<const json*> left{&value};
	while(!left.empty() && count <= most) {
		const json* at = left.back();
		left.pop_back();
		++count;
		if(at->is_structured())
			for(const json& part : *at) left.push_back(&part);
	}
	return count;
}

/// @brief Whether a value can be added at a pointer: its parent, if there, holds values (or is null, which becomes an
/// object). nlohmann::json asserts, rather than throwing, when an "add" reaches inside a number or a text.
/// @param document The document.
/// @param path Where the value goes.
/// @return false if the parent is a value that can't hold anything.
[[nodiscard]] bool takesValueAt(const json& document, const json::json_pointer& path) {
	if(path.empty()) return true;
	const json::json_pointer parent = path.parent_pointer();
	if(!document.contains(parent)) return true; // the patch then fails as the RFC says
	const json& holder = document.at(parent);
	return holder.is_structured() || holder.is_null();
}

/// @brief A text member of a patch operation.
/// @param operation The operation.
/// @param key The member.
/// @return Its text; nothing if the operation isn't an object or the member isn't a text.
[[nodiscard]] std::optional<std::string> textOf(const json& operation, const char* key) {
	if(!operation.is_object()) return std::nullopt;
	const auto found = operation.find(key);
	if(found == operation.end() || !found->is_string()) return std::nullopt;
	return found->get<std::string>();
}

/// @brief Applies one operation of a patch: a move as JSON Patch defines it (a remove, then an add), so that where
/// the value goes is checked after the remove.
/// @param document The document, changed.
/// @param operation The operation.
/// @param copied The values the patch's copies have copied so far, counted on.
/// @return false if it is refused: a copy past the budget, or an add inside a value that holds nothing.
/// @throws json::exception If the operation doesn't apply.
[[nodiscard]] bool applyOperation(json& document, const json& operation, std::size_t& copied) {
	const std::optional<std::string> op = textOf(operation, "op");
	const std::optional<std::string> from = textOf(operation, "from");
	const std::optional<std::string> path = textOf(operation, "path");
	json step = operation;
	if(op == "copy" && from) {
		copied += valuesIn(document.at(json::json_pointer(*from)), mostCopied);
		if(copied > mostCopied) return false;
	}
	if(op == "move" && from && path) {
		json value = document.at(json::json_pointer(*from));
		document.patch_inplace(json::array({{{"op", "remove"}, {"path", *from}}}));
		step = {{"op", "add"}, {"path", *path}, {"value", std::move(value)}};
	}
	if((op == "add" || op == "copy" || op == "move") && path && !takesValueAt(document, json::json_pointer(*path)))
		return false;
	document.patch_inplace(json::array({std::move(step)}));
	return true;
}

/// @brief applyPatch(value, patch): a value with a JSON Patch applied, operation after operation.
/// @param call The value and the patch (a list of { op, path, from?, value? }).
/// @return The patched value, or null if the patch doesn't apply (or is bigger than its limits).
Value applyPatch(const Call& call) {
	const json patch = call[1].toJson();
	if(!patch.is_array() || patch.size() > mostOperations) return {};
	json document = call[0].toJson();
	std::size_t copied = 0;
	try {
		for(const json& operation : patch)
			if(!applyOperation(document, operation, copied)) return {};
	} catch(const std::exception&) { // json::exception, or memory running out
		return {};
	}
	return Value::ownJson(std::move(document));
}

/// @brief formatJson(value, indent?): a value as JSON text, one member or item per line.
/// @param call The value and the indent (spaces per level, 2 unless given, 0 for one line).
/// @return The text.
/// @throws DocumentError If it would be longer than textLimit.
Value formatJson(const Call& call) {
	const std::int64_t indent = call.size() > 1 ? std::clamp<std::int64_t>(saturated(call.number(1)), 0, 8) : 2;
	std::string text =
	    call[0].toJson().dump(indent == 0 ? -1 : static_cast<int>(indent), ' ', false, json::error_handler_t::replace);
	if(text.size() > textLimit) call.fail("the JSON text would be longer than 64 MiB");
	return Value::fromText(std::move(text));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "diffLines",
	 .arguments = "original, modified, options?",
	 .about = "The hunks of a line diff: { hunks: [{ header, lines: [{ kind, old, new, text }], rows? }], added, "
	          "removed, same }; options { context, ignoreWhitespace, split, limit }",
	 .fewest = 2,
	 .most = 3,
	 .apply = diffLinesOf},
    {.name = "atPointer",
	 .arguments = "value, pointer",
	 .about = "The part of a value a JSON Pointer names ('/nodes/0/id'), or null",
	 .fewest = 2,
	 .most = 2,
	 .apply = atPointer},
    {.name = "applyPatch",
	 .arguments = "value, patch",
	 .about = "The value with a JSON Patch applied, or null if it doesn't apply",
	 .fewest = 2,
	 .most = 2,
	 .apply = applyPatch},
    {.name = "formatJson",
	 .arguments = "value, indent?",
	 .about = "The value as JSON text, one member per line (indent 2, 0 for one line)",
	 .fewest = 1,
	 .most = 2,
	 .apply = formatJson},
});

} // namespace

std::span<const Function> diffFunctions() { return table; }

} // namespace affresco::format::detail
