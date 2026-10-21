/// @file
/// @brief Expression functions of fuzzy matching, as a quick pick or a command palette filters: fuzzy, fuzzySort.
///
/// A query matches a text when its characters appear in the text in order, in any case ("fb" matches "FooBar"). The
/// score rewards matches at the start of a word or a camelCase hump, matches next to each other and a match at the
/// start, and takes a little for each gap. The match is the leftmost one, tightened from its end backwards (as fzf's
/// first algorithm does), so finding it is linear in the text.

#include "ExpressionFunctions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format::detail {
namespace {

/// The score of one matched character.
constexpr int matchScore = 16;
/// Added for a match at the start of a word: after a space, a separator or a lowercase-to-uppercase change.
constexpr int boundaryBonus = 8;
/// Added for a match right after the previous one.
constexpr int consecutiveBonus = 8;
/// Added for a match on the text's first character.
constexpr int prefixBonus = 8;
/// Taken for the first character of a gap between matches.
constexpr int gapStart = 3;
/// Taken for each further character of a gap.
constexpr int gapExtension = 1;

/// @brief A character in lower case, for ASCII letters (other characters are compared as they are).
/// @param c A character or a codepoint.
/// @return It in lower case.
template <typename Char> constexpr Char lowered(Char c) noexcept {
	return c >= Char('A') && c <= Char('Z') ? static_cast<Char>(c - Char('A') + Char('a')) : c;
}

/// @brief Whether a character separates words.
/// @param c A character or a codepoint.
/// @return true for a space and the usual separators of names and paths.
template <typename Char> constexpr bool separates(Char c) noexcept {
	constexpr std::string_view separators = " \t/\\_-.:,;()[]{}'\"";
	return c < Char(0x80) && separators.find(static_cast<char>(c)) != std::string_view::npos;
}

/// @brief The bonus for a match at a position: the start of a word or a camelCase hump.
/// @param text The text.
/// @param index The position.
/// @return The bonus.
template <typename Char> int boundary(std::basic_string_view<Char> text, std::size_t index) noexcept {
	if(index == 0) return boundaryBonus;
	const Char before = text[index - 1], at = text[index];
	if(separates(before)) return boundaryBonus;
	if(before >= Char('a') && before <= Char('z') && at >= Char('A') && at <= Char('Z')) return boundaryBonus - 1;
	return 0;
}

/// @brief Matches a query in a text.
/// @param query The query, in lower case.
/// @param text The text.
/// @param positions Set to the matched positions (in elements of the text), one per character of the query.
/// @return The score, or std::nullopt if the query doesn't match.
template <typename Char>
std::optional<int> match(std::basic_string_view<Char> query, std::basic_string_view<Char> text,
                         std::vector<std::size_t>& positions) {
	positions.clear();
	if(query.empty()) return 0;
	// The leftmost match: where its last character ends
	std::size_t found = 0, end = 0;
	for(std::size_t i = 0; i < text.size(); ++i)
		if(lowered(text[i]) == query[found] && ++found == query.size()) {
			end = i + 1;
			break;
		}
	if(found < query.size()) return std::nullopt;
	// Tightened backwards: the latest start that still matches up to that end
	std::size_t start = end;
	for(std::size_t left = query.size(); left > 0;)
		if(lowered(text[--start]) == query[left - 1]) --left;
	// Scored forwards from there
	int score = 0;
	bool after = false; // the previous character matched
	for(std::size_t i = start, next = 0; i < end && next < query.size(); ++i) {
		if(lowered(text[i]) != query[next]) {
			score -= after ? gapStart : gapExtension;
			after = false;
			continue;
		}
		score += matchScore + boundary(text, i) + (after ? consecutiveBonus : 0) + (i == 0 ? prefixBonus : 0);
		positions.push_back(i);
		++next;
		after = true;
	}
	return score;
}

/// @brief A text decoded to codepoints, and where each one starts in the UTF-8.
struct Decoded {
	std::u32string characters;      ///< The codepoints, ASCII letters in lower case if it is a query.
	std::vector<std::size_t> bytes; ///< Where each starts, and the text's size at the end.
};

/// @brief Decodes UTF-8 (an invalid byte stands for itself).
/// @param text The text.
/// @param lower Whether to put ASCII letters in lower case.
/// @return The codepoints.
Decoded decode(std::string_view text, bool lower) {
	Decoded decoded;
	for(std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t size = std::min(utf8CharacterSize(lead), text.size() - i);
		char32_t c = size == 1 ? lead : lead & (0xFFu >> (size + 1));
		for(std::size_t k = 1; k < size; ++k) c = (c << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
		decoded.characters += lower ? lowered(c) : c;
		decoded.bytes.push_back(i);
		i += size;
	}
	decoded.bytes.push_back(text.size());
	return decoded;
}

/// @brief A query, ready to match many texts.
class Query {
public:
	/// @brief Prepares a query.
	/// @param text The query as written.
	explicit Query(std::string_view text) {
		ascii_ = std::ranges::all_of(text, [](char c) { return static_cast<unsigned char>(c) < 0x80; });
		if(ascii_) std::ranges::transform(text, std::back_inserter(bytes_), [](char c) { return lowered(c); });
		else characters_ = decode(text, true).characters;
	}

	/// @brief Whether the query is empty.
	/// @return true if it matches every text with no positions.
	[[nodiscard]] bool empty() const noexcept { return ascii_ ? bytes_.empty() : characters_.empty(); }

	/// @brief Matches a text.
	/// @param text The text.
	/// @param positions Set to the matched positions, as byte offsets in the text.
	/// @return The score, or std::nullopt.
	std::optional<int> operator()(std::string_view text, std::vector<std::size_t>& positions) const {
		// An ASCII query only matches ASCII bytes, which always start a character: the bytes can be matched directly
		if(ascii_) return match<char>(bytes_, text, positions);
		const Decoded decoded = decode(text, false);
		const std::optional<int> score = match<char32_t>(characters_, decoded.characters, positions);
		for(std::size_t& position : positions) position = decoded.bytes[position];
		return score;
	}

private:
	bool ascii_ = true;         ///< Whether the query is ASCII.
	std::string bytes_;         ///< An ASCII query, in lower case.
	std::u32string characters_; ///< Another query's codepoints, ASCII letters in lower case.
};

/// @brief The matched ranges in characters, and the text cut into matched and unmatched parts.
/// @param text The text.
/// @param positions The matched byte offsets, in order.
/// @return [ranges, parts].
std::pair<Value, Value> describe(std::string_view text, const std::vector<std::size_t>& positions) {
	std::vector<Value> ranges, parts;
	const auto part = [&](std::size_t from, std::size_t to, bool matched) {
		if(from < to)
			parts.push_back(Value::fromObject(
			    {{"text", Value::fromText(text.substr(from, to - from))}, {"match", Value::fromBoolean(matched)}}));
	};
	std::size_t done = 0, character = 0; // bytes described, and the characters before them
	const auto charactersTo = [&](std::size_t byte) {
		for(; done < byte; ++done)
			if((static_cast<unsigned char>(text[done]) & 0xC0u) != 0x80u) ++character;
		return character;
	};
	std::size_t cut = 0; // the end of the last part
	for(std::size_t i = 0; i < positions.size();) {
		const std::size_t first = positions[i];
		std::size_t last = first + utf8CharacterSize(static_cast<unsigned char>(text[first]));
		for(++i; i < positions.size() && positions[i] == last; ++i)
			last += utf8CharacterSize(static_cast<unsigned char>(text[last]));
		last = std::min(last, text.size());
		part(cut, first, false);
		part(first, last, true);
		cut = last;
		const auto from = static_cast<std::int64_t>(charactersTo(first));
		ranges.push_back(Value::fromList(
		    {Value::fromInteger(from), Value::fromInteger(static_cast<std::int64_t>(charactersTo(last)))}));
	}
	part(cut, text.size(), false);
	return {Value::fromList(std::move(ranges)), Value::fromList(std::move(parts))};
}

/// @brief `fuzzy(query, text)`.
/// @param call [query, text].
/// @return { score, ranges, parts }, or null if the query doesn't match.
Value fuzzy(const Call& call) {
	const std::string text = call[1].toText();
	std::vector<std::size_t> positions;
	const std::optional<int> score = Query(call[0].toText())(text, positions);
	if(!score) return {};
	auto [ranges, parts] = describe(text, positions);
	return Value::fromObject(
	    {{"score", Value::fromInteger(*score)}, {"ranges", std::move(ranges)}, {"parts", std::move(parts)}});
}

/// @brief The text an item is matched by.
/// @param item The item.
/// @param key The field to match, or "" for the item itself.
/// @return The text (a Value, since a short text lives inside its Value), or std::nullopt if it has none: the item
/// is then left out.
std::optional<Value> textOf(const Value& item, std::string_view key) {
	if(key.empty()) return item.isText() ? std::optional(item) : std::nullopt;
	if(!item.isObject()) return std::nullopt;
	std::optional<Value> field = item.member(key);
	return field && field->isText() ? std::move(field) : std::nullopt;
}

/// @brief One matching item, before its result is made.
struct Candidate {
	int score = 0;          ///< Its score.
	std::size_t length = 0; ///< Its text's length: the shorter one wins a tie.
	std::size_t index = 0;  ///< Its position: the earlier one wins a tie.
};

/// @brief `fuzzySort(list, query, key?, limit?)`.
/// @param call [list, query, key?, limit?].
/// @return The matching items, best first, each as { item, score, ranges, parts, index }.
Value fuzzySort(const Call& call) {
	const Value& list = call[0];
	if(list.isNull()) return Value::fromList({});
	if(!list.isList()) call.fail("expected a list, got " + list.dump());
	const Query query(call[1].toText());
	const std::string key = call.size() > 2 && !call[2].isNull() ? call[2].toText() : std::string();
	std::size_t limit = list.size();
	if(call.size() > 3 && !call[3].isNull())
		limit = static_cast<std::size_t>(std::clamp<double>(call.number(3), 0.0, static_cast<double>(limit)));

	std::vector<Candidate> candidates;
	std::vector<std::size_t> positions;
	for(std::size_t i = 0; i < list.size(); ++i) {
		const Value item = list.item(i);
		const std::optional<Value> text = textOf(item, key);
		if(!text) continue;
		if(const std::optional<int> score = query(text->text(), positions))
			candidates.push_back({*score, text->text().size(), i});
	}
	const auto better = [](const Candidate& a, const Candidate& b) {
		return a.score != b.score ? a.score > b.score : a.length != b.length ? a.length < b.length : a.index < b.index;
	};
	limit = std::min(limit, candidates.size());
	// An empty query keeps the list's order
	if(!query.empty())
		std::ranges::partial_sort(candidates, candidates.begin() + static_cast<std::ptrdiff_t>(limit), better);

	std::vector<Value> results;
	results.reserve(limit);
	for(std::size_t i = 0; i < limit; ++i) {
		const Candidate& candidate = candidates[i];
		Value item = list.item(candidate.index);
		const Value text = *textOf(item, key);
		(void)query(text.text(), positions); // matched again, for the kept ones only
		auto [ranges, parts] = describe(text.text(), positions);
		results.push_back(
		    Value::fromObject({{"item", std::move(item)},
			                   {"score", Value::fromInteger(candidate.score)},
			                   {"ranges", std::move(ranges)},
			                   {"parts", std::move(parts)},
			                   {"index", Value::fromInteger(static_cast<std::int64_t>(candidate.index))}}));
	}
	return Value::fromList(std::move(results));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "fuzzy",
	 .arguments = "query, text",
	 .about = "How the query's characters match the text, in order and in any case: { score, ranges, parts }, or null",
	 .fewest = 2,
	 .most = 2,
	 .apply = fuzzy},
    {.name = "fuzzySort",
	 .arguments = "list, query, key?, limit?",
	 .about = "The items the query matches (by their field key, if given), best first, as { item, score, ranges, "
	          "parts, index }",
	 .fewest = 2,
	 .most = 4,
	 .apply = fuzzySort},
});

} // namespace

std::span<const Function> fuzzyFunctions() { return table; }

} // namespace affresco::format::detail
