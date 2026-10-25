/// @file
/// @brief Expression functions: texts and conversions (upper, trim, split, pad, plural, number, json, applyEdits, ...).

#include "ExpressionFunctions.hpp"
#include "Locale.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <exception>
#include <optional>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief `upper(text)`: Latin letters, accented ones too, in capitals (changeCase()).
/// @param call [text].
/// @return The text.
Value upper(const Call& call) { return Value::fromText(changeCase(call[0].toText(), true)); }

/// @brief `lower(text)`: Latin letters, accented ones too, in small letters (changeCase()).
/// @param call [text].
/// @return The text.
Value lower(const Call& call) { return Value::fromText(changeCase(call[0].toText(), false)); }

constexpr std::int64_t longestSlug = 256; ///< slug()'s most, at most.

/// @brief `slug(text, most?)`: an id made of the text, as a file or a theme is named after a title.
/// @param call [text, most?].
/// @return Its ASCII letters and digits in small letters, each run of anything else as one dash, without dashes at
/// either end; at most `most` characters (64 by default, 1 to 256); "" if nothing is left.
Value slug(const Call& call) {
	const std::string text = call[0].toText();
	const auto most = static_cast<std::size_t>(
	    std::clamp<std::int64_t>(call.size() > 1 ? saturated(call.number(1)) : 64, 1, longestSlug));
	std::string id;
	for(const char c : text) {
		if(id.size() == most) break;
		const auto u = static_cast<unsigned char>(c);
		if(u < 128 && std::isalnum(u) != 0) id.push_back(static_cast<char>(std::tolower(u)));
		else if(!id.empty() && id.back() != '-') id.push_back('-');
	}
	while(!id.empty() && id.back() == '-') id.pop_back();
	return Value::fromText(std::move(id));
}

/// @brief `trim(text)`.
/// @param call [text].
/// @return It without surrounding whitespace.
Value trimmed(const Call& call) { return Value::fromText(trim(call[0].toText())); }

/// @brief `text(x)`: as a template writes it.
/// @param call [x].
/// @return The text.
Value text(const Call& call) { return Value::fromText(call[0].toText()); }

/// @brief `number(x)`: a number as it is, a text read as a number.
/// @param call [x].
/// @return The number (an integer if the text has no fraction or exponent), or null if it isn't one.
Value number(const Call& call) {
	if(call[0].isNumber()) return call[0];
	try {
		std::size_t used = 0;
		const std::string text = call[0].toText();
		const double n = std::stod(text, &used);
		return used == text.size() ? numeric(n, text.find_first_of(".eE") == std::string::npos) : Value();
	} catch(const std::exception&) {
		return {}; // not a number, or out of range
	}
}

/// @brief `json(x)`.
/// @param call [x].
/// @return x as JSON text.
Value toJsonText(const Call& call) { return Value::fromText(call[0].dump()); }

/// @brief `parseJson(text)`.
/// @param call [text].
/// @return The value, or null if it isn't JSON (or nests too deeply).
Value parseJson(const Call& call) {
	try {
		return Value::ownJson(parseUntrusted(call[0].toText(), true));
	} catch(const std::exception&) {
		return {};
	}
}

/// @brief `parseJsonc(text)`.
/// @param call [text].
/// @return The value, comments and trailing commas allowed, or null if it isn't JSON (or nests too deeply).
Value parseJsonc(const Call& call) {
	try {
		return Value::ownJson(parseUntrusted(plainJson(call[0].toText())));
	} catch(const std::exception&) {
		return {};
	}
}

constexpr std::size_t longestShellWord = 4096;  ///< A word of shellLine(), at most 4096 bytes.
constexpr std::size_t mostShellWords = 1000;    ///< Words, and variables, of shellLine(), at most 1000 of each.
constexpr std::size_t longestShellLine = 60000; ///< shellLine()'s line: under a terminal's 64 KiB write.

