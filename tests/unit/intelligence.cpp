/// @file
/// @brief The examples' intelligence (examples/shared/intelligence): the settings file never holds a key and is its
/// user's only, the providers' requests and answers have their APIs' shapes, failures say no secret, the screen
/// queries find what an agent needs, and the settings' natives keep keys in the secret store.

#include "Headless.hpp"

#include "intelligence/ClaudeCode.hpp"
#include "intelligence/Intelligence.hpp"
#include "intelligence/Providers.hpp"
#include "intelligence/ScreenQueries.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

using affresco::json;
namespace intelligence = examples::intelligence;
namespace network = affresco::network;

namespace {

/// @brief A data folder of its own for one test, removed after it.
struct DataFolder {
	/// @brief Creates it.
	DataFolder() { std::filesystem::create_directories(path); }
	DataFolder(const DataFolder&) = delete;
	DataFolder& operator=(const DataFolder&) = delete;
	/// @brief Removes it.
	~DataFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	/// The folder: unique to this process.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-intelligence-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-intelligence-" + std::to_string(getpid()));
#endif
};

/// @brief A file's text.
/// @param file The file.
/// @return Its text, or "".
std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief The catalogue's endpoint of a provider, with a key and a model.
/// @param catalog The catalogue (outlives the endpoint).
/// @param id The provider.
/// @param key Its key.
/// @return The endpoint.
intelligence::Endpoint endpoint(const intelligence::Catalog& catalog, const std::string& id, const std::string& key) {
	intelligence::Endpoint chosen = *intelligence::endpointOf(catalog, {}, id);
	chosen.key = key;
	if(chosen.model.empty()) chosen.model = "llama3.2";
	return chosen;
}

/// @brief A header's value.
/// @param request The request.
/// @param name Its name (lower case).
/// @return Its value, or "".
std::string header(const network::Request& request, const std::string& name) {
	for(const auto& [key, value] : request.headers)
		if(key == name) return value;
	return "";
}

} // namespace

TEST_CASE("intelligence: the catalogue lists the four kinds of provider with their fallback models") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	REQUIRE(catalog.find("anthropic") != nullptr);
	CHECK(catalog.find("anthropic")->kind == intelligence::Kind::Anthropic);
	CHECK(catalog.find("anthropic")->needsKey);
	CHECK(catalog.find("anthropic")->models.size() == 4);
	CHECK(catalog.find("openai")->kind == intelligence::Kind::OpenAI);
	CHECK(catalog.find("openai-compatible")->baseUrl == "http://127.0.0.1:11434/v1");
	CHECK_FALSE(catalog.find("openai-compatible")->needsKey);
	CHECK(catalog.find("claude-code")->kind == intelligence::Kind::ClaudeCode);
	CHECK(catalog.find("nobody") == nullptr);
}

TEST_CASE("intelligence: the settings file round-trips, holds no key, and is its user's only") {
	const DataFolder data;
	intelligence::Settings settings;
	settings.provider = "openai-compatible";
	settings.model = "llama3.2";
	settings.providers["openai-compatible"] = {"http://127.0.0.1:1234/v1", "llama3.2"};
	settings.save(data.path);
	const intelligence::Settings read = intelligence::Settings::load(data.path);
	CHECK(read.provider == "openai-compatible");
	CHECK(read.model == "llama3.2");
	CHECK(read.providers.at("openai-compatible").baseUrl == "http://127.0.0.1:1234/v1");
	const json file = json::parse(readFile(intelligence::Settings::file(data.path)));
	CHECK(file.size() == 3); // provider, model, providers: nothing else, no key
#ifndef _WIN32
	struct stat info{};
	REQUIRE(::stat(intelligence::Settings::file(data.path).c_str(), &info) == 0);
	CHECK((info.st_mode & 0777) == 0600);
#endif
	// A malformed file gives no settings, and unknown fields are dropped
	std::ofstream(intelligence::Settings::file(data.path)) << R"({"provider": 3, "key": "sk-leak"})";
	const intelligence::Settings malformed = intelligence::Settings::load(data.path);
	CHECK(malformed.provider.empty());
	CHECK(malformed.toJson().dump().find("sk-leak") == std::string::npos);
}

