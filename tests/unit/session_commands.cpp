/// @file
/// @brief Unit tests for channel::Session's command messages (getCommands, runCommand, undoRun, allowRun, denyRun,
/// revokeRun) over a fake CommandService: what the session checks, what it passes on, and what it limits.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/core/Commands.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief A command service that records what it was asked, and answers what a test gives it.
class FakeService final : public affresco::CommandService {
public:
	/// @brief One call, as the service heard it.
	struct Call {
		std::string kind = {};         ///< "list", "run", "undo" or "decide".
		std::string author = {};       ///< Who asked.
		json message = json::object(); ///< What it was asked (list: `filter` and `all`).
	};

	mutable std::vector<Call> calls = {}; ///< Every call, oldest first.
	json listed = json::array();          ///< The commands list() gives.
	json ranAnswer = {{"type", "ran"}, {"run", 1}, {"changed", json::array()}, {"changes", 0}}; ///< run()'s answer.
	std::vector<Answer> answers = {}; ///< The answers run() was given, to answer later.

	/// @brief Records the call.
	/// @return The commands the test gave.
	[[nodiscard]] json list(std::string_view author, std::string_view filter, bool all) const override {
		calls.push_back({"list", std::string(author), {{"filter", filter}, {"all", all}}});
		return {{"type", "commands"}, {"commands", listed}};
	}
	/// @brief Records the call, keeps the answer and answers ranAnswer.
	void run(std::string_view author, const json& message, Answer answer) override {
		calls.push_back({"run", std::string(author), message});
		answers.push_back(answer);
		answer(ranAnswer);
	}
	/// @brief Records the call.
	/// @return "undone".
	json undo(std::string_view author, const json& message) override {
		calls.push_back({"undo", std::string(author), message});
		return {{"type", "undone"}, {"run", message["run"]}, {"changed", json::array({1})}, {"changes", 1}};
	}
	/// @brief Records the call.
	/// @return "done".
	json decide(std::string_view reviewer, const json& message) override {
		calls.push_back({"decide", std::string(reviewer), message});
		return {{"type", "done"}};
	}
	/// @brief No runs.
	/// @return Empty lists.
	[[nodiscard]] json pending(std::string_view /*reader*/) const override {
		return {{"runs", json::array()}, {"runGrants", json::array()}};
	}
};

/// @brief A screen whose studio reviews and whose agent reads only the theme and the "open" action.
/// @return The document.
json limitedScreen() {
	json document = affresco::test::permissionsScreen();
	document["state"]["theme"] = "dark";
	document["actions"] = {{"open", json::array()}, {"secret", json::array()}};
	document["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", {"/state/theme", "/actions/open"}}}});
	return document;
}

/// @brief A screen, a peer on it, and the service.
struct Fixture {
	affresco::format::Rules rules;                                                  ///< The format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< The peer's end.
	FakeService service;                                                            ///< The commands.
	affresco::channel::Session session;                                             ///< The protocol.

	/// @brief Connects the peer as @p author, with the service.
	/// @param author The channel's author.
	/// @param limits What the peer may send.
	explicit Fixture(std::string author = "agent", const affresco::channel::PeerLimits& limits = {})
	    : session(channel, versions, std::move(author), {}, nullptr, limits) {
		session.setCommands(&service);
	}

	/// @brief Sends a message as the peer and gives the last answer.
	/// @param message The message.
	/// @return The answer.
	json ask(const json& message) {
		channel.receive(message.dump());
		return channel.sent.back();
	}
};

} // namespace

TEST_CASE("channel: the command messages are refused while the host runs no commands") {
	Fixture f;
	f.session.setCommands(nullptr);
	for(const char* type : {"getCommands", "runCommand", "undoRun", "allowRun", "denyRun", "revokeRun"}) {
		CAPTURE(type);
		const json answer = f.ask({{"type", type}, {"id", 7}});
		CHECK(answer["id"] == 7);
		CHECK(answer["error"].get<std::string>().contains("this host runs no commands"));
	}
	CHECK(f.service.calls.empty());
}

TEST_CASE("channel: getCommands passes its filter and all to the service, as the peer's author") {
	Fixture f;
	f.service.listed = json::array({{{"name", "open"}}});
	const json answer =
	    f.ask({{"type", "getCommands"}, {"id", 1}, {"author", "claude"}, {"filter", "Op"}, {"all", true}});
	CHECK(answer["type"] == "commands");
	CHECK(answer["id"] == 1);
	CHECK(answer["revision"].is_number_integer());
	CHECK(answer["commands"].size() == 1);
	REQUIRE(f.service.calls.size() == 1);
	CHECK(f.service.calls[0].author == "agent/claude");
	CHECK(f.service.calls[0].message == json{{"filter", "Op"}, {"all", true}});

	CHECK(f.ask({{"type", "getCommands"}, {"filter", 3}})["error"].get<std::string>().contains("getCommands.filter"));
	CHECK(f.ask({{"type", "getCommands"}, {"filter", std::string(257, 'a')}})["error"].get<std::string>().contains(
	    "at most 256 bytes"));
	CHECK(f.ask({{"type", "getCommands"}, {"all", "yes"}})["error"].get<std::string>().contains("getCommands.all"));
}

