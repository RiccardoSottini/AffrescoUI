/// @file
/// @brief The network module's sockets are the host's only: close-on-exec, so no program the host starts (a browser
/// opened for a sign-in, a shell) holds a connection or a listener.

#include "Tcp.hpp" // modules/net/src

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <thread>

#ifndef _WIN32
#include <fcntl.h>

namespace {

/// @brief How many of this process's first descriptors are open and would pass to a program it starts.
/// @return The count.
int inheritable() {
	int count = 0;
	for(int descriptor = 0; descriptor < 1024; ++descriptor) {
		const int flags = fcntl(descriptor, F_GETFD);
		if(flags >= 0 && (flags & FD_CLOEXEC) == 0) ++count;
	}
	return count;
}

} // namespace

TEST_CASE("net: a listener, its connections and outgoing ones are close-on-exec") {
	const int before = inheritable();
	std::string error;
	const std::unique_ptr<affresco::net::tcp::Listener> listener =
	    affresco::net::tcp::Listener::open("127.0.0.1", 0, error);
	REQUIRE(listener);
	std::unique_ptr<affresco::net::tcp::Connection> accepted;
	std::thread accepting([&] { accepted = listener->accept(); });
	const std::unique_ptr<affresco::net::tcp::Connection> outgoing =
	    affresco::net::tcp::Connection::open("127.0.0.1", listener->port());
	accepting.join();
	REQUIRE(outgoing);
	REQUIRE(accepted);
	CHECK(inheritable() == before);
}
#endif
