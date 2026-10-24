/// @file
/// @brief HTTP for a worker thread (Http.hpp).

#include "intelligence/Http.hpp"

#include <affresco/net/Client.hpp>
#include <affresco/net/Url.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

namespace examples::intelligence {

namespace network = affresco::network;

NetworkFactory systemNetwork() {
	return [](affresco::Executor& executor) -> std::unique_ptr<network::Network> {
		return std::make_unique<affresco::net::Client>(executor);
	};
}

void PumpedExecutor::post(std::function<void()> task) {
	{
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
	}
	wake_.notify_one();
}

void PumpedExecutor::pump(std::chrono::milliseconds wait) {
	std::vector<std::function<void()>> tasks;
	{
		std::unique_lock lock(mutex_);
		wake_.wait_for(lock, wait, [this] { return !tasks_.empty(); });
		tasks.assign(std::make_move_iterator(tasks_.begin()), std::make_move_iterator(tasks_.end()));
		tasks_.clear();
	}
	affresco::runTasks(tasks);
}

HttpSession::HttpSession(const NetworkFactory& factory, const std::string& origin) : network_(factory(executor_)) {
	if(const auto refused = network_->allow(origin)) throw std::runtime_error(*refused);
}

HttpSession::~HttpSession() {
	network_.reset(); // cancels what is in flight: no handler runs afterwards
	executor_.pump(std::chrono::milliseconds(0));
}

Outcome HttpSession::fetch(network::Request request, const std::function<bool()>& stopped) {
	// The handlers run on this thread (pump below), so the outcome needs no lock; it lives as long as the transfer
	auto outcome = std::make_shared<std::optional<Outcome>>();
	const auto deadline = std::chrono::steady_clock::now() + request.timeout + std::chrono::seconds(5);
	network::Handlers handlers;
	handlers.done = [outcome](network::Response response) { *outcome = std::move(response); };
	handlers.failed = [outcome](network::Failure failure) { *outcome = std::move(failure); };
	std::unique_ptr<network::Transfer> transfer = network_->start(std::move(request), std::move(handlers));
	while(!outcome->has_value()) {
		if(stopped && stopped()) return network::Failure{"cancelled", "stopped", 0, {}};
		if(std::chrono::steady_clock::now() > deadline) return network::Failure{"timeout", "no answer in time", 0, {}};
		executor_.pump(std::chrono::milliseconds(50));
	}
	return std::move(**outcome);
}

std::optional<std::string> originOf(const std::string& url) {
	const std::optional<affresco::net::Url> parsed = affresco::net::parseUrl(url);
	if(!parsed || (parsed->scheme != "https" && parsed->scheme != "http")) return std::nullopt;
	return parsed->scheme + "://" + parsed->authority();
}

} // namespace examples::intelligence
