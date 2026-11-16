/// @file
/// @brief Tests of the commands as tools: the list a peer gets (CommandCatalog), from a JSON app and a Lisp app.

#include "LispApp.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <optional>
#include <string>

namespace {

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::Runtime;
using affresco::vcs::Repository;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

/// @brief A JSON screen on a headless window.
struct JsonApp {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::format::Rules rules;            ///< What revisions are judged by.
	Repository repository;                    ///< The history.
	std::unique_ptr<Runtime> runtime;         ///< The running screen.

	/// @brief Builds a screen.
	/// @param document The document.
	explicit JsonApp(const json& document)
	    : window(createHeadlessWindow(400, 300, clock)), repository(rules, document),
	      runtime(std::make_unique<Runtime>(repository, "")) {
		runtime->build(*window);
		window->render();
	}
};

/// @brief A screen with some actions.
/// @param actions Its `actions`.
/// @return The document.
json screenWith(const json& actions) {
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"state", {{"done", false}}},
	        {"actions", actions},
	        {"root", {{"type", "column"}, {"id", "root"}}}};
}

/// The board's commands in JSON: a typed one, one with a list of names, one with none, and a plain action.
const char* const boardActions = R"json({
	"move": [{ "set": "state.done", "to": true }],
	"move-card": { "title": "Move card", "category": "Board", "description": "Moves a card to another column.",
		"arguments": { "card": { "type": "string", "description": "The card's id" },
		               "to": { "enum": ["todo", "doing", "done"] } },
		"do": "move" },
	"clear-done": { "title": "Clear done", "category": "Board", "description": "Removes the finished cards.",
		"do": [{ "set": "state.done", "to": false }] },
	"rename": { "title": "Rename", "category": "Edit", "description": "Renames a card.", "arguments": ["card", "name"],
		"do": "move" }
})json";

/// The same commands in Lisp.
const char* const boardProgram = R"lisp(
(defstate done false "Whether the work is done.")
(defn move "Moves." [] (set "state.done" true))
(defcommand move-card "Moves a card to another column." :title "Move card" :category "Board"
  :arguments {:card {:type string :description "The card's id"} :to {:enum [todo doing done]}}
  [card to] (move))
(defcommand clear-done "Removes the finished cards." :title "Clear done" :category "Board" []
  (set "state.done" false))
(defcommand rename "Renames a card." :title "Rename" :category "Edit" [card name] (move))
(defscreen main (column#root))
)lisp";

} // namespace

TEST_CASE("format: a command list is the same from a JSON app and a Lisp app, and leaves plain actions out") {
	JsonApp fromJson(screenWith(json::parse(boardActions)));
	const lispapp::App fromLisp(boardProgram);
	const json listed = fromJson.runtime->commandList();
	CHECK(listed == fromLisp.runtime->commandList());
	REQUIRE(listed.size() == 3); // "move" is a plain action, and the Lisp "move" a function: neither is a tool
	CHECK(listed[0]["name"] == "clear-done"); // by category, then title
	CHECK(listed[1]["name"] == "move-card");
	CHECK(listed[2]["name"] == "rename");
	CHECK(listed[1] == json{{"name", "move-card"},
	                        {"title", "Move card"},
	                        {"category", "Board"},
	                        {"description", "Moves a card to another column."},
	                        {"arguments",
	                         {{"card", {{"type", "string"}, {"description", "The card's id"}}},
	                          {"to", {{"enum", {"todo", "doing", "done"}}}}}},
	                        {"enabled", true}});
	CHECK(listed[0]["arguments"] == json::object());                                           // none
	CHECK(listed[2]["arguments"] == json{{"card", json::object()}, {"name", json::object()}}); // names, untyped
	CHECK(fromJson.runtime->command("move-card") == listed[1]);
	CHECK_FALSE(fromJson.runtime->command("move").has_value());
	CHECK_FALSE(fromLisp.runtime->command("move").has_value());
}

TEST_CASE("format: a command entry has its translated title, its key and whether it is enabled") {
	json document = screenWith(json::parse(R"json({
		"save": { "title": "{{t('command.save')}}", "category": "{{t('category.file')}}",
			"description": "{{t('command.saveAbout')}}", "enablement": "{{state.done}}",
			"do": [{ "set": "state.done", "to": false }] }
	})json"));
	document["strings"] = json::parse(R"json({ "default": "it",
		"it": { "command.save": "Salva", "category.file": "File", "command.saveAbout": "Salva il documento." } })json");
	document["keys"] = {{"f5", "save"}};
	JsonApp app(document);
	const json listed = app.runtime->commandList();
	REQUIRE(listed.size() == 1);
	CHECK(listed[0]["title"] == "Salva");
	CHECK(listed[0]["category"] == "File");
	CHECK(listed[0]["description"] == "Salva il documento.");
	CHECK(listed[0]["enabled"] == false);
	CHECK(listed[0]["key"] == "f5");
}

