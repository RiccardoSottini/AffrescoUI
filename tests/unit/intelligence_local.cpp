/// @file
/// @brief The Local LLM provider (modules/intelligence, providers.json's "local"): its presets fill the base URL, the
/// models come live from the server (no server and no model are told apart), and a model without tool calling still
/// works: a server that refuses tools, or a model that calls none, goes on with the JSON answers claude-code gives,
/// read even inside a code fence, with text around it or after its reasoning. A fake network; no real server.

#include "AgentHarness.hpp"
#include "Headless.hpp"

#include <affresco/intelligence/Intelligence.hpp>
#include <affresco/intelligence/Providers.hpp>
#include <affresco/intelligence/Settings.hpp>

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using affresco::json;
namespace intelligence = affresco::intelligence;
namespace network = affresco::network;
using agentharness::DataFolder;
using agentharness::FakeNetwork;
using agentharness::FakeServer;
using agentharness::Harness;

namespace {

/// What a fake server answers: a body or a failure.
using Answer = std::variant<std::string, network::Failure>;

/// @brief A Chat Completions answer with text only, no tool call.
/// @param text The text.
/// @return The body.
std::string said(const std::string& text) {
	return json{{"choices", {{{"message", {{"role", "assistant"}, {"content", text}}}, {"finish_reason", "stop"}}}},
	            {"usage", {{"completion_tokens", 20}}}}
	    .dump();
}

/// @brief Whether a request offered the model tools.
/// @param request The request.
/// @return Whether its body has "tools".
bool offersTools(const network::Request& request) { return json::parse(request.body).contains("tools"); }

/// @brief A failure with a status and a body, as the net module gives one.
/// @param code The status.
/// @param body The body.
/// @return It.
network::Failure refusal(int code, const std::string& body) {
	return {"status", "status " + std::to_string(code), code, body};
}

/// @brief A screen with the settings' natives on a fake network: actions that call intelligence.providers and
/// intelligence.models, intelligence.save and intelligence.test (with the event as the arguments) into state.out,
/// failures into state.error.
struct LocalSettings {
	DataFolder data;                                                     ///< intelligence.json.
	affresco::detail::ManualClock clock;                                 ///< The window's clock.
	std::unique_ptr<affresco::Window> window;                            ///< A headless window.
	affresco::Screen screen;                                             ///< The screen.
	std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>(); ///< The local server.
	std::shared_ptr<affresco::MemorySecretStore> keys = std::make_shared<affresco::MemorySecretStore>(); ///< Keys.

	/// @brief Builds it.
	LocalSettings()
	    : window(affresco::detail::createHeadlessWindow(400, 300, clock)),
	      screen(affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2,
		"state": {"out": null, "error": ""},
		"actions": {
			"providers": [{"call": "intelligence.providers", "into": "state.out"}],
			"models": [{"call": "intelligence.models", "with": "{{event}}", "into": "state.out",
			            "onError": [{"set": "state.error", "to": "{{error}}"}]}],
			"save": [{"call": "intelligence.save", "with": "{{event}}", "into": "state.out",
			          "onError": [{"set": "state.error", "to": "{{error}}"}]}],
			"test": [{"call": "intelligence.test", "with": "{{event}}", "into": "state.out",
			          "onError": [{"set": "state.error", "to": "{{error}}"}]}]
		},
		"root": {"id": "page"}})")) {
		intelligence::CallsOptions options;
		options.keys = keys;
		options.claudeInstalled = [] { return false; };
		options.network = [server = server](affresco::Executor& runner) -> std::unique_ptr<network::Network> {
			return std::make_unique<FakeNetwork>(server, runner);
		};
		intelligence::registerCalls(screen, data.path, std::move(options));
		screen.build(*window);
	}

	/// @brief Runs an action with an event (a slow call runs at once: the screen has no executor).
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

	/// @brief The requests the server got.
	/// @return Them.
	[[nodiscard]] std::vector<network::Request> requests() const {
		const std::scoped_lock lock(server->mutex);
		return server->requests;
	}
};

} // namespace