/// @brief A word quoted for a POSIX shell: in single quotes, a quote as `'\''`.
/// @param word The word.
/// @param out The line, to add it to.
/// @return Whether it could be: text of at most longestShellWord bytes, with no control character.
bool appendShellWord(std::string_view word, std::string& out) {
	if(word.size() > longestShellWord ||
	   std::ranges::any_of(word, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; }))
		return false;
	if(!out.empty()) out += ' ';
	out += '\'';
	for(const char c : word) {
		if(c == '\'') out += "'\\''";
		else out += c;
	}
	out += '\'';
	return true;
}

/// @brief Whether a text is a name an environment variable may have: a letter or `_`, then letters, digits or `_`.
/// @param name The text.
/// @return Whether it is (at most 200 bytes).
bool variableName(std::string_view name) {
	const auto word = [](char c) { return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
	return !name.empty() && name.size() <= 200 && word(name.front()) &&
	       std::ranges::all_of(name, [&word](char c) { return word(c) || (c >= '0' && c <= '9'); });
}

/// @brief `shellLine(words, variables?)`.
/// @param call [words, variables?].
/// @return The words as a POSIX shell's line, each in single quotes, after `env 'NAME=value'` for each variable that
/// isn't null; or null if a word or value isn't plain text, a name isn't a variable's, or the line is too long.
Value shellLine(const Call& call) {
	const Value& words = call[0];
	if(!words.isList() || words.size() > mostShellWords) return {};
	std::string line;
	if(call.size() > 1 && !call[1].isNull()) {
		const Value& variables = call[1];
		if(!variables.isObject() || variables.size() > mostShellWords) return {};
		for(const Value::Field& variable : variables.fields()) {
			if(variable.value.isNull()) continue;
			if(!variableName(variable.key) || !variable.value.isText()) return {};
			if(line.empty()) line = "env";
			if(!appendShellWord(variable.key + "=" + std::string(variable.value.text()), line)) return {};
		}
	}
	for(std::size_t i = 0; i < words.size(); ++i) {
		const Value word = words.item(i);
		if(!word.isText() || !appendShellWord(word.text(), line)) return {};
	}
	if(line.size() > longestShellLine) return {};
	return Value::fromText(std::move(line));
}

/// @brief `fixed(number, digits)`.
/// @param call [number, digits].
/// @return The number with that many digits after the point (0 to 12).
Value fixed(const Call& call) {
	const int digits = static_cast<int>(std::clamp<std::int64_t>(saturated(call.number(1)), 0, 12));
	std::array<char, 64> buffer{};
	std::snprintf(buffer.data(), buffer.size(), "%.*f", digits, call.number(0));
	return Value::fromText(std::string_view(buffer.data()));
}

/// @brief `pad(value, width, fill?)`: the fill (default "0") put before the text until it is that wide.
/// @param call [value, width, fill?].
/// @return The text.
Value pad(const Call& call) {
	std::string text = call[0].toText();
	const auto width = static_cast<std::size_t>(std::max<std::int64_t>(saturated(call.number(1)), 0));
	if(width > 10000) call.fail("width too large");
	const std::string fill = call.size() > 2 ? call[2].toText() : "0";
	if(text.size() >= width || fill.empty()) return Value::fromText(std::move(text));
	// As many fills as it takes to reach the width, the last one possibly passing it
	const std::size_t fills = (width - text.size() + fill.size() - 1) / fill.size();
	std::string result;
	result.reserve(fills * fill.size() + text.size());
	for(std::size_t i = 0; i < fills; ++i) result += fill;
	return Value::fromText(result + text);
}

/// @brief `plural(n, word, plural?)`: "1 card", "2 cards".
/// @param call [n, word, plural?].
/// @return The count and the word.
Value plural(const Call& call) {
	const double n = call.number(0);
	const std::string word = n == 1.0 ? call[1].toText() : call.size() > 2 ? call[2].toText() : call[1].toText() + "s";
	return Value::fromText(call[0].toText() + " " + word);
}

/// @brief `split(text, separator)`; with "" as the separator, the characters (not bytes, so "é" stays whole).
/// @param call [text, separator].
/// @return The parts.
Value split(const Call& call) {
	const std::string text = call[0].toText(), separator = call[1].toText();
	std::vector<Value> parts;
	if(separator.empty()) {
		for(std::size_t i = 0; i < text.size();) {
			const std::size_t size = std::min(utf8CharacterSize(static_cast<unsigned char>(text[i])), text.size() - i);
			parts.push_back(Value::fromText(std::string_view(text).substr(i, size)));
			i += size;
		}
		return Value::fromList(std::move(parts));
	}
	std::size_t start = 0;
	for(std::size_t end; (end = text.find(separator, start)) != std::string::npos; start = end + separator.size())
		parts.push_back(Value::fromText(std::string_view(text).substr(start, end - start)));
	parts.push_back(Value::fromText(std::string_view(text).substr(start)));
	return Value::fromList(std::move(parts));
}

/// @brief A place's number: a whole number from 0.
/// @param edit An edit.
/// @param key Its member.
/// @return The number, or std::nullopt if it is missing or isn't one.
std::optional<std::size_t> placeOf(const Value& edit, std::string_view key) {
	const std::optional<Value> value = edit.member(key);
	if(!value || !value->isNumber()) return std::nullopt;
	const double n = value->number();
	if(!(n >= 0.0) || n > 1e15 || n != std::floor(n)) return std::nullopt;
	return static_cast<std::size_t>(n);
}

/// @brief The byte offset of a place (from 0, columns in characters) in a text.
/// @param lineStarts Where each line starts.
/// @param text The text.
/// @param line The line.
/// @param column The column.
/// @return The offset, or std::nullopt past the line or the text.
std::optional<std::size_t> offsetIn(const std::vector<std::size_t>& lineStarts, std::string_view text,
                                    std::optional<std::size_t> line, std::optional<std::size_t> column) {
	if(!line || !column || *line >= lineStarts.size()) return std::nullopt;
	std::size_t at = lineStarts[*line];
	const std::size_t end = *line + 1 < lineStarts.size() ? lineStarts[*line + 1] - 1 : text.size();
	for(std::size_t step = 0; step < *column; ++step) {
		if(at >= end) return std::nullopt;
		at += std::min(utf8CharacterSize(static_cast<unsigned char>(text[at])), end - at);
	}
	return at;
}

/// @brief `applyEdits(text, edits)`: each edit `{ line, column, endLine, endColumn, text }` (from 0, columns in
/// characters) replaces its range with its text; the ranges are those of the text given, so the edits may come in
/// any order.
/// @param call [text, edits].
/// @return The text edited, or null if an edit isn't one, is past the text or overlaps another (none is applied).
Value applyEdits(const Call& call) {
	const std::string text = call[0].toText();
	const Value& edits = call[1];
	if(!edits.isList()) return {};
	std::vector<std::size_t> lineStarts{0};
	for(std::size_t i = 0; i < text.size(); ++i)
		if(text[i] == '\n') lineStarts.push_back(i + 1);
	struct Range {
		std::size_t from, to, index;
	};
	std::vector<Range> ranges;
	for(std::size_t i = 0; i < edits.size(); ++i) {
		const Value edit = edits.item(i);
		if(!edit.isObject()) return {};
		const auto from = offsetIn(lineStarts, text, placeOf(edit, "line"), placeOf(edit, "column"));
		const auto to = offsetIn(lineStarts, text, placeOf(edit, "endLine"), placeOf(edit, "endColumn"));
		if(!from || !to || *to < *from) return {};
		ranges.push_back({*from, *to, i});
	}
	std::ranges::stable_sort(ranges, {}, &Range::from);
	for(std::size_t i = 1; i < ranges.size(); ++i)
		if(ranges[i].from < ranges[i - 1].to) return {};
	std::string out;
	std::size_t at = 0;
	for(const Range& range : ranges) {
		call.appendText(out, std::string_view(text).substr(at, range.from - at));
		const std::optional<Value> inserted = edits.item(range.index).member("text");
		if(inserted && !inserted->isNull()) call.appendText(out, inserted->toText());
		at = range.to;
	}
	call.appendText(out, std::string_view(text).substr(at));
	return Value::fromText(std::move(out));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "upper",
	 .arguments = "text",
	 .about = "The text with its letters in capitals",
	 .takes = Takes::One,
	 .apply = upper},
    {.name = "lower",
	 .arguments = "text",
	 .about = "The text with its letters in small letters",
	 .takes = Takes::One,
	 .apply = lower},
    {.name = "trim",
	 .arguments = "text",
	 .about = "The text without whitespace around it",
	 .takes = Takes::One,
	 .apply = trimmed},
    {.name = "split",
	 .arguments = "text, separator",
	 .about = "The parts between separators; with '', the characters",
	 .fewest = 2,
	 .most = 2,
	 .apply = split},
    {.name = "number",
	 .arguments = "value",
	 .about = "A text read as a number, or null if it isn't one",
	 .takes = Takes::One,
	 .apply = number},
    {.name = "text",
	 .arguments = "value",
	 .about = "The value as a template writes it",
	 .takes = Takes::One,
	 .apply = text},
    {.name = "slug",
	 .arguments = "text, most?",
	 .about = "An id of the text: its letters and digits in small letters, a dash for each run of anything else, "
	          "at most `most` characters (64 by default); '' if nothing is left",
	 .fewest = 1,
	 .most = 2,
	 .apply = slug,
	 .remark = "most from 1 to 256"},
    {.name = "fixed",
	 .arguments = "number, digits",
	 .about = "The number written with that many digits (0 to 12) after the point",
	 .fewest = 2,
	 .most = 2,
	 .apply = fixed},
    {.name = "pad",
	 .arguments = "value, width, fill?",
	 .about = "The text with the fill (default '0') before it, up to the width",
	 .fewest = 2,
	 .most = 3,
	 .apply = pad,
	 .remark = "width up to 10000"},
    {.name = "plural",
	 .arguments = "count, word, plural?",
	 .about = R"("1 card", "2 cards": the count and the word (plural defaults to the word and 's'))",
	 .fewest = 2,
	 .most = 3,
	 .apply = plural},
    {.name = "parseJson",
	 .arguments = "text",
	 .about = "The JSON text as a value, or null if it isn't JSON",
	 .takes = Takes::One,
	 .apply = parseJson},
    {.name = "parseJsonc",
	 .arguments = "text",
	 .about = "The JSON text as a value, as settings files write it (with comments and trailing commas), or null if it "
	          "isn't JSON",
	 .takes = Takes::One,
	 .apply = parseJsonc},
    {.name = "shellLine",
	 .arguments = "words, variables?",
	 .about = "The words as a line for a POSIX shell, each in single quotes, after `env 'NAME=value'` for each "
	          "variable of the object that isn't null; null if a word or value isn't text without control characters, "
	          "or a name isn't a variable's",
	 .fewest = 1,
	 .most = 2,
	 .apply = shellLine,
	 .remark = "up to 1000 words and 1000 variables of 4096 bytes, a line up to 60000"},
    {.name = "json", .arguments = "value", .about = "The value as JSON text", .takes = Takes::One, .apply = toJsonText},
    {.name = "applyEdits",
	 .arguments = "text, edits",
	 .about = "The text with each edit { line, column, endLine, endColumn, text } (from 0, columns in characters) "
	          "made, or null if one is past the text or two overlap",
	 .fewest = 2,
	 .most = 2,
	 .apply = applyEdits},
});

} // namespace

std::span<const Function> textFunctions() { return table; }

} // namespace affresco::format::detail
