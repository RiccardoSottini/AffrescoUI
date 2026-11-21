/// @file
/// @brief channel::Session's "getPicks" and "getSource" messages: the elements the person pointed at, and where a
/// node is written. The session knows nothing of windows or nodes: it checks the shapes and asks the host's
/// callbacks (setPicks(), setSource()), which only the host can answer.

#include "affresco/channel/Session.hpp"

#include <cstddef>
#include <string>

namespace affresco::channel {

namespace {

/// The longest `at` a "getSource" may name, in bytes: an id, a pointer or an address is far shorter.
constexpr std::size_t longestAt = 512;

/// @brief Reads a "getSource"'s `at`.
/// @param message The message.
/// @return The node it names: "#id", a JSON Pointer or a definition's address.
/// @throws DocumentError If it isn't a text of 1 to longestAt bytes.
std::string atOf(const json& message) {
	const auto at = message.find("at");
	if(at == message.end() || !at->is_string() || at->get_ref<const std::string&>().empty() ||
	   at->get_ref<const std::string&>().size() > longestAt)
		expect::fail("getSource.at", R"(expected a node ("#id"), a JSON Pointer or a definition's address)");
	return at->get<std::string>();
}

} // namespace

// getPicks and getSource: reads, which change nothing; any peer may make them unless its reads are limited
bool Session::handlePickRead(const Incoming& in) {
	if(in.type == "getPicks") {
		if(!picks_) expect::fail("getPicks", "this host keeps no picks");
		send({{"type", "picks"}, {"picks", picks_()}, {"id", in.id}});
	} else if(in.type == "getSource") {
		const std::string at = atOf(in.message);
		if(!source_) expect::fail("getSource", "this host can't tell a node's source");
		send({{"type", "source"}, {"source", source_(at)}, {"id", in.id}});
	} else {
		return false;
	}
	return true;
}

} // namespace affresco::channel
