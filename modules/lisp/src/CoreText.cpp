/// @file
/// @brief The language's own functions on text: `str join subs upper-case lower-case name keyword symbol gensym`,
/// `format`, errors as values (`ex-info ex-message ex-data`) and reading values from text (`parse-long parse-double
/// parse-boolean`).

#include "Characters.hpp"
#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief `str`. @param machine The machine. @param arguments Values. @return Them as one string, nil as nothing.
Value str(Machine& machine, Arguments arguments) {
	std::string text;
	for(const Value& argument : arguments) printValue(machine, argument, text, false);
	return machine.heap.string(text);
}

/// @brief `join`: `(join items)` and `(join separator items)`, as `clojure.string/join` has them.
/// @param machine The machine.
/// @param arguments Maybe a separator, and a list or a vector.
/// @return Its items as one string, each as `str` shows it.
Value join(Machine& machine, Arguments arguments) {
	const bool separated = arguments.size() == 2;
	if((arguments.size() != 1 && !separated) || (separated && arguments[0].kind() != Kind::String) ||
	   (!arguments.back().isSequence() && !arguments.back().isNil()))
		throw Problem("\"join\" takes a list of items, and maybe a separator before it");
	const std::string_view separator = separated ? arguments[0].text() : std::string_view();
	const Value& items = arguments.back();
	std::string text;
	for(std::size_t at = 0; at < items.count(); ++at) {
		machine.heap.charge(at > 0 ? separator.size() : 0); // as it grows: a separator can be long
		if(at > 0) text += separator;
		printValue(machine, items.at(at), text, false);
	}
	return machine.heap.string(text);
}

/// @brief `subs`, by characters.
/// @param machine The machine.
/// @param arguments A string, a start, maybe an end.
/// @return The part.
Value subs(Machine& machine, Arguments arguments) {
	if(arguments.size() < 2 || arguments.size() > 3 || arguments[0].kind() != Kind::String)
		throw Problem("\"subs\" takes a string, a start and maybe an end");
	for(const Value& index : arguments.subspan(1))
		if(wholeNumber(index).value_or(-1) < 0) throw Problem("\"subs\" takes indexes from 0");
	const std::string_view text = arguments[0].text();
	machine.spendText(text.size());
	const std::size_t start = offsetOf(text, static_cast<std::size_t>(wholeNumber(arguments[1]).value_or(0)));
	const std::size_t end = arguments.size() == 3
	                            ? offsetOf(text, static_cast<std::size_t>(wholeNumber(arguments[2]).value_or(0)))
	                            : text.size();
	if(start == std::string_view::npos || end == std::string_view::npos || end < start)
		throw Problem("\"subs\": indexes outside the string");
	return machine.heap.string(text.substr(start, end - start));
}

/// @brief What `upper-case` and `lower-case` share: ASCII letters only, so the text stays valid UTF-8.
/// @param machine The machine.
/// @param arguments A string.
/// @param change What to do to a letter.
/// @return The string.
Value changedCase(Machine& machine, Arguments arguments, int (*change)(int)) {
	if(arguments.size() != 1 || arguments[0].kind() != Kind::String) throw Problem("changing case takes a string");
	std::string text(arguments[0].text());
	machine.heap.charge(text.size());
	for(char& c : text)
		if(static_cast<unsigned char>(c) < 0x80U) c = static_cast<char>(change(static_cast<unsigned char>(c)));
	return machine.heap.string(text);
}

/// @brief `pr-str`. @param machine The machine. @param arguments Values. @return They as Lisp text, read back as they
/// are.
Value printed(Machine& machine, Arguments arguments) {
	std::string text;
	for(std::size_t at = 0; at < arguments.size(); ++at) {
		if(at > 0) text += ' ';
		printValue(machine, arguments[at], text, true);
	}
	return machine.heap.string(text);
}

/// @brief `upper-case`. @param machine The machine. @param arguments A string. @return It in capitals.
Value upperCase(Machine& machine, Arguments arguments) { return changedCase(machine, arguments, std::toupper); }

