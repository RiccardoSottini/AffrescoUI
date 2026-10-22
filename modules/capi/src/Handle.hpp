#pragma once

/// @file
/// @brief What an afr_screen holds, and what every function of the C API shares: the last error, argument checks, and
/// the guard that keeps C++ exceptions from crossing into C.

#include "affresco/affresco.h"

#include "Headless.hpp" // the render module's manual clock and frame drawing, as automation uses them

#include "affresco/Screen.hpp"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::capi {

/// @brief The longest JSON text, path or name accepted from the host, in bytes.
inline constexpr std::size_t maximumText = std::size_t{1} << 30;

/// @brief The longest path given to get_state() and set_state(), in bytes.
inline constexpr std::size_t maximumPath = 4096;

/// @brief A failure to report to C: its status and message.
struct Failure {
	afr_status status;   ///< What afr_status the call returns.
	std::string message; ///< What afr_last_error() says.
};

/// @brief Records why the current call failed, for afr_last_error().
/// @param status The status.
/// @param message Why.
/// @return @p status.
afr_status fail(afr_status status, std::string_view message) noexcept;

/// @brief Clears the last error: the current call has succeeded so far.
void clearError() noexcept;

/// @brief Reports the exception being handled: a Failure as it is, DocumentError as AFR_ERROR_DOCUMENT, a JSON error
/// as AFR_ERROR_ARGUMENT, anything else as AFR_ERROR_INTERNAL.
/// @return Its status.
afr_status reportException() noexcept;

/// @brief Runs a call's body, turning every exception into a status and a message.
/// @param body Returns the call's status; may throw.
/// @return Its status, or the exception's.
template <typename Body> afr_status guarded(Body&& body) noexcept {
	try {
		clearError();
		return body();
	} catch(...) {
		return reportException();
	}
}

/// @brief Checks a NUL-terminated string from the host: present, at most @p limit bytes.
/// @param text The string.
/// @param what Its name, for the error.
/// @param limit Its longest length.
/// @return It.
/// @throws Failure If it is NULL or too long.
std::string_view checkedString(const char* text, std::string_view what, std::size_t limit = maximumText);

/// @brief Checks a string given with its length: present (unless empty), at most maximumText bytes.
/// @param text The string.
/// @param length Its length in bytes.
/// @param what Its name, for the error.
/// @return It.
/// @throws Failure If it is NULL with a length, or too long.
std::string_view checkedText(const char* text, std::size_t length, std::string_view what);

/// @brief Copies a string into memory the caller frees with afr_free().
/// @param text The string.
/// @param out Receives it, NUL-terminated.
/// @throws std::bad_alloc If there is no memory.
void copyOut(std::string_view text, char** out);

/// @brief Throws a Failure if a required pointer is NULL.
/// @param pointer The pointer.
/// @param what Its name.
/// @throws Failure If it is NULL.
void require(const void* pointer, std::string_view what);

/// @brief Reads an enum a C caller passed as the int it holds. C allows any int there, but in C++ a value outside the
/// enum's range is undefined even to compare, so the check that refuses it reads it this way.
/// @tparam Enum The enum, the size of an int.
/// @param value What was passed.
/// @return The int it holds.
template <typename Enum> int fromC(const Enum& value) noexcept {
	static_assert(sizeof(Enum) == sizeof(int));
	int raw = 0;
	std::memcpy(&raw, &value, sizeof raw);
	return raw;
}

/// @brief The work posted to a headless screen, run in afr_screen_step(): its Executor.
class TaskQueue final : public Executor {
public:
	/// @brief Queues a task. Safe from any thread.
	/// @param task The task.
	void post(std::function<void()> task) override;

	/// @brief Runs the tasks queued so far; the ones they post wait for the next call.
	void run();

private:
	std::mutex mutex_;                           ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_;   ///< Queued.
	std::vector<std::function<void()>> running_; ///< Being run.
};

} // namespace affresco::capi

/// @brief A native call in progress: what the host answered.
struct afr_call {
	std::string result = "null";      ///< The value, JSON text.
	std::optional<std::string> error; ///< The error, if the host failed it.
};

/// @brief A screen and what shows it. The members are destroyed in reverse: the screen before its window.
struct afr_screen {
	afr_mode mode = AFR_MODE_WINDOW;                  ///< Where it is shown.
	std::filesystem::path socketDirectory;            ///< Where listen() puts its sockets.
	float scale = 1.0f;                               ///< Headless: pixels per window unit; embedded: the last frame's.
	affresco::SteadyClock steadyClock;                ///< Embedded: real time.
	affresco::detail::ManualClock manualClock;        ///< Headless: moved by afr_screen_step().
	affresco::capi::TaskQueue tasks;                  ///< Headless: its executor.
	std::optional<affresco::Application> application; ///< Window and headless.
	std::unique_ptr<affresco::Embedding> embedding;   ///< Embedded.
	affresco::Window* window = nullptr;               ///< The window, whichever owns it.
	afr_event_callback onEvent = nullptr;             ///< See afr_screen_on_event().
	void* onEventUser = nullptr;                      ///< Given to onEvent.
	std::optional<affresco::Screen> screen;           ///< The screen; destroyed first.

	/// @brief Where work for the screen's thread is posted, in its mode.
	/// @return The executor.
	[[nodiscard]] affresco::Executor& executor() noexcept;
};
