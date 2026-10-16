#pragma once

/// @file
/// @brief channel::Session: the live-document protocol over a Channel.

#include "sgl/core/Channel.hpp"
#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"

#include <string>

namespace sgl::channel {

/// @brief The live-document protocol (SCREEN_FORMAT.md, "Live screens") over a Channel.
///
/// The peer on the other end reads the document and its history, sends revisions and, if the document lets it
/// review, decides on proposals, sets the mode and revokes grants. It is told about every commit and every change
/// to the proposals.
/// Everything it sends is authored by `author` (followed by the name it gives itself): a channel can't claim to be
/// someone it isn't. "request" and "reply" messages are not interpreted: they come out of onRelay, stamped with
/// "from", for whoever connects peers to each other.
class Session {
public:
	/// @brief Starts the protocol and sends "hello".
	/// @param channel The transport; must outlive the session.
	/// @param versions The document; must outlive the session.
	/// @param author The name the document's permissions know this peer by.
	Session(Channel& channel, VersionControl& versions, std::string author);
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	/// @brief Sends a message to the peer.
	/// @param message One protocol message.
	void send(const json& message);
	/// @brief The name this peer is authored as.
	[[nodiscard]] const std::string& author() const noexcept { return author_; }

	Signal<const json&> onRelay; ///< A "request" or "reply" for other peers, with "from" set.

private:
	/// @brief Handles one message from the peer; malformed or refused ones get an "error" reply.
	/// @param line The message.
	void handle(const std::string& line);
	/// @brief The "review" message: proposals, preview, mode and grants.
	[[nodiscard]] json reviewState() const;

	Channel& channel_;                                                    ///< The transport.
	VersionControl& versions_;                                            ///< The document.
	std::string author_;                                                  ///< See author().
	Connection messages_ = 0, decisions_ = 0, commits_ = 0, reviews_ = 0; ///< Disconnected on destruction.
};

} // namespace sgl::channel
