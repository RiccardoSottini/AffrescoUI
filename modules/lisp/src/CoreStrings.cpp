/// @file
/// @brief The functions of `clojure.string` (Machine.hpp, defineStrings()), written `str/join` with its alias: native
/// functions registered as `clojure.string/<name>`. Those the language also has bare are the same functions under the
/// namespace's names. Positions and lengths are in characters (Characters.hpp), and no function takes a regular
/// expression: a match is a string.

#include "Characters.hpp"
#include "Machine.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// The namespace's functions the language has bare, by their bare names.
constexpr std::array<std::string_view, 3> sameAsBare = {"join", "lower-case", "upper-case"};

/// What the trimming functions take away: spaces, tabs and line breaks.
constexpr std::string_view spaces = " \t\n\r";

/// What `trim-newline` takes away.
constexpr std::string_view lineBreaks = "\n\r";

/// What `#"..."` reads as, in this language: `:id "..."`, a node's id (Reader.cpp).
constexpr std::string_view idKeyword = "id";

/// @brief Finds a part in a text in time linear in both, by Knuth, Morris and Pratt's method, so that no text and
/// part a program gives make `replace` or `split` slow. The part is never empty.
class Search {
public:
	/// @brief Prepares a search, spending what it costs.
	/// @param machine The machine, whose budget it spends.
	/// @param text The text, which must outlive the search.
	/// @param part The part, not empty, which must outlive the search.
	Search(Machine& machine, std::string_view text, std::string_view part) : text_(text), part_(part) {
		machine.heap.charge(part.size() * sizeof(std::size_t));
		machine.spendText(text.size() + part.size());
		border_.assign(part.size(), 0);
		for(std::size_t at = 1, length = 0; at < part.size(); ++at) {
			while(length > 0 && part[at] != part[length]) length = border_[length - 1];
			if(part[at] == part[length]) ++length;
			border_[at] = length;
		}
	}

	/// @brief The next place the part starts, overlapping the last one found unless restartAt() says otherwise.
	/// @return Its offset in bytes, or npos when there is none.
	[[nodiscard]] std::size_t next() noexcept {
		while(at_ < text_.size()) {
			while(matched_ > 0 && text_[at_] != part_[matched_]) matched_ = border_[matched_ - 1];
			if(text_[at_] == part_[matched_]) ++matched_;
			++at_;
			if(matched_ == part_.size()) {
				matched_ = border_[matched_ - 1];
				return at_ - part_.size();
			}
		}
		return std::string_view::npos;
	}

	/// @brief Continues from an offset with nothing matched: after a match that is replaced, so none overlap.
	/// @param at The offset, at or after the last one next() reached.
	void restartAt(std::size_t at) noexcept {
		at_ = at;
		matched_ = 0;
	}

private:
	std::string_view text_;           ///< See the constructor.
	std::string_view part_;           ///< See the constructor.
	std::vector<std::size_t> border_; ///< For each length of the part matched, the longest end that also starts it.
	std::size_t at_ = 0;              ///< The next byte of the text to look at.
	std::size_t matched_ = 0;         ///< How much of the part the bytes before at_ end with.
};

/// @brief Refuses a regular expression where a match should be: `#"..."` reads as `:id "..."`.
/// @param arguments The arguments.
/// @param at Where the match is.
/// @throws Problem If it is the keyword `:id`.
void refuseRegularExpression(Arguments arguments, std::size_t at) {
	if(at < arguments.size() && arguments[at].kind() == Kind::Keyword && arguments[at].text() == idKeyword)
		throw Problem("a regular expression (#\"...\") isn't in this language: give the text to match, as a string");
}

/// @brief The texts of a function's first arguments, checked.
/// @param arguments The arguments.
/// @param texts How many must be strings, first.
/// @param most How many it may have in all.
/// @param usage What the function takes, its error.
/// @throws Problem If they aren't.
void checkTexts(Arguments arguments, std::size_t texts, std::size_t most, std::string_view usage) {
	if(arguments.size() < texts || arguments.size() > most ||
	   std::ranges::any_of(arguments.first(texts), [](const Value& value) { return value.kind() != Kind::String; }))
		throw Problem(std::string(usage));
}

/// @brief A whole number given as a position or a limit.
/// @param value The value.
/// @param usage What the function takes, its error.
/// @return Its value.
/// @throws Problem If it isn't a whole number.
std::int64_t wholeArgument(const Value& value, std::string_view usage) {
	const std::optional<std::int64_t> whole = wholeNumber(value);
	if(!whole) throw Problem(std::string(usage));
	return *whole;
}

