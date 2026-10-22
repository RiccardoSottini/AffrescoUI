#pragma once

/// @file
/// @brief The Debug Adapter Protocol's messages, as the `dap` provider reads and writes them: an adapter's message
/// checked, its lists and texts bounded, the sources it names placed against the granted folder, the arguments of a
/// request completed, and its output events limited per second. No threads and no I/O, so tests and fuzzers reach it.

#include "affresco/core/Json.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace affresco::services::dap {

inline constexpr std::size_t mostThreads = 1000;                    ///< Threads in a `threads` answer.
inline constexpr std::size_t mostFrames = 500;                      ///< Frames in a `stackTrace` answer.
inline constexpr std::size_t mostVariables = 1000;                  ///< Variables (or scopes) in one answer: one page.
inline constexpr std::size_t mostBreakpoints = 10000;               ///< Breakpoints in an answer.
inline constexpr std::size_t mostCompletions = 500;                 ///< Targets in a `completions` answer.
inline constexpr std::size_t longestValue = 4096;                   ///< A variable's value or an evaluated result.
inline constexpr std::size_t longestOutput = std::size_t{64} << 10; ///< One output event's text.
inline constexpr std::size_t longestText = 1000;                    ///< A name, a message or a path, in bytes.
inline constexpr std::size_t outputEventsPerSecond = 200;           ///< Output events let through each second.
inline constexpr std::size_t outputBytesPerSecond = std::size_t{1} << 20; ///< Their text, each second.

/// @brief A response to a request the service sent.
struct Response {
	std::int64_t requestSeq = 0; ///< The request's `seq`.
	std::string command;         ///< The request's command.
	bool success = false;        ///< Whether it succeeded.
	std::string message;         ///< Why not (the adapter's short reason or its error's text), cut and UTF-8.
	json body;                   ///< Its body, or null.
};

/// @brief An event the adapter sent.
struct Event {
	std::string event; ///< Its name, e.g. "stopped".
	json body;         ///< Its body, or null.
};

/// @brief A request the adapter sent (a reverse request, e.g. `runInTerminal`).
struct Request {
	std::int64_t seq = 0; ///< Its `seq`, which the response names.
	std::string command;  ///< Its command.
	json arguments;       ///< Its arguments, or null.
};

/// @brief One message from the adapter, or why it is malformed.
using Message = std::variant<Response, Event, Request, std::string>;

/// @brief Reads one message body from the adapter (bounded JSON, an object of a known type with the keys it needs).
/// @param body The body, as the frame reader gave it.
/// @return The message, or a string saying why it is malformed.
[[nodiscard]] Message readMessage(std::string_view body);

/// @brief Frames a request: `{"seq", "type": "request", "command", "arguments"}`.
/// @param seq Its sequence number.
/// @param command The command.
/// @param arguments Its arguments, or null for none.
/// @return The message.
[[nodiscard]] json requestMessage(std::int64_t seq, std::string_view command, const json& arguments);

/// @brief The response to a reverse request, from the screen's answer.
/// @param seq The response's own sequence number.
/// @param request The request answered.
/// @param value The screen's value, or std::nullopt if it refused.
/// @param error Why it refused.
/// @return The message.
[[nodiscard]] json responseMessage(std::int64_t seq, const Request& request, const std::optional<json>& value,
                                   const std::string& error);

/// @brief Bounds the body of a successful response: its lists cut to their limits (with `truncated` set), its long
/// texts cut, and each source it names placed (place()).
/// @param command The request's command.
/// @param body The body.
/// @param root The granted folder (canonical), or empty.
/// @return The body to give the screen.
[[nodiscard]] json boundResponse(std::string_view command, json body, const std::filesystem::path& root);

/// @brief Bounds an event's body as boundResponse() does a response's (an output's text, the source it names).
/// @param event The event's name.
/// @param body The body.
/// @param root The granted folder (canonical), or empty.
/// @return The body to give the screen.
[[nodiscard]] json boundEvent(std::string_view event, json body, const std::filesystem::path& root);

/// @brief Places a source an adapter named against the granted folder: an absolute `path` under it gains `relative`
/// (its path under the folder, with `/`) and `inside: true`; any other path gets `inside: false`, so a screen opens
/// it only when the person asks to.
/// @param source A DAP `Source` (it is left alone if it isn't an object).
/// @param root The granted folder (canonical), or empty (then nothing is inside).
void place(json& source, const std::filesystem::path& root);

/// @brief Completes a request's arguments before they are sent: `${workspaceFolder}` in a `launch` or `attach`
/// becomes the granted folder, a relative `source.path` becomes the path under it, and a `variables` or `stackTrace`
/// request asks for one page at most.
/// @param command The command.
/// @param arguments Its arguments (changed in place).
/// @param root The granted folder (canonical), or empty.
/// @return Why the request is refused (a relative path that leaves the folder, or one with no folder), or
/// std::nullopt.
[[nodiscard]] std::optional<std::string> prepareArguments(std::string_view command, json& arguments,
                                                          const std::filesystem::path& root);

/// @brief Text from the adapter for people: cut to longestText bytes (on a character's boundary) and only if it is
/// UTF-8, else the fallback.
/// @param value The value.
/// @param fallback What to say if it isn't usable text.
/// @return The text.
[[nodiscard]] std::string textOf(const json& value, std::string_view fallback);

/// @brief Lets output events through up to outputEventsPerSecond and outputBytesPerSecond, and counts those dropped.
class OutputBudget {
public:
	using Clock = std::chrono::steady_clock; ///< What the seconds are counted on.

	/// @brief Whether one more output event may pass now.
	/// @param bytes The size of its text.
	/// @param now The time.
	/// @return The number dropped since the last that passed (to say so once), or std::nullopt to drop this one.
	[[nodiscard]] std::optional<std::size_t> admit(std::size_t bytes, Clock::time_point now) noexcept;

private:
	Clock::time_point second_{}; ///< When the current second began.
	std::size_t events_ = 0;     ///< Events passed in it.
	std::size_t bytes_ = 0;      ///< Their bytes.
	std::size_t dropped_ = 0;    ///< Dropped since the last that passed.
};

} // namespace affresco::services::dap
