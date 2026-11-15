/// @file
/// @brief The reader's forms after a '#' (Reader.hpp): a function literal `#(...)`, a set literal `#{...}` and a
/// reader conditional `#?(...)`. The lexer gives each its own token, and the bracket after it is read here. And
/// Clojure's ways to write a character or a number, read as the nearest JSON value.

#include "Reader.hpp"
#include "Sets.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

/// The most parameters a function literal takes, `%` to `%20`.
constexpr int mostLambdaParameters = 20;

/// @brief How many bytes a UTF-8 character takes, from its first byte.
/// @param lead The first byte.
/// @return 1 to 4, or 0 if no character starts with it.
constexpr std::size_t characterLength(unsigned char lead) noexcept {
	if(lead < 0x80U) return 1;
	if(lead >= 0xC2U && lead <= 0xDFU) return 2;
	if(lead >= 0xE0U && lead <= 0xEFU) return 3;
	if(lead >= 0xF0U && lead <= 0xF4U) return 4;
	return 0;
}

/// @brief Reads digits as a whole number.
/// @param digits The digits, maybe after a '-'.
/// @param base 10 or 16.
/// @return The number, or std::nullopt if they aren't all digits or it doesn't fit 64 bits.
std::optional<std::int64_t> wholeOf(std::string_view digits, int base) {
	std::int64_t number = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number, base);
	if(error != std::errc() || end != digits.data() + digits.size()) return std::nullopt;
	return number;
}

/// @brief Tells whether a text is digits, maybe after a '-'.
/// @param text The text.
/// @param hex Whether hexadecimal digits count.
/// @return true if it is, with a digit at least.
bool isDigits(std::string_view text, bool hex) {
	if(text.starts_with('-')) text.remove_prefix(1);
	return !text.empty() && std::ranges::all_of(text, [&](char c) {
		return (c >= '0' && c <= '9') || (hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
	});
}

/// @brief Tells whether a form is its own value, so that a set of such forms is made as the reader reads it: neither a
/// word nor a list, nor a vector or a map holding one. The reader's nesting limit bounds the depth.
/// @param form The form.
/// @param depth How many forms hold it.
/// @return true if so.
bool isConstantForm(const Value& form, int depth) {
	switch(form.kind()) {
	case Kind::Symbol:
	case Kind::List:
		return false;
	case Kind::Vector:
		for(std::size_t at = 0; at < form.count(); ++at)
			if(!isConstantForm(form.at(at), depth + 1)) return false;
		return true;
	case Kind::Map: // its keys are text, or it would be a (%map ...) list
		for(std::size_t at = 0; at < form.count(); ++at)
			if(!isConstantForm(form.valueAt(at), depth + 1)) return false;
		return true;
	default: // a set the reader made is one of constants
		return true;
	}
}

} // namespace

Value Reader::hashForm(const Token& token, int depth) {
	switch(token.kind) {
	case TokenKind::Lambda:
		return lambda(token, depth);
	case TokenKind::SetOf:
		return setOf(token, depth);
	case TokenKind::ConditionalSplice:
		lexer_.fail(token.offset,
		            "#?@ splices its items into the list or the vector around it, and here there is none");
	default:
		return chosenOrNext(token, depth);
	}
}

Value Reader::lambda(const Token& token, int depth) {
	if(lambda_)
		lexer_.fail(token.offset, "a #(...) inside another: name the inner function's parameter with (fn [x] ...)");
	shallowEnough(token, depth + 2); // the fn form and its body
	const Place place = placeAt(token.offset);
	const Token open = lexer_.next(); // the '(' the lexer saw after the '#'
	LambdaParameters used;
	lambda_ = &used;
	Value body;
	try {
		body = list(open, depth + 1);
	} catch(...) {
		lambda_ = nullptr;
		throw;
	}
	lambda_ = nullptr;
	std::vector<Value> parameters;
	for(int number = 1; number <= used.most; ++number) // unused ones in between are parameters still
		parameters.push_back(heap_.symbol(number == 1 ? std::string("%") : "%" + std::to_string(number), place));
	if(used.rest) {
		parameters.push_back(heap_.symbol("&", place));
		parameters.push_back(heap_.symbol("%&", place));
	}
	const std::vector<Value> function = {heap_.symbol("fn", place),
	                                     heap_.sequence(Kind::Vector, std::move(parameters), place), body};
	return heap_.sequence(Kind::List, std::span<const Value>(function), place);
}