/// @brief Whether a character is a space, a tab or a line break.
/// @param c The character.
/// @return true if it is.
bool isSpace(char c) noexcept { return spaces.contains(c); }

/// @brief `blank?`. @param machine The machine. @param arguments A string or nil. @return Whether it has only spaces.
Value blank(Machine& machine, Arguments arguments) {
	if(arguments.size() != 1 || (!arguments[0].isNil() && arguments[0].kind() != Kind::String))
		throw Problem("\"str/blank?\" takes a string or nil");
	machine.spendText(arguments[0].text().size());
	return Value::boolean(std::ranges::all_of(arguments[0].text(), isSpace));
}

/// @brief `capitalize`: as `upper-case` and `lower-case` change case, ASCII letters only.
/// @param machine The machine.
/// @param arguments A string.
/// @return It with its first character upper case and the rest lower.
Value capitalize(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 1, 1, "\"str/capitalize\" takes a string");
	std::string text(arguments[0].text());
	machine.heap.charge(text.size());
	for(std::size_t at = 0; at < text.size(); ++at) {
		const auto byte = static_cast<unsigned char>(text[at]);
		if(byte < 0x80U) text[at] = static_cast<char>(at == 0 ? std::toupper(byte) : std::tolower(byte));
	}
	return machine.heap.string(text);
}

/// @brief `starts-with?`. @param machine The machine. @param arguments A string and a part. @return Whether it does.
Value startsWith(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 2, 2, "\"str/starts-with?\" takes a string and a part");
	machine.spendText(arguments[1].text().size());
	return Value::boolean(arguments[0].text().starts_with(arguments[1].text()));
}

/// @brief `ends-with?`. @param machine The machine. @param arguments A string and a part. @return Whether it does.
Value endsWith(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 2, 2, "\"str/ends-with?\" takes a string and a part");
	machine.spendText(arguments[1].text().size());
	return Value::boolean(arguments[0].text().ends_with(arguments[1].text()));
}

/// @brief `includes?`. @param machine The machine. @param arguments A string and a part. @return Whether it has it.
Value includes(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 2, 2, "\"str/includes?\" takes a string and a part");
	if(arguments[1].text().empty()) return Value::boolean(true);
	Search search(machine, arguments[0].text(), arguments[1].text());
	return Value::boolean(search.next() != std::string_view::npos);
}

/// @brief `index-of`.
/// @param machine The machine.
/// @param arguments A string, a part and maybe a position to look from, in characters (a negative one is 0).
/// @return Where the part first starts, in characters, or nil.
Value indexOf(Machine& machine, Arguments arguments) {
	constexpr std::string_view usage = "\"str/index-of\" takes a string, a part and maybe a position";
	checkTexts(arguments, 2, 3, usage);
	const std::string_view text = arguments[0].text();
	const std::string_view part = arguments[1].text();
	const auto from = static_cast<std::size_t>(
	    std::max<std::int64_t>(0, arguments.size() == 3 ? wholeArgument(arguments[2], usage) : std::int64_t{0}));
	machine.spendText(text.size());
	const std::size_t start = offsetOf(text, from);
	if(start == std::string_view::npos) return {};
	if(part.empty()) return Value::integer(static_cast<std::int64_t>(from));
	Search search(machine, text, part);
	search.restartAt(start);
	const std::size_t found = search.next();
	if(found == std::string_view::npos) return {};
	return Value::integer(static_cast<std::int64_t>(from) +
	                      static_cast<std::int64_t>(charactersIn(text.substr(start, found - start))));
}

/// @brief `last-index-of`.
/// @param machine The machine.
/// @param arguments A string, a part and maybe the last position it may start at, in characters.
/// @return Where the part last starts, in characters, or nil.
Value lastIndexOf(Machine& machine, Arguments arguments) {
	constexpr std::string_view usage = "\"str/last-index-of\" takes a string, a part and maybe a position";
	checkTexts(arguments, 2, 3, usage);
	const std::string_view text = arguments[0].text();
	const std::string_view part = arguments[1].text();
	auto latest = std::numeric_limits<std::size_t>::max();
	if(arguments.size() == 3) {
		const std::int64_t given = wholeArgument(arguments[2], usage);
		if(given < 0) return {};
		latest = static_cast<std::size_t>(given);
	}
	machine.spendText(text.size());
	if(part.empty()) return Value::integer(static_cast<std::int64_t>(std::min(latest, charactersIn(text))));
	Search search(machine, text, part);
	std::size_t counted = 0;    // the bytes counted in characters so far
	std::size_t characters = 0; // what they hold
	std::optional<std::size_t> last;
	for(std::size_t found = search.next(); found != std::string_view::npos; found = search.next()) {
		characters += charactersIn(text.substr(counted, found - counted));
		counted = found;
		if(characters > latest) break;
		last = characters;
	}
	return last ? Value::integer(static_cast<std::int64_t>(*last)) : Value();
}