TEST_CASE("intelligence local: the Local LLM is shown, with Ollama, LM Studio and llama.cpp as presets") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	const intelligence::Provider* local = catalog.find("local");
	REQUIRE(local != nullptr);
	CHECK(local->shown);
	CHECK(local->name == "Local LLM");
	CHECK(local->kind == intelligence::Kind::OpenAICompatible);
	CHECK_FALSE(local->needsKey);
	CHECK(local->tools == intelligence::Tools::Tried);
	CHECK(local->timeout == std::chrono::seconds(300));
	REQUIRE(local->presets.size() == 3);
	CHECK(local->presets[0].baseUrl == "http://127.0.0.1:11434/v1");
	CHECK(local->presets[1].baseUrl == "http://127.0.0.1:1234/v1");
	CHECK(local->presets[2].baseUrl == "http://127.0.0.1:8080/v1");
	CHECK(local->baseUrl == local->presets[0].baseUrl);
	// The other kinds keep their way: API providers are given tools, claude-code none, in two minutes
	CHECK(catalog.find("anthropic")->tools == intelligence::Tools::Given);
	CHECK(catalog.find("claude-code")->tools == intelligence::Tools::None);
	CHECK(catalog.find("openai")->timeout == std::chrono::seconds(120));
	CHECK(catalog.find("claude-code")->presets.empty());

	const auto parse = [](const json& provider) {
		return intelligence::Catalog::fromJson({{"providers", json::array({provider})}}, "test");
	};
	const auto with = [](const json& extra) {
		json provider = {{"id", "x"}, {"name", "X"}, {"kind", "openai-compatible"}};
		provider.update(extra);
		return provider;
	};
	CHECK_THROWS_AS((void)parse(with({{"tools", "maybe"}})), std::runtime_error);
	CHECK_THROWS_AS((void)parse(with({{"timeout", 5}})), std::runtime_error);
	CHECK_THROWS_AS((void)parse(with({{"timeout", 3600}})), std::runtime_error);
	CHECK_THROWS_AS((void)parse(with({{"presets", {{{"id", "a"}, {"name", "A"}, {"baseUrl", "http://example.com"}}}}})),
	                std::runtime_error); // http:// only on this computer
	CHECK_THROWS_AS((void)parse({{"id", "y"},
	                             {"name", "Y"},
	                             {"kind", "openai"},
	                             {"presets", {{{"id", "a"}, {"name", "A"}, {"baseUrl", "https://a.example"}}}}}),
	                std::runtime_error); // an API provider's base URL is never the person's
	CHECK(parse(with({{"tools", false}})).providers[0].tools == intelligence::Tools::None);
}

TEST_CASE("intelligence local: a chat request waits the provider's timeout, and the models list has no embeddings") {
	const intelligence::Catalog catalog = intelligence::Catalog::standard();
	const std::optional<intelligence::Endpoint> local = intelligence::endpointOf(catalog, {}, "local");
	REQUIRE(local.has_value());
	if(!local) return; // for the analysers: REQUIRE has stopped already
	intelligence::Endpoint endpoint = *local;
	endpoint.model = "qwen2.5-7b-instruct";
	const network::Request request = intelligence::chatRequest(endpoint, {"Be brief.", json::array()}, {});
	CHECK(request.timeout == std::chrono::seconds(300));
	CHECK(request.url == "http://127.0.0.1:11434/v1/chat/completions");
	CHECK(json::parse(request.body)["max_tokens"] == intelligence::maxOutputTokens);

	const std::string listed = R"({"object": "list", "data": [{"id": "qwen2.5-7b-instruct"},
		{"id": "text-embedding-nomic-embed-text-v1.5"}, {"id": "llama-3.2-1b-instruct"}]})";
	const std::vector<intelligence::Model> models =
	    intelligence::parseModels(intelligence::Kind::OpenAICompatible, listed);
	REQUIRE(models.size() == 2);
	CHECK(models[0].id == "llama-3.2-1b-instruct");
	CHECK(models[1].id == "qwen2.5-7b-instruct");
}

