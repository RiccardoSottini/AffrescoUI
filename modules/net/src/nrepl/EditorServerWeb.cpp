/// @file
/// @brief EditorServer (affresco/net/EditorServer.hpp) in a browser, in place of the nrepl/ sources
/// (cmake/AffrescoUIWeb.cmake): a page can't listen on a port, so it never listens.

#include "affresco/net/EditorServer.hpp"

#include <stdexcept>
#include <utility>

namespace affresco::net {

/// @brief Nothing: there is no listener in a browser.
struct EditorServer::Impl {
	EditorHost host;             ///< The program and the pairing question.
	EditorServerOptions options; ///< The port and the limits.
	std::uint16_t port = 0;      ///< The port listened on: none.
};

EditorServer::EditorServer(Executor& executor, EditorHost host, EditorServerOptions options)
    : impl_(std::make_unique<Impl>(Impl{.host = std::move(host), .options = std::move(options), .port = 0})) {
	static_cast<void>(executor);
	throw std::runtime_error("a browser can't listen for an editor");
}

EditorServer::~EditorServer() = default;

std::uint16_t EditorServer::port() const noexcept { return impl_->port; }

} // namespace affresco::net