TEST_CASE("intelligence: the Messages API request has its headers, tools and body") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	const intelligence::Endpoint anthropic = endpoint(catalog, "anthropic", "sk-ant-test");
	intelligence::Conversation conversation{"the rules", json::array()};
	intelligence::addUser(intelligence::Kind::Anthropic, conversation, "Rename the page");
	const network::Request request = intelligence::chatRequest(
	    anthropic, conversation, {{"get_screen", "reads it", {{"type", "object"}, {"properties", json::object()}}}});
	CHECK(request.method == "POST");
	CHECK(request.url == "https://api.anthropic.com/v1/messages");
	CHECK(header(request, "x-api-key") == "sk-ant-test");
	CHECK(header(request, "anthropic-version") == "2023-06-01");
	CHECK(header(request, "authorization").empty());
	const json body = json::parse(request.body);
	CHECK(body["model"] == "claude-sonnet-5-5");
	CHECK(body["system"] == "the rules");
	CHECK(body["max_tokens"] == intelligence::maxOutputTokens);
	CHECK(body["messages"][0]["role"] == "user");
	CHECK(body["tools"][0]["name"] == "get_screen");
	CHECK(body["tools"][0].contains("input_schema"));

	const network::Request models = intelligence::modelsRequest(anthropic);
	CHECK(models.method == "GET");
	CHECK(models.url == "https://api.anthropic.com/v1/models?limit=100");
}

TEST_CASE("intelligence: a Messages API answer with tool calls, then the results in one user message") {
	const intelligence::Turn turn = intelligence::parseTurn(intelligence::Kind::Anthropic, R"({
		"content": [{"type": "thinking", "thinking": "", "signature": "x"}, {"type": "text", "text": "Looking."},
		            {"type": "tool_use", "id": "toolu_1", "name": "get_screen", "input": {"outline": true}}],
		"stop_reason": "tool_use", "usage": {"output_tokens": 42}})");
	CHECK(turn.text == "Looking.");
	REQUIRE(turn.calls.size() == 1);
	CHECK(turn.calls[0].id == "toolu_1");
	CHECK(turn.calls[0].input["outline"] == true);
	CHECK(turn.stop == "tools");
	CHECK(turn.outputTokens == 42);
	CHECK(turn.message["content"].size() == 3); // kept whole, the thinking block included

	intelligence::Conversation conversation;
	intelligence::addAssistant(conversation, turn);
	intelligence::addResults(intelligence::Kind::Anthropic, conversation, {{"toolu_1", "#root panel", false}});
	REQUIRE(conversation.messages.size() == 2);
	CHECK(conversation.messages[1]["role"] == "user");
	CHECK(conversation.messages[1]["content"][0]["type"] == "tool_result");
	CHECK(conversation.messages[1]["content"][0]["tool_use_id"] == "toolu_1");
	CHECK_THROWS((void)intelligence::parseTurn(intelligence::Kind::Anthropic, R"({"type": "error"})"));
}

TEST_CASE("intelligence: Chat Completions requests for OpenAI and a compatible server, and their answers") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	intelligence::Conversation conversation{"the rules", json::array()};
	intelligence::addUser(intelligence::Kind::OpenAI, conversation, "Hello");

	const network::Request openai =
	    intelligence::chatRequest(endpoint(catalog, "openai", "sk-proj-abc"), conversation, {});
	CHECK(openai.url == "https://api.openai.com/v1/chat/completions");
	CHECK(header(openai, "authorization") == "Bearer sk-proj-abc");
	const json body = json::parse(openai.body);
	CHECK(body["messages"][0]["role"] == "system");
	CHECK(body["messages"][1]["content"] == "Hello");
	CHECK(body.contains("max_completion_tokens"));
	CHECK_FALSE(body.contains("tools"));

	const network::Request local =
	    intelligence::chatRequest(endpoint(catalog, "openai-compatible", ""), conversation, {});
	CHECK(local.url == "http://127.0.0.1:11434/v1/chat/completions");
	CHECK(header(local, "authorization").empty()); // no key, no header
	CHECK(json::parse(local.body).contains("max_tokens"));

	const intelligence::Turn turn = intelligence::parseTurn(intelligence::Kind::OpenAI, R"({"choices": [{"message": {
		"role": "assistant", "content": null, "tool_calls": [{"id": "call_1", "type": "function",
		"function": {"name": "find_nodes", "arguments": "{\"text\": \"Done\"}"}}]}, "finish_reason": "tool_calls"}],
		"usage": {"completion_tokens": 7}})");
	REQUIRE(turn.calls.size() == 1);
	CHECK(turn.calls[0].name == "find_nodes");
	CHECK(turn.calls[0].input["text"] == "Done");
	CHECK(turn.stop == "tools");
	intelligence::addAssistant(conversation, turn);
	intelligence::addResults(intelligence::Kind::OpenAI, conversation, {{"call_1", "1 found", false}});
	CHECK(conversation.messages.back()["role"] == "tool");
	CHECK(conversation.messages.back()["tool_call_id"] == "call_1");
	// Malformed arguments give an empty object, not a crash
	const intelligence::Turn odd = intelligence::parseTurn(intelligence::Kind::OpenAI, R"({"choices": [{"message": {
		"tool_calls": [{"id": "c", "function": {"name": "reply", "arguments": "not json"}}]}}]})");
	CHECK(odd.calls[0].input == json::object());
}

