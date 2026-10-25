/// @file
/// @brief Locales: numbers, percents and currencies by CLDR's patterns ("#,##0.00 ¤"), lists, and text compared for
/// sorting.

#include "Locale.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief A CLDR number pattern, read: "¤#,##0.00" is the prefix "¤", groups of 3 and 2 decimals.
struct NumberPattern {
	std::string_view prefix;   ///< Before the number.
	std::string_view suffix;   ///< After it.
	bool grouped = false;      ///< Whether the integer digits are grouped.
	std::size_t primary = 3;   ///< The last group's size.
	std::size_t secondary = 3; ///< The others' (2 in "#,##,##0").
	int fewestDecimals = 0;    ///< "0" after the point.
	int mostDecimals = 0;      ///< "0" and "#" after it.
};

/// @brief Reads one sub-pattern (the positive or the negative one).
/// @param pattern "#,##0.###".
/// @return Its parts.
NumberPattern readPattern(std::string_view pattern) {
	NumberPattern result;
	const std::size_t first = pattern.find_first_of("#0,.");
	const std::size_t last = pattern.find_last_of("#0,.");
	if(first == std::string_view::npos) {
		result.prefix = pattern;
		return result;
	}
	result.prefix = pattern.substr(0, first);
	result.suffix = pattern.substr(last + 1);
	const std::string_view body = pattern.substr(first, last - first + 1);
	const std::size_t point = body.find('.');
	const std::string_view integer = body.substr(0, point);
	const std::size_t comma = integer.rfind(',');
	if(comma != std::string_view::npos) {
		result.grouped = true;
		result.primary = integer.size() - comma - 1;
		const std::size_t before = comma > 0 ? integer.rfind(',', comma - 1) : std::string_view::npos;
		result.secondary = before != std::string_view::npos ? comma - before - 1 : result.primary;
	}
	if(point != std::string_view::npos)
		for(const char digit : body.substr(point + 1)) {
			if(digit == '0') ++result.fewestDecimals;
			if(digit == '0' || digit == '#') ++result.mostDecimals;
		}
	return result;
}

/// @brief Appends a pattern's prefix or suffix with the locale's symbols: "¤", "%", "-", text between quotes.
/// @param out Where.
/// @param affix The prefix or suffix.
/// @param locale The locale.
/// @param currency The currency's symbol.
void appendAffix(std::string& out, std::string_view affix, const LocaleData& locale, std::string_view currency) {
	constexpr std::string_view sign = "¤";
	bool quoted = false;
	for(std::size_t i = 0; i < affix.size(); ++i) {
		const char character = affix[i];
		if(character == '\'') quoted = !quoted;
		else if(quoted) out += character; // NOLINT(bugprone-branch-clone): kept apart, so a quoted '%' stays literal
		else if(character == '%') out += locale.percentSign;
		else if(character == '-') out += locale.minusSign;
		else if(affix.substr(i).starts_with(sign)) {
			out += currency;
			i += sign.size() - 1;
		} else out += character;
	}
}

/// @brief Writes a non-negative number with a fixed count of decimals, rounded half away from zero as JavaScript's
/// Intl does: the shortest decimal that reads back as the number is rounded, so 0.125 and 1.005 round up as written.
/// @param value The number (finite, not negative).
/// @param decimals The decimals (0 to 20).
/// @return "1234.50".
std::string fixedDecimals(double value, int decimals) {
	std::array<char, 400> buffer{}; // the largest double, written out in full, has 309 digits
	const auto written = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed);
	std::string plain(buffer.data(), written.ptr);
	if(!plain.contains('.')) plain += '.';
	const std::size_t point = plain.find('.');
	const std::size_t length = point + 1 + static_cast<std::size_t>(decimals);
	const bool roundUp = plain.size() > length && plain[length] >= '5';
	plain.resize(length, '0');
	for(std::size_t i = length; roundUp && i-- > 0;) {
		if(plain[i] == '.') continue;
		if(plain[i] != '9') {
			++plain[i];
			break;
		}
		plain[i] = '0';
		if(i == 0) plain.insert(plain.begin(), '1'); // 9.99 to 10.0
	}
	if(decimals == 0) plain.pop_back();
	return plain;
}