/// @brief `lower-case`. @param machine The machine. @param arguments A string. @return It in small letters.
Value lowerCase(Machine& machine, Arguments arguments) { return changedCase(machine, arguments, std::tolower); }

/// @brief `name`. @param machine The machine. @param arguments A string, a symbol or a keyword. @return Its text as a
/// string.
Value nameOf(Machine& machine, Arguments arguments) {
	if(arguments.size() != 1 || (arguments[0].kind() != Kind::String && arguments[0].kind() != Kind::Symbol &&
	                             arguments[0].kind() != Kind::Keyword))
		throw Problem("\"name\" takes a string, a symbol or a keyword");
	return arguments[0].kind() == Kind::String ? arguments[0] : machine.heap.string(arguments[0].text());
}

/// @brief `keyword`. @param machine The machine. @param arguments A string, a symbol or a keyword. @return The keyword.
Value makeKeyword(Machine& machine, Arguments arguments) {
	return machine.heap.keyword(nameOf(machine, arguments).text());
}

/// @brief `symbol`. @param machine The machine. @param arguments A string, a symbol or a keyword. @return The symbol.
Value makeSymbol(Machine& machine, Arguments arguments) {
	return machine.heap.symbol(nameOf(machine, arguments).text());
}

/// @brief `gensym`: a name for a macro's own variable, numbered within the definition being compiled, so that the same
/// source always gives the same names.
/// @param machine The machine.
/// @param arguments Maybe a string to start it with.
/// @return The symbol.
Value gensym(Machine& machine, Arguments arguments) {
	const std::string start = arguments.empty() ? "g" : std::string(arguments[0].text());
	return machine.heap.symbol(start + "__" + std::to_string(++machine.gensymCounter()));
}

/// @brief `%read`, until a host gives one: a path of the running app (`state.count`) has nothing to read while
/// building.
/// @param arguments The path.
/// @return Nothing: it always refuses.
Value readWithoutApp(Machine& /*machine*/, Arguments arguments) {
	throw Problem("\"" + notation::Lexer::excerpt(arguments.empty() ? "" : arguments[0].text()) +
	              "\" is read while the app runs, and nothing reads it while building");
}

/// @brief A JSON value as an expression writes it.
/// @param value The value.
/// @return The text.
std::string expressionOf(const json& value) {
	if(value.is_string()) return expressionText(value.get_ref<const std::string&>());
	if(value.is_number_float()) { // an expression reads no exponent: 0.000001, not 1e-06
		std::array<char, 400> digits{};
		const auto end =
		    std::to_chars(digits.data(), digits.data() + digits.size(), value.get<double>(), std::chars_format::fixed);
		const std::string text(digits.data(), end.ptr);
		return text.contains('.') ? text : text + ".0";
	}
	if(value.is_array()) {
		std::string text = "[";
		for(const json& item : value) text.append(text.size() > 1 ? ", " : "").append(expressionOf(item));
		return text + "]";
	}
	if(value.is_object()) {
		std::string text = "{";
		for(const auto& [key, member] : value.items())
			text.append(text.size() > 1 ? ", " : "")
			    .append(expressionText(key))
			    .append(": ")
			    .append(expressionOf(member));
		return text + "}";
	}
	return value.dump();
}

/// @brief `%expression`: what the build worked out, as a binding's text holds it.
/// @param machine The machine.
/// @param arguments The value.
/// @return The text.
Value expressionPart(Machine& machine, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"%expression\" takes one value");
	return machine.heap.string(expressionOfValue(machine, arguments[0]));
}

