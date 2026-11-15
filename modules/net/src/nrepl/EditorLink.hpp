#pragma once

/// @file
/// @brief The seam between the editor server's wire and its protocol: the wire (EditorServer.cpp,
/// EditorConnection.cpp) decodes each connection's bytes and hands every message to its EditorPeer; the peer
/// (EditorPeer.cpp and the files beside it) answers the nREPL operations through its EditorLink. Main thread only.

#include "Bencode.hpp"

#include "affresco/net/EditorServer.hpp"

#include <cstddef>
#include <memory>
#include <string_view>

namespace affresco::net {

/// @brief One connection, as the protocol sees it: where its answers go. Implemented by the wire; it outlives the
/// connection's EditorPeer.
class EditorLink {
public:
	virtual ~EditorLink() = default;

	/// @brief Sends a message to the editor, after the ones sent before it.
	/// @param message The message, a dictionary.
	virtual void send(const bencode::Value& message) = 0;

	/// @brief Closes the connection, once what was sent before has gone. The peer hears closed() afterwards.
	/// @param why Why, for the log (never a message's content).
	virtual void close(std::string_view why) = 0;
};

/// @brief One connection, as the wire sees it: what it does with each message. Implemented by the protocol.
class EditorPeer {
public:
	virtual ~EditorPeer() = default;

	/// @brief Takes one message the editor sent, decoded within the server's limits. Called by the wire, on the main
	/// thread, in the order the messages came.
	/// @param message The message (not necessarily a dictionary: the peer checks).
	virtual void receive(const bencode::Value& message) = 0;

	/// @brief The connection closed, whoever closed it: nothing more is received, and nothing sent arrives. Called
	/// once, before the peer is destroyed.
	virtual void closed() = 0;
};

/// @brief What the connections of one server share, owned by the server and outliving its peers.
struct EditorShared {
	std::size_t refusals = 0; ///< Pairings the person refused or left unanswered (EditorServerOptions::maxRefusals).
	bool asking = false;      ///< A pairing question is open: one at a time.
};

/// @brief Makes the protocol's side of a new connection.
/// @param link Where its answers go; outlives the peer.
/// @param host The host's program and pairing question; outlives the peer.
/// @param options The server's limits; outlive the peer.
/// @param shared What the server's connections share; outlives the peer.
/// @return The peer.
[[nodiscard]] std::unique_ptr<EditorPeer> makeEditorPeer(EditorLink& link, EditorHost& host,
                                                         const EditorServerOptions& options, EditorShared& shared);

} // namespace affresco::net
