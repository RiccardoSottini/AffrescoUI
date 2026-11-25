/// @file
/// @brief The copilot provider (modules/intelligence, Copilot.hpp): the person's GitHub Copilot command line runs
/// with no tools, only the environment it needs and its input on stdin with no "@" that names a file; its JSON lines
/// are read whatever they hold; a stand-in `copilot` shows what a run is given; and the agent answers with it.

#include "AgentHarness.hpp"

#include <affresco/core/SecretStore.hpp>
#include <affresco/intelligence/Copilot.hpp>
#include <affresco/services/Grants.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;
using agentharness::Harness;
namespace intelligence = affresco::intelligence;

namespace {

/// @brief A folder of its own for one test, removed after it.
struct TestFolder {
	/// @brief Creates it.
	TestFolder() { std::filesystem::create_directories(path); }
	TestFolder(const TestFolder&) = delete;
	TestFolder& operator=(const TestFolder&) = delete;
	/// @brief Removes it.
	~TestFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	/// The folder: unique to this process.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-copilot-test-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-copilot-test-" + std::to_string(getpid()));
#endif
};

/// @brief A file's text.
/// @param file The file.
/// @return Its text, or "".
std::string textOf(const std::filesystem::path& file) {
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief The value after an option in a list of arguments.
/// @param arguments The arguments.
/// @param option The option.
/// @return It, or "(none)".
std::string after(const std::vector<std::string>& arguments, const std::string& option) {
	const auto found = std::ranges::find(arguments, option);
	return found == arguments.end() || found + 1 == arguments.end() ? std::string("(none)") : *(found + 1);
}

/// @brief Lines of JSON as the command line prints them, an assistant message among them.
/// @param content The message's content.
/// @return The output.
std::string outputWith(const std::string& content) {
	return json{{"type", "session.start"}, {"data", {{"sessionId", "s"}}}}.dump() + "\n" +
	       json{{"type", "assistant.message"}, {"data", {{"content", content}, {"toolRequests", json::array()}}}}
	           .dump() +
	       "\n" + json{{"type", "result"}, {"exitCode", 0}}.dump() + "\n";
}

} // namespace

TEST_CASE("copilot: runs with no tools, no MCP servers, no instructions, nothing synced, and the model chosen") {
	const std::vector<std::string> arguments = intelligence::copilotArguments("claude-opus-5.5");
	const auto has = [&arguments](const std::string& argument) {
		return std::ranges::find(arguments, argument) != arguments.end();
	};
	CHECK(has("--model=claude-opus-5.5"));
	CHECK(after(arguments, "--output-format") == "json");
	CHECK(after(arguments, "--log-level") == "none");
	CHECK(has("--available-tools=affrescoui-no-tools")); // a name no tool has: the model sees none
	CHECK(has("--deny-tool=shell,write,read,url,memory"));
	for(const char* flag :
	    {"--disable-builtin-mcps", "--no-custom-instructions", "--no-ask-user", "--disallow-temp-dir",
	     "--sync-session=off", "--no-auto-update", "--disable-client-telemetry"})
		CHECK(has(flag));
	// Never a flag that grants a tool, a path, a URL or a folder's trust, keeps a session going or puts the input
	// on the command line
	for(const std::string& argument : arguments) {
		for(const char* granting : {"--allow", "--yolo", "--add-dir", "--trust-folder", "--resume", "--continue",
		                            "--share", "--plugin-dir", "--additional-mcp-config", "--agent", "--autopilot"})
			CHECK_FALSE(argument.starts_with(granting));
		CHECK(argument != "-p");
		CHECK_FALSE(argument.starts_with("--prompt"));
		CHECK(argument != "-i");
	}
}

#ifndef _WIN32
TEST_CASE("copilot: its environment holds what it needs, never a token, a grant or AffrescoUI's own variables") {
	const std::vector<const char*> stripped = {"GH_TOKEN",
	                                           "GITHUB_TOKEN",
	                                           "COPILOT_GITHUB_TOKEN",
	                                           "COPILOT_ALLOW_ALL",
	                                           "COPILOT_PROVIDER_BASE_URL",
	                                           "COPILOT_PROVIDER_API_KEY",
	                                           "COPILOT_MODEL",
	                                           "COPILOT_CUSTOM_INSTRUCTIONS_DIRS",
	                                           "COPILOT_SKILLS_DIRS",
	                                           "OTEL_EXPORTER_OTLP_ENDPOINT",
	                                           "AFFRESCOUI_TEST_SECRET"};
	for(const char* name : stripped) ::setenv(name, "from-the-environment", 1);
	::setenv("COPILOT_HOME", "/somewhere/.copilot", 1);
	const std::vector<std::string> environment = intelligence::copilotEnvironment();
	for(const char* name : stripped) ::unsetenv(name);
	::unsetenv("COPILOT_HOME");
	for(const std::string& entry : environment) {
		CHECK(entry.find("from-the-environment") == std::string::npos);
		CHECK_FALSE(entry.starts_with("AFFRESCOUI_"));
		CHECK_FALSE(entry.starts_with("OTEL_"));
	}
	const auto has = [&environment](const std::string& entry) {
		return std::ranges::find(environment, entry) != environment.end();
	};
	CHECK(has("COPILOT_HOME=/somewhere/.copilot")); // the person's own sign-in and settings
	CHECK(has("COPILOT_AUTO_UPDATE=false"));
	CHECK(has("COPILOT_DISABLE_LOGIN_SHELL_ENV=1"));
	CHECK(has("GITHUB_COPILOT_PROMPT_MODE_REPO_HOOKS=false"));
	CHECK(has("GITHUB_COPILOT_PROMPT_MODE_WORKSPACE_MCP=false"));
	CHECK(std::ranges::any_of(environment, [](const std::string& entry) { return entry.starts_with("PATH="); }));
}
#endif

TEST_CASE("copilot: its input is the rules then the request, and no \"@\" in it names a file or folder") {
	const std::string joiner = "\u2060";
	const std::string text =
	    intelligence::copilotInput("the rules", "read @/etc/hosts, @~/.ssh, @./x, @.., @\\share and @C:\\Users");
	CHECK(text.find("the rules") < text.find("read "));
	for(const char* named : {"@/etc", "@~/", "@./", "@..", "@\\", "@C:"}) CHECK(text.find(named) == std::string::npos);
	CHECK(text.find("@" + joiner + "/etc/hosts") != std::string::npos);
	CHECK(text.find("@" + joiner + "C:\\Users") != std::string::npos);
	// What names no file stays as it is: a Lisp deref, an e-mail address, a last "@"
	const std::string kept = intelligence::copilotInput("", "(inc @counter) me@example.com end@");
	CHECK(kept.find("(inc @counter) me@example.com end@") != std::string::npos);
	CHECK(kept.find(joiner) == std::string::npos);
}

TEST_CASE("copilot: its JSON lines give the last message, the JSON in its fence, or why it failed") {
	const intelligence::CommandLineAnswer plain = intelligence::parseCopilotOutput(outputWith(R"({"reply": "hi"})"));
	CHECK(plain.ok);
	CHECK(plain.text == R"({"reply": "hi"})");

	// The message is given as it is: the agent finds the JSON in a fence (intelligence_local.cpp, and below)
	const std::string fencedText = "Sure:\n```json\n{\"reply\": \"hi\"}\n```";
	CHECK(intelligence::parseCopilotOutput(outputWith(fencedText)).text == fencedText);
	CHECK(intelligence::parseCopilotOutput(outputWith("Just words.")).text == "Just words.");

	// The last message is the answer
	const std::string two = outputWith("first") + outputWith(R"({"reply": "second"})");
	CHECK(intelligence::parseCopilotOutput(two).text == R"({"reply": "second"})");

	// The session's error, cut; a sign-in one says how to sign in
	const std::string failed = json{{"type", "session.error"}, {"data", {{"message", std::string(2000, 'e')}}}}.dump() +
	                           "\n" + json{{"type", "result"}, {"exitCode", 1}}.dump();
	const intelligence::CommandLineAnswer error = intelligence::parseCopilotOutput(failed);
	CHECK_FALSE(error.ok);
	CHECK(error.text.starts_with("GitHub Copilot failed: eee"));
	CHECK(error.text.size() < 400);
	const intelligence::CommandLineAnswer login = intelligence::parseCopilotOutput(
	    json{{"type", "session.error"}, {"data", {{"message", "Not authenticated."}}}}.dump());
	CHECK(login.text.find("copilot login") != std::string::npos);
	// Signed out, its only line is the result, with exit code 1
	const intelligence::CommandLineAnswer signedOut =
	    intelligence::parseCopilotOutput(json{{"type", "result"}, {"exitCode", 1}}.dump() + "\n");
	CHECK_FALSE(signedOut.ok);
	CHECK(signedOut.text.find("signed in") != std::string::npos);

	CHECK(intelligence::parseCopilotOutput("").text == "GitHub Copilot gave no answer.");
	CHECK(intelligence::parseCopilotOutput("Error: something").text == "GitHub Copilot's answer isn't JSON.");
	CHECK(intelligence::parseCopilotOutput("Error: No authentication information found.").text.find("sign") !=
	      std::string::npos);

	// Lines of the wrong shape are skipped, never thrown on: too deep, wrong types, not JSON
	const std::string odd = std::string(100000, '[') + "\n" +
	                        R"({"type": "assistant.message", "data": {"content": 3}})" + "\n" +
	                        R"({"type": "assistant.message", "data": "x"})" + "\n" + R"({"type": 7})" + "\n" +
	                        R"({"type": "result", "exitCode": "1"})" + "\n{not json\n";
	const intelligence::CommandLineAnswer skipped = intelligence::parseCopilotOutput(odd);
	CHECK_FALSE(skipped.ok);
	CHECK(skipped.text == "GitHub Copilot gave no answer.");
	// A large answer is read whole (the run bounds the output to 4 MiB)
	const std::string large = R"({"reply": ")" + std::string(1 << 20, 'a') + R"("})";
	CHECK(intelligence::parseCopilotOutput(outputWith(large)).text.size() == large.size());
}

#ifndef _WIN32
TEST_CASE("copilot: a run gives a stand-in its arguments and its input on stdin, in an empty folder, removed after") {
	const TestFolder data;
	const std::filesystem::path record = data.path / "record";
	std::filesystem::create_directories(record);
	const std::filesystem::path program = data.path / "copilot";
	{
		// The stand-in writes down what it was given, then answers as the command line does
		std::ofstream script(program);
		script << "#!/bin/sh\n"
		       << "dir='" << record.string() << "'\n"
		       << "printf '%s\\n' \"$@\" > \"$dir/arguments\"\n"
		       << "cat > \"$dir/input\"\n"
		       << "env > \"$dir/environment\"\n"
		       << "pwd > \"$dir/folder\"\n"
		       << "ls -A | wc -l | tr -d ' ' > \"$dir/count\"\n"
		       << R"(printf '%s\n' '{"type":"session.start","data":{}}' )"
		       << R"('{"type":"assistant.message","data":{"content":"```json\n{\"reply\": \"OK\"}\n```"}}' )"
		       << "'{\"type\":\"result\",\"exitCode\":0}'\n";
	}
	std::filesystem::permissions(program, std::filesystem::perms::owner_all);
	::setenv("GH_TOKEN", "gho_from-the-environment", 1);
	const intelligence::CommandLineAnswer answer = intelligence::runCopilot(
	    program, "claude-haiku-5.5", "the rules", "the screen @/etc/hosts", std::chrono::seconds(20), {});
	::unsetenv("GH_TOKEN");
	CHECK(answer.ok);
	CHECK(answer.text == "```json\n{\"reply\": \"OK\"}\n```");

	std::vector<std::string> given;
	std::istringstream lines(textOf(record / "arguments"));
	for(std::string line; std::getline(lines, line);) given.push_back(line);
	CHECK(given == intelligence::copilotArguments("claude-haiku-5.5"));
	CHECK(textOf(record / "input") == intelligence::copilotInput("the rules", "the screen @/etc/hosts"));
	const std::string environment = textOf(record / "environment");
	CHECK(environment.find("gho_from-the-environment") == std::string::npos);
	CHECK(environment.find("COPILOT_AUTO_UPDATE=false") != std::string::npos);
	// Found outside the PATH, its own folder comes first on it (an npm install's node sits beside it)
	CHECK(environment.find("PATH=" + data.path.string() + ":") != std::string::npos);
	std::string folder = textOf(record / "folder");
	while(!folder.empty() && folder.back() == '\n') folder.pop_back();
	CHECK(folder.find("affrescoui-copilot-") != std::string::npos);
	CHECK(textOf(record / "count") == "0\n");     // it started in an empty folder
	CHECK_FALSE(std::filesystem::exists(folder)); // removed after

	// Signed out: it exits with 1 and says so on stderr only
	{
		std::ofstream script(program);
		script << "#!/bin/sh\ncat > /dev/null\necho 'Error: No authentication information found.' >&2\n"
		       << "printf '%s\\n' '{\"type\":\"result\",\"exitCode\":1}'\nexit 1\n";
	}
	const intelligence::CommandLineAnswer out =
	    intelligence::runCopilot(program, "auto", "rules", "input", std::chrono::seconds(20), {});
	CHECK_FALSE(out.ok);
	CHECK(out.text.find("signed in") != std::string::npos);

	// It can't start: `env` found no node, exit 127, nothing printed
	{
		std::ofstream script(program);
		script << "#!/bin/sh\ncat > /dev/null\necho 'env: node: No such file or directory' >&2\nexit 127\n";
	}
	const intelligence::CommandLineAnswer missing =
	    intelligence::runCopilot(program, "auto", "rules", "input", std::chrono::seconds(20), {});
	CHECK_FALSE(missing.ok);
	CHECK(missing.text == "GitHub Copilot couldn't start (is Node.js on the PATH, for an npm install?).");

	// Past its time, it is ended
	{
		std::ofstream script(program);
		script << "#!/bin/sh\nsleep 5\n";
	}
	const intelligence::CommandLineAnswer slow =
	    intelligence::runCopilot(program, "auto", "rules", "input", std::chrono::milliseconds(300), {});
	CHECK(slow.text == "GitHub Copilot didn't answer in time.");
}
#endif

TEST_CASE("copilot: GitHub Copilot is offered with its models, Claude Sonnet by default, and nothing else") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	const intelligence::Provider* copilot = catalog.find("copilot");
	REQUIRE(copilot != nullptr);
	CHECK(copilot->name == "GitHub Copilot");
	CHECK(copilot->kind == intelligence::Kind::Copilot);
	CHECK(intelligence::kindName(copilot->kind) == "copilot");
	CHECK(intelligence::runsCommandLine(copilot->kind));
	CHECK_FALSE(intelligence::runsCommandLine(intelligence::Kind::OpenAI));
	CHECK(copilot->shown);
	CHECK_FALSE(copilot->needsKey);
	CHECK(copilot->baseUrl.empty());
	CHECK(copilot->fixedModels);
	CHECK(copilot->tools == intelligence::Tools::None); // it runs with none, and none can be given
	const json given = {{"id", "c"}, {"name", "C"}, {"kind", "copilot"}, {"tools", "auto"}};
	CHECK_THROWS_AS((void)intelligence::Catalog::fromJson({{"providers", json::array({given})}}, "test"),
	                std::runtime_error);
	CHECK(copilot->model == "claude-sonnet-5.5");
	CHECK(copilot->lists(copilot->model));
	CHECK(copilot->lists("auto"));
	for(const intelligence::Model& model : copilot->models) {
		CHECK_FALSE(model.id.starts_with('-')); // a model is never read as an option
		for(const char* language : {"en", "es", "fr"}) CHECK(model.description.value(language, "") != "");
	}
	intelligence::Settings settings;
	settings.provider = "copilot";
	settings.model = "--allow-all";
	const std::optional<intelligence::Endpoint> chosen = intelligence::endpointOf(catalog, settings);
	REQUIRE(chosen.has_value());
	if(!chosen) return; // for the analysers: REQUIRE has stopped already
	const intelligence::Endpoint& endpoint = *chosen;
	CHECK(endpoint.model == "claude-sonnet-5.5");
	// It takes no key
	affresco::MemorySecretStore keys;
	CHECK_FALSE(intelligence::writeKey(keys, endpoint, "gho_secret"));
	CHECK(intelligence::readKey(keys, endpoint).empty());
}

