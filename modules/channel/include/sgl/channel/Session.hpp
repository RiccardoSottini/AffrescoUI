#pragma once

#include "sgl/core/Channel.hpp"
#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"

#include <string>

namespace sgl::channel {

// The live-document protocol (SCREEN_FORMAT.md, "Live screens") over a Channel: whatever is on the other end reads
// the document, sends revisions to a VersionControl, and (if the document lets it review) decides on proposals and
// switches auto-accept. It is told about every commit and every change to the proposals.
//
// Everything arriving here is authored by `author` (the name the document's permissions know it by), whatever name
// the sender gives itself: a channel can't claim to be someone it isn't. Both `channel` and `versions` must outlive
// the session.
//
// "request" and "reply" messages are not interpreted: they come out of onRelay, stamped with "from", for whoever
// connects peers to each other (e.g. a client's request in plain words, for an AI agent on another session).
class Session {
public:
	Session(Channel& channel, VersionControl& versions, std::string author); // sends "hello"
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	void send(const json& message);
	[[nodiscard]] const std::string& author() const noexcept { return author_; }

	Signal<const json&> onRelay;

private:
	void handle(const std::string& line);
	[[nodiscard]] json reviewState() const;

	Channel& channel_;
	VersionControl& versions_;
	std::string author_;
	Connection messages_ = 0, decisions_ = 0, commits_ = 0, reviews_ = 0;
};

} // namespace sgl::channel
