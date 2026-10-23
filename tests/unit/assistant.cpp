/// @file
/// @brief The screen asks its agents (format's Asks, the "ask" step, Screen::Impl's relay): the request goes to the
/// peers from the person, replies come back to "ask:reply" with the proposals their author made since, the
/// "requests" and "peers" variables show them, and a reply never decides for the person.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen whose person asks the agents and keeps what comes back; the agent's changes wait for review.
constexpr const char* askingScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "draft": "Make the title blue", "asked": "", "replies": [], "seen": null, "peers": null },
	"permissions": { "allow": ["studio"], "reviewers": ["studio"] },
	"actions": {
		"send": [{ "ask": "{{state.draft}}", "into": "state.asked" }],
		"ask:reply": [{ "push": "state.replies", "value": "{{event}}" }],
		"look": [{ "set": "state.seen", "to": "{{requests}}" }, { "set": "state.peers", "to": "{{peers}}" }],
		"approve": [{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }]
	},
	"root": { "type": "column", "id": "root", "children": [{ "type": "label", "id": "title", "text": "Tasks" }] }
})json";

/// @brief A screen built in a headless window, the channels first: they outlive the screen.
struct Asking {
	affresco::test::MemoryChannel agent;      ///< What the agent is sent.
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Asking(const char* text = askingScreen)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief Runs one of the screen's actions, as the end of the event would.
	/// @param name The action.
	void run(const char* name) {
		screen.runAction(name, window->root(), "{}");
		window->runDeferredTasks();
		window->render();
	}

	/// @brief The screen's state.
	/// @return It.
	[[nodiscard]] const json& state() { return screen.versions().state(); }

	/// @brief The requests the agent was sent.
	/// @return Them, oldest first.
	[[nodiscard]] std::vector<json> requests() const {
		std::vector<json> found;
		std::ranges::copy_if(agent.sent, std::back_inserter(found),
		                     [](const json& message) { return message.value("type", "") == "request"; });
		return found;
	}

	/// @brief Plays a reply from the agent.
	/// @param reply The reply's fields besides its type.
	void reply(json reply) {
		reply["type"] = "reply";
		agent.receive(reply.dump());
		window->runDeferredTasks();
		window->render();
	}
};

} // namespace