/// @brief `%text`: what the build worked out, as a part of a binding's text that `str` joins: a text or a number as
/// itself (`(str prefix x)` with `prefix` "Hello" is `"Hello{{x}}"`), anything else as a placeholder. A text holding a
/// brace is a placeholder too, so no value becomes an expression.
/// @param machine The machine.
/// @param arguments The value.
/// @return The text.
Value textPart(Machine& machine, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"%text\" takes one value");
	const Value& value = arguments[0];
	const bool plain = value.isNil() || value.kind() == Kind::Boolean || value.isNumber() ||
	                   (value.kind() == Kind::String && !value.text().contains('{') && !value.text().contains('}'));
	if(!plain) return machine.heap.string("{{" + expressionOfValue(machine, value) + "}}");
	std::string text;
	printValue(machine, value, text, false);
	return machine.heap.string(text);
}

/// @brief `%binding`: a binding's text, from its parts.
/// @param machine The machine.
/// @param arguments The parts, strings.
/// @return The text.
Value bindingText(Machine& machine, Arguments arguments) {
	std::string text;
	for(const Value& part : arguments) {
		machine.heap.charge(part.text().size()); // as it grows: these can be long, and many
		text += part.text();
	}
	machine.spend(text.size());
	return machine.heap.string(keptSpaces(std::move(text)));
}

/// @brief `%joined`: the text of a `str` a binding joins, from its parts (joinedText()).
/// @param machine The machine.
/// @param arguments The parts, strings.
/// @return The text.
Value joinedBinding(Machine& machine, Arguments arguments) {
	const Value text = bindingText(machine, arguments);
	return machine.heap.string(joinedText(std::string(text.text())));
}

/// The most a `format` directive's width or precision may be, so that a short pattern never makes a long text.
constexpr std::size_t mostFormatWidth = 1000;

/// @brief A directive of `format`, as `%-05.2f` writes it.
struct Directive {
	std::string_view written;             ///< As written, from its '%'.
	bool left = false;                    ///< `-`: padded on the right.
	bool zeros = false;                   ///< `0`: a number padded with zeros.
	std::size_t width = 0;                ///< The fewest characters it shows.
	std::optional<std::size_t> precision; ///< `.N`: digits after the point, or the most characters of `%s`.
	std::string_view conversion;          ///< The character that says what it shows, `f`.
};

/// @brief A width or a precision of a directive, refused above mostFormatWidth.
/// @param pattern The pattern.
/// @param at Where its digits start; moved past them.
/// @return Its value.
std::size_t directiveNumber(std::string_view pattern, std::size_t& at) {
	std::size_t value = 0;
	for(; at < pattern.size() && pattern[at] >= '0' && pattern[at] <= '9'; ++at) {
		value = value * 10 + static_cast<std::size_t>(pattern[at] - '0');
		if(value > mostFormatWidth) throw Problem("format takes a width or a precision up to 1,000");
	}
	return value;
}

/// @brief Reads a directive of `format`.
/// @param pattern The pattern.
/// @param at Where its '%' is; moved past it.
/// @return The directive.
Directive readDirective(std::string_view pattern, std::size_t& at) {
	const std::size_t start = at++;
	Directive directive;
	for(; at < pattern.size() && (pattern[at] == '-' || pattern[at] == '0'); ++at)
		(pattern[at] == '-' ? directive.left : directive.zeros) = true;
	directive.width = directiveNumber(pattern, at);
	if(at < pattern.size() && pattern[at] == '.') directive.precision = directiveNumber(pattern, ++at);
	if(at >= pattern.size())
		throw Problem("format's pattern ends inside the directive \"" +
		              notation::Lexer::excerpt(pattern.substr(start)) + "\"");
	const std::size_t end = nextCharacter(pattern, at);
	directive.conversion = pattern.substr(at, end - at);
	directive.written = pattern.substr(start, end - start);
	at = end;
	if(!std::string_view("sdfx%n").contains(directive.conversion) || directive.conversion.size() != 1)
		throw Problem("format doesn't have %" + std::string(directive.conversion) + ": it has %s %d %f %x %%");
	if(directive.zeros && !std::string_view("dfx").contains(directive.conversion))
		throw Problem("format's " + std::string(directive.written) + ": the 0 flag is for numbers");
	if(directive.precision && !std::string_view("fs").contains(directive.conversion))
		throw Problem("format's " + std::string(directive.written) + ": a precision is for %f and %s");
	return directive;
}