/// @brief Writes the digits of a non-negative number, grouped and with the locale's separators.
/// @param value The number.
/// @param pattern Its grouping.
/// @param locale The locale.
/// @param fewest Fewest decimals.
/// @param most Most decimals.
/// @return The digits.
std::string digits(double value, const NumberPattern& pattern, const LocaleData& locale, int fewest, int most) {
	std::string plain = fixedDecimals(value, most);
	std::size_t point = plain.find('.');
	if(point != std::string::npos) {
		std::size_t keep = plain.size();
		while(keep > point + 1 + static_cast<std::size_t>(fewest) && plain[keep - 1] == '0') --keep;
		if(keep == point + 1) keep = point;
		plain.resize(keep);
	}
	point = plain.find('.');
	const std::string_view integer = std::string_view(plain).substr(0, point);
	const auto minimum = static_cast<std::size_t>(locale.minimumGroupingDigits);
	const bool group = pattern.grouped && integer.size() >= pattern.primary + minimum;
	std::string out;
	for(std::size_t i = 0; i < integer.size(); ++i) {
		const std::size_t left = integer.size() - i;
		const bool boundary = left > pattern.primary && (left - pattern.primary) % pattern.secondary == 0;
		if(group && i > 0 && (left == pattern.primary || boundary)) out += locale.group;
		out += integer[i];
	}
	if(point != std::string::npos) {
		out += locale.decimal;
		out += std::string_view(plain).substr(point + 1);
	}
	return out;
}

/// @brief Formats a number by a full pattern ("positive;negative").
/// @param locale The locale.
/// @param value The number.
/// @param pattern The pattern.
/// @param currency The symbol for "¤".
/// @param fewest Fewest decimals, or -1 for the pattern's.
/// @param most Most decimals, or -1 for the pattern's.
/// @return The text.
std::string byPattern(const LocaleData& locale, double value, std::string_view pattern, std::string_view currency,
                      int fewest, int most) {
	if(std::isnan(value)) return "NaN";
	const std::size_t semicolon = pattern.find(';');
	const NumberPattern positive = readPattern(pattern.substr(0, semicolon));
	const bool negative = std::signbit(value) && value != 0;
	const bool ownNegative = negative && semicolon != std::string_view::npos;
	const NumberPattern shape = ownNegative ? readPattern(pattern.substr(semicolon + 1)) : positive;
	fewest = fewest < 0 ? positive.fewestDecimals : fewest;
	most = std::max(fewest, most < 0 ? positive.mostDecimals : std::min(most, 20));
	std::string out;
	if(negative && !ownNegative) out += locale.minusSign;
	appendAffix(out, shape.prefix, locale, currency);
	out += std::isinf(value) ? std::string("∞") : digits(std::fabs(value), positive, locale, fewest, most);
	appendAffix(out, shape.suffix, locale, currency);
	return out;
}

/// @brief Decodes the next UTF-8 character.
/// @param text The text.
/// @param at Where; moved past it.
/// @return Its code point (the byte itself for a malformed one).
char32_t nextCharacter(std::string_view text, std::size_t& at) noexcept {
	const auto lead = static_cast<unsigned char>(text[at++]);
	const std::size_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
	if(extra == 0 || at + extra > text.size()) return lead;
	char32_t value = lead & (0x3F >> extra);
	for(std::size_t i = 0; i < extra; ++i) value = (value << 6) | (static_cast<unsigned char>(text[at++]) & 0x3F);
	return value;
}

/// @brief A character's base letter, lowercase and without accents (Latin letters, U+00C0 to U+017F).
/// @param character The character.
/// @return Its base; itself, lowercased if ASCII, for the others.
char32_t baseLetter(char32_t character) noexcept {
	// U+00C0 onwards: each one's base letter ('.' for × and ÷, which are no letters)
	static constexpr std::string_view bases =
	    "aaaaaaaceeeeiiiidnooooo.ouuuuytsaaaaaaaceeeeiiiidnooooo.ouuuuytyaaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiii"
	    "iiiiiijjkkkllllllllllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
	if(character >= 'A' && character <= 'Z') return character + ('a' - 'A');
	if(character >= 0xC0 && character < 0xC0 + bases.size() && bases[character - 0xC0] != '.')
		return static_cast<char32_t>(bases[character - 0xC0]);
	return character;
}

/// @brief Tells whether a character is an uppercase Latin letter (U+0000 to U+017F).
/// @param character The character.
/// @return Whether.
bool isUpper(char32_t character) noexcept {
	if(character >= 'A' && character <= 'Z') return true;
	if(character >= 0xC0 && character <= 0xDE) return character != 0xD7;
	// Latin Extended-A pairs each uppercase letter with the next code point, from an even one or an odd one
	const bool evenPairs = (character >= 0x100 && character <= 0x137) || (character >= 0x14A && character <= 0x177);
	const bool oddPairs = (character >= 0x139 && character <= 0x148) || (character >= 0x179 && character <= 0x17E);
	return (evenPairs && character % 2 == 0) || (oddPairs && character % 2 == 1);
}

/// @brief Lowercases a Latin letter (U+0000 to U+017F).
/// @param character The character.
/// @return Its lowercase; itself for the others.
char32_t lowerCase(char32_t character) noexcept {
	if(!isUpper(character)) return character;
	return character < 0x100 ? character + 0x20 : character + 1;
}