/// @brief What `replace` and `replace-first` share.
/// @param machine The machine.
/// @param arguments A string, the text to match and its replacement.
/// @param every Every match, or only the first.
/// @param usage What the function takes, its error.
/// @return The string, its matches replaced.
Value replaced(Machine& machine, Arguments arguments, bool every, std::string_view usage) {
	refuseRegularExpression(arguments, 1);
	checkTexts(arguments, 3, 3, usage);
	const std::string_view text = arguments[0].text();
	const std::string_view match = arguments[1].text();
	const std::string_view replacement = arguments[2].text();
	std::string out;
	std::size_t copied = 0;
	const auto replaceAt = [&](std::size_t at, std::size_t size) {
		machine.heap.charge(replacement.size()); // as it grows: a replacement can be long, and matches many
		out.append(text.substr(copied, at - copied)).append(replacement);
		copied = at + size;
	};
	if(match.empty()) { // before every character and at the end, as in Clojure
		machine.spendText(text.size());
		for(std::size_t at = 0; at < text.size() && (every || at == 0); at = nextCharacter(text, at)) replaceAt(at, 0);
		if(every || text.empty()) replaceAt(text.size(), 0);
	} else {
		Search search(machine, text, match);
		for(std::size_t at = search.next(); at != std::string_view::npos;
		    at = every ? search.next() : std::string_view::npos) {
			replaceAt(at, match.size());
			search.restartAt(copied);
		}
	}
	out.append(text.substr(copied));
	return machine.heap.string(out);
}

/// @brief `replace`. @param machine The machine. @param arguments A string, a match, a replacement. @return It.
Value replace(Machine& machine, Arguments arguments) {
	return replaced(machine, arguments, true, "\"str/replace\" takes a string, the text to match and a replacement");
}

/// @brief `replace-first`. @param machine The machine. @param arguments A string, a match, a replacement. @return It.
Value replaceFirst(Machine& machine, Arguments arguments) {
	return replaced(machine, arguments, false,
	                "\"str/replace-first\" takes a string, the text to match and a replacement");
}

/// @brief A text's pieces between a separator, as Clojure's `split` gives them: with a limit above 0, at most that
/// many and all kept; with 0, the empty ones at the end dropped; below 0, all of them. An empty separator is between
/// every two characters, and an empty text is one empty piece.
/// @param machine The machine.
/// @param text The text.
/// @param separator The separator.
/// @param limit The limit.
/// @param lines Each piece before a separator also loses a '\r' it ends with (`split-lines`).
/// @return The pieces, strings.
std::vector<Value> pieces(Machine& machine, std::string_view text, std::string_view separator, std::int64_t limit,
                          bool lines) {
	if(text.empty()) return {machine.heap.string(text)};
	const auto most = limit > 0 ? static_cast<std::size_t>(limit) : std::numeric_limits<std::size_t>::max();
	std::vector<Value> found;
	std::size_t start = 0;
	const auto add = [&](std::size_t end, std::size_t next) {
		std::string_view piece = text.substr(start, end - start);
		if(lines && end < text.size() && piece.ends_with('\r')) piece.remove_suffix(1);
		found.push_back(machine.heap.string(piece));
		start = next;
	};
	if(separator.empty()) {
		machine.spendText(text.size());
		for(std::size_t at = 0; at < text.size() && found.size() + 1 < most;) {
			at = nextCharacter(text, at);
			add(at, at);
		}
	} else {
		Search search(machine, text, separator);
		for(std::size_t at = search.next(); at != std::string_view::npos && found.size() + 1 < most;
		    at = search.next()) {
			add(at, at + separator.size());
			search.restartAt(start);
		}
	}
	add(text.size(), text.size());
	if(limit == 0)
		while(!found.empty() && found.back().text().empty()) found.pop_back();
	return found;
}

