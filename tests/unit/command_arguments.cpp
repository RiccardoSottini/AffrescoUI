/// @file
/// @brief Tests of the commands as tools: a command's typed arguments, as declared and as a peer sends them.

#include "CommandArguments.hpp"
#include "LispApp.hpp"

#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <optional>
#include <string>

namespace {

using affresco::DocumentError;
using affresco::json;
using affresco::format::checkArgumentDeclarations;
using affresco::format::checkCommandArguments;

/// @brief The error a declaration of arguments gives.
/// @param arguments The declaration.
/// @return Its message, or "" if it is fine.
std::string declarationError(const json& arguments) {
	try {
		checkArgumentDeclarations(arguments, "actions.move-card.arguments");
	} catch(const DocumentError& error) {
		return error.what();
	}
	return "";
}

/// @brief What a run's arguments give, against a command's declaration.
/// @param declared The command's arguments.
/// @param arguments What a peer sends; defaults are filled in.
/// @return The refusal, or "".
std::string sent(const json& declared, json& arguments) {
	const json command = {{"name", "move-card"}, {"arguments", affresco::format::argumentTypes(declared)}};
	return checkCommandArguments(command, arguments).value_or("");
}

/// @brief The same, for arguments not needed afterwards.
/// @param declared The command's arguments.
/// @param arguments What a peer sends.
/// @return The refusal, or "".
std::string sentCopy(const json& declared, json arguments) { return sent(declared, arguments); }

/// The board's command's arguments.
const json board = json::parse(R"json({ "card": { "type": "string", "description": "The card's id" },
                                        "to": { "enum": ["todo", "doing", "done"] } })json");

/// @brief A screen with one command.
/// @param command The command.
/// @return The document.
json screenWith(const json& command) {
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"actions", {{"move-card", command}}},
	        {"root", {{"type", "column"}, {"id", "root"}}}};
}

/// @brief The error a document gives when it is read.
/// @param document The document.
/// @return The message, or "".
std::string readError(const json& document) {
	try {
		affresco::format::Rules().validate(document);
	} catch(const DocumentError& error) {
		return error.what();
	}
	return "";
}

} // namespace

TEST_CASE("format: a command declares its arguments with every keyword of a state's type") {
	CHECK(declarationError(json::array({"card", "to"})).empty()); // as before: names
	CHECK(declarationError(json::object()).empty());
	CHECK(declarationError(json::parse(R"json({
		"a": { "type": "string", "minLength": 1, "maxLength": 8, "description": "A text", "default": "x" },
		"b": { "type": ["number", "null"], "minimum": 0, "maximum": 10 },
		"c": { "type": "integer", "examples": [1], "title": "C", "note": "n" },
		"d": { "type": "boolean", "default": false },
		"e": { "type": "array", "items": { "type": "string" }, "minItems": 1, "maxItems": 3 },
		"f": { "type": "object", "properties": { "x": { "type": "number" } }, "required": ["x"],
		       "additionalProperties": false },
		"g": { "enum": ["todo", "doing"] }, "h": { "const": 3 }, "i": true, "j": {}, "k": { "type": "null" }
	})json"))
	          .empty());
}

TEST_CASE("format: a command's argument declarations that are wrong name their place") {
	CHECK(
	    declarationError(json::parse(R"({"card": {"kind": "string"}})")).contains("actions.move-card.arguments.card"));
	CHECK(declarationError(json::parse(R"({"card": {"type": "text"}})")).contains("actions.move-card.arguments.card"));
	CHECK(declarationError(json::parse(R"({"to": {"enum": ["a"], "default": "b"}})"))
	          .contains("actions.move-card.arguments.to.default"));
	CHECK(declarationError(json::parse(R"({"a b": {}})")).contains("1 to 64 letters"));
	CHECK(declarationError(json::object({{"", json::object()}})).contains("1 to 64 letters"));
	CHECK(declarationError(json::object({{std::string(65, 'a'), json::object()}})).contains("1 to 64 letters"));
	CHECK(declarationError(json::object({{std::string(64, 'a'), json::object()}})).empty());
	CHECK(declarationError(json::array({"card", 3})).contains("actions.move-card.arguments[1]"));
	CHECK(declarationError("card").contains("a list of names, or an object of types"));
	json many = json::object();
	for(int i = 0; i < 33; ++i) many["a" + std::to_string(i)] = json::object();
	CHECK(declarationError(many).contains("at most 32 arguments"));
	many.erase("a0");
	CHECK(declarationError(many).empty());
}

