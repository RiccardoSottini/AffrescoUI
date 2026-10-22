/// @file
/// @brief LoopbackRedirect: a browser sign-in's one-time redirect listener (LoopbackRedirect.hpp).

#include "oauth/LoopbackRedirect.hpp"

#include "oauth/OAuth.hpp"
#include "Tcp.hpp"

#include <chrono>
#include <string_view>

namespace affresco::net {

namespace {

constexpr std::size_t maxRequestBytes = 8192;              ///< The largest request head taken.
constexpr std::chrono::seconds requestTimeout{10};         ///< The longest a request may take to arrive.
constexpr std::string_view callbackPath = "/callback";     ///< The one path answered.

/// @brief A whole response with a small HTML page.
/// @param status The status line's code and reason.
/// @param message The page's text.
/// @return The bytes.
std::string page(std::string_view status, std::string_view message) {
	const std::string body = "<!doctype html><meta charset=\"utf-8\"><title>Sign-in</title><p>" + std::string(message) +
	                         "</p>";
	return "HTTP/1.1 " + std::string(status) + "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
	       std::to_string(body.size()) +
	       "\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n\r\n" + body;
}

} // namespace

std::unique_ptr<LoopbackRedirect> LoopbackRedirect::open(Executor& executor, std::string state, Received received,
                                                         std::string& error) {
	auto listener = tcp::Listener::open("127.0.0.1", 0, error);
	if(!listener) return nullptr;
	return std::unique_ptr<LoopbackRedirect>(
	    new LoopbackRedirect(executor, std::move(listener), std::move(state), std::move(received)));
}

LoopbackRedirect::LoopbackRedirect(Executor& executor, std::unique_ptr<tcp::Listener> listener, std::string state,
                                   Received received)
    : executor_(executor), listener_(std::move(listener)), state_(std::move(state)), received_(std::move(received)),
      thread_([this] { serve(); }) {}

LoopbackRedirect::~LoopbackRedirect() {
	alive_.reset();
	listener_->close();
	const std::scoped_lock lock(mutex_);
	if(current_) current_->close();
}

std::string LoopbackRedirect::uri() const {
	return "http://127.0.0.1:" + std::to_string(listener_->port()) + std::string(callbackPath);
}

void LoopbackRedirect::serve() {
	while(auto connection = listener_->accept()) {
		{
			const std::scoped_lock lock(mutex_);
			current_ = connection.get();
		}
		const bool done = answer(*connection);
		{
			const std::scoped_lock lock(mutex_);
			current_ = nullptr;
		}
		connection->close();
		if(done) return;
	}
}

bool LoopbackRedirect::answer(tcp::Connection& connection) {
	std::string head;
	const auto deadline = std::chrono::steady_clock::now() + requestTimeout;
	while(!head.contains("\r\n\r\n")) {
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if(head.size() > maxRequestBytes || left.count() <= 0 || connection.receive(head, left) != tcp::Received::Data)
			return false;
	}
	const std::string_view line = std::string_view(head).substr(0, head.find("\r\n"));
	const std::size_t space = line.find(' ', 4);
	const std::string_view target = line.starts_with("GET ") && space != std::string_view::npos
	                                    ? line.substr(4, space - 4)
	                                    : std::string_view();
	const std::string_view path = target.substr(0, target.find('?'));
	if(path != callbackPath) {
		static_cast<void>(connection.send(page("404 Not Found", "Not found."), requestTimeout));
		return false;
	}
	const std::string_view query = target.size() > path.size() ? target.substr(path.size() + 1) : std::string_view();
	const auto state = oauth::queryField(query, "state");
	if(!state || !oauth::equalInConstantTime(*state, state_)) {
		// Any program on this machine can reach the listener, so a forged redirect is refused without ending the
		// sign-in that was started: the browser's own redirect may still come
		static_cast<void>(connection.send(page("400 Bad Request", "This sign-in isn't the one started."), requestTimeout));
		return false;
	}
	const auto code = oauth::queryField(query, "code");
	if(!code || code->empty() || code->size() > oauth::maxTokenBytes) {
		static_cast<void>(connection.send(page("400 Bad Request", "The sign-in was refused."), requestTimeout));
		const auto why = oauth::queryField(query, "error");
		deliver(std::nullopt, "the provider refused the sign-in" + (why && why->size() <= 64 ? " (" + *why + ")" : ""));
		return true;
	}
	static_cast<void>(
	    connection.send(page("200 OK", "Signed in. You can close this window and go back to the application."),
	                    requestTimeout));
	deliver(code, {});
	return true;
}

void LoopbackRedirect::deliver(std::optional<std::string> code, std::string error) {
	executor_.post([alive = std::weak_ptr<bool>(alive_), received = received_, code = std::move(code),
	                error = std::move(error)]() mutable {
		if(alive.lock()) received(std::move(code), std::move(error));
	});
}

} // namespace affresco::net
