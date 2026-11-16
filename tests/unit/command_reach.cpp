/// @file
/// @brief Tests of the commands as tools: what a command reaches (CommandReach), in JSON and in Lisp.

#include "CommandReach.hpp"

#include <affresco/core/Services.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using affresco::CommandReach;
using affresco::json;
namespace services = affresco::services;

/// @brief A host's services, by hand: `files.read` reads and `files.write` writes.
class FileServices final : public services::Services {
public:
	FileServices() {
		specs_["read"] = {.name = "read", .effect = services::Effect::Read, .arguments = {}, .about = "reads a file"};
		specs_["write"] = {
		    .name = "write", .effect = services::Effect::Write, .arguments = {}, .about = "writes a file"};
	}
	/// @copydoc Services::load
	[[nodiscard]] std::vector<std::string> load(const json& /*section*/) override { return {}; }
	/// @copydoc Services::method
	[[nodiscard]] const services::MethodSpec* method(std::string_view service, std::string_view name) const override {
		if(service != "files") return nullptr;
		const auto it = specs_.find(std::string(name));
		return it == specs_.end() ? nullptr : &it->second;
	}
	/// @copydoc Services::invoke
	[[nodiscard]] std::optional<std::string> invoke(std::string_view /*service*/, std::string_view /*name*/,
	                                                std::shared_ptr<services::Call> /*call*/) override {
		return "not here";
	}
	/// @copydoc Services::subscribe
	[[nodiscard]] std::optional<std::string> subscribe(std::string_view /*service*/, std::string_view /*stream*/,
	                                                   std::shared_ptr<services::Call> /*call*/) override {
		return "no streams";
	}
	/// @copydoc Services::status
	[[nodiscard]] json status() const override { return json::object(); }
	/// @copydoc Services::onStatus
	void onStatus(std::function<void()> /*callback*/) override {}
	/// @copydoc Services::onRequest
	void onRequest(services::RequestHandler /*handler*/) override {}

private:
	std::map<std::string, services::MethodSpec> specs_; ///< The methods of `files`, by name.
};

/// @brief What a command of a JSON screen reaches.
/// @param actions The screen's actions.
/// @param name The command.
/// @return Its reach.
CommandReach jsonReach(const json& actions, const std::string& name) {
	const FileServices files;
	const json document = {{"format", "affrescoui-screen"},
	                       {"version", 2},
	                       {"state", {{"which", "a"}}},
	                       {"actions", actions},
	                       {"root", {{"type", "column"}, {"id", "root"}}}};
	return affresco::format::reachOf(document, name, &files);
}

/// @brief What a command of a Lisp program reaches, the program built as a screen builds it.
/// @param program The program, with no screen: one is added.
/// @param name The command.
/// @return Its reach.
CommandReach lispReach(const std::string& program, const std::string& name) {
	const FileServices files;
	const affresco::format::Rules rules;
	const affresco::vcs::Repository repository(
	    rules, affresco::lisp::sourceDocument(program + "\n(defscreen main (column#root))\n", "app.lisp"));
	return affresco::format::reachOf(repository.document(), name, &files);
}

/// @brief A command of steps.
/// @param steps Its steps.
/// @return The command.
json command(const json& steps) { return {{"title", "Go"}, {"do", steps}}; }

} // namespace

TEST_CASE("format: a command that reaches nothing outside the screen") {
	const CommandReach reach =
	    jsonReach({{"go", command(json::parse(R"([{"set": "state.which", "to": "b"}])"))}}, "go");
	CHECK_FALSE(reach.control);
	CHECK(reach.controlStep.empty());
	CHECK(reach.personUses.empty());
	CHECK_FALSE(reach.writesRecords);
	CHECK_FALSE(lispReach(R"lisp((defcommand go "Goes." :title "Go" [] (set "state.which" "b")))lisp", "go").control);
}

TEST_CASE("format: a command that holds a control step, directly or through run") {
	const json actions = {{"go", command(json::parse(R"([{"ask": "hello"}])"))},
	                      {"helper", json::parse(R"([{"set": "state.which", "to": "b"}, {"undo": true}])")},
	                      {"through", command(json::parse(R"([{"run": "helper"}])"))}};
	const CommandReach direct = jsonReach(actions, "go");
	CHECK(direct.control);
	CHECK(direct.controlStep == "ask");
	const CommandReach through = jsonReach(actions, "through");
	CHECK(through.control);
	CHECK(through.controlStep == "undo");
}

