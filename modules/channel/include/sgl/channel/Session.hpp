#pragma once

#include "sgl/core/Channel.hpp"
#include "sgl/core/Json.hpp"
#include "sgl/core/VersionControl.hpp"

#include <string>

namespace sgl::channel {

// The live-document protocol (SCREEN_FORMAT.md, "Live screens") over a Channel: whatever is on the other end reads
// the document and sends revisions to a VersionControl; it hears back about proposals the user decided on.
//
// Everything arriving here is authored by `author` (the name the document's permissions know it by), whatever name
// the sender gives itself: a channel can't claim to be someone it isn't. Both `channel` and `versions` must outlive
// the session.
class Session {
public:
	Session(Channel& channel, VersionControl& versions, std::string author); // sends "hello"
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	void send(const json& message);

private:
	void handle(const std::string& line);

	Channel& channel_;
	VersionControl& versions_;
	std::string author_;
	Connection messages_ = 0;
	Connection decisions_ = 0;
};

} // namespace sgl::channel