TEST_CASE("channel: runCommand is checked field by field, and the service hears only the fields it knows") {
	Fixture f;
	const auto error = [&f](json message) {
		message["type"] = "runCommand";
		return f.ask(message)["error"].get<std::string>();
	};
	CHECK(error({}).contains("runCommand.command: expected text"));
	CHECK(error({{"command", ""}}).contains("runCommand.command"));
	CHECK(error({{"command", std::string(1025, 'c')}}).contains("at most 1,024 bytes"));
	CHECK(error({{"command", "a"}, {"arguments", json::array()}}).contains("runCommand.arguments: expected an object"));
	CHECK(error({{"command", "a"}, {"evidence", "video"}}).contains("runCommand.evidence"));
	CHECK(error({{"command", "a"}, {"scale", 0}}).contains("runCommand.scale"));
	CHECK(error({{"command", "a"}, {"scale", 1.5}}).contains("runCommand.scale"));
	CHECK(f.service.calls.empty());

	const json answer = f.ask({{"type", "runCommand"}, {"id", "r"}, {"command", "open"}, {"extra", "dropped"}});
	CHECK(answer["type"] == "ran");
	CHECK(answer["id"] == "r");
	CHECK_FALSE(answer.contains("changes")); // `changed` says it, and more
	REQUIRE(f.service.calls.size() == 1);
	CHECK(f.service.calls[0].message ==
	      json{{"command", "open"}, {"arguments", json::object()}, {"evidence", "state"}, {"scale", 1}});
}

TEST_CASE("channel: runCommand's arguments are held to the peer's limit, said in bytes") {
	affresco::channel::PeerLimits limits;
	limits.maxArgumentBytes = 16;
	Fixture f("agent", limits);
	const json answer =
	    f.ask({{"type", "runCommand"}, {"command", "a"}, {"arguments", {{"text", std::string(40, 'x')}}}});
	CHECK(answer["error"].get<std::string>().contains("runCommand.arguments"));
	CHECK(answer["error"].get<std::string>().contains("at most 16"));
	CHECK(f.service.calls.empty());
}

TEST_CASE("channel: runs and undos share a rate, and the run past it is refused") {
	affresco::channel::PeerLimits limits;
	limits.runBurst = 2;
	Fixture f("agent", limits);
	CHECK(f.ask({{"type", "runCommand"}, {"command", "a"}})["type"] == "ran");
	CHECK(f.ask({{"type", "undoRun"}, {"run", 1}})["type"] == "undone");
	CHECK(f.ask({{"type", "runCommand"}, {"command", "a"}})["error"].get<std::string>().contains("too many runs"));
	CHECK(f.service.calls.size() == 2);
}

TEST_CASE("channel: undoRun names a run, and passes it on as the peer's author") {
	Fixture f;
	CHECK(f.ask({{"type", "undoRun"}})["error"].get<std::string>().contains("undoRun.run"));
	CHECK(f.ask({{"type", "undoRun"}, {"run", 0}})["error"].get<std::string>().contains("undoRun.run"));
	CHECK(f.ask({{"type", "undoRun"}, {"run", "3"}})["error"].get<std::string>().contains("undoRun.run"));
	const json answer = f.ask({{"type", "undoRun"}, {"run", 3}, {"author", "bot"}});
	CHECK(answer["type"] == "undone");
	REQUIRE(f.service.calls.size() == 1);
	CHECK(f.service.calls[0].author == "agent/bot");
	CHECK(f.service.calls[0].message == json{{"type", "undoRun"}, {"run", 3}});
}

TEST_CASE("channel: an attack: a peer that may not review can't allow, deny or revoke a run") {
	Fixture f;
	for(const json& message : {json{{"type", "allowRun"}, {"run", 1}}, json{{"type", "denyRun"}, {"run", 1}},
	                           json{{"type", "revokeRun"}, {"rule", "agent:open"}}}) {
		CAPTURE(message);
		CHECK(f.ask(message)["error"].get<std::string>().contains("permission denied: 'agent' may not review runs"));
	}
	CHECK(f.service.calls.empty());
}