/// @brief Uppercases a Latin letter (U+0000 to U+017F).
/// @param character The character.
/// @return Its capital; itself for the others.
char32_t upperCase(char32_t character) noexcept {
	if(character >= 'a' && character <= 'z') return character - 0x20;
	if(character >= 0xE0 && character <= 0xFE) return character == 0xF7 ? character : character - 0x20;
	if(character == 0xFF) return 0x178; // ÿ, whose capital is in Latin Extended-A
	if(character > 0x100 && character < 0x180 && isUpper(character - 1)) return character - 1;
	return character;
}

/// @brief Compares two texts at one strength.
/// @param a A text.
/// @param b Another.
/// @param key What is compared of each character.
/// @return Negative, zero or positive.
template <typename Key> int compareBy(std::string_view a, std::string_view b, Key key) noexcept {
	std::size_t i = 0, j = 0;
	while(i < a.size() && j < b.size()) {
		const auto x = key(nextCharacter(a, i)), y = key(nextCharacter(b, j));
		if(x != y) return x < y ? -1 : 1;
	}
	return (i < a.size()) - (j < b.size());
}

} // namespace

std::string formatNumber(const LocaleData& locale, double number, int fewest, int most) {
	return byPattern(locale, number, locale.decimalPattern, {}, std::max(fewest, 0), std::max(most, 0));
}

std::string formatPercent(const LocaleData& locale, double ratio, int decimals) {
	decimals = std::clamp(decimals, 0, 20);
	return byPattern(locale, ratio * 100.0, locale.percentPattern, {}, decimals, decimals);
}

std::string formatCurrency(const LocaleData& locale, double amount, std::string_view code) {
	for(std::size_t i = 0; i < currencyCount; ++i)
		if(currencyCodes[i] == code)
			return byPattern(locale, amount, locale.currencyPattern, locale.currencySymbols[i], currencyDigits[i],
			                 currencyDigits[i]);
	return byPattern(locale, amount, locale.currencyPattern, code, 2, 2);
}

std::string formatList(const LocaleData& locale, std::span<const std::string> items, bool disjunction) {
	const std::array<std::string_view, 4>& patterns = disjunction ? locale.orList : locale.andList;
	// Writes a pattern up to its {1}, with {0} as the item: where the rest starts, or npos if it has no {1}
	const auto write = [](std::string& out, std::string_view pattern, std::string_view item) {
		for(std::size_t i = 0; i < pattern.size(); ++i) {
			if(pattern.substr(i).starts_with("{0}")) out += item, i += 2;
			else if(pattern.substr(i).starts_with("{1}")) return i + 3;
			else out += pattern[i];
		}
		return std::string_view::npos;
	};
	if(items.empty()) return {};
	if(items.size() == 1) return items[0];
	// The patterns nest: the first item, then the rest of the list (the start's, the middle's, the end's, or the
	// pair's), as {0} and {1}. Each has one {1}, so the parts before it are written in order, the parts after it in
	// reverse: linear in the list, where joining each level's text into the next was quadratic.
	const std::size_t last = items.size() - 1;
	std::string out;
	std::vector<std::pair<std::string_view, std::size_t>> rests; // a pattern after its {1}, and its item
	for(std::size_t i = 0; i < last; ++i) {
		const std::string_view pattern = last == 1       ? patterns[3]
		                                 : i == 0        ? patterns[0]
		                                 : i + 1 == last ? patterns[2]
		                                                 : patterns[1];
		const std::size_t rest = write(out, pattern, items[i]);
		if(rest == std::string_view::npos) break; // the rest of the list isn't written
		rests.emplace_back(pattern.substr(rest), i);
		if(i + 1 == last) out += items[last];
	}
	for(const auto& [rest, i] : std::views::reverse(rests)) write(out, rest, items[i]);
	return out;
}

int compareText(std::string_view a, std::string_view b) noexcept {
	if(const int primary = compareBy(a, b, baseLetter); primary != 0) return primary;
	if(const int secondary = compareBy(a, b, lowerCase); secondary != 0) return secondary;
	return compareBy(a, b, isUpper); // lowercase first (false before true)
}

std::string changeCase(std::string_view text, bool upper) {
	std::string result;
	result.reserve(text.size());
	for(std::size_t at = 0; at < text.size();) {
		const std::size_t start = at;
		const char32_t character = nextCharacter(text, at);
		char32_t changed = character;
		if(character != 0x130 && character != 0x131) // the Turkish i's, which depend on the language
			changed = upper ? upperCase(character) : character == 0x178 ? 0xFF : lowerCase(character);
		if(changed == character) result.append(text.substr(start, at - start));
		else if(changed < 0x80) result += static_cast<char>(changed);
		else { // every Latin letter is two bytes in UTF-8
			result += static_cast<char>(0xC0 | (changed >> 6));
			result += static_cast<char>(0x80 | (changed & 0x3F));
		}
	}
	return result;
}

} // namespace affresco::format::detail