/// @brief A whole number as `%d` or `%x` shows it.
/// @param value The value.
/// @param hexadecimal In base 16, a negative number as its 64-bit two's complement, as in Clojure.
/// @param directive The directive, for its error.
/// @return The digits, maybe after a '-'.
std::string wholeDigits(const Value& value, bool hexadecimal, const Directive& directive) {
	std::array<char, 24> digits{};
	const int base = hexadecimal ? 16 : 10;
	std::to_chars_result end{};
	if(value.kind() == Kind::Unsigned) {
		end = std::to_chars(digits.data(), digits.data() + digits.size(), value.asUnsigned(), base);
	} else if(const std::optional<std::int64_t> whole =
	              value.kind() == Kind::Integer ? value.asInteger() : wholeNumber(value)) {
		end = hexadecimal ? std::to_chars(digits.data(), digits.data() + digits.size(),
		                                  static_cast<std::uint64_t>(*whole), base)
		                  : std::to_chars(digits.data(), digits.data() + digits.size(), *whole, base);
	} else {
		throw Problem("format's " + std::string(directive.written) + " takes a whole number, given " +
		              std::string(describeKind(value)));
	}
	return {digits.data(), end.ptr};
}

/// @brief What a directive shows of a value, before it is padded.
/// @param machine The machine.
/// @param directive The directive: `%s`, `%d`, `%x` or `%f`.
/// @param value The value.
/// @return The text.
std::string directiveText(Machine& machine, const Directive& directive, const Value& value) {
	if(directive.conversion == "d" || directive.conversion == "x")
		return wholeDigits(value, directive.conversion == "x", directive);
	if(directive.conversion == "f") {
		if(!value.isNumber())
			throw Problem("format's " + std::string(directive.written) + " takes a number, given " +
			              std::string(describeKind(value)));
		std::array<char, 1400> digits{}; // the most digits of a double, a sign, a point and 1,000 after it
		const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), value.asDouble(),
		                               std::chars_format::fixed, static_cast<int>(directive.precision.value_or(6)));
		return {digits.data(), end.ptr};
	}
	std::string text;
	printValue(machine, value, text, false);
	if(directive.precision) text.resize(std::min(text.size(), offsetOf(text, *directive.precision)));
	return text;
}

/// @brief Pads what a directive shows to its width.
/// @param machine The machine, charged for the padding.
/// @param directive The directive.
/// @param text What it shows; padded.
void pad(Machine& machine, const Directive& directive, std::string& text) {
	const std::size_t characters = charactersIn(text);
	if(characters >= directive.width) return;
	const std::size_t missing = directive.width - characters;
	machine.heap.charge(missing);
	if(directive.left) text.append(missing, ' ');
	else if(directive.zeros) text.insert(text.starts_with('-') ? 1 : 0, missing, '0');
	else text.insert(0, missing, ' ');
}

/// @brief `format`: `(format "%s has %d items (%.1f%%)" name n share)`.
/// @param machine The machine.
/// @param arguments A pattern, and the values its directives show.
/// @return The text.
Value format(Machine& machine, Arguments arguments) {
	if(arguments.empty() || arguments[0].kind() != Kind::String)
		throw Problem("\"format\" takes a pattern (a string) and the values it shows");
	const std::string_view pattern = arguments[0].text();
	machine.spendText(pattern.size());
	std::string text;
	std::size_t next = 1; // the next value to show
	for(std::size_t at = 0; at < pattern.size();) {
		const std::size_t percent = std::min(pattern.find('%', at), pattern.size());
		machine.heap.charge(percent - at); // the text grows as it is written: a short %.1000f shows 1,300 bytes
		text.append(pattern.substr(at, percent - at));
		if(percent == pattern.size()) break;
		at = percent;
		const Directive directive = readDirective(pattern, at);
		std::string shown = directive.conversion == "%" ? "%" : directive.conversion == "n" ? "\n" : "";
		if(shown.empty()) {
			if(next >= arguments.size())
				throw Problem("format's " + std::string(directive.written) + " has no value: give one more");
			shown = directiveText(machine, directive, arguments[next++]);
		}
		pad(machine, directive, shown);
		machine.heap.charge(shown.size());
		text += shown;
	}
	return machine.heap.string(text);
}