std::optional<std::string> Reader::lambdaParameter(const Token& token) {
	const std::string_view written = token.text.substr(1);
	const std::size_t dot = written.find('.');
	const std::string_view number = written.substr(0, dot);
	const std::string_view path = dot == std::string_view::npos ? std::string_view() : written.substr(dot);
	if(number == "&") {
		lambda_->rest = true;
		return "%&" + std::string(path);
	}
	if(!number.empty() && !std::ranges::all_of(number, [](char c) { return c >= '0' && c <= '9'; }))
		return std::nullopt; // a word of its own, such as %map
	const std::int64_t whole = number.empty() ? 1 : number.size() > 3 ? 1000 : wholeOf(number, 10).value_or(1000);
	if(whole == 0) lexer_.fail(token.offset, "a #(...) names its parameters %, %1 to %20, and %&");
	if(whole > mostLambdaParameters)
		lexer_.fail(token.offset, "a #(...) takes at most " + std::to_string(mostLambdaParameters) + " parameters");
	const int index = static_cast<int>(whole);
	lambda_->most = std::max(lambda_->most, index);
	return (index == 1 ? std::string("%") : "%" + std::to_string(index)) + std::string(path);
}

Value Reader::setOf(const Token& token, int depth) {
	shallowEnough(token, depth + 1);
	const Place place = placeAt(token.offset);
	lexer_.next(); // the '{' the lexer saw after the '#'
	std::vector<Value> items;
	std::vector<std::size_t> offsets; // where each item starts, for the error of one twice
	for(Token item = lexer_.next(); item.kind != TokenKind::CloseMap; item = lexer_.next()) {
		if(item.kind == TokenKind::End)
			lexer_.fail(item.offset, "expected '}' to close the set opened at " + lexer_.placeOf(token.offset));
		elements(item, depth + 1, items);
		offsets.resize(items.size(), item.offset);
	}
	// The same form twice is refused as a map's key twice is, constant or not (`#{x x}`), as Clojure's reader does
	std::optional<std::size_t> repeat;
	const Value set = setOfItems(heap_, nullptr, items, place, &repeat);
	if(repeat) {
		const Value& item = items[*repeat];
		const bool text = item.kind() == Kind::String || item.kind() == Kind::Keyword || item.kind() == Kind::Symbol;
		lexer_.fail(offsets[*repeat],
		            (text ? "the item \"" + notation::Lexer::excerpt(item.text()) + "\" twice"
		                  : "the same " + std::string(item.isNil() ? "nil" : describeKind(item).substr(2)) + " twice") +
		                " in a set: a set holds each item once");
	}
	if(std::ranges::all_of(items, [](const Value& item) { return isConstantForm(item, 0); })) return set;
	items.insert(items.begin(), heap_.symbol("%set", place)); // worked out when it runs
	return heap_.sequence(Kind::List, std::move(items), place);
}

std::optional<Value> Reader::conditional(const Token& token, int depth) {
	shallowEnough(token, depth + 1);
	lexer_.next(); // the '(' the lexer saw after the '#?' or '#?@'
	std::optional<Value> chosen;
	for(Token item = lexer_.next(); item.kind != TokenKind::CloseNode; item = lexer_.next()) {
		if(item.kind == TokenKind::End)
			lexer_.fail(item.offset,
			            "expected ')' to close the reader conditional opened at " + lexer_.placeOf(token.offset));
		if(item.kind != TokenKind::Key)
			lexer_.fail(item.offset, "a reader conditional holds pairs of a platform's keyword and a form, "
			                         "such as :affresco 1 :default 2, and found " +
			                             notation::Lexer::shown(item));
		const Token next = lexer_.next();
		if(next.kind == TokenKind::CloseNode || next.kind == TokenKind::End)
			lexer_.fail(next.offset, "the keyword " + notation::Lexer::shown(item) + " has no form after it");
		// Every branch is read; one not chosen leaves no parameter of a function literal behind
		const std::optional<LambdaParameters> before = lambda_ ? std::optional(*lambda_) : std::nullopt;
		const std::string_view feature = item.text.substr(1);
		const bool chooses = !chosen && (feature == "affresco" || feature == "default"); // the first, as Clojure
		const Value branch = form(next, depth + 1);
		if(chooses) chosen = branch;
		else if(before) *lambda_ = *before;
	}
	return chosen;
}