TEST_CASE("intelligence: the models lists, OpenAI's filtered to chat models") {
	const std::vector<intelligence::Model> openai = intelligence::parseModels(
	    intelligence::Kind::OpenAI, R"({"data": [{"id": "gpt-5"}, {"id": "text-embedding-3-large"}, {"id": "whisper-1"},
	                                   {"id": "gpt-5-mini"}, {"id": "dall-e-3"}, {"id": "o3"}]})");
	REQUIRE(openai.size() == 3);
	CHECK(openai[0].id == "gpt-5");
	CHECK(openai[2].id == "o3");
	const std::vector<intelligence::Model> anthropic = intelligence::parseModels(
	    intelligence::Kind::Anthropic, R"({"data": [{"id": "claude-opus-5-5", "display_name": "Claude Opus 5.5"}]})");
	REQUIRE(anthropic.size() == 1);
	CHECK(anthropic[0].name == "Claude Opus 5.5");
	CHECK(intelligence::parseModels(intelligence::Kind::OpenAICompatible, R"({"data": [{"id": "llama3.2"}]})").size() ==
	      1);
	CHECK(intelligence::parseModels(intelligence::Kind::OpenAI, "<html>").empty());
}

TEST_CASE("intelligence: failures say what happened without a secret") {
	const std::string key = "sk-proj-SECRET1234567890";
	const std::string refused = intelligence::describeFailure(
	    {"status", "401", 401, R"({"error": {"message": "Incorrect API key provided: sk-proj-SECRET1234567890"}})"},
	    "OpenAI", key);
	CHECK(refused.find("refused the API key") != std::string::npos);
	CHECK(refused.find("SECRET") == std::string::npos);
	const std::string other = intelligence::describeFailure(
	    {"status", "400", 400,
		 R"({"error": {"message": "bad request for key sk-proj-SECRET1234567890 and sk-other-zzzzzz"}})"},
	    "OpenAI", key);
	CHECK(other.find("400") != std::string::npos);
	CHECK(other.find("SECRET") == std::string::npos);
	CHECK(other.find("sk-other") == std::string::npos);
	CHECK(intelligence::describeFailure({"timeout", "slow", 0, {}}, "Anthropic (Claude)", key).find("in time") !=
	      std::string::npos);
	CHECK(intelligence::redact("a token abcdefghijklmnopqrstuvwxyz0123456789 here", "") == "a token [hidden] here");
	CHECK(intelligence::redact("plain words stay", "") == "plain words stay");
}