/// @brief `ex-info`: an error as a value, for `throw`.
/// @param machine The machine.
/// @param arguments A message and data, a map.
/// @return `{:message message :data data}`.
Value errorInfo(Machine& machine, Arguments arguments) {
	if(arguments.size() != 2 || arguments[0].kind() != Kind::String || arguments[1].kind() != Kind::Map)
		throw Problem("\"ex-info\" takes a message (a string) and data (a map)");
	return machine.heap.map({{"message", arguments[0]}, {"data", arguments[1]}});
}

/// @brief `ex-message`.
/// @param arguments What a `catch` bound.
/// @return `:message` of a map, the text of a string that was thrown, else nil.
Value errorMessage(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"ex-message\" takes one value");
	if(arguments[0].kind() == Kind::String) return arguments[0];
	return arguments[0].kind() == Kind::Map ? arguments[0].lookup("message") : Value();
}

/// @brief `ex-data`. @param arguments What a `catch` bound. @return `:data` of a map, else nil.
Value errorData(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"ex-data\" takes one value");
	return arguments[0].kind() == Kind::Map ? arguments[0].lookup("data") : Value();
}

/// The longest text the `parse-` functions read a number from; a longer one is nil.
constexpr std::size_t longestNumber = 40;

/// @brief The text a `parse-` function reads, checked.
/// @param arguments Its arguments.
/// @param name Its name, for the error.
/// @return The text.
std::string_view parsedText(Arguments arguments, std::string_view name) {
	if(arguments.size() != 1 || arguments[0].kind() != Kind::String)
		throw Problem("\"" + std::string(name) + "\" takes a string");
	return arguments[0].text();
}

