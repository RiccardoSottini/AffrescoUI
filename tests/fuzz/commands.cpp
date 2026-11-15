/// @file
/// @brief Fuzzes channel::Session's command messages (getCommands, runCommand, undoRun, allowRun, denyRun,
/// revokeRun) with raw lines from three peers of the JSON template, over a command service that keeps every run
/// waiting until the input says.
///
/// The input is lines. Each goes to the agent; to the reviewer ("studio") when it starts with '>'; to a reader whose
/// reads are limited ("guest") when it starts with '?'. A line "!" answers every run that waits, "~" takes the
/// service away from the sessions or gives it back. Every message a session sends must be one JSON object with a
/// "type"; the service must never hear a decision from a peer that may not review, nor a run over the argument limit;
/// and the limited reader must never be sent what a run changed, a picture, or a command its reads don't show. An
/// input's first maxLines lines are delivered.

#include "Fuzz.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/core/Channel.hpp>
#include <affresco/core/Commands.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using affresco::json;

namespace {

/// The largest `arguments` the peers may send, in bytes: small, so inputs reach it.
constexpr std::size_t maxArgumentBytes = 512;
/// How many lines of an input are delivered.
constexpr int maxLines = 32;

/// @brief A Channel kept in memory that checks every message a session sends, and drops it.
class CheckedChannel final : public affresco::Channel {
public:
	/// @brief Checks the channel of a reader whose reads are limited, too.
	/// @param limited Whether its reads are limited.
	explicit CheckedChannel(bool limited = false) : limited_(limited) {}

	/// @brief Checks a message from the session.
	/// @param message The message.
	void send(std::string_view message) override {
		json parsed;
		try {
			parsed = json::parse(message);
		} catch(const json::parse_error&) {
			affresco::fuzz::broken("a session sends JSON");
		}
		if(!parsed.is_object() || !parsed.contains("type") || !parsed["type"].is_string())
			affresco::fuzz::broken("a session sends objects with a \"type\"");
		if(limited_) checkLimited(parsed);
	}

	/// @brief Delivers a line as if the peer had sent it.
	/// @param line The line, without its line break.
	void receive(const std::string& line) { onMessage(line); }

private:
	/// @brief Checks that a limited reader is sent nothing it may not read.
	/// @param message The message.
	static void checkLimited(const json& message) {
		for(const char* field : {"changed", "picture", "region"})
			if(message.contains(field) && message["type"] != "review" && message["type"] != "document")
				affresco::fuzz::broken("a limited reader is never sent what a run changed, nor a picture");
		if(message["type"] == "commands")
			for(const json& command : message.value("commands", json::array()))
				if(command.value("name", "") != "shown")
					affresco::fuzz::broken("a limited reader is sent only the commands its reads show");
	}