TEST_CASE("intelligence: the screen queries give the outline, a node, a search and the guide by topic") {
	const json document =
	    json::parse(R"({"styles": {"title": {"type": "label"}}, "state": {"n": 1}, "root": {"id": "page",
		"children": [{"id": "title", "style": "title", "text": "Tasks"}, {"type": "row", "children": [
			{"id": "add", "type": "label", "text": "Add", "onClick": "add"}, {"type": "textarea", "bind": "state.draft",
			 "placeholder": "New task"}]}]}})");
	const std::string tree = intelligence::outline(document);
	CHECK(tree.find("#page panel") != std::string::npos);
	CHECK(tree.find(" #title label \"Tasks\"") != std::string::npos);
	CHECK(tree.find("#add label \"Add\" onClick=add") != std::string::npos);
	CHECK(intelligence::outline(document, "#page", 0) == "#page panel +4\n");
	CHECK(intelligence::nodeAt(document, "#title")["text"] == "Tasks");
	CHECK(intelligence::nodeAt(document, "#page", 0)["children"][0] == "#title");
	CHECK_THROWS((void)intelligence::nodeAt(document, "#missing"));
	const std::string found = intelligence::findNodes(document, "", "new", "", json(), 20);
	CHECK(found.find("1 found") == 0);
	CHECK(found.find("bind=state.draft") != std::string::npos);
	CHECK(intelligence::findNodes(document, "label", "", "", json(), 20).find("2 found") == 0);

	const std::string guide = "# Format\nIntro.\n## Revisions\nEdits.\n```\n## not a heading\n```\n### label\nText.\n";
	CHECK(intelligence::formatGuide(guide)->find("Topics (pass one as `topic`): Revisions, label.") !=
	      std::string::npos);
	CHECK(intelligence::formatGuide(guide, "revisions")->find("Edits.") != std::string::npos);
	CHECK_FALSE(intelligence::formatGuide(guide, "nothing").has_value());
	CHECK(intelligence::readFormatGuide().find("## Live screens") != std::string::npos);
}

namespace {

/// @brief A screen with the settings' natives, a headless window and actions that call them.
struct SettingsScreen {
	affresco::detail::ManualClock clock;      ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.
	std::shared_ptr<affresco::MemorySecretStore> keys = std::make_shared<affresco::MemorySecretStore>(); ///< Keys.
	int changes = 0; ///< Times the settings changed.

	/// @brief Builds it.
	/// @param data The data folder.
	explicit SettingsScreen(const std::filesystem::path& data)
	    : window(affresco::detail::createHeadlessWindow(400, 300, clock)),
	      screen(affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2,
		"state": {"out": null, "error": ""},
		"actions": {
			"providers": [{"call": "intelligence.providers", "into": "state.out"}],
			"stage": [{"call": "intelligence.stageKey", "with": {"provider": "openai", "key": "{{event.key}}"}}],
			"save": [{"call": "intelligence.save", "with": "{{event}}", "into": "state.out",
			          "onError": [{"set": "state.error", "to": "{{error}}"}]}],
			"forget": [{"call": "intelligence.forget", "with": {"provider": "{{event.provider}}"}, "into": "state.out"}]
		},
		"root": {"id": "page"}})")) {
		intelligence::CallsOptions options;
		options.keys = keys;
		options.claudeInstalled = [] { return false; };
		options.changed = [this] { ++changes; };
		intelligence::registerCalls(screen, data, std::move(options));
		screen.build(*window);
	}

	/// @brief Runs an action with an event.
	/// @param name The action.
	/// @param event Its event.
	/// @return state.out.
	json run(const std::string& name, const json& event = json::object()) {
		screen.runAction(name, window->root(), event.dump());
		window->render();
		return json::parse(screen.stateJson())["out"];
	}

	/// @brief state.error.
	/// @return It.
	[[nodiscard]] std::string error() const { return json::parse(screen.stateJson())["error"].get<std::string>(); }
};

/// @brief A provider of an intelligence.providers answer.
/// @param answer The answer.
/// @param id The provider.
/// @return Its entry.
json providerIn(const json& answer, const std::string& id) {
	for(const json& provider : answer["providers"])
		if(provider["id"] == id) return provider;
	return {};
}

} // namespace

