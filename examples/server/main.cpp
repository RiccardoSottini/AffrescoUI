/// @file
/// @brief affrescoui_server: the document server, headless (examples/server/README.md).
///
///   affrescoui_server --folder DIR [--listen 127.0.0.1:7420] [--allow-remote] [--origin URL]...
///                     [--userinfo URL --people FILE]
///   affrescoui_server --folder DIR --add-token NAME

#include "DocumentServer.hpp"
#include "UserInfoVerifier.hpp"

#include <affresco/core/Executor.hpp>

#include <algorithm>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <pthread.h>
#endif

namespace {

/// @brief The main thread's queue: tasks posted from any thread run in order on the thread that calls run().
class MainLoop final : public affresco::Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		{
			const std::scoped_lock lock(mutex_);
			tasks_.push_back(std::move(task));
		}
		ready_.notify_one();
	}
	/// @brief Runs tasks until stop().
	void run() {
		for(;;) {
			std::function<void()> task;
			{
				std::unique_lock lock(mutex_);
				ready_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
				if(stopped_) return;
				task = std::move(tasks_.front());
				tasks_.pop_front();
			}
			task();
		}
	}
	/// @brief Makes run() return.
	void stop() {
		{
			const std::scoped_lock lock(mutex_);
			stopped_ = true;
		}
		ready_.notify_one();
	}

private:
	std::mutex mutex_;                         ///< Guards what follows.
	std::condition_variable ready_;            ///< Signalled on post() and stop().
	std::deque<std::function<void()>> tasks_; ///< Waiting.
	bool stopped_ = false;                     ///< See stop().
};

/// @brief The command line.
struct Options {
	std::filesystem::path folder;              ///< --folder.
	std::string listen = "127.0.0.1:7420";     ///< --listen.
	std::string addToken;                      ///< --add-token.
	std::string userinfo;                      ///< --userinfo: OAuth access tokens are checked there.
	std::filesystem::path people;              ///< --people: who, by OAuth subject, may connect.
	affresco::net::PeerServerOptions server;   ///< --allow-remote, --origin.
};

/// @brief Reads the command line.
/// @param arguments The arguments, without the program's name.
/// @return The options, or std::nullopt after printing the usage.
std::optional<Options> parse(const std::vector<std::string>& arguments) {
	Options options;
	for(std::size_t i = 0; i < arguments.size(); ++i) {
		const std::string& argument = arguments[i];
		const bool hasValue = i + 1 < arguments.size();
		if(argument == "--folder" && hasValue) options.folder = arguments[++i];
		else if(argument == "--listen" && hasValue) options.listen = arguments[++i];
		else if(argument == "--add-token" && hasValue) options.addToken = arguments[++i];
		else if(argument == "--userinfo" && hasValue) options.userinfo = arguments[++i];
		else if(argument == "--people" && hasValue) options.people = arguments[++i];
		else if(argument == "--origin" && hasValue) options.server.allowedOrigins.push_back(arguments[++i]);
		else if(argument == "--allow-remote") options.server.allowRemote = true;
		else {
			options.folder.clear();
			break;
		}
	}
	if(!options.folder.empty() && options.userinfo.empty() == options.people.empty()) return options;
	std::cerr << "usage: affrescoui_server --folder DIR [--listen 127.0.0.1:7420] [--allow-remote] [--origin URL]...\n"
	             "                         [--userinfo URL --people FILE]\n"
	             "       affrescoui_server --folder DIR --add-token NAME\n";
	return std::nullopt;
}

/// @brief Serves until SIGINT or SIGTERM (POSIX), or until the process is ended (Windows).
/// @param options The options.
/// @return The exit status.
int serve(const Options& options) {
	MainLoop loop;
	using namespace affresco::server;
	const std::filesystem::path tokenPath = options.folder / "tokens";
	auto tokens = options.userinfo.empty() || std::filesystem::exists(tokenPath)
	                  ? std::make_shared<TokenFile>(tokenPath)
	                  : nullptr;
	auto userinfo = options.userinfo.empty() ? nullptr
	                                         : std::make_shared<UserInfoVerifier>(options.userinfo, options.people);
	std::vector<std::shared_ptr<affresco::net::TokenVerifier>> verifiers;
	if(tokens) verifiers.push_back(tokens);
	if(userinfo) verifiers.push_back(userinfo);
	const auto people = [tokens, userinfo] {
		std::vector<std::string> names = tokens ? tokens->people() : std::vector<std::string>();
		for(const std::string& name : userinfo ? userinfo->people() : std::vector<std::string>())
			if(std::ranges::find(names, name) == names.end()) names.push_back(name);
		return names;
	};
	DocumentServer server(loop, options.folder, std::make_shared<ChainVerifier>(std::move(verifiers)), people,
	                      options.listen, options.server);
	std::cout << "serving " << server.documents().size() << " documents on port " << server.port() << std::endl;
#ifndef _WIN32
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);
	std::jthread waiter([&loop, signals] {
		int received = 0;
		sigwait(&signals, &received);
		loop.post([&loop] { loop.stop(); });
	});
#endif
	loop.run();
	return 0;
}

} // namespace

/// @brief Serves the documents, or adds a token.
/// @param argc The argument count.
/// @param argv The arguments.
/// @return The exit status.
int main(int argc, char** argv) {
#ifndef _WIN32
	sigset_t signals; // blocked in every thread, so sigwait() receives them
	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &signals, nullptr);
#endif
	const auto options = parse(std::vector<std::string>(argv + 1, argv + argc));
	if(!options) return 2;
	try {
		if(!options->addToken.empty()) {
			std::filesystem::create_directories(options->folder);
			const std::string token = affresco::server::TokenFile::add(options->folder / "tokens", options->addToken);
			std::cout << token << std::endl; // shown once: only its hash is kept
			return 0;
		}
		return serve(*options);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_server: " << error.what() << std::endl;
		return 1;
	}
}