/// @brief `split`.
/// @param machine The machine.
/// @param arguments A string, a separator (a string) and maybe a limit.
/// @return The pieces, a vector.
Value split(Machine& machine, Arguments arguments) {
	constexpr std::string_view usage = "\"str/split\" takes a string, a separator (a string) and maybe a limit";
	refuseRegularExpression(arguments, 1);
	checkTexts(arguments, 2, 3, usage);
	const std::int64_t limit = arguments.size() == 3 ? wholeArgument(arguments[2], usage) : 0;
	return machine.heap.sequence(Kind::Vector, pieces(machine, arguments[0].text(), arguments[1].text(), limit, false));
}

/// @brief `split-lines`. @param machine The machine. @param arguments A string. @return Its lines, at "\n" or "\r\n".
Value splitLines(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 1, 1, "\"str/split-lines\" takes a string");
	return machine.heap.sequence(Kind::Vector, pieces(machine, arguments[0].text(), "\n", 0, true));
}

/// @brief What the trimming functions share.
/// @param machine The machine.
/// @param arguments A string.
/// @param away The characters to take away.
/// @param start From its start.
/// @param end From its end.
/// @param usage What the function takes, its error.
/// @return The string, trimmed.
Value trimmed(Machine& machine, Arguments arguments, std::string_view away, bool start, bool end,
              std::string_view usage) {
	checkTexts(arguments, 1, 1, usage);
	std::string_view text = arguments[0].text();
	machine.spendText(text.size());
	const std::size_t first = start ? text.find_first_not_of(away) : 0;
	if(first == std::string_view::npos) return machine.heap.string("");
	text.remove_prefix(first);
	if(end) text = text.substr(0, text.find_last_not_of(away) + 1);
	return machine.heap.string(text);
}

/// @brief `trim`. @param machine The machine. @param arguments A string. @return It without spaces around it.
Value trim(Machine& machine, Arguments arguments) {
	return trimmed(machine, arguments, spaces, true, true, "\"str/trim\" takes a string");
}

/// @brief `triml`. @param machine The machine. @param arguments A string. @return It without spaces before it.
Value trimLeft(Machine& machine, Arguments arguments) {
	return trimmed(machine, arguments, spaces, true, false, "\"str/triml\" takes a string");
}

/// @brief `trimr`. @param machine The machine. @param arguments A string. @return It without spaces after it.
Value trimRight(Machine& machine, Arguments arguments) {
	return trimmed(machine, arguments, spaces, false, true, "\"str/trimr\" takes a string");
}

/// @brief `trim-newline`. @param machine The machine. @param arguments A string. @return It without line breaks after
/// it.
Value trimNewline(Machine& machine, Arguments arguments) {
	return trimmed(machine, arguments, lineBreaks, false, true, "\"str/trim-newline\" takes a string");
}

/// @brief `reverse`, of a text. @param machine The machine. @param arguments A string. @return Its characters in the
/// other order, each kept whole.
Value reversed(Machine& machine, Arguments arguments) {
	checkTexts(arguments, 1, 1, "\"str/reverse\" takes a string");
	const std::string_view text = arguments[0].text();
	machine.heap.charge(text.size());
	std::string out;
	out.reserve(text.size());
	for(std::size_t end = text.size(); end > 0;) {
		std::size_t start = end - 1;
		while(start > 0 && continuesCharacter(text[start])) --start;
		out.append(text.substr(start, end - start));
		end = start;
	}
	return machine.heap.string(out);
}

/// The namespace's own functions, by their globals' names, sorted: made on first use (Machine::defineLazyNatives()).
constexpr auto natives = std::to_array<LazyNative>({
    {"clojure.string/blank?", blank},
    {"clojure.string/capitalize", capitalize},
    {"clojure.string/ends-with?", endsWith},
    {"clojure.string/includes?", includes},
    {"clojure.string/index-of", indexOf},
    {"clojure.string/last-index-of", lastIndexOf},
    {"clojure.string/replace", replace},
    {"clojure.string/replace-first", replaceFirst},
    {"clojure.string/reverse", reversed},
    {"clojure.string/split", split},
    {"clojure.string/split-lines", splitLines},
    {"clojure.string/starts-with?", startsWith},
    {"clojure.string/trim", trim},
    {"clojure.string/trim-newline", trimNewline},
    {"clojure.string/triml", trimLeft},
    {"clojure.string/trimr", trimRight},
});
static_assert(std::ranges::is_sorted(natives, {}, &LazyNative::name), "Machine::defineLazyNatives() takes them sorted");

} // namespace

void defineStrings(Machine& machine) {
	for(const std::string_view name : sameAsBare)
		machine.defineLanguageValue("clojure.string/" + std::string(name), machine.languageGlobal(name).value);
	machine.defineLazyNatives(natives);
}

} // namespace affresco::lisp