TEST_CASE("intelligence local: Test connection asks the provider the form shows, not the one saved") {
	LocalSettings settings;
	settings.server->answer = [](const network::Request&) -> Answer { return said("OK"); };
	// Claude Code is saved; the form shows the Local LLM at LM Studio's address with a model, not yet saved
	intelligence::Settings saved;
	saved.provider = "claude-code";
	saved.model = "opus";
	saved.save(settings.data.path);

	SUBCASE("the form's provider, model and server are asked, and nothing is saved") {
		const json result = settings.run(
		    "test", {{"provider", "local"}, {"model", "qwen3-4b"}, {"baseUrl", "http://127.0.0.1:1234/v1"}});
		CHECK(result["ok"] == true);
		CHECK(result["message"] == "Connected: qwen3-4b answered.");
		REQUIRE(settings.requests().size() == 1);
		CHECK(settings.requests()[0].url == "http://127.0.0.1:1234/v1/chat/completions");
		CHECK(json::parse(settings.requests()[0].body)["model"] == "qwen3-4b");
		const intelligence::Settings after = intelligence::Settings::load(settings.data.path);
		CHECK(after.provider == "claude-code");
		CHECK(after.model == "opus");
	}
	SUBCASE("no server at the form's address says so, rather than testing the saved provider") {
		settings.server->answer = [](const network::Request&) -> Answer {
			return network::Failure{"network", "connection refused", 0, {}};
		};
		const json result = settings.run("test", {{"provider", "local"}, {"model", "qwen3-4b"}});
		CHECK(result["ok"] == false);
		CHECK(result["message"].get<std::string>().find("Can't reach Local LLM") != std::string::npos);
	}
	SUBCASE("a key saved for one server never goes to another one tested") {
		settings.run("save", {{"provider", "local"}, {"model", "qwen3-4b"}, {"key", "lm-studio-token-1"}});
		settings.run("test", {{"provider", "local"}, {"model", "qwen3-4b"}, {"baseUrl", "http://127.0.0.1:8080/v1"}});
		// Held in a name: a loop over a member of the temporary settings.requests() returns would read freed memory
		const std::vector<network::Request> requests = settings.requests();
		REQUIRE(requests.size() == 1);
		CHECK(requests[0].url == "http://127.0.0.1:8080/v1/chat/completions");
		for(const auto& [name, value] : requests[0].headers) CHECK(name != "authorization");
	}
	SUBCASE("a base URL elsewhere, over http, is refused before anything is sent") {
		const json result =
		    settings.run("test", {{"provider", "local"}, {"model", "m"}, {"baseUrl", "http://example.com/v1"}});
		CHECK(result["ok"] == false);
		CHECK(settings.requests().empty());
	}
	SUBCASE("with no provider given, the one in use is tested with its saved model and server, as before") {
		settings.run("save", {{"provider", "local"}, {"model", "llama3.2"}});
		const json result = settings.run("test");
		CHECK(result["message"] == "Connected: llama3.2 answered.");
		REQUIRE(settings.requests().size() == 1);
		CHECK(settings.requests()[0].url == "http://127.0.0.1:11434/v1/chat/completions");
	}
}

