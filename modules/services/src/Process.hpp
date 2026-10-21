#pragma once

/// @file
/// @brief ChildProcess: a program started from an argument vector, with no shell, in its own process group (a job
/// object on Windows), and pipes for its stdin and stdout.

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief What to start: the program, its arguments, where and with what environment.
struct Launch {
	std::filesystem::path program;                       ///< An absolute path; never looked up in PATH.
	std::vector<std::string> arguments;                  ///< After the program's name.
	std::filesystem::path directory;                     ///< The working directory, or empty for the host's.
	std::optional<std::vector<std::string>> environment; ///< "NAME=value" entries, or std::nullopt for the host's.
};

/// @brief A running child. Reads and writes block, so each belongs on a thread of its own; end() may be called from
/// any thread and makes a blocked read return.
class ChildProcess {
public:
	/// @brief Starts a program.
	/// @param launch What to start.
	/// @return The child.
	/// @throws std::runtime_error If it can't be started (the message names the program, never its environment).
	[[nodiscard]] static std::unique_ptr<ChildProcess> start(const Launch& launch);
	/// @brief Ends the child (end()) and closes its pipes.
	~ChildProcess();

	ChildProcess(const ChildProcess&) = delete;
	ChildProcess& operator=(const ChildProcess&) = delete;
	ChildProcess(ChildProcess&&) = delete;
	ChildProcess& operator=(ChildProcess&&) = delete;

	/// @brief Writes bytes to the child's stdin, all of them.
	/// @param bytes The bytes.
	/// @return false if the child closed its stdin or ended.
	[[nodiscard]] bool write(std::string_view bytes);
	/// @brief Reads what the child wrote to its stdout.
	/// @param buffer Where the bytes go.
	/// @param size Its size.
	/// @return The count read; 0 once the child closed its stdout (or ended); -1 on an error.
	[[nodiscard]] std::ptrdiff_t read(char* buffer, std::size_t size);
	/// @brief Closes the child's stdin, so it reads the end of its input.
	void closeInput() noexcept;
	/// @brief Ends the child and everything it started, and waits for it. Once only; later calls do nothing.
	/// @return Its exit code, or -1 if it was ended by a signal or couldn't be waited for.
	int end() noexcept;
	/// @brief Waits for the child to exit by itself (after it closed its stdout), then ends what it left behind.
	/// @return Its exit code, or -1 as for end().
	int wait() noexcept;

private:
	struct Platform;
	/// @brief Takes the platform's handles.
	/// @param platform The handles.
	explicit ChildProcess(std::unique_ptr<Platform> platform) noexcept;

	std::unique_ptr<Platform> platform_; ///< The pid or process handle, the job, and the pipes.
};

} // namespace affresco::services