TEST_CASE("format: a command's arguments are checked when the document is read") {
	const json good = {{"title", "Move"}, {"arguments", board}, {"do", json::array()}};
	CHECK(readError(screenWith(good)).empty());
	json bad = good;
	bad["arguments"]["to"] = {{"enums", {"todo"}}};
	CHECK(readError(screenWith(bad)).contains("actions.move-card.arguments.to"));
}

TEST_CASE("format: a command's typed arguments that are no parameter of its function are a build error") {
	const auto program = [](const std::string& parameters) {
		return R"lisp((defcommand move-card "Moves a card." :title ~(t "command.moveCard")
  :arguments {:card {:type string} :to {:enum [todo doing done]}}
  )lisp" + parameters +
		       R"lisp( (set "state.x" to))
(defscreen main (column#root))
)lisp";
	};
	const json fine = affresco::lisp::sourceDocument(program("[card to]"), "app.lisp");
	CHECK(readError(fine).empty());
	const json wrong = affresco::lisp::sourceDocument(program("[card column]"), "app.lisp");
	CHECK(readError(wrong).ends_with(
	    "actions.move-card.arguments.to: the function 'move-card' has no parameter 'to' (it takes card, column)"));
	// A command written as data, whose "do" names a function of the program
	const json named = affresco::lisp::sourceDocument(R"lisp((defn move "Moves." [id status] (set "state.x" status))
(defpart commands "The commands." :actions {:go {:title "Go" :arguments {:id {} :where {}} :do "move"}})
(defscreen main (column#root))
)lisp",
	                                                  "app.lisp");
	CHECK(readError(named).contains("actions.go.arguments.where: the function 'move' has no parameter 'where'"));
}

TEST_CASE("format: a command's arguments a peer sends must be an object of the names it declares") {
	CHECK(sentCopy(board, json::array()) == R"(move-card: arguments must be an object, like {"card": "…"})");
	CHECK(sentCopy(board, "card") == R"(move-card: arguments must be an object, like {"card": "…"})");
	CHECK(sentCopy(board, {{"card", "a"}, {"colum", "done"}}) == "move-card: no argument 'colum' (it takes card, to)");
	CHECK(sentCopy(board, {{"card", "a"}, {"a b", 1}}) == "move-card: no argument by that name (it takes card, to)");
	CHECK(sentCopy(board, {{"card", std::string(70 * 1024, 'x')}}).contains("larger than 64 KiB"));
}

TEST_CASE("format: a command's argument that doesn't match its type is refused without its value") {
	CHECK(sentCopy(board, {{"card", "a"}, {"to", "elsewhere"}}) == "move-card: to: expected one of todo, doing, done");
	CHECK(sentCopy(board, {{"card", 42}, {"to", "done"}}) == "move-card: card: expected text");
	const json limits = json::parse(R"({"n": {"type": "integer", "maximum": 3}, "tags": {"type": "array",
		"items": {"type": "string"}}})");
	CHECK(sentCopy(limits, {{"n", 9}, {"tags", json::array()}}) == "move-card: n: expected at most 3");
	CHECK(sentCopy(limits, {{"n", 1}, {"tags", {"a", 2}}}) == "move-card: tags[1]: expected text");
	const std::string secret = "do-not-echo";
	CHECK_FALSE(sentCopy(board, {{"card", "a"}, {"to", secret}}).contains(secret));
}

TEST_CASE("format: a command's missing arguments take their defaults, else null, else are refused") {
	json arguments = {{"card", "a"}};
	CHECK(sent(board, arguments) == "move-card: to is missing"); // an enum of texts allows no null
	const json typed = json::parse(R"({"card": {"type": "string"}, "to": {"enum": ["todo", "done"], "default": "todo"},
		"note": {"type": ["string", "null"]}, "any": {}})");
	arguments = {{"card", "a"}};
	CHECK(sent(typed, arguments).empty());
	CHECK(arguments == json{{"card", "a"}, {"to", "todo"}, {"note", nullptr}, {"any", nullptr}});
	arguments = json::object();
	CHECK(sent(typed, arguments) == "move-card: card is missing");
	arguments = json::object();
	CHECK(sent(json::array({"card", "to"}), arguments).empty()); // names: anything, null when missing
	CHECK(arguments == json{{"card", nullptr}, {"to", nullptr}});
}

TEST_CASE("format: a command with no arguments takes only an empty object") {
	CHECK(sentCopy(json(), json::object()).empty());
	CHECK(sentCopy(json(), {{"card", "a"}}) == "move-card: no argument 'card' (it takes none)");
	CHECK(sentCopy(json(), json::array()) == "move-card: arguments must be an object, like {}");
}
