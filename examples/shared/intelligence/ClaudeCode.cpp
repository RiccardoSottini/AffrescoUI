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
			const std::filesystem::path candidate =
			    std::filesystem::temp_directory_path(error) /
			    ("affrescoui-claude-" + std::to_string(random()) + std::to_string(random()));
			if(!error && std::filesystem::create_directory(candidate, error) && !error) {
				std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
				                             std::filesystem::perm_options::replace, error); // this user's only
				path = candidate;
			}
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

std::vector<std::string> claudeEnvironment() {
	static constexpr std::array names{
#ifdef _WIN32
	    "PATH", "USERPROFILE", "APPDATA", "LOCALAPPDATA", "SystemRoot", "TEMP", "TMP", "CLAUDE_CONFIG_DIR",
#else
	    "HOME",   "PATH",     "USER",        "LOGNAME",    "TMPDIR",     "CLAUDE_CONFIG_DIR", "LANG",
	    "LC_ALL", "LC_CTYPE", "LC_MESSAGES", "LC_COLLATE", "LC_NUMERIC", "LC_TIME",           "LC_MONETARY",
#endif
	};
	std::vector<std::string> entries;
	for(const char* name : names)
		if(const std::string value = environment(name); !value.empty())
			entries.push_back(std::string(name) + "=" + value);
	entries.emplace_back("DISABLE_AUTOUPDATER=1");
	entries.emplace_back("ENABLE_CLAUDEAI_MCP_SERVERS=false");
	return entries;
}

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
	        "--output-format",
	        "json",
	        "--model",
	        model,
	        "--tools",
	        "",                    // no tools at all: it can't run commands, read or write files
	        "--restricted",        // and none come back from settings; their files are ignored
	        "--safe-mode",         // no hooks, plugins, skills, CLAUDE.md, MCP servers of the person's
	        "--strict-mcp-config", // no MCP servers but those given (none)
	        "--disable-slash-commands",
	        "--no-session-persistence", // nothing of the screen is kept
	        "--permission-mode",
	        "dontAsk", // anything that would need a permission is refused
	        "--system-prompt",
	        system};
}

ClaudeAnswer parseClaudeOutput(const std::string& output) {
	const json parsed = json::parse(output, nullptr, false);
	if(!parsed.is_object()) {
		const bool loggedOut = output.find("login") != std::string::npos || output.find("Login") != std::string::npos;
		return {false, loggedOut ? "Claude Code isn't signed in: run `claude` once in a terminal to sign in."
		                         : "Claude Code's answer isn't JSON."};
	}
	const std::string result =
	    parsed.contains("result") && parsed["result"].is_string() ? parsed["result"].get<std::string>() : "";
	const bool failed =
	    parsed.contains("is_error") && parsed["is_error"].is_boolean() && parsed["is_error"].get<bool>();
	if(failed) {
		const bool subtype = parsed.contains("subtype") && parsed["subtype"].is_string();
		std::string said = !result.empty() ? result : subtype ? parsed["subtype"].get<std::string>() : "an error";
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
	launch.environment = claudeEnvironment(); // what it needs, never the host's keys or AffrescoUI's settings
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
	// A program it started that left its group may keep its stdout open after it ended: the read never waits for
	// more than 100 ms, so the watchdog's end is always seen
	while(written && !ended) {
		if(!child->readable(std::chrono::milliseconds(100))) continue;
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
