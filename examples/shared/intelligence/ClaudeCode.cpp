/// @file
/// @brief The claude-code provider (ClaudeCode.hpp), on the services module's ChildProcess: an argument vector, no
/// shell, its own process group, only stdin, stdout and stderr inherited.

#include "intelligence/ClaudeCode.hpp"

#include "process/Process.hpp" // the services module's ChildProcess (its src folder, as tests/unit/forge_scm.cpp)

#include <affresco/core/Json.hpp>
#include <affresco/services/Grants.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <random>
#include <system_error>
#include <thread>

namespace examples::intelligence {

using affresco::json;

namespace {

/// The most the command line may print.
constexpr std::size_t maxOutput = std::size_t{4} << 20;

/// @brief An environment variable.
/// @param name Its name.
/// @return Its value, or "".
std::string environment(const char* name) {
#ifdef _WIN32
	char* value = nullptr;
	std::size_t length = 0;
	if(_dupenv_s(&value, &length, name) != 0 || value == nullptr) return "";
	std::string copy(value);
	std::free(value);
	return copy;
#else
	const char* value = std::getenv(name);
	return value ? value : "";
#endif
}

/// @brief An empty folder of its own in the temporary folder, removed with it.
struct EmptyFolder {
	std::filesystem::path path; ///< The folder, or empty if it couldn't be made.

	/// @brief Makes it, with a random name.
	EmptyFolder() {
		std::random_device random;
		std::error_code error;
		for(int attempt = 0; attempt < 8 && path.empty(); ++attempt) {
			const std::filesystem::path candidate = std::filesystem::temp_directory_path(error) /
			                                        ("affrescoui-claude-" + std::to_string(random()) + std::to_string(random()));
			if(!error && std::filesystem::create_directory(candidate, error) && !error) path = candidate;
		}
	}
	EmptyFolder(const EmptyFolder&) = delete;
	EmptyFolder& operator=(const EmptyFolder&) = delete;
	EmptyFolder(EmptyFolder&&) = delete;
	EmptyFolder& operator=(EmptyFolder&&) = delete;
	/// @brief Removes it.
	~EmptyFolder() {
		std::error_code error;
		if(!path.empty()) std::filesystem::remove_all(path, error);
	}
};

} // namespace

std::optional<std::filesystem::path> findClaude() {
	std::filesystem::path found = affresco::services::findProgram("claude");
	if(!found.empty()) return found;
	// An app opened from the desktop often has a short PATH: the places the installers put it
	const std::string home = environment("HOME");
	std::vector<std::filesystem::path> places{"/opt/homebrew/bin/claude", "/usr/local/bin/claude"};
	if(!home.empty()) {
		places.insert(places.begin(), std::filesystem::path(home) / ".claude" / "local" / "claude");
		places.insert(places.begin(), std::filesystem::path(home) / ".local" / "bin" / "claude");
	}
	std::error_code error;
	for(const std::filesystem::path& place : places)
		if(std::filesystem::is_regular_file(place, error)) return place;
	return std::nullopt;
}

std::vector<std::string> claudeArguments(const std::string& model, const std::string& system) {
	return {"-p",
	        "--output-format", "json",
	        "--model", model,
	        "--tools", "",                     // no tools at all: it can't run commands, read or write files
	        "--restricted",                    // and none come back from settings; their files are ignored
	        "--safe-mode",                     // no hooks, plugins, skills, CLAUDE.md, MCP servers of the person's
	        "--strict-mcp-config",             // no MCP servers but those given (none)
	        "--disable-slash-commands",
	        "--no-session-persistence",        // nothing of the screen is kept
	        "--permission-mode", "dontAsk",    // anything that would need a permission is refused
	        "--system-prompt", system};
}

ClaudeAnswer parseClaudeOutput(const std::string& output) {
	const json parsed = json::parse(output, nullptr, false);
	if(!parsed.is_object()) {
		const bool loggedOut = output.find("login") != std::string::npos || output.find("Login") != std::string::npos;
		return {false, loggedOut ? "Claude Code isn't signed in: run `claude` once in a terminal to sign in."
		                         : "Claude Code's answer isn't JSON."};
	}
	const std::string result = parsed.contains("result") && parsed["result"].is_string() ? parsed["result"].get<std::string>() : "";
	if(parsed.value("is_error", false)) {
		std::string said = result.empty() ? parsed.value("subtype", std::string("an error")) : result;
		if(said.size() > 300) said = said.substr(0, 300) + "…";
		return {false, "Claude Code failed: " + said};
	}
	return {true, result};
}

ClaudeAnswer runClaude(const std::filesystem::path& program, const std::string& model, const std::string& system,
                       const std::string& input, std::chrono::milliseconds timeout,
                       const std::function<bool()>& stopped) {
	const EmptyFolder folder;
	if(folder.path.empty()) return {false, "Can't make a temporary folder for Claude Code."};
	affresco::services::Launch launch;
	launch.program = program;
	launch.arguments = claudeArguments(model, system);
	launch.directory = folder.path;
	std::unique_ptr<affresco::services::ChildProcess> child;
	try {
		child = affresco::services::ChildProcess::start(launch);
	} catch(const std::exception&) {
		return {false, "Can't start Claude Code."};
	}

	// A watchdog ends the child at the time limit or when stopped; ending it makes a blocked read return
	std::mutex mutex;
	std::condition_variable finished;
	bool done = false;
	std::atomic<bool> ended = false;
	std::thread watchdog([&] {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::unique_lock lock(mutex);
		while(!done) {
			if(std::chrono::steady_clock::now() >= deadline || (stopped && stopped())) {
				ended = true;
				lock.unlock();
				child->end();
				return;
			}
			finished.wait_for(lock, std::chrono::milliseconds(100));
		}
	});

	const bool written = child->write(input);
	child->closeInput();
	std::string output;
	std::array<char, 65536> buffer{};
	bool tooLarge = false;
	while(written) {
		const std::ptrdiff_t count = child->read(buffer.data(), buffer.size());
		if(count <= 0) break;
		if(output.size() + static_cast<std::size_t>(count) > maxOutput) {
			tooLarge = true;
			break;
		}
		output.append(buffer.data(), static_cast<std::size_t>(count));
	}
	if(tooLarge) child->end();
	const int status = child->wait();
	{
		const std::scoped_lock lock(mutex);
		done = true;
	}
	finished.notify_all();
	watchdog.join();

	if(ended) return {false, stopped && stopped() ? "Stopped." : "Claude Code didn't answer in time."};
	if(tooLarge) return {false, "Claude Code's answer was too large."};
	ClaudeAnswer answer = parseClaudeOutput(output);
	if(answer.ok && status != 0) return {false, "Claude Code stopped with status " + std::to_string(status) + "."};
	return answer;
}

} // namespace examples::intelligence