/// @brief Tells whether a text is digits, at least one.
/// @param text The text.
/// @return true if it is.
bool allDigits(std::string_view text) noexcept {
	return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// @brief `parse-long`. @param arguments A string. @return The whole number it writes (`-12`, `+3`), or nil.
Value parseLong(Machine& /*machine*/, Arguments arguments) {
	std::string_view text = parsedText(arguments, "parse-long");
	const bool negative = text.starts_with('-');
	if(negative || text.starts_with('+')) text.remove_prefix(1);
	if(arguments[0].text().size() > longestNumber || !allDigits(text)) return {};
	std::uint64_t magnitude = 0;
	if(std::from_chars(text.data(), text.data() + text.size(), magnitude).ec != std::errc()) return {};
	constexpr auto most = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
	if(magnitude > most + (negative ? 1 : 0)) return {};
	return Value::integer(negative ? static_cast<std::int64_t>(0 - magnitude) : static_cast<std::int64_t>(magnitude));
}

/// @brief Tells whether a text writes a decimal number as Clojure's `parse-double` reads one: digits with a point
/// somewhere or none, and maybe an exponent.
/// @param text The text, without its sign.
/// @return true if it does.
bool decimalNumber(std::string_view text) {
	const std::size_t exponent = std::min(text.find_first_of("eE"), text.size());
	const std::string_view mantissa = text.substr(0, exponent);
	if(exponent < text.size()) {
		std::string_view power = text.substr(exponent + 1);
		if(power.starts_with('-') || power.starts_with('+')) power.remove_prefix(1);
		if(!allDigits(power)) return false;
	}
	const std::size_t point = mantissa.find('.');
	if(point == std::string_view::npos) return allDigits(mantissa);
	const std::string_view before = mantissa.substr(0, point);
	const std::string_view after = mantissa.substr(point + 1);
	return (before.empty() || allDigits(before)) && (after.empty() || allDigits(after)) && mantissa.size() > 1;
}

/// @brief A text without the spaces and control characters around it, as Java's `trim()` reads it for `parse-double`.
/// @param text The text.
/// @return What is left.
std::string_view withoutControls(std::string_view text) noexcept {
	const auto control = [](char c) { return static_cast<unsigned char>(c) <= 0x20U; };
	while(!text.empty() && control(text.front())) text.remove_prefix(1);
	while(!text.empty() && control(text.back())) text.remove_suffix(1);
	return text;
}

/// @brief `parse-double`: Clojure's forms (`1.5`, `-2e3`, `.5`, `5.`, a suffix `d` or `f`, spaces around), but nil for
/// `NaN` and `Infinity`, and for a number too large for a double, as JSON has neither.
/// @param arguments A string.
/// @return The number, or nil.
Value parseDouble(Machine& /*machine*/, Arguments arguments) {
	std::string_view text = parsedText(arguments, "parse-double");
	if(text.size() > longestNumber) return {};
	text = withoutControls(text);
	const bool negative = text.starts_with('-');
	if(negative || text.starts_with('+')) text.remove_prefix(1);
	if(!text.empty() && std::string_view("dDfF").contains(text.back())) text.remove_suffix(1);
	if(!decimalNumber(text)) return {};
	double value = 0.0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if(error == std::errc::result_out_of_range) { // too small is zero, as in Clojure; too large has no JSON value
		const std::size_t exponent = text.find_first_of("eE");
		if(exponent == std::string_view::npos || text[exponent + 1] != '-') return {};
		value = 0.0;
	} else if(error != std::errc() || end != text.data() + text.size()) {
		return {};
	}
	return Value::real(negative ? -value : value);
}

/// @brief `parse-boolean`. @param arguments A string. @return true for "true", false for "false", else nil.
Value parseBoolean(Machine& /*machine*/, Arguments arguments) {
	const std::string_view text = parsedText(arguments, "parse-boolean");
	if(text == "true" || text == "false") return Value::boolean(text == "true");
	return {};
}

/// Functions made on first use (Machine::defineLazyNatives()), sorted by name: a program that never calls them doesn't
/// pay for them when the machine starts.
constexpr auto lazyNatives = std::to_array<LazyNative>({
    {"ex-data", errorData},
    {"ex-info", errorInfo},
    {"ex-message", errorMessage},
    {"format", format},
    {"parse-boolean", parseBoolean},
    {"parse-double", parseDouble},
    {"parse-long", parseLong},
});
static_assert(std::ranges::is_sorted(lazyNatives, {}, &LazyNative::name),
              "Machine::defineLazyNatives() takes them sorted");

} // namespace

std::string expressionOfValue(Machine& machine, const Value& value) {
	if(value.kind() == Kind::Symbol) return pathExpression(value.text());
	if(value.kind() == Kind::Function || value.kind() == Kind::Native)
		throw Problem("a binding was given a function: a name it reads from the running app is also a definition's, "
		              "or a parameter's; rename one");
	return expressionOf(valueToTree(machine, value, ""));
}

void defineText(Machine& machine) {
	machine.defineNative("%read", readWithoutApp);
	machine.defineNative("%expression", expressionPart);
	machine.defineNative("%binding", bindingText);
	machine.defineNative("%text", textPart);
	machine.defineNative("%joined", joinedBinding);
	machine.defineNative("str", str);
	machine.defineNative("join", join);
	machine.defineNative("pr-str", printed);
	machine.defineNative("subs", subs);
	machine.defineNative("upper-case", upperCase);
	machine.defineNative("lower-case", lowerCase);
	machine.defineNative("name", nameOf);
	machine.defineNative("keyword", makeKeyword);
	machine.defineNative("symbol", makeSymbol);
	machine.defineNative("gensym", gensym);
	machine.defineLazyNatives(lazyNatives);
}

} // namespace affresco::lisp