TEST_CASE("intelligence local: the settings list the presets, ask a preset's server, and tell no server from none") {
	LocalSettings settings;
	const json providers = settings.run("providers");
	json local;
	for(const json& provider : providers["providers"])
		if(provider["id"] == "local") local = provider;
	REQUIRE(local.is_object());
	CHECK(local["editableUrl"] == true);
	CHECK(local["takesKey"] == true);
	CHECK(local["needsKey"] == false);
	REQUIRE(local["presets"].size() == 3);
	CHECK(local["presets"][1] ==
	      json{{"id", "lm-studio"}, {"name", "LM Studio"}, {"baseUrl", "http://127.0.0.1:1234/v1"}});

	SUBCASE("a server that answers: its chat models, from the base URL the form shows (not yet saved)") {
		settings.server->answer = [](const network::Request&) -> Answer {
			return R"({"data": [{"id": "qwen3-4b"}, {"id": "text-embedding-nomic-embed-text-v1.5"}]})";
		};
		const json models = settings.run("models", {{"provider", "local"}, {"baseUrl", "http://127.0.0.1:1234/v1/"}});
		CHECK(models == json::array({{{"id", "qwen3-4b"}, {"name", "qwen3-4b"}}}));
		REQUIRE(settings.requests().size() == 1);
		CHECK(settings.requests()[0].url == "http://127.0.0.1:1234/v1/models");
	}
	SUBCASE("a key saved for one server never goes to another one asked for its models") {
		settings.run("save", {{"provider", "local"}, {"model", "qwen3-4b"}, {"key", "lm-studio-token-1"}});
		settings.server->answer = [](const network::Request&) -> Answer { return R"({"data": []})"; };
		settings.run("models", {{"provider", "local"}});
		settings.run("models", {{"provider", "local"}, {"baseUrl", "http://127.0.0.1:8080/v1"}});
		const std::vector<network::Request> requests = settings.requests();
		REQUIRE(requests.size() == 2);
		const auto authorized = [](const network::Request& request) {
			for(const auto& [name, value] : request.headers)
				if(name == "authorization") return value;
			return std::string();
		};
		CHECK(authorized(requests[0]) == "Bearer lm-studio-token-1"); // its own server, at the saved base URL
		CHECK(requests[1].url == "http://127.0.0.1:8080/v1/models");
		CHECK(authorized(requests[1]).empty());
	}
	SUBCASE("a server with no model loaded answers an empty list") {
		settings.server->answer = [](const network::Request&) -> Answer { return R"({"object": "list", "data": []})"; };
		CHECK(settings.run("models", {{"provider", "local"}}) == json::array());
		CHECK(settings.error().empty());
	}
	SUBCASE("no server: the call fails, saying why, so the form can say to start one") {
		settings.server->answer = [](const network::Request&) -> Answer {
			return network::Failure{"network", "connection refused", 0, {}};
		};
		settings.run("models", {{"provider", "local"}});
		CHECK(settings.error().find("Can't reach Local LLM") != std::string::npos);
	}
	SUBCASE("a base URL elsewhere, over http, is refused before anything is sent") {
		settings.run("models", {{"provider", "local"}, {"baseUrl", "http://example.com/v1"}});
		CHECK(settings.error().find("http:// is only for a server on this computer") != std::string::npos);
		CHECK(settings.requests().empty());
	}
}

TEST_CASE("intelligence local: a server that refuses tools gets the JSON answers, in a fence, and is remembered") {
	Harness harness("local", "gemma3:1b");
	const json revision = {{"message", "Rename the page"}, {"set", {{"#title/text", "Launch"}}}};
	SUBCASE("Ollama's 400 \"does not support tools\"") {
		harness.server->answer = [&revision](const network::Request& request) -> Answer {
			if(offersTools(request))
				return refusal(
				    400, R"({"error": {"message": "registry.ollama.ai/library/gemma3:1b does not support tools"}})");
			return said("Sure! Here it is:\n```json\n" +
			            json{{"reply", "Renamed it: it waits for your review."}, {"revision", revision}}.dump(2) +
			            "\n```\nLet me know {if} you need more.");
		};
	}
	SUBCASE("llama-server's 500 \"tools param requires --jinja flag\"") {
		harness.server->answer = [&revision](const network::Request& request) -> Answer {
			if(offersTools(request))
				return refusal(500, R"({"error": {"code": 500, "message": "tools param requires --jinja flag"}})");
			return said("<think>The person wants {a new title}.</think>\n" +
			            json{{"reply", "Renamed it: it waits for your review."}, {"revision", revision}}.dump());
		};
	}
	const json reply = harness.ask("Rename the page to Launch");
	CHECK(reply["text"] == "Renamed it: it waits for your review.");
	REQUIRE(harness.revisions.size() == 1);
	CHECK(harness.revisions[0]["set"]["#title/text"] == "Launch");
	CHECK(harness.revisions[0]["author"] == "local");
	std::vector<network::Request> requests = harness.requests();
	REQUIRE(requests.size() == 2);
	CHECK(offersTools(requests[0]));
	CHECK_FALSE(offersTools(requests[1]));
	const json second = json::parse(requests[1].body);
	CHECK(second["messages"][0]["content"].get<std::string>().find("You have no tools") != std::string::npos);
	CHECK(second["messages"][1]["content"].get<std::string>().find("Rename the page to Launch") != std::string::npos);

	// The agent remembers that this server and model refuse tools: the next request goes without them at once
	harness.ask("Rename it again");
	requests = harness.requests();
	REQUIRE(requests.size() == 3);
	CHECK_FALSE(offersTools(requests[2]));
}

