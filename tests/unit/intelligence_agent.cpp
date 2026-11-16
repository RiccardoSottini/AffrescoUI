/// @file
/// @brief The intelligence module's in-process agent (affresco/intelligence/Agent.hpp), with a fake network and a
/// stand-in screen: the tool loop reads, revises and replies within its bounds, only the provider's origin is allowed, a
/// missing key or a refused one is a reply without the key, claude-code retries a rejected revision once, and only
/// the screen's own requests are taken. No real API is called.

#include "AgentHarness.hpp"

#include <affresco/intelligence/Agent.hpp>
#include <affresco/intelligence/Intelligence.hpp>
#include <affresco/intelligence/ScreenQueries.hpp>

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;
namespace intelligence = affresco::intelligence;
namespace network = affresco::network;
using agentharness::Harness;
using agentharness::ThreadExecutor;
using agentharness::toolUse;

TEST_CASE("intelligence agent: an Anthropic tool loop reads the screen, proposes a revision and replies") {
	Harness harness("anthropic", "claude-haiku-5-5");
	harness.keys->write("intelligence/anthropic", "sk-ant-test-key");
	int turn = 0;
	harness.server->answer = [&turn](const network::Request&) -> std::variant<std::string, network::Failure> {
		switch(turn++) {
		case 0:
			return toolUse("t1", "get_screen", {{"outline", true}});
		case 1:
			return toolUse("t2", "submit_revision",
			               {{"message", "Rename the page"}, {"set", {{"#title/text", "Launch"}}}, {"type", "accept"}});
		default:
			return toolUse("t3", "reply", {{"text", "Renamed it: it waits for your review."}});
		}
	};
	const json reply = harness.ask("Rename the page to Launch");
	CHECK(reply["request"] == "ask-1");
	CHECK(reply["text"] == "Renamed it: it waits for your review.");
	CHECK_FALSE(reply.contains("error"));
	CHECK(harness.sent.front()["type"] == "reply"); // first, the request is marked working
	CHECK(harness.sent.front()["done"] == false);

	REQUIRE(harness.revisions.size() == 1);
	const json& revision = harness.revisions[0];
	CHECK(revision["type"] == "revision"); // the model's "type" is ignored: only the edits are taken
	CHECK(revision["author"] == "anthropic");
	CHECK(revision["set"]["#title/text"] == "Launch");
	CHECK_FALSE(revision.contains("proposal"));

	const std::vector<network::Request> requests = harness.requests();
	REQUIRE(requests.size() == 3);
	CHECK(harness.server->allowed.size() == 1);
	CHECK(harness.server->allowed.front() == "https://api.anthropic.com");
	const json second = json::parse(requests[1].body);
	CHECK(second["messages"][0]["content"].get<std::string>().find("Rename the page to Launch") != std::string::npos);
	CHECK(second["messages"][2]["content"][0]["type"] == "tool_result");
	CHECK(second["messages"][2]["content"][0]["content"].get<std::string>().find("#title label") != std::string::npos);
	const json third = json::parse(requests[2].body);
	CHECK(third["messages"][4]["content"][0]["content"].get<std::string>().find("proposal 7 waits") !=
	      std::string::npos);
	for(const network::Request& request : requests)
		for(const auto& [name, value] : request.headers)
			if(name == "x-api-key") CHECK(value == "sk-ant-test-key");
	for(const json& message : harness.sent) CHECK(message.dump().find("sk-ant-test-key") == std::string::npos);
}

