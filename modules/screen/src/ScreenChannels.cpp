/// @file
/// @brief affresco::Screen's channels: the ones its file declares ("channels"), and the peers it connects or listens
/// for.

#include "ScreenImpl.hpp"

#include <string_view>
#include <utility>

namespace affresco {

namespace {

/// @brief Whether a "channels" address is a remote peer's (net::connectPeer()) rather than a local socket's.
/// @param address The address.
/// @return Whether it starts with ws:// or wss://.
bool isPeerAddress(std::string_view address) noexcept {
	return address.starts_with("ws://") || address.starts_with("wss://");
}

} // namespace

void Screen::Impl::run(const std::string& action, const json& event) {
	if(Window* window = runtime.window(); window && runtime.hasAction(action))
		runtime.runAction(action, window->root(), event);
}

void Screen::Impl::open(DeclaredChannel& channel) {
	if(!executor) return;
	if(Window* window = runtime.window(); window && channel.retry) window->cancelTimer(std::exchange(channel.retry, 0));
	if(!tryConnect(channel)) {
		retry(channel);
		return;
	}
	watch(channel);
}

bool Screen::Impl::tryConnect(DeclaredChannel& channel) {
	if(!channel.url.empty()) {
		channel.remote = net::connectPeer(*executor, channel.url, channel.token);
		if(channel.remote) return true;
		run(channel.name + ":closed", {{"type", "closed"},
		                               {"message", "Can't connect to " + channel.url +
		                                               ": it takes a wss:// address (ws:// on the loopback) "
		                                               "and a token from the host"}});
		return false;
	}
	try {
		channel.socket = channel::SocketChannel::connect(*executor, channel.path);
	} catch(const std::exception& error) {
		run(channel.name + ":closed", {{"type", "closed"}, {"message", error.what()}});
		return false;
	}
	return true;
}

void Screen::Impl::watch(DeclaredChannel& channel) {
	Channel& connected = channel.remote ? *channel.remote : static_cast<Channel&>(*channel.socket);
	const std::string name = channel.name;
	connected.onMessage.connect([this, name](const std::string& line) {
		json message;
		try {
			message = parseUntrusted(line);
		} catch(const std::exception&) {
			return; // not a message: nothing to run
		}
		if(message.is_object() && message.contains("type") && message["type"].is_string())
			run(name + ":" + message["type"].get<std::string>(), message);
	});
	connected.onClosed.connect([this, name, guard = alive] {
		// Dropped after the signal that says so has finished
		executor->post([this, name, guard] {
			if(!*guard) return;
			DeclaredChannel& c = channels.at(name);
			c.socket.reset();
			c.remote.reset();
			run(name + ":closed", {{"type", "closed"}, {"message", "The other end closed the connection"}});
			retry(c);
		});
	});
	run(name + ":open", {{"type", "open"}});
}

void Screen::Impl::retry(DeclaredChannel& channel) {
	Window* window = runtime.window();
	if(!window || channel.reconnectMs <= 0.0 || channel.retry) return;
	channel.retry = window->setTimer(channel.reconnectMs, [this, name = channel.name] {
		DeclaredChannel& c = channels.at(name);
		c.retry = 0;
		if(!c.socket && !c.remote) open(c);
	});
}

void Screen::Impl::send(const std::string& name, const json& message) {
	const auto it = channels.find(name);
	if(it == channels.end()) {
		runtime.onError("AffrescoUI screen: send: no channel '" + name + "' in \"channels\"");
		return;
	}
	if(it->second.remote) { // a remote peer's channel: messages wait for its handshake
		it->second.remote->send(message.dump());
		return;
	}
	if(!it->second.socket || !it->second.socket->isOpen()) {
		run(name + ":error", {{"type", "error"}, {"message", "Not connected to '" + name + "'"}});
		return;
	}
	it->second.socket->send(message.dump());
}

void Screen::setSocketDirectory(std::filesystem::path directory) { impl_->socketDirectory = std::move(directory); }

bool Screen::connectChannel(const std::string& name, std::string address, std::string token) {
	Impl& impl = *impl_;
	const auto it = impl.channels.find(name);
	if(it == impl.channels.end()) return false;
	Impl::DeclaredChannel& channel = it->second;
	if(Window* window = impl.runtime.window(); window && channel.retry)
		window->cancelTimer(std::exchange(channel.retry, 0));
	channel.socket.reset(); // no handler runs after: the signals go with their channel
	channel.remote.reset();
	channel.url.clear();
	channel.token.clear();
	if(isPeerAddress(address)) {
		channel.url = std::move(address);
		channel.token = std::move(token);
	} else {
		const std::filesystem::path socket = address;
		channel.path = socket.is_absolute() ? socket : impl.socketDirectory / socket;
	}
	impl.open(channel);
	return true;
}

void Screen::startChannels(Executor& executor) {
	Impl& impl = *impl_;
	impl.executor = &executor;
	impl.services.setExecutor(&executor);
	impl.relay.set(&executor);
	const json& document = impl.repository.document();
	if(!document.contains("channels")) return;
	for(const auto& [name, spec] : document["channels"].items()) {
		if(impl.channels.contains(name)) continue;
		Impl::DeclaredChannel& channel = impl.channels[name];
		channel.name = name;
		const std::string address = spec["socket"].get<std::string>();
		const std::filesystem::path socket = address;
		if(isPeerAddress(address)) channel.url = address;
		else channel.path = socket.is_absolute() ? socket : impl.socketDirectory / socket;
		const json& reconnect = spec.value("reconnect", json(false));
		channel.reconnectMs = reconnect.is_object()   ? reconnect.value("after", 2000.0)
		                      : reconnect == true     ? 2000.0
		                      : reconnect.is_number() ? reconnect.get<double>()
		                                              : 0.0;
		impl.open(channel);
	}
}

/* Channel */

void Screen::connect(Channel& channel, std::string author) { impl_->add(channel, nullptr, std::move(author)); }

void Screen::listen(Executor& executor, const std::filesystem::path& path, std::string author) {
	Impl* impl = impl_.get();
	impl->servers.push_back(std::make_unique<channel::SocketServer>(
	    executor, path, [impl, &executor, author = std::move(author)](std::unique_ptr<Channel> connection) {
		    impl->adopt(executor, std::move(connection), author);
	    }));
}

std::uint16_t Screen::listenWebSocket(Executor& executor, std::string_view address,
                                      std::shared_ptr<net::TokenVerifier> verifier, net::PeerServerOptions options) {
	Impl* impl = impl_.get();
	auto server = std::make_unique<net::PeerServer>(
	    executor, address, std::move(verifier),
	    [impl, &executor](std::unique_ptr<Channel> connection, std::string author) {
		    impl->adopt(executor, std::move(connection), std::move(author));
	    },
	    std::move(options));
	const std::uint16_t port = server->port();
	impl->peerServers.push_back(std::move(server));
	return port;
}

} // namespace affresco