TEST_CASE(
    "intelligence local: a model that calls no tool answers again in JSON, and an unreadable one is asked again") {
	Harness harness("local", "llama3.2:1b");
	int round = 0;
	harness.server->answer = [&round](const network::Request& request) -> Answer {
		++round;
		if(offersTools(request)) return said("I would rename the title to Launch.");
		if(round == 2) return said(R"({"reply": "Renamed", "revision": {"set": {"#title/text": "Launch"},})");
		return said(R"(```
{"reply": "Renamed it: it waits for your review.", "revision": {"message": "Rename", "set": {"#title/text": "Launch"}}}
```)");
	};
	const json reply = harness.ask("Rename the page to Launch");
	CHECK(reply["text"] == "Renamed it: it waits for your review.");
	REQUIRE(harness.revisions.size() == 1);
	const std::vector<network::Request> requests = harness.requests();
	REQUIRE(requests.size() == 3);
	CHECK(offersTools(requests[0]));
	CHECK_FALSE(offersTools(requests[1]));
	const std::string third = json::parse(requests[2].body)["messages"][1]["content"].get<std::string>();
	CHECK(third.find("It isn't one JSON object that can be read") != std::string::npos);

	// Not remembered: a model that answered in text once is offered tools again
	round = 0;
	harness.ask("Rename it");
	CHECK(offersTools(harness.requests()[3]));

	// A model that never answers with a readable object ends the request with why, not with its broken text
	harness.server->answer = [](const network::Request& request) -> Answer {
		if(offersTools(request)) return said("Let me see.");
		return said(R"({"reply": "Renamed" # a comment, "revision": {}})");
	};
	const json broken = harness.ask("Rename it once more");
	CHECK(broken["error"].get<std::string>().find("didn't answer with a JSON object that could be read") !=
	      std::string::npos);
	CHECK(harness.requests().size() == 6 + 4); // one with tools, then three without
}

TEST_CASE("intelligence local: a refused key or a server error isn't taken for refused tools") {
	Harness harness("local", "qwen3-4b");
	SUBCASE("401") {
		harness.server->answer = [](const network::Request&) -> Answer {
			return refusal(401, R"({"error": {"message": "invalid token for tools"}})");
		};
		const json reply = harness.ask("Rename the page");
		CHECK(reply["error"].get<std::string>().find("refused the API key (401)") != std::string::npos);
	}
	SUBCASE("a context too small") {
		harness.server->answer = [](const network::Request&) -> Answer {
			return refusal(
			    400,
			    R"({"error": "The number of tokens to keep from the initial prompt is greater than the context length"})");
		};
		const json reply = harness.ask("Rename the page");
		CHECK(reply["error"].get<std::string>().find("Local LLM answered 400") != std::string::npos);
		CHECK(reply["error"].get<std::string>().find("context length") != std::string::npos);
	}
	CHECK(harness.requests().size() == 1);
}

TEST_CASE("intelligence local: claude-code's answer is read after reasoning or inside a fence too, its way unchanged") {
	Harness harness("claude-code", "haiku");
	harness.claudeAnswers = {"<think>Plan: set {the title}.</think>```json\n{\"read\": [\"#title\"]}\n```",
	                         "Done, here: {\"reply\": \"Renamed it.\", \"revision\": {\"set\": {\"#title/text\": "
	                         "\"Launch\"}}} and that's all {really}."};
	const json reply = harness.ask("Rename the page to Launch");
	CHECK(reply["text"] == "Renamed it.");
	REQUIRE(harness.revisions.size() == 1);
	CHECK(harness.claudeRuns == 2);
	CHECK(harness.claudeInputs[1].find("What you asked to see:\n#title:") != std::string::npos);
	CHECK(harness.requests().empty()); // no network at all
}

TEST_CASE("intelligence local: a provider given tools reports a refusal of them, never falls back") {
	Harness harness("openai-compatible", "llama3.2"); // hidden, "tools" left to its kind: given
	harness.server->answer = [](const network::Request&) -> Answer {
		return refusal(400, R"({"error": {"message": "llama3.2 does not support tools"}})");
	};
	const json reply = harness.ask("Rename the page");
	CHECK(reply["error"].get<std::string>().find("400") != std::string::npos);
	CHECK(reply["error"].get<std::string>().find("does not support tools") != std::string::npos);
	CHECK(harness.requests().size() == 1); // not asked again without tools
	harness.ask("Rename it again");
	const std::vector<network::Request> requests = harness.requests();
	REQUIRE(requests.size() == 2);
	CHECK(offersTools(requests[1])); // and not remembered as refusing them
}

TEST_CASE("intelligence local: an answer nested too deep is refused cleanly, in JSON or in a tool's arguments") {
	const std::string deep = std::string(10000, '[') + std::string(10000, ']');
	const std::string answer =
	    R"({"reply": "Done.", "revision": {"message": "m", "set": {"#title/text": )" + deep + "}}}";
	SUBCASE("a JSON answer of a server that refused tools: asked again, then the request ends") {
		Harness harness("local", "gemma3:1b");
		harness.server->answer = [&answer](const network::Request& request) -> Answer {
			if(offersTools(request)) return refusal(400, R"({"error": "this model does not support tools"})");
			return said("```json\n" + answer + "\n```");
		};
		const json reply = harness.ask("Rename the page");
		CHECK(reply["error"].get<std::string>().find("didn't answer with a JSON object") != std::string::npos);
		CHECK(harness.revisions.empty());
	}
	SUBCASE("a command line's answer: shown as the reply, nothing submitted") {
		Harness harness("claude-code", "haiku");
		harness.claudeAnswers = {answer};
		const json reply = harness.ask("Rename the page");
		CHECK(reply["text"].get<std::string>().starts_with(R"({"reply": "Done.")"));
		CHECK(harness.revisions.empty());
	}
	SUBCASE("a tool's arguments: read as none") {
		const json call = {{"id", "call-1"},
		                   {"type", "function"},
		                   {"function", {{"name", "submit_revision"}, {"arguments", R"({"set": )" + deep + "}"}}}};
		const std::string body =
		    json{{"choices",
			      {{{"message", {{"role", "assistant"}, {"tool_calls", {call}}}}, {"finish_reason", "tool_calls"}}}}}
		        .dump();
		const intelligence::Turn turn = intelligence::parseTurn(intelligence::Kind::OpenAICompatible, body);
		REQUIRE(turn.calls.size() == 1);
		CHECK(turn.calls[0].input == json::object());
		CHECK_THROWS_AS((void)intelligence::parseTurn(intelligence::Kind::OpenAI, deep), std::runtime_error);
		CHECK(intelligence::parseAnswerJson(deep).is_discarded());
	}
}
