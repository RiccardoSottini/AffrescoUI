/// @file
/// @brief The language's own functions on text: `str join subs upper-case lower-case name keyword symbol gensym`.

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <cctype>
#include <cstdint>
#include <span>
#include <string>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief `str`. @param machine The machine. @param arguments Values. @return Them as one string, nil as nothing.
Value str(Machine& machine, Arguments arguments) {
	std::string text;
	for(const Value& argument : arguments) printValue(machine, argument, text, false);
	return machine.heap.string(text);
}

/// @brief `join`.
/// @param machine The machine.
/// @param arguments A separator and a list or a vector.
/// @return Its items as one string.
Value join(Machine& machine, Arguments arguments) {
	if(arguments.size() != 2 || arguments[0].kind() != Kind::String ||
	   (!arguments[1].isSequence() && !arguments[1].isNil()))
		throw Problem("\"join\" takes a separator and a list of items");
	std::string text;
	for(std::size_t at = 0; at < arguments[1].count(); ++at) {
		if(at > 0) text += arguments[0].text();
		printValue(machine, arguments[1].at(at), text, false);
	}
	return machine.heap.string(text);
}

/// @brief Where a character starts in a string.
/// @param text The string.
/// @param index Which character.
/// @return Its offset; the string's size for the one past the end, and past that npos.
std::size_t offsetOf(std::string_view text, std::size_t index) noexcept {
	std::size_t at = 0;
	for(std::size_t passed = 0; passed < index; ++passed) {
		if(at >= text.size()) return std::string_view::npos;
		++at;
		while(at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0U) == 0x80U) ++at;
	}
	return at;
}

/// @brief `subs`, by characters.
/// @param machine The machine.
/// @param arguments A string, a start, maybe an end.
/// @return The part.
Value subs(Machine& machine, Arguments arguments) {
	if(arguments.size() < 2 || arguments.size() > 3 || arguments[0].kind() != Kind::String)
		throw Problem("\"subs\" takes a string, a start and maybe an end");
	for(const Value& index : arguments.subspan(1))
		if(index.kind() != Kind::Integer || index.asInteger() < 0) throw Problem("\"subs\" takes indexes from 0");
	const std::string_view text = arguments[0].text();
	const std::size_t start = offsetOf(text, static_cast<std::size_t>(arguments[1].asInteger()));
	const std::size_t end =
	    arguments.size() == 3 ? offsetOf(text, static_cast<std::size_t>(arguments[2].asInteger())) : text.size();
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
	return machine.heap.string(expressionOf(valueToTree(machine, arguments[0], "")));
}

/// @brief `%binding`: a binding's text, from its parts.
/// @param machine The machine.
/// @param arguments The parts, strings.
/// @return The text.
Value bindingText(Machine& machine, Arguments arguments) {
	std::string text;
	for(const Value& part : arguments) text += part.text();
	machine.spend(text.size());
	return machine.heap.string(text);
}

} // namespace

void defineText(Machine& machine) {
	machine.defineNative("%read", readWithoutApp);
	machine.defineNative("%expression", expressionPart);
	machine.defineNative("%binding", bindingText);
	machine.defineNative("str", str);
	machine.defineNative("join", join);
	machine.defineNative("subs", subs);
	machine.defineNative("upper-case", upperCase);
	machine.defineNative("lower-case", lowerCase);
	machine.defineNative("name", nameOf);
	machine.defineNative("keyword", makeKeyword);
	machine.defineNative("symbol", makeSymbol);
	machine.defineNative("gensym", gensym);
}

} // namespace affresco::lisp