TEST_CASE("intelligence agent: the tool loop stops at its step limit, a second revision goes onto the same proposal") {
	Harness harness("openai-compatible", "llama3.2", 3);
	harness.server->answer = [](const network::Request&) -> std::variant<std::string, network::Failure> {
		return json{{"choices",
		             {{{"message",
		                {{"role", "assistant"},
		                 {"content", ""},
		                 {"tool_calls",
		                  {{{"id", "c"},
		                    {"type", "function"},
		                    {"function",
		                     {{"name", "submit_revision"},
		                      {"arguments", R"({"message": "m", "set": {"/state/board": "Q4"}})"}}}}}}}},
		               {"finish_reason", "tool_calls"}}}}}
		    .dump();
	};
	const json reply = harness.ask("Loop forever");
	CHECK(harness.requests().size() == 3);
	CHECK(reply["error"].get<std::string>().find("stopped after 3 steps") != std::string::npos);
	REQUIRE(harness.revisions.size() == 3);
	CHECK_FALSE(harness.revisions[0].contains("proposal"));
	CHECK(harness.revisions[1]["proposal"] == 7); // onto the proposal the first opened
	CHECK(harness.server->allowed.size() == 1);
	CHECK(harness.server->allowed.front() == "http://127.0.0.1:11434");
	const json body = json::parse(harness.requests()[1].body);
	CHECK(body["messages"].back()["role"] == "tool");
}

