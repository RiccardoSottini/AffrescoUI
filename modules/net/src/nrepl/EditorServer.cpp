/// @file
/// @brief EditorServer (affresco/net/EditorServer.hpp): an EditorListener (EditorConnection.hpp) whose connections
/// each get the nREPL protocol's peer (makeEditorPeer).
///
/// Threads and ownership: the main thread owns the server, its listener, every connection and every peer, and is the
/// only thread that calls the host. The listener's acceptor thread and each connection's reader and writer threads
/// reach it only through the executor; the destructor closes them all and joins every thread before it returns.

#include "affresco/net/EditorServer.hpp"

#include "nrepl/EditorConnection.hpp"

#include <utility>

namespace affresco::net {

/// @brief The host, the options and what the connections share, then the listener, which is destroyed first: its
/// connections' peers refer to everything above it.
struct EditorServer::Impl {
	/// @brief Listens.
	/// @param executor Runs deliveries on the main thread.
	/// @param givenHost The program and the pairing question.
	/// @param givenOptions The port and the limits.
	Impl(Executor& executor, EditorHost givenHost, EditorServerOptions givenOptions)
	    : host(std::move(givenHost)), options(std::move(givenOptions)),
	      listener(executor, options,
		           [this](EditorLink& link) { return makeEditorPeer(link, host, options, shared); }) {}

	EditorHost host;             ///< The program and the pairing question.
	EditorServerOptions options; ///< The port and the limits.
	EditorShared shared;         ///< What the connections share.
	EditorListener listener;     ///< The socket and the connections; destroyed first.
};

EditorServer::EditorServer(Executor& executor, EditorHost host, EditorServerOptions options)
    : impl_(std::make_unique<Impl>(executor, std::move(host), std::move(options))) {}

EditorServer::~EditorServer() = default;

std::uint16_t EditorServer::port() const noexcept { return impl_->listener.port(); }

} // namespace affresco::net