	bool limited_ = false; ///< Whether the peer's reads are limited.
};

/// @brief A command service that lists two commands, keeps every run waiting until answer(), and checks what the
/// sessions pass on.
class FuzzService final : public affresco::CommandService {
public:
	/// @brief Lists a command every reader may see and one the limited reader may not.
	/// @return The commands.
	[[nodiscard]] json list(std::string_view /*author*/, std::string_view /*filter*/, bool /*all*/) const override {
		return {
		    {"type", "commands"},
		    {"commands", json::array({{{"name", "shown"}, {"run", "ask"}}, {{"name", "hidden"}, {"run", "allow"}}})}};
	}
	/// @brief Keeps the run waiting.
	/// @param message The runCommand, as the session checked it.
	/// @param answer Gives its answers.
	void run(std::string_view /*author*/, const json& message, Answer answer) override {
		const json& arguments = message.value("arguments", json());
		if(!arguments.is_object() ||
		   arguments.dump(-1, ' ', false, json::error_handler_t::replace).size() > maxArgumentBytes)
			affresco::fuzz::broken("the service hears only arguments within the limit");
		const int run = static_cast<int>(waiting_.size()) + 1;
		answer({{"type", "waiting"}, {"run", run}, {"reason", "fuzz"}, {"expires", 120}});
		waiting_.emplace_back(run, std::move(answer));
	}
	/// @brief Answers every run that waits: "ran", with what changed and a picture.
	void answer() {
		for(auto& [run, give] : std::exchange(waiting_, {}))
			give({{"type", "ran"},
			      {"run", run},
			      {"changed", json::array({{{"op", "replace"}, {"path", "/state/x"}}})},
			      {"changes", 1},
			      {"picture", "AA=="},
			      {"region", {{"x", 0}}}});
	}
	/// @brief Undoes nothing.
	/// @return "undone".
	json undo(std::string_view /*author*/, const json& message) override {
		return {{"type", "undone"}, {"run", message.value("run", 0)}, {"changed", json::array()}, {"changes", 0}};
	}
	/// @brief Checks the decision comes from the reviewer, or from a session withdrawing its own runs.
	/// @param reviewer Who decided.
	/// @param message The decision.
	/// @return "done".
	json decide(std::string_view reviewer, const json& message) override {
		const bool withdrawn =
		    message.value("type", "") == "denyRun" && message.value("reason", "") == "the peer went away";
		if(!reviewer.starts_with("studio") && !withdrawn)
			affresco::fuzz::broken("only a peer that may review decides on runs");
		return {{"type", "done"}};
	}
	/// @brief No runs.
	/// @return Empty lists.
	[[nodiscard]] json pending(std::string_view /*reader*/) const override {
		return {{"runs", json::array()}, {"runGrants", json::array()}};
	}

private:
	std::vector<std::pair<int, Answer>> waiting_; ///< The runs that wait, and how to answer them.
};

/// @brief Limits small enough for an input to reach them, and that never refill while it runs.
/// @return The limits.
affresco::channel::PeerLimits fuzzLimits() {
	affresco::channel::PeerLimits limits;
	limits.maxMessageBytes = 4096;
	limits.messagesPerSecond = 1e-9;
	limits.messageBurst = 24;
	limits.runsPerMinute = 1e-9;
	limits.runBurst = 8;
	limits.maxArgumentBytes = maxArgumentBytes;
	return limits;
}

/// @brief The JSON template, whose guest reads only /actions/shown.
/// @return The document.
json fuzzScreen() {
	json document = affresco::fuzz::jsonTemplate();
	document["actions"]["shown"] = json::array();
	document["permissions"]["data"] = json::array({{{"author", "guest"}, {"read", {"/actions/shown"}}}});
	return document;
}

/// @brief The screen under version control, its three peers and the service. Members are destroyed in reverse
/// order, so the sessions go before the channels, the service and the repository they use.
struct Peers {
	affresco::format::Rules rules;                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, fuzzScreen()};        ///< The screen.
	FuzzService service;                                            ///< The commands.
	CheckedChannel agentChannel, studioChannel, guestChannel{true}; ///< What each peer is sent.
	affresco::channel::Session agent{agentChannel, versions, "agent", {}, nullptr, fuzzLimits()};    ///< Runs.
	affresco::channel::Session studio{studioChannel, versions, "studio", {}, nullptr, fuzzLimits()}; ///< Reviews.
	affresco::channel::Session guest{guestChannel, versions, "guest", {}, nullptr, fuzzLimits()};    ///< Reads little.

	/// @brief Gives the sessions the service.
	Peers() { offer(true); }

	/// @brief Gives the sessions the service, or takes it away.
	/// @param on Whether to give it.
	void offer(bool on) {
		for(affresco::channel::Session* session : std::array{&agent, &studio, &guest})
			session->setCommands(on ? &service : nullptr);
	}
};

} // namespace

/// @brief Silences the log before the first input.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: feeds the input to the three peers, one line at a time.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	Peers peers; // new ones: inputs don't add up
	bool offered = true;
	std::string_view rest = affresco::fuzz::text(data, size);
	for(int lines = 0; !rest.empty() && lines < maxLines; ++lines) {
		const std::size_t end = rest.find('\n');
		const std::string_view line = rest.substr(0, end);
		rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
		if(line == "!") peers.service.answer();
		else if(line == "~") peers.offer(offered = !offered);
		else if(line.starts_with('>')) peers.studioChannel.receive(std::string(line.substr(1)));
		else if(line.starts_with('?')) peers.guestChannel.receive(std::string(line.substr(1)));
		else peers.agentChannel.receive(std::string(line));
	}
	return 0;
}
