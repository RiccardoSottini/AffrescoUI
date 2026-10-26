#pragma once

/// @file
/// @brief What several unit tests share: a small screen with permissions, a Channel kept in memory, and an Executor
/// that runs posted tasks when a test asks.

#include <affresco/core/Channel.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::test {

/// @brief A screen with three nodes and permissions: the studio changes anything and reviews; the agent may change
/// #board only; nobody may change #statusBar.
/// @return The document.
inline json permissionsScreen() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "count": 0 },
		"permissions": {
			"allow": ["studio", "agent(#board)"],
			"deny": ["*(#statusBar)"],
			"reviewers": ["studio"]
		},
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "panel", "id": "board", "color": "#ffffff" },
			{ "type": "label", "id": "title", "text": "Tasks" },
			{ "type": "panel", "id": "statusBar", "color": "#202020" }
		] }
	})json");
}

/// @brief A Channel that keeps what is sent, for a test to read; receive() plays a message from the other end.
class MemoryChannel final : public Channel {
public:
	/// @brief Keeps a message.
	/// @param message The message.
	void send(std::string_view message) override { sent.push_back(json::parse(message)); }

	/// @brief Delivers a message as if the other end had sent it.
	/// @param line The message, without a line break.
	void receive(const std::string& line) { onMessage(line); }

	std::vector<json> sent; ///< Every message sent, parsed, oldest first.
};

/// @brief An Executor for tests without an Application: tasks wait until runPosted(). Main thread only.
class QueueExecutor final : public Executor {
public:
	/// @brief Keeps a task.
	/// @param task The task.
	void post(std::function<void()> task) override { tasks_.push_back(std::move(task)); }

	/// @brief Runs the tasks posted so far, and those they post.
	void runPosted() {
		while(!tasks_.empty()) {
			std::vector<std::function<void()>> tasks;
			tasks.swap(tasks_);
			for(auto& task : tasks) task();
		}
	}

private:
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

/// Whether this build runs under a sanitizer, which checks memory but slows the code down: timing checks skip it.
/// @return True under AddressSanitizer (GCC or clang).
constexpr bool underSanitizer() {
#if defined(__SANITIZE_ADDRESS__)
	return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
	return true;
#else
	return false;
#endif
#else
	return false;
#endif
}

/// @brief Whether this build's times mean anything: an optimised build (NDEBUG) without a sanitizer. A Debug build,
/// such as the quality report's with coverage, runs several times slower.
/// @return true if a test may hold the build to a time.
constexpr bool timedBuild() {
#ifdef NDEBUG
	return !underSanitizer();
#else
	return false;
#endif
}

/// The time limit of a test that checks its cost grows no faster than its size: the seconds given in an optimised
/// build, 24 times as many in a debug or sanitized one, which runs up to 40 times slower (where the cost the test
/// guards against takes many minutes).
/// @param seconds The limit in an optimised build.
/// @return The limit in this build.
constexpr double timeLimit(double seconds) {
#ifdef NDEBUG
	return underSanitizer() ? seconds * 24 : seconds;
#else
	return seconds * 24;
#endif
}

} // namespace affresco::test