TEST_CASE("format: a command entry's title and description are cut on a character") {
	std::string title(127, 'a');
	title += "è and more"; // "è" is two bytes, from byte 127: it doesn't fit in 128
	const std::string description = std::string(600, 'b');
	JsonApp app(screenWith({{"long", {{"title", title}, {"description", description}, {"do", "clear"}}},
	                        {"clear", json::array({{{"set", "state.done"}, {"to", false}}})}}));
	const json listed = app.runtime->commandList();
	REQUIRE(listed.size() == 1);
	CHECK(listed[0]["title"] == std::string(127, 'a'));
	CHECK(listed[0]["description"].get<std::string>().size() == 512);
}

TEST_CASE("format: a command list holds at most 1,024 commands") {
	json actions = json::object();
	for(int i = 0; i < 1100; ++i)
		actions["c" + std::to_string(i)] = {{"title", "Command " + std::to_string(10000 + i)},
		                                    {"do", json::array({{{"set", "state.done"}, {"to", true}}})}};
	JsonApp app(screenWith(actions));
	const json listed = app.runtime->commandList();
	CHECK(listed.size() == 1024);
	CHECK(listed.back()["name"] == "c1023"); // the first, by title
}

namespace {

/// @brief The error a screen with run rules gives when it is read.
/// @param run Its `permissions.run`.
/// @return The message, or "".
std::string runRulesError(const json& run) {
	json document = screenWith(json::parse(boardActions));
	document["permissions"] = {{"allow", {"studio"}}, {"run", run}};
	try {
		affresco::format::Rules().validate(document);
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "";
}

} // namespace

TEST_CASE("format: a command's run rules are read and checked with the permissions") {
	const json good = json::parse(R"json({"allow": ["agent(move-card)", "agent(card.*)", "agent/helper"],
		"ask": ["agent"], "deny": ["*(clear-done)", "*(*)"]})json");
	CHECK(runRulesError(good).empty());
	json document = screenWith(json::parse(boardActions));
	document["permissions"] = {{"allow", {"studio"}}, {"run", good}};
	const affresco::Permissions permissions = affresco::format::Rules().permissions(document);
	REQUIRE(permissions.run.allow.size() == 3);
	CHECK(permissions.run.allow[0].author == "agent");
	CHECK(permissions.run.allow[0].pattern == "move-card");
	CHECK(permissions.run.allow[1].pattern == "card.*");
	CHECK(permissions.run.allow[2].author == "agent/helper");
	CHECK(permissions.run.allow[2].pattern.empty()); // every command
	REQUIRE(permissions.run.deny.size() == 2);
	CHECK(permissions.run.deny[1].pattern.empty());
	CHECK(permissions.allow.size() == 1); // the change rules don't see run rules

	CHECK(runRulesError({{"allow", {"role:editor"}}})
	          .ends_with("permissions.run: roles aren't supported in run rules yet"));
	CHECK(runRulesError({{"allow", {"agent(/actions)"}}}).contains("run rules name commands, like agent(moveCard)"));
	CHECK(runRulesError({{"allow", {"agent(#list)"}}}).contains("run rules name commands, like agent(moveCard)"));
	CHECK(runRulesError({{"review", {"agent"}}}).contains("permissions.run.review"));
	CHECK(runRulesError({{"allow", {"agent(a*b)"}}}).contains("permissions.run.allow[0]"));
	CHECK(runRulesError({{"allow", {"agent()"}}}).contains("permissions.run.allow[0]"));
	CHECK(runRulesError({{"allow", {std::string(257, 'a')}}}).contains("at most 256 bytes"));
	CHECK(runRulesError({{"allow", json::array({"agent"})}}).empty());
	json many = json::array();
	for(int i = 0; i < 65; ++i) many.push_back("agent(c" + std::to_string(i) + ")");
	CHECK(runRulesError({{"deny", many}}).contains("at most 64 rules"));
}

TEST_CASE("format: a command's run rules can't be changed by a revision") {
	json document = screenWith(json::parse(boardActions));
	document["permissions"] = {{"allow", {"agent"}}, {"run", {{"ask", {"agent"}}}}};
	const affresco::format::Rules rules;
	Repository repository(rules, document);
	const json revision = {{"author", "agent"},
	                       {"message", "m"},
	                       {"set", {{"/permissions/run/allow", json::array({"agent(clear-done)"})}}}};
	const std::optional<std::string> error = repository.commit(revision);
	REQUIRE(error.has_value());
	CHECK(error->contains("permissions"));
	CHECK(rules.permissions(repository.document()).run.allow.empty());
}

TEST_CASE("format: a command the run rules name must be the document's") {
	json document = screenWith(json::parse(boardActions));
	document["permissions"] = {{"run", {{"allow", {"agent(move-card)", "agent(card.*)", "agent(gone)"}}}}};
	std::string all;
	for(const std::string& finding : affresco::format::Rules().findings(document, json::object()))
		all += finding + "\n";
	INFO(all);
	CHECK(all.contains("there is no command 'gone'"));
	CHECK_FALSE(all.contains("no command 'move-card'"));
	CHECK_FALSE(all.contains("card.*"));
}

// NOLINTEND(bugprone-unchecked-optional-access)