TEST_CASE("channel: a reviewer's decisions are checked and passed on with only their fields") {
	Fixture f("studio");
	CHECK(f.ask({{"type", "allowRun"}, {"run", 2}, {"always", true}, {"for", "8h"}, {"x", 1}})["type"] == "done");
	CHECK(f.ask({{"type", "denyRun"}, {"run", 2}, {"reason", "not now"}})["type"] == "done");
	CHECK(f.ask({{"type", "revokeRun"}, {"rule", "agent:open"}})["type"] == "done");
	CHECK(f.ask({{"type", "allowRun"}, {"run", 2}, {"always", 1}})["error"].get<std::string>().contains(
	    "allowRun.always"));
	CHECK(f.ask({{"type", "revokeRun"}})["error"].get<std::string>().contains("revokeRun.rule"));
	REQUIRE(f.service.calls.size() == 3);
	CHECK(f.service.calls[0].message == json{{"type", "allowRun"}, {"run", 2}, {"always", true}, {"for", "8h"}});
	CHECK(f.service.calls[1].message == json{{"type", "denyRun"}, {"run", 2}, {"reason", "not now"}});
	CHECK(f.service.calls[2].message == json{{"type", "revokeRun"}, {"rule", "agent:open"}});
}

TEST_CASE("channel: a run that waits is answered again with the id that asked, and denied when the peer leaves") {
	Fixture f;
	f.service.ranAnswer = {{"type", "waiting"}, {"run", 4}, {"reason", "a reviewer decides"}, {"expires", 120}};
	CHECK(f.ask({{"type", "runCommand"}, {"id", 9}, {"command", "open"}})["type"] == "waiting");
	REQUIRE(f.service.answers.size() == 1);
	f.service.answers[0]({{"type", "rejected"}, {"run", 4}, {"error", "denied"}});
	CHECK(f.channel.sent.back()["id"] == 9);
	CHECK(f.channel.sent.back()["run"] == 4);

	CHECK(f.ask({{"type", "runCommand"}, {"id", 10}, {"command", "open"}})["type"] == "waiting");
	f.service.calls.clear();
	f.session.setCommands(nullptr);
	REQUIRE(f.service.calls.size() == 1); // the run answered already is not denied again
	CHECK(f.service.calls[0].kind == "decide");
	CHECK(f.service.calls[0].author == "agent");
	CHECK(f.service.calls[0].message == json{{"type", "denyRun"}, {"run", 4}, {"reason", "the peer went away"}});

	const std::size_t sent = f.channel.sent.size();
	f.service.answers[1]({{"type", "ran"}, {"run", 4}});
	CHECK(f.channel.sent.size() == sent); // a session that left hears nothing more
}

TEST_CASE("channel: a reader whose reads are limited sees only the commands it may read, and no evidence") {
	affresco::format::Rules rules;
	affresco::vcs::Repository versions{rules, limitedScreen()};
	MemoryChannel channel;
	FakeService service;
	affresco::channel::Session session(channel, versions, "agent");
	session.setCommands(&service);
	service.listed = json::array({{{"name", "open"}}, {{"name", "secret"}}});
	service.ranAnswer = {{"type", "ran"},
	                     {"run", 1},
	                     {"changed", json::array({1})},
	                     {"changes", 1},
	                     {"region", {}},
	                     {"picture", "x"},
	                     {"width", 1},
	                     {"height", 1},
	                     {"scale", 1},
	                     {"errors", json::array({"expected a list, got \"s3cret\""})}};

	channel.receive(json{{"type", "getCommands"}}.dump());
	CHECK(channel.sent.back()["commands"] == json::array({{{"name", "open"}}}));

	channel.receive(json{{"type", "runCommand"}, {"command", "open"}, {"evidence", "picture"}}.dump());
	const json ran = channel.sent.back();
	CHECK(service.calls.back().message["evidence"] == "none");
	CHECK(ran["changes"] == 1);
	for(const char* field : {"changed", "region", "picture", "width", "height", "scale", "errors"}) {
		CAPTURE(field);
		CHECK_FALSE(ran.contains(field));
	}
	channel.receive(json{{"type", "undoRun"}, {"run", 1}}.dump());
	CHECK_FALSE(channel.sent.back().contains("changed"));
	CHECK(channel.sent.back()["changes"] == 1);

	// An attack: a run that failed while it ran says why in words that can quote what it read
	service.ranAnswer = {
	    {"type", "rejected"}, {"run", 2}, {"failed", true}, {"error", "expected a list, got \"s3cret\""}};
	channel.receive(json{{"type", "runCommand"}, {"command", "open"}}.dump());
	CHECK(channel.sent.back()["error"] == "the run failed while it ran");
	service.ranAnswer = {{"type", "rejected"}, {"error", "runCommand.arguments: expected an object"}};
	channel.receive(json{{"type", "runCommand"}, {"command", "open"}}.dump());
	CHECK(channel.sent.back()["error"] == "runCommand.arguments: expected an object"); // a refusal before it ran
}
