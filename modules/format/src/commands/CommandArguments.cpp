/// @file
/// @brief The typed arguments of a command (CommandArguments.hpp).

#include "CommandArguments.hpp"
#include "Contributions.hpp"
#include "Picks.hpp"
#include "StateTypes.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// How many bytes of arguments a run may send.
constexpr std::size_t maxArgumentBytes = std::size_t{64} * 1024U;

/// @brief Tells whether a token opens a form.
/// @param kind The token's kind.
/// @return true for '(', '[' and '{'.
bool opensForm(notation::TokenKind kind) {
	using notation::TokenKind;
	return kind == TokenKind::OpenNode || kind == TokenKind::OpenList || kind == TokenKind::OpenMap;
}

/// @brief Tells whether a token closes a form.
/// @param kind The token's kind.
/// @return true for ')', ']' and '}'.
bool closesForm(notation::TokenKind kind) {
	using notation::TokenKind;
	return kind == TokenKind::CloseNode || kind == TokenKind::CloseList || kind == TokenKind::CloseMap;
}

/// @brief Tells whether a token stands before the form it makes: a quote, an unquote, a reader form after '#'.
/// @param kind The token's kind.
/// @return true if so.
bool prefixesForm(notation::TokenKind kind) {
	using notation::TokenKind;
	return kind == TokenKind::Quote || kind == TokenKind::Backquote || kind == TokenKind::Unquote ||
	       kind == TokenKind::Splice || kind == TokenKind::Lambda || kind == TokenKind::SetOf ||
	       kind == TokenKind::Conditional || kind == TokenKind::ConditionalSplice;
}

/// @brief Skips one form of a definition's text, the token that starts it already read (a prefix's form with it).
/// @param lexer The lexer.
/// @param first The form's first token.
/// @return false if the text ends first.
bool skipForm(notation::Lexer& lexer, const notation::Token& first) {
	if(prefixesForm(first.kind)) return skipForm(lexer, lexer.next()); // ~(t "title"): the form after it
	if(!opensForm(first.kind)) return first.kind != notation::TokenKind::End;
	for(int depth = 1; depth > 0;) {
		const notation::Token token = lexer.next();
		if(token.kind == notation::TokenKind::End) return false;
		if(opensForm(token.kind)) ++depth;
		else if(closesForm(token.kind)) --depth;
	}
	return true;
}

/// @brief The parameters of a function's definition, read from its text: `(defn name "doc" :key value [a b & more]
/// ...)`. One that destructures, or has several arities, isn't read.
/// @param text The definition's text.
/// @return The names, or std::nullopt if they can't be read.
std::optional<std::vector<std::string>> parametersOf(std::string_view text) {
	using notation::TokenKind;
	try {
		notation::Lexer lexer(text, true);
		if(lexer.next().kind != TokenKind::OpenNode || lexer.next().kind != TokenKind::Word ||
		   lexer.next().kind != TokenKind::Word)
			return std::nullopt;
		notation::Token token = lexer.next();
		while(token.kind != TokenKind::OpenList) { // a docstring, options
			if(token.kind == TokenKind::Key && !skipForm(lexer, lexer.next())) return std::nullopt;
			if(token.kind != TokenKind::Key && token.kind != TokenKind::String) return std::nullopt;
			token = lexer.next();
		}
		std::vector<std::string> names;
		for(token = lexer.next(); token.kind != TokenKind::CloseList; token = lexer.next()) {
			if(token.kind != TokenKind::Word) return std::nullopt;
			if(token.text != "&") names.emplace_back(token.text);
		}
		return names;
	} catch(const DocumentError&) {
		return std::nullopt;
	}
}

/// @brief The text of a program's definition.
/// @param document The document.
/// @param name The definition's name.
/// @return Its text, or nullptr.
const std::string* definitionText(const json& document, const std::string& name) {
	const auto program = document.find("program");
	if(program == document.end() || !program->is_object()) return nullptr;
	for(const auto& [file, entries] : program->items()) {
		if(!entries.is_object()) continue;
		const auto text = entries.find(name);
		if(text != entries.end() && text->is_string()) return &text->get_ref<const std::string&>();
	}
	return nullptr;
}

/// @brief The function of the program a command's `do` runs: `[{"lisp": f}]`, or the name of a function that no
/// action of the document has.
/// @param document The document.
/// @param command The command.
/// @return Its name, or "" if it runs none or more than a function.
std::string functionRun(const json& document, const json& command) {
	const auto run = command.find("do");
	if(run == command.end()) return "";
	if(run->is_string()) {
		const std::string& name = run->get_ref<const std::string&>();
		const bool action = document.contains("actions") && document["actions"].contains(name);
		return action ? "" : name;
	}
	const json* step = run->is_array() && run->size() == 1 ? &(*run)[0] : &*run;
	if(!step->is_object() || step->size() != 1 || !step->contains("lisp") || !(*step)["lisp"].is_string()) return "";
	return (*step)["lisp"].get<std::string>();
}

/// @brief Names a list of texts: "card, to", or "none".
/// @param names The texts.
/// @return The words.
std::string listed(const std::vector<std::string>& names) {
	std::string words;
	for(const std::string& name : names) words += (words.empty() ? "" : ", ") + name;
	return words.empty() ? std::string("none") : words;
}