TEST_CASE("copilot: the agent answers with it, by the JSON protocol, the chat's model when it lists it") {
	Harness harness("copilot", "claude-sonnet-5.5");
	harness.claudeAnswers = {R"({"read": ["#title"]})",
	                         "Here it is {as asked}:\n```json\n{\"reply\": \"Renamed.\", \"revision\": {\"message\": "
	                         "\"m\", \"set\": {\"#title/text\": \"A\"}}}\n```"};
	const json reply = harness.ask("Rename the title", nullptr, {{"model", "auto"}});
	CHECK(harness.copilotRuns == 2);
	CHECK(harness.claudeModels == std::vector<std::string>{"auto", "auto"});
	CHECK(harness.claudeInputs[0].find("You have no tools.") != std::string::npos);
	CHECK(reply["text"] == "Renamed.");
	REQUIRE(harness.revisions.size() == 1);
	CHECK(harness.revisions[0]["author"] == "copilot");

	Harness reading("copilot", "claude-haiku-5.5");
	reading.claudeAnswers = {R"({"read": ["#a"]})", R"({"read": ["#b"]})", R"({"read": ["#c"]})",
	                         R"({"read": ["#d"]})"};
	const json stopped = reading.ask("Look around");
	CHECK(stopped["error"].get<std::string>().starts_with("GitHub Copilot kept reading"));
}