Value Reader::chosenOrNext(Token token, int depth) {
	// Iterative, so that a long run of conditionals that choose nothing takes no stack
	for(;;) {
		if(token.kind != TokenKind::Conditional) return form(token, depth);
		if(const std::optional<Value> chosen = conditional(token, depth)) return *chosen;
		token = lexer_.next();
	}
}

Value Reader::character(const Token& token) {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 6> names = {
	    {{"newline", "\n"}, {"space", " "}, {"tab", "\t"}, {"return", "\r"}, {"formfeed", "\f"}, {"backspace", "\b"}}};
	const std::string_view name = token.text.substr(1);
	const Place place = placeAt(token.offset);
	if(name.size() == 1 && static_cast<unsigned char>(name[0]) < 0x80) return heap_.string(name, place); // ASCII
	// NOLINTNEXTLINE(readability-qualified-auto): std::array's iterator is a pointer only in some standard libraries
	if(const auto named = std::ranges::find(names, name, &std::pair<std::string_view, std::string_view>::first);
	   named != names.end())
		return heap_.string(named->second, place);
	const bool unicode = name.size() == 5 && name.front() == 'u' && isDigits(name.substr(1), true);
	if(unicode || characterLength(static_cast<unsigned char>(name.front())) == name.size()) {
		// JSON reads \uXXXX and checks the bytes are UTF-8, and refuses half of a surrogate pair
		const std::string written = unicode ? "\"\\" + std::string(name) + "\"" : "\"" + std::string(name) + "\"";
		return heap_.string(lexer_.unquote(written, token.offset), place);
	}
	lexer_.fail(token.offset, "unknown character " + notation::Lexer::shown(token) +
	                              ": write one character after the backslash, \\newline, \\space, \\tab, \\return, "
	                              "\\formfeed, \\backspace or \\u and four hexadecimal digits");
}

std::optional<json> Reader::clojureNumber(const Token& token) const {
	const std::string_view text = token.text;
	if(text.ends_with('N') && isDigits(text.substr(0, text.size() - 1), false)) return bigInteger(token); // 12N
	if(text.ends_with('M') && notation::isJsonNumber(text.substr(0, text.size() - 1))) { // 1.5M, a big decimal
		Token number = token;
		number.text = text.substr(0, text.size() - 1);
		return lexer_.constant(number);
	}
	if(const std::size_t slash = text.find('/'); slash != std::string_view::npos &&
	                                             isDigits(text.substr(0, slash), false) &&
	                                             isDigits(text.substr(slash + 1), false) && text[slash + 1] != '-')
		return ratio(token); // 1/2
	const bool negative = text.starts_with('-');
	const std::string_view unsigned_ = text.substr(negative ? 1 : 0);
	if((unsigned_.starts_with("0x") || unsigned_.starts_with("0X")) && isDigits(unsigned_.substr(2), true) &&
	   !unsigned_.substr(2).starts_with('-')) // 0x1F, hexadecimal
		return json(wholeIn(token, (negative ? "-" : "") + std::string(unsigned_.substr(2)), 16));
	return std::nullopt;
}

std::int64_t Reader::wholeIn(const Token& token, std::string_view digits, int base) const {
	const std::optional<std::int64_t> number = wholeOf(digits, base);
	if(!number) lexer_.fail(token.offset, "the number " + notation::Lexer::shown(token) + " doesn't fit 64 bits");
	return *number;
}

json Reader::bigInteger(const Token& token) const {
	const std::string_view digits = token.text.substr(0, token.text.size() - 1);
	std::uint64_t large = 0; // past the int64's, as JSON reads 18446744073709551615
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), large);
	if(!digits.starts_with('-') && error == std::errc() && end == digits.data() + digits.size()) return json(large);
	return json(wholeIn(token, digits, 10));
}

json Reader::ratio(const Token& token) const {
	const std::size_t slash = token.text.find('/');
	const std::int64_t numerator = wholeIn(token, token.text.substr(0, slash), 10);
	const std::int64_t denominator = wholeIn(token, token.text.substr(slash + 1), 10);
	if(denominator == 0) lexer_.fail(token.offset, "the ratio " + notation::Lexer::shown(token) + " divides by zero");
	if(numerator % denominator == 0) return json(numerator / denominator);
	return json(static_cast<double>(numerator) / static_cast<double>(denominator));
}

} // namespace affresco::lisp