TEST_CASE("assistant: the person's request reaches the agents, and the reply brings the agent's proposal") {
	Asking asking;
	asking.screen.connect(asking.agent, "agent");
	asking.run("send");

	const std::vector<json> requests = asking.requests();
	REQUIRE(requests.size() == 1);
	CHECK(requests[0]["text"] == "Make the title blue");
	CHECK(requests[0]["from"] == "user");
	CHECK(requests[0]["source"] == "screen");
	CHECK(requests[0]["id"] == asking.state()["asked"]);

	// The agent proposes (the permissions ask for review), says it is working, then answers
	const affresco::RevisionResult proposed = asking.screen.submitRevision(
	    R"({"author": "agent/helper", "message": "Blue title", "set": {"#title/text": "Blue tasks"}})");
	REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
	asking.reply({{"request", requests[0]["id"]}, {"text", "On it"}, {"done", false}});
	asking.reply({{"request", requests[0]["id"]}, {"text", "Proposed a **blue** title"}});

	const json& replies = asking.state()["replies"];
	REQUIRE(replies.size() == 2);
	CHECK(replies[0]["done"] == false);
	CHECK(replies[0]["proposals"] == json::array({proposed.proposal})); // reported once
	CHECK(replies[1]["from"] == "agent");
	CHECK(replies[1]["text"] == "Proposed a **blue** title");
	CHECK(replies[1]["done"] == true);
	CHECK(replies[1]["error"] == "");
	CHECK(replies[1]["proposals"].empty());
	CHECK(replies[1]["request"] == requests[0]["id"]);

	asking.run("look");
	const json& seen = asking.state()["seen"];
	REQUIRE(seen.size() == 1);
	CHECK(seen[0]["state"] == "answered");
	CHECK(seen[0]["text"] == "Make the title blue");
	CHECK(seen[0]["replies"].size() == 2);
	CHECK(seen[0]["proposals"] == json::array({proposed.proposal}));
	CHECK(seen[0]["time"].is_number_integer());
	REQUIRE(asking.state()["peers"].size() == 1);
	CHECK(asking.state()["peers"][0]["author"] == "agent");

	// The person decides
	asking.run("approve");
	CHECK(asking.screen.proposals().empty());
	CHECK(asking.screen.revision() == 1);
}

TEST_CASE("assistant: a reply is never the person's decision") {
	std::string screen = askingScreen;
	const std::string handler = R"("ask:reply": [{ "push": "state.replies", "value": "{{event}}" }])";
	screen.replace(screen.find(handler), handler.size(),
	               R"("ask:reply": [{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }])");
	Asking asking(screen.c_str());
	asking.screen.connect(asking.agent, "agent");
	asking.run("send");
	REQUIRE(asking.screen.submitRevision(R"({"author": "agent", "set": {"#title/text": "Mine"}})").status ==
	        affresco::RevisionStatus::Proposed);
	asking.reply({{"request", asking.state()["asked"]}, {"text", "Accept it"}});

	CHECK(asking.screen.proposals().size() == 1);
	CHECK(asking.screen.revision() == 0);
	CHECK(std::ranges::any_of(asking.agent.sent, [](const json& message) {
		return message.value("type", "") == "error" &&
		       message.value("message", "").find("never the person's decision") != std::string::npos;
	}));
}

TEST_CASE("assistant: with no agent connected, the request fails at once") {
	Asking asking;
	asking.run("send");
	const json& replies = asking.state()["replies"];
	REQUIRE(replies.size() == 1);
	CHECK(replies[0]["error"] == "no agent is connected");
	CHECK(replies[0]["request"] == asking.state()["asked"]);
	asking.run("look");
	CHECK(asking.state()["seen"][0]["state"] == "failed");
	CHECK(asking.state()["peers"].empty());
}

TEST_CASE("assistant: replies to unknown requests, and fields of the wrong type, are not the screen's") {
	Asking asking;
	asking.screen.connect(asking.agent, "agent");
	asking.run("send");
	const json id = asking.state()["asked"];

	asking.reply({{"request", "ask-0-0"}, {"text", "Not yours"}});
	asking.reply({{"request", 7}, {"text", "Not a request"}});
	CHECK(asking.state()["replies"].empty());

	asking.reply({{"request", id}, {"text", 42}, {"done", "yes"}, {"error", json::array()}});
	const json& replies = asking.state()["replies"];
	REQUIRE(replies.size() == 1);
	CHECK(replies[0]["text"] == "");
	CHECK(replies[0]["done"] == true);
	CHECK(replies[0]["error"] == "");
}

TEST_CASE("assistant: a note that the agent is working, with nothing to say yet, is no reply") {
	Asking asking;
	asking.screen.connect(asking.agent, "agent");
	asking.run("send");
	asking.reply({{"request", asking.state()["asked"]}, {"done", false}});
	CHECK(asking.state()["replies"].empty()); // no action ran
	asking.run("look");
	CHECK(asking.state()["seen"][0]["state"] == "working");
	CHECK(asking.state()["seen"][0]["replies"].empty());
}

TEST_CASE("assistant: requests and replies are bounded") {
	Asking asking;
	asking.screen.connect(asking.agent, "agent");

	SUBCASE("an empty or long request is refused") {
		for(const std::string& draft : {std::string("   "), std::string(16385, 'a')}) {
			std::string screen = askingScreen;
			const std::string set = R"("draft": "Make the title blue")";
			screen.replace(screen.find(set), set.size(), R"("draft": ")" + draft + "\"");
			Asking refused(screen.c_str());
			refused.screen.connect(refused.agent, "agent");
			refused.run("send");
			CHECK(refused.state()["asked"] == "");
			CHECK(refused.requests().empty());
		}
	}

	SUBCASE("a long reply is cut at a character boundary") {
		asking.run("send");
		std::string text(70000, 'a');
		text.replace(65535, 2, "\xc3\xa9"); // é across the limit
		asking.reply({{"request", asking.state()["asked"]}, {"text", text}});
		const std::string kept = asking.state()["replies"][0]["text"];
		CHECK(kept.size() == 65535);
		CHECK_NOTHROW((void)json(kept).dump()); // still valid UTF-8
	}

	SUBCASE("at most 64 requests are kept, the finished ones go first") {
		for(int i = 0; i < 70; ++i) asking.run("send");
		asking.run("look");
		CHECK(asking.state()["seen"].size() == 64);
	}
}