TEST_CASE("intelligence agent: no key, a refused key, a rejected revision and a request from elsewhere") {
	SUBCASE("no key") {
		Harness harness("openai", "gpt-5");
		const json reply = harness.ask("Hi");
		CHECK(reply["error"].get<std::string>().find("needs an API key") != std::string::npos);
		CHECK(harness.requests().empty());
	}
	SUBCASE("a refused key") {
		Harness harness("openai", "gpt-5");
		harness.keys->write("intelligence/openai", "sk-proj-REFUSED999999");
		harness.server->answer = [](const network::Request&) -> std::variant<std::string, network::Failure> {
			return network::Failure{"status", "401", 401,
			                        R"({"error": {"message": "Incorrect API key sk-proj-REFUSED999999"}})"};
		};
		const json reply = harness.ask("Hi");
		CHECK(reply["error"].get<std::string>().find("refused the API key") != std::string::npos);
		CHECK(reply.dump().find("REFUSED") == std::string::npos);
	}
	SUBCASE("no provider") {
		Harness harness("", "");
		const json reply = harness.ask("Hi");
		CHECK(reply["error"].get<std::string>().find("AffrescoUI Launcher's Settings") != std::string::npos);
	}
	SUBCASE("claude-code retries a rejected revision once") {
		Harness harness("claude-code", "haiku");
		harness.claudeAnswers = {R"({"reply": "Done", "revision": {"message": "m", "set": {"#titel/text": "A"}}})",
		                         "```json\n{\"reply\": \"Fixed\", \"revision\": {\"message\": \"m\", \"set\": "
		                         "{\"#title/text\": \"A\"}}}\n```"};
		harness.verdict = [](const json& revision) {
			return revision["set"].contains("#titel/text")
			           ? json{{"type", "rejected"}, {"error", "no node with id 'titel'"}}
					   : json{{"type", "proposed"}, {"proposal", 2}};
		};
		const json reply = harness.ask("Change the title");
		CHECK(harness.claudeRuns == 2);
		CHECK(reply["text"] == "Fixed");
		REQUIRE(harness.revisions.size() == 2);
		CHECK(harness.revisions[1]["author"] == "claude-code");
	}
	SUBCASE("claude-code reads first, then answers") {
		Harness harness("claude-code", "haiku");
		harness.claudeAnswers = {R"({"read": ["#title"], "guide": ["label"]})", R"({"reply": "It says Tasks."})"};
		const json reply = harness.ask("What does the title say?");
		CHECK(harness.claudeRuns == 2);
		CHECK(reply["text"] == "It says Tasks.");
		CHECK(harness.revisions.empty());
	}
	SUBCASE("only the screen's own requests, and at most eight wait") {
		Harness harness("claude-code", "haiku");
		harness.agent->send(json{{"type", "request"}, {"id", "r"}, {"text", "x"}, {"from", "studio"}}.dump());
		harness.agent->send(R"({"type": "document", "id": "intelligence-99"})"); // an answer nobody asked for
		harness.agent->send("not json");
		harness.executor.runUntil([] { return false; }, std::chrono::milliseconds(100));
		CHECK(harness.sent.empty());
		CHECK(harness.claudeRuns == 0);
	}
	SUBCASE("an answer that changes nothing and says nothing isn't \"Done.\"") {
		Harness harness("claude-code", "haiku");
		harness.claudeAnswers = {R"({"revision": {"message": "m", "set": {"#gone/text": "A"}}})",
		                         R"({"read": ["#gone"]})", R"({"read": ["#gone"]})", R"({"read": ["#gone"]})",
		                         R"({"read": ["#gone"]})"};
		harness.verdict = [](const json&) { return json{{"type", "rejected"}, {"error", "no node with id 'gone'"}}; };
		const json reply = harness.ask("Change it");
		CHECK(harness.claudeRuns == 5); // the revision, then three reads, and the fourth read is one too many
		CHECK(reply["error"].get<std::string>().find("made no change") != std::string::npos);

		Harness quiet("claude-code", "haiku");
		quiet.claudeAnswers = {"{}"};
		CHECK(quiet.ask("Change it")["error"].get<std::string>().find("No change was made") != std::string::npos);
	}
}

TEST_CASE("intelligence agent: on a program, the assistant is told of it, finds the definition and changes it") {
	Harness harness("claude-code", "opus");
	harness.document = {
	    {"root", {{"id", "page"}, {"children", {{{"id", "title"}, {"type", "label"}, {"text", "Tasks"}}}}}},
	    {"program",
		 {{"app.lisp",
		   {{"page", ";; The page.\n(defscreen page (column#page (label#title :text \"Tasks\")))"},
		    {"defkey mod+/b", "(defkey \"mod+/b\" toggle-side)"}}}}}};
	const json fixed = {
	    {"reply", "Renamed it."},
	    {"revision",
		 {{"message", "m"},
		  {"set", {{"/program/app.lisp/page", "(defscreen page (column#page (label#title :text \"Launch\")))"}}}}}};
	harness.claudeAnswers = {R"({"revision": {"message": "m", "set": {"#title/text": "Launch"}}})",
	                         R"({"find": ["#title"]})", R"({"read": ["/program/app.lisp/page"]})", fixed.dump()};
	harness.verdict = [](const json& revision) {
		return revision["set"].contains("#title/text")
		           ? json{{"type", "rejected"},
				          {"error", "no node with id 'title' (a program builds what isn't written here: change its "
				                    "definition, under /program)"}}
				   : json{{"type", "proposed"}, {"proposal", 4}};
	};
	const json reply = harness.ask("Rename the page to Launch");
	CHECK(reply["text"] == "Renamed it.");
	REQUIRE(harness.claudeRuns == 4);
	CHECK(harness.claudeInputs[0].find("This app is a program in AffrescoUI Lisp") != std::string::npos);
	CHECK(harness.claudeInputs[0].find(";; The page.") != std::string::npos); // a small screen is sent whole
	CHECK(harness.claudeInputs[2].find("find \"#title\": 1 found\n/program/app.lisp/page defscreen") !=
	      std::string::npos);
	const std::string outline = intelligence::outline(harness.document); // a large one as its outline
	CHECK(outline.starts_with("/program/app.lisp/page defscreen\n/program/app.lisp/defkey mod+~1b defkey\n#page"));
	CHECK(intelligence::findNodes(harness.document, "defkey", "", "", json()) ==
	      "1 found\n/program/app.lisp/defkey mod+~1b defkey");
	CHECK(harness.claudeInputs[3].find(";; The page.") != std::string::npos);
	REQUIRE(harness.revisions.size() == 2);
	CHECK(harness.revisions[1]["set"].contains("/program/app.lisp/page"));
}

TEST_CASE("intelligence agent: at most six requests a minute are taken, the rest answered \"too many requests\"") {
	Harness harness("claude-code", "haiku");
	harness.claudeAnswers.assign(6, R"({"reply": "OK"})");
	for(int i = 1; i <= 7; ++i)
		harness.agent->send(json{{"type", "request"},
		                         {"id", "r" + std::to_string(i)},
		                         {"text", "Hi"},
		                         {"from", "user"},
		                         {"source", "screen"}}
		                        .dump());
	const auto finals = [&harness] {
		int count = 0;
		for(const json& message : harness.sent)
			if(message.value("type", "") == "reply" && message.value("done", false)) ++count;
		return count;
	};
	REQUIRE(harness.executor.runUntil([&] { return finals() == 6; }));
	harness.executor.runUntil([] { return false; }, std::chrono::milliseconds(100));
	CHECK(harness.claudeRuns == 6); // the seventh never reached the provider
	int refused = 0;
	for(const json& message : harness.sent)
		if(message.value("request", "") == "r7") {
			CHECK(message["error"].get<std::string>().find("Too many requests") != std::string::npos);
			++refused;
		}
	CHECK(refused == 1);
}

TEST_CASE("intelligence agent: a screen that ends disconnects from it, so no message reaches a screen that is gone") {
	ThreadExecutor executor;
	intelligence::AgentOptions options;
	options.catalog = intelligence::Catalog::standard();
	options.keys = std::make_shared<affresco::MemorySecretStore>();
	intelligence::ProviderAgent agent(executor, std::move(options));
	REQUIRE(agent.onMessage.empty());
	{
		affresco::Screen screen =
		    affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 2, "root": {"id": "page"}})");
		screen.connect(agent, "agent");
		CHECK_FALSE(agent.onMessage.empty());
	}
	CHECK(agent.onMessage.empty()); // what the agent delivers after this finds no one
	agent.onMessage(R"({"type": "getDocument", "id": "late"})");
	executor.runUntil([] { return false; }, std::chrono::milliseconds(50));
}

TEST_CASE("intelligence agent: a base URL edited in intelligence.json never receives the key, nor is used unchecked") {
	SUBCASE("a compatible server's key stays with its origin") {
		Harness harness("openai-compatible", "llama3.2");
		harness.keys->write("intelligence/openai-compatible",
		                    R"({"origin": "http://127.0.0.1:11434", "key": "local-secret-key"})");
		intelligence::Settings settings = intelligence::Settings::load(harness.data.path);
		settings.providers["openai-compatible"].baseUrl =
		    "https://elsewhere.example.com/v1"; // edited by another program
		settings.save(harness.data.path);
		harness.server->answer = [](const network::Request&) -> std::variant<std::string, network::Failure> {
			return json{
			    {"choices", {{{"message", {{"role", "assistant"}, {"content", "Hello"}}}, {"finish_reason", "stop"}}}}}
			    .dump();
		};
		const json reply = harness.ask("Hi");
		CHECK(reply["text"] == "Hello");
		const std::vector<network::Request> requests = harness.requests();
		REQUIRE(requests.size() == 1);
		CHECK(requests[0].url.starts_with("https://elsewhere.example.com/"));
		for(const auto& [name, value] : requests[0].headers) {
			CHECK(name != "Authorization");
			CHECK(value.find("local-secret-key") == std::string::npos);
		}
	}
	SUBCASE("an API provider's base URL is the catalogue's, and a bad URL is refused before any request") {
		Harness harness("anthropic", "claude-haiku-5-5");
		harness.keys->write("intelligence/anthropic", "sk-ant-test-key");
		intelligence::Settings settings = intelligence::Settings::load(harness.data.path);
		settings.providers["anthropic"].baseUrl = "https://evil.example";
		settings.save(harness.data.path);
		harness.server->answer = [](const network::Request&) -> std::variant<std::string, network::Failure> {
			return toolUse("t", "reply", {{"text", "OK"}});
		};
		CHECK(harness.ask("Hi")["text"] == "OK");
		REQUIRE(harness.requests().size() == 1);
		CHECK(harness.requests()[0].url.starts_with("https://api.anthropic.com/"));
	}
	SUBCASE("a compatible server on plain http elsewhere") {
		Harness harness("openai-compatible", "llama3.2");
		intelligence::Settings settings = intelligence::Settings::load(harness.data.path);
		settings.providers["openai-compatible"].baseUrl = "http://evil.example/v1";
		settings.save(harness.data.path);
		const json reply = harness.ask("Hi");
		CHECK(reply["error"].get<std::string>().find("http:// is only for a server on this computer") !=
		      std::string::npos);
		CHECK(harness.requests().empty());
	}
}

TEST_CASE("intelligence agent: a request's model is used when the provider lists it, else the person's default") {
	for(const auto& [options, expected] : std::vector<std::pair<json, std::string>>{
	        {json{{"model", "opus"}}, "opus"},
	        {json{{"model", "fable"}}, "fable"},
	        {json{{"model", "claude-opus-4"}}, "haiku"}, // not one Claude Code lists
	        {json{{"model", 5}}, "haiku"},
	        {json{{"model", std::string(300, 'a')}}, "haiku"},
	        {json(), "haiku"}}) {
		Harness harness("claude-code", "haiku");
		harness.claudeAnswers = {R"({"reply": "OK"})"};
		CHECK(harness.ask("Hi", nullptr, options)["text"] == "OK");
		REQUIRE(harness.claudeModels.size() == 1);
		CHECK(harness.claudeModels[0] == expected);
	}
}

TEST_CASE(
    "intelligence agent: the person cancels: a waiting request is dropped, the one in progress stops unanswered") {
	Harness harness("claude-code", "haiku");
	std::atomic<bool> started = false;
	std::atomic<bool> stopped = false;
	harness.claudeRun = [&](const std::function<bool()>& isStopped) -> intelligence::ClaudeAnswer {
		started = true;
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!isStopped() && std::chrono::steady_clock::now() < until)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		stopped = isStopped();
		return {false, "Stopped."};
	};
	const auto request = [](const char* id) {
		return json{{"type", "request"}, {"id", id}, {"text", "Hi"}, {"from", "user"}, {"source", "screen"}}.dump();
	};
	const auto cancel = [](const char* id) {
		return json{{"type", "cancel"}, {"request", id}, {"from", "user"}, {"source", "screen"}}.dump();
	};
	harness.agent->send(request("ask-1"));
	harness.agent->send(request("ask-2"));                                      // waits behind the first
	harness.agent->send(cancel("ask-2"));                                       // dropped from the queue
	harness.agent->send(json{{"type", "cancel"}, {"request", "ask-1"}}.dump()); // not the screen's own: ignored
	for(int tick = 0; tick < 500 && !started; ++tick) // the agent reads the screen first, through the executor
		harness.executor.runUntil([] { return false; }, std::chrono::milliseconds(10));
	REQUIRE(started);
	CHECK_FALSE(stopped);
	harness.agent->send(cancel("ask-1"));
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while(!stopped && std::chrono::steady_clock::now() < until) // the run ends on its own: nothing is posted
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	REQUIRE(stopped);
	harness.executor.runUntil([] { return false; }, std::chrono::milliseconds(200));
	for(const json& message : harness.sent)
		CHECK_FALSE((message.value("type", "") == "reply" && message.value("done", true))); // no answer
	CHECK(std::ranges::none_of(harness.sent,
	                           [](const json& message) { return message.value("request", "") == "ask-2"; }));

	harness.claudeRun = nullptr; // the agent goes on with the next request
	harness.claudeAnswers = {R"({"reply": "OK"})"};
	CHECK(harness.ask("Again")["text"] == "OK");
}

TEST_CASE("intelligence agent: assets and run-once steps reach the screen, attachments reach the model") {
	const json file = {{"id", "att-1"},   {"name", "Clip.mp3"}, {"type", "mp3"},
	                   {"size", 3145728}, {"duration", 201.5},  {"path", "/Users/someone/Files/Clip.mp3"}};
	SUBCASE("claude-code links an attachment and writes an SVG, with a step that opens it") {
		Harness harness("claude-code", "haiku");
		const json revision = {
		    {"message", "Link the file and open the drawing"},
		    {"set", {{"/state/items/0/sound", "asset:clip.mp3"}}},
		    {"assets",
			 {{{"name", "clip.mp3"}, {"attachment", "att-1"}},
			  {{"name", "drawing.svg"}, {"type", "svg"}, {"text", "<svg xmlns='http://www.w3.org/2000/svg'/>"}}}},
		    {"run", {{{"image", "open"}, {"with", {{"asset", "drawing.svg"}}}}}}};
		harness.claudeAnswers = {json{{"reply", "Linked; it waits for your review."}, {"revision", revision}}.dump()};
		const json reply = harness.ask("Use this file for the first item", json::array({file}));
		CHECK(reply["text"] == "Linked; it waits for your review.");
		REQUIRE(harness.revisions.size() == 1);
		CHECK(harness.revisions[0]["assets"] == revision["assets"]);
		CHECK(harness.revisions[0]["run"] == revision["run"]);
		REQUIRE(harness.claudeInputs.size() == 1);
		const std::string& said = harness.claudeInputs[0];
		CHECK(said.find("\"assets\"") != std::string::npos); // the system prompt explains them
		CHECK(said.find("carefully drawn SVG") != std::string::npos);
		CHECK(said.find("\"id\":\"att-1\"") != std::string::npos); // the attachment's metadata
		CHECK(said.find("201.5") != std::string::npos);
		CHECK(said.find("/Users/someone") == std::string::npos); // never where the file is
	}
	SUBCASE("an unknown attachment, bytes and an SVG too large are refused before the screen sees them") {
		Harness harness("claude-code", "haiku");
		const auto answer = [](const json& asset) {
			return json{{"reply", "Done"}, {"revision", {{"message", "m"}, {"assets", json::array({asset})}}}}.dump();
		};
		SUBCASE("unknown") { harness.claudeAnswers = {answer({{"name", "a.mp3"}, {"attachment", "att-9"}})}; }
		SUBCASE("bytes") { harness.claudeAnswers = {answer({{"name", "a.png"}, {"type", "png"}, {"data", "AAAA"}})}; }
		SUBCASE("large") {
			harness.claudeAnswers = {
			    answer({{"name", "a.svg"}, {"type", "svg"}, {"text", std::string(512 * 1024 + 1, ' ')}})};
		}
		harness.claudeAnswers.push_back(harness.claudeAnswers.front()); // the retry repeats it
		const json reply = harness.ask("Link it", json::array({file}));
		CHECK(reply.contains("error"));
		CHECK(harness.revisions.empty());
	}
	SUBCASE("an API model's submit_revision carries them, and a later request may link an earlier attachment") {
		Harness harness("anthropic", "claude-haiku-5-5");
		harness.keys->write("intelligence/anthropic", "sk-ant-test-key");
		int turn = 0;
		harness.server->answer = [&turn](const network::Request&) -> std::variant<std::string, network::Failure> {
			if(turn++ % 2 == 0)
				return toolUse("t", "submit_revision",
				               {{"message", "Link it"},
				                {"assets", {{{"name", "clip.mp3"}, {"attachment", "att-1"}}}},
				                {"run", {{{"set", "state.page"}, {"to", "playlist"}}}}});
			return toolUse("r", "reply", {{"text", "It waits for your review."}});
		};
		harness.ask("Here is a file", json::array({file}));
		CHECK(harness.ask("Now link it")["text"] == "It waits for your review.");
		REQUIRE(harness.revisions.size() == 2);
		CHECK(harness.revisions[1]["assets"][0]["attachment"] == "att-1");
		CHECK(harness.revisions[1]["run"][0]["set"] == "state.page");
		const json second = json::parse(harness.requests()[2].body);
		CHECK(second["messages"][0]["content"].get<std::string>().find("attached earlier") != std::string::npos);
		bool described = false;
		for(const json& tool : second["tools"])
			if(tool["name"] == "submit_revision") described = tool["input_schema"]["properties"].contains("assets");
		CHECK(described);
	}
}
