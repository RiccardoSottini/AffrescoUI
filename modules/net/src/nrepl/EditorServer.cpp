/// @file
/// @brief EditorServer (affresco/net/EditorServer.hpp): the listener on 127.0.0.1 and its connections. A stub until
/// the wire's package (f2b) fills it: it never listens, and nothing calls it yet.

#include "affresco/net/EditorServer.hpp"

#include <stdexcept>
#include <utility>

namespace affresco::net {

/// @brief The listener and the connections: none yet.
struct EditorServer::Impl {
	EditorHost host;             ///< The program and the pairing question.
	EditorServerOptions options; ///< The port and the limits.
	std::uint16_t port = 0;      ///< The port listened on: none.
};

EditorServer::EditorServer(Executor& executor, EditorHost host, EditorServerOptions options)
    : impl_(std::make_unique<Impl>(Impl{.host = std::move(host), .options = std::move(options), .port = 0})) {
	static_cast<void>(executor);
	throw std::runtime_error("the editor server isn't built yet");
}

EditorServer::~EditorServer() = default;

std::uint16_t EditorServer::port() const noexcept { return impl_->port; }

} // namespace affresco::net