TEST_CASE("intelligence natives: providers, save with a staged key, the key in the store only, forget") {
	const DataFolder data;
	SettingsScreen settings(data.path);
	json answer = settings.run("providers");
	CHECK(answer["current"]["provider"] == "");
	CHECK(answer["keyStore"] == "memory");
	CHECK(providerIn(answer, "openai")["hasKey"] == false);
	CHECK(providerIn(answer, "claude-code")["available"] == false); // the stand-in says it isn't installed
	CHECK(providerIn(answer, "anthropic")["models"].size() == 4);

	settings.run("stage", {{"key", "  sk-proj-typed-1234  "}});
	CHECK(json::parse(settings.screen.stateJson()).dump().find("sk-proj") == std::string::npos); // never in the state
	answer = settings.run("save", {{"provider", "openai"}, {"model", "gpt-5-mini"}});
	CHECK(answer["current"]["provider"] == "openai");
	CHECK(answer["current"]["model"] == "gpt-5-mini");
	CHECK(providerIn(answer, "openai")["hasKey"] == true);
	CHECK(settings.keys->read("intelligence/openai").value_or("") == "sk-proj-typed-1234"); // trimmed, in the store
	CHECK(readFile(intelligence::Settings::file(data.path)).find("sk-proj") == std::string::npos);
	CHECK(settings.screen.stateJson().find("sk-proj") == std::string::npos);
	CHECK(settings.changes == 1);

	// The staged key was used once: saving again keeps the stored one
	settings.run("save", {{"provider", "openai"}});
	CHECK(settings.keys->read("intelligence/openai").value_or("") == "sk-proj-typed-1234");

	answer = settings.run("forget", {{"provider", "openai"}});
	CHECK(providerIn(answer, "openai")["hasKey"] == false);
	CHECK_FALSE(settings.keys->read("intelligence/openai").has_value());
}

TEST_CASE("intelligence natives: a base URL is https or the loopback, and a new one erases the saved key") {
	const DataFolder data;
	SettingsScreen settings(data.path);
	settings.run("save", {{"provider", "openai-compatible"}, {"model", "llama3.2"}, {"key", "local-key-1"}});
	CHECK(settings.keys->read("intelligence/openai-compatible").value_or("") == "local-key-1");

	settings.run("save", {{"provider", "openai-compatible"}, {"baseUrl", "http://example.com/v1"}});
	CHECK(settings.error().find("http:// is only for a server on this computer") != std::string::npos);
	CHECK(settings.keys->read("intelligence/openai-compatible").value_or("") ==
	      "local-key-1"); // a refused save changes nothing

	json answer =
	    settings.run("save", {{"provider", "openai-compatible"}, {"baseUrl", "https://openrouter.ai/api/v1/"}});
	CHECK(providerIn(answer, "openai-compatible")["baseUrl"] == "https://openrouter.ai/api/v1");
	CHECK_FALSE(settings.keys->read("intelligence/openai-compatible").has_value()); // the old server's key is gone

	// The API providers' base URLs can't be changed: their keys go only to their own servers
	answer = settings.run("save", {{"provider", "anthropic"}, {"baseUrl", "https://evil.example.com"}});
	CHECK(providerIn(answer, "anthropic")["baseUrl"] == "https://api.anthropic.com");

	settings.run("save", {{"provider", "nobody"}});
	CHECK(settings.error().find("no such provider") != std::string::npos);
	settings.run("save", {{"provider", "openai"}, {"key", "has spaces inside"}});
	CHECK(settings.error().find("the key has characters a key can't have") != std::string::npos);
}

TEST_CASE("intelligence: claude-code runs with no tools, no customisations and nothing kept, and its answer is read") {
	const std::vector<std::string> arguments = intelligence::claudeArguments("haiku", "the rules");
	const auto after = [&arguments](const std::string& option) {
		const auto found = std::ranges::find(arguments, option);
		return found == arguments.end() || found + 1 == arguments.end() ? std::string("(none)") : *(found + 1);
	};
	CHECK(after("--tools").empty()); // "" disables every tool
	CHECK(after("--model") == "haiku");
	CHECK(after("--permission-mode") == "dontAsk");
	CHECK(after("--system-prompt") == "the rules");
	for(const char* flag : {"-p", "--restricted", "--safe-mode", "--strict-mcp-config", "--no-session-persistence"})
		CHECK(std::ranges::find(arguments, flag) != arguments.end());
	CHECK(std::ranges::find(arguments, "--dangerously-skip-permissions") == arguments.end());

	const intelligence::ClaudeAnswer ok =
	    intelligence::parseClaudeOutput(R"({"type": "result", "is_error": false, "result": "{\"reply\": \"hi\"}"})");
	CHECK(ok.ok);
	CHECK(ok.text == R"({"reply": "hi"})");
	CHECK_FALSE(intelligence::parseClaudeOutput(R"({"is_error": true, "result": "overloaded"})").ok);
	CHECK(intelligence::parseClaudeOutput("Please run /login").text.find("signed in") != std::string::npos);
}