TEST_CASE("format: a command that runs a computed name, or an action the document doesn't have") {
	const json actions = {{"computed", command(json::parse(R"([{"run": "{{state.which}}"}])"))},
	                      {"native", command(json::parse(R"([{"run": "openSettings"}])"))}};
	const CommandReach computed = jsonReach(actions, "computed");
	CHECK(computed.control);
	CHECK(computed.controlStep == "a computed run");
	const CommandReach native = jsonReach(actions, "native");
	CHECK(native.control);
	CHECK(native.controlStep == "the application's own action 'openSettings'");
}

TEST_CASE("format: a command that reaches a control step through a Lisp function, advice or a hook") {
	const std::string helpers = R"lisp(
(defn asks "Asks the agents." [] (ask "hello"))
(defn quiet "Does nothing outside." [] (set "state.which" "b"))
(defhook after-go "Runs after going.")
)lisp";
	const CommandReach function = lispReach(helpers + R"lisp((defcommand go "Goes." :title "Go" [] (asks)))lisp", "go");
	CHECK(function.control);
	CHECK(function.controlStep.contains("a control or a review step in the function"));
	const CommandReach advised =
	    lispReach(helpers + R"lisp((defcommand go "Goes." :title "Go" [] (quiet)) (advice go :before asks))lisp", "go");
	CHECK(advised.control);
	const CommandReach hooked = lispReach(
	    helpers + R"lisp((defcommand go "Goes." :title "Go" [] (run-hooks after-go)) (add-hook after-go asks))lisp",
	    "go");
	CHECK(hooked.control);
	CHECK_FALSE(lispReach(helpers + R"lisp((defcommand go "Goes." :title "Go" [] (quiet)))lisp", "go").control);
}

TEST_CASE("format: a command that uses a service lists the uses that write, not those that read") {
	const json actions = {{"read", command(json::parse(R"([{"invoke": "files.read", "with": {"path": "a"}}])"))},
	                      {"write", command(json::parse(R"([{"invoke": "files.write", "with": {"path": "a"}}])"))}};
	const CommandReach reads = jsonReach(actions, "read");
	CHECK_FALSE(reads.control);
	CHECK(reads.personUses.empty());
	const CommandReach writes = jsonReach(actions, "write");
	CHECK_FALSE(writes.control);
	CHECK(writes.personUses == std::vector<std::string>{"uses files.write (write: writes a file)"});
	const CommandReach lisp = lispReach(
	    R"lisp((defn save "Saves." [path] (invoke "files.write" :with {:path path}))
(defcommand go "Goes." :title "Go" [] (save "a") (invoke "files.read" :with {:path "a"})))lisp",
	    "go");
	CHECK_FALSE(lisp.control);
	CHECK(lisp.personUses == writes.personUses);
}

TEST_CASE("format: an attack: a command reaches the action a step names in onDone, onError, then, else or onEvent") {
	// A handler given as text runs the action of that name: what it uses is the command's to show, as `run` is
	const json save = json::parse(R"([{"invoke": "files.write", "with": {"path": "a"}}])");
	for(const char* key : {"onDone", "onError", "then", "else", "onEvent"}) {
		CAPTURE(key);
		json read = json::parse(R"({"invoke": "files.read", "with": {"path": "a"}})");
		read[key] = "save";
		const CommandReach reach = jsonReach({{"save", save}, {"go", command(json::array({read}))}}, "go");
		CHECK(reach.personUses == std::vector<std::string>{"uses files.write (write: writes a file)"});
	}
}

TEST_CASE("format: a command that writes records") {
	const CommandReach reach =
	    jsonReach({{"go", command(json::parse(R"([{"insert": "cards", "value": {"title": "x"}}])"))}}, "go");
	CHECK(reach.writesRecords);
	CHECK_FALSE(reach.control);
}

TEST_CASE("format: a command of a program that doesn't build reaches control") {
	json document = affresco::lisp::sourceDocument(
	    R"lisp((defcommand go "Goes." :title "Go" [] 1)
(defscreen main (column#root))
)lisp",
	    "app.lisp");
	document["program"]["app.lisp"]["go"] = R"((defcommand go "Goes." :title "Go" [] ()";
	const CommandReach reach = affresco::format::reachOf(document, "go", nullptr);
	CHECK(reach.control);
	CHECK(reach.controlStep.starts_with("the program can't be built"));
}