/// @brief Checks that a command's typed arguments are its function's parameters.
/// @param document The document.
/// @param name The command.
/// @param command The command's value.
/// @throws DocumentError If one isn't.
void checkParameters(const json& document, const std::string& name, const json& command) {
	const auto arguments = command.find("arguments");
	if(arguments == command.end() || !arguments->is_object()) return;
	const std::string function = functionRun(document, command);
	const std::string* text = function.empty() ? nullptr : definitionText(document, function);
	const std::optional<std::vector<std::string>> parameters = text ? parametersOf(*text) : std::nullopt;
	if(!parameters) return;
	for(const auto& [argument, type] : arguments->items())
		if(std::ranges::find(*parameters, argument) == parameters->end())
			expect::fail(std::format("actions.{}.arguments.{}", name, argument),
			             std::format("the function '{}' has no parameter '{}' (it takes {})", function, argument,
			                         listed(*parameters)));
}

/// @brief What a mismatch says, for a run: the problem without the value sent, an enumeration of texts unquoted.
/// @param mismatch The mismatch.
/// @param type The argument's type.
/// @param argument The argument's name.
/// @return "<place>: <problem>".
std::string mismatchText(const Mismatch& mismatch, const json& type, const std::string& argument) {
	const std::string place = cutToCharacter(mismatch.place, 128);
	const auto values = type.is_object() ? type.find("enum") : type.end();
	const bool texts = type.is_object() && values != type.end() &&
	                   std::ranges::all_of(*values, [](const json& value) { return value.is_string(); });
	if(mismatch.place != argument || !texts || !mismatch.problem.starts_with("expected one of"))
		return place + ": " + cutToCharacter(mismatch.problemWithoutValue(), 400);
	std::vector<std::string> names;
	for(const json& value : *values) names.push_back(cutToCharacter(value.get<std::string>(), 64));
	if(names.size() > 16) names.resize(16);
	return place + ": expected one of " + cutToCharacter(listed(names), 400);
}

/// @brief Checks the names a run sends: every one an argument the command declares.
/// @param name The command.
/// @param types Its arguments' types.
/// @param arguments The arguments sent.
/// @return Why they are refused, or std::nullopt.
std::optional<std::string> checkNames(const std::string& name, const json& types, const json& arguments) {
	for(const auto& [key, value] : arguments.items()) {
		if(types.contains(key)) continue;
		std::vector<std::string> names;
		for(const auto& [argument, type] : types.items()) names.push_back(argument);
		const std::string which =
		    isArgumentName(key) ? std::format("no argument '{}'", key) : "no argument by that name";
		return std::format("{}: {} (it takes {})", name, which, listed(names));
	}
	return std::nullopt;
}

} // namespace

bool isArgumentName(std::string_view name) noexcept {
	return !name.empty() && name.size() <= maxArgumentNameBytes && std::ranges::all_of(name, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	});
}

void checkArgumentDeclarations(const json& arguments, const std::string& place) {
	if(arguments.is_array()) {
		for(std::size_t i = 0; i < arguments.size(); ++i)
			static_cast<void>(expect::string(arguments[i], place + "[" + std::to_string(i) + "]"));
		return;
	}
	if(!arguments.is_object()) expect::fail(place, "expected a list of names, or an object of types, one per argument");
	if(arguments.size() > maxCommandArguments) expect::fail(place, "a command takes at most 32 arguments");
	for(const auto& [name, type] : arguments.items()) {
		const std::string at = place + "." + cutToCharacter(name, 80);
		if(!isArgumentName(name)) expect::fail(at, "an argument's name is 1 to 64 letters, digits, '_' or '-'");
		checkType(type, at);
		if(type.is_object() && type.contains("default"))
			if(const std::optional<Mismatch> mismatch = typeMismatch(type["default"], type, at + ".default"))
				expect::fail(mismatch->place, mismatch->problem);
	}
}

void checkCommandArgumentsOf(const json& document) {
	const auto actions = document.find("actions");
	if(actions == document.end() || !actions->is_object()) return;
	for(const auto& [name, action] : actions->items()) {
		if(!isCommand(action) || !action.contains("arguments")) continue;
		checkArgumentDeclarations(action["arguments"], "actions." + name + ".arguments");
		checkParameters(document, name, action);
	}
}

json argumentTypes(const json& declared) {
	if(declared.is_object()) return declared;
	json types = json::object();
	if(declared.is_array())
		for(const json& name : declared)
			if(name.is_string()) types[name.get<std::string>()] = json::object();
	return types;
}

std::optional<std::string> checkCommandArguments(const json& command, json& arguments) {
	const std::string name = cutToCharacter(command.value("name", std::string()), 80);
	const json types = argumentTypes(command.value("arguments", json()));
	if(!arguments.is_object()) {
		const std::string like = types.empty() ? "{}" : "{\"" + types.begin().key() + "\": \"…\"}";
		return name + ": arguments must be an object, like " + like;
	}
	if(arguments.dump().size() > maxArgumentBytes) return name + ": the arguments are larger than 64 KiB";
	if(std::optional<std::string> refused = checkNames(name, types, arguments)) return refused;
	for(const auto& [argument, type] : types.items()) {
		if(const auto value = arguments.find(argument); value != arguments.end()) {
			if(const std::optional<Mismatch> mismatch = typeMismatch(*value, type, argument))
				return name + ": " + mismatchText(*mismatch, type, argument);
		} else if(type.is_object() && type.contains("default")) {
			arguments[argument] = type["default"];
		} else if(type == true || (type.is_object() && (allowedKinds(type) & bit(Kind::Null)) != 0)) {
			arguments[argument] = nullptr;
		} else {
			return std::format("{}: {} is missing", name, argument);
		}
	}
	return std::nullopt;
}

} // namespace affresco::format
