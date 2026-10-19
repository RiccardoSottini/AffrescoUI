/// @file
/// @brief Fuzzes channel::Session, the live protocol, with raw lines from two peers of the Kanban template: an agent
/// and the reviewer ("studio"), each through an in-memory channel.
///
/// The input is lines. Each goes to the agent, or to the reviewer when it starts with '>' (taken off). A peer's
/// message, however malformed, must get an answer and never take the app down, and everything a session sends must
/// be one JSON object with a "type".

#include "Fuzz.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/core/Channel.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using affresco::json;

namespace {

/// @brief A Channel kept in memory that checks every message a session sends, and drops it.
class CheckedChannel final : public affresco::Channel {
public:
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
	}

	/// @brief Delivers a line as if the peer had sent it.
	/// @param line The line, without its line break.
	void receive(const std::string& line) { onMessage(line); }
};

/// @brief The Kanban template under version control, and its two peers. Members are destroyed in reverse order, so
/// the sessions go before the channels and the repository they use.
struct Peers {
	affresco::format::Rules rules;                                               ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::fuzz::kanbanTemplate()}; ///< The screen.
	CheckedChannel agentChannel, studioChannel;                                  ///< What each peer is sent.
	affresco::channel::Session agent{agentChannel, versions, "agent"};           ///< May propose; asks for /actions.
	affresco::channel::Session studio{studioChannel, versions, "studio"};        ///< Changes anything and reviews.
};

} // namespace

/// @brief Silences the log before the first input.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point: feeds the input to the two peers, one line at a time.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	Peers peers; // new ones: inputs don't add up
	std::string_view rest = affresco::fuzz::text(data, size);
	while(!rest.empty()) {
		const std::size_t end = rest.find('\n');
		std::string_view line = rest.substr(0, end);
		rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
		if(line.starts_with('>')) peers.studioChannel.receive(std::string(line.substr(1)));
		else peers.agentChannel.receive(std::string(line));
	}
	return 0;
}
