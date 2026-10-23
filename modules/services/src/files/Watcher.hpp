#pragma once

/// @file
/// @brief Watcher: the changes under a folder, from FSEvents (macOS), inotify (Linux) or ReadDirectoryChangesW
/// (Windows), filtered by ignore globs and debounced into bounded batches of relative paths.

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace affresco::services {

constexpr std::size_t mostChangesPerBatch = 1000;  ///< More paths than this in one batch become a rescan.
constexpr std::size_t mostWatchedFolders = 100000; ///< Folders watched one by one (inotify), at most.

/// @brief Whether a path matches a glob: `*` and `?` stay within one name, `**` spans names. A glob without a `/`
/// matches any one name of the path (`node_modules`, `*.o`); one with a `/` matches the whole path (`build/**`).
/// @param glob The glob.
/// @param path A relative path, with `/` between names.
/// @return Whether it matches.
[[nodiscard]] bool globMatches(std::string_view glob, std::string_view path) noexcept;

/// @brief Whether any of some globs matches a path (globMatches()).
/// @param globs The globs.
/// @param path A relative path, with `/` between names.
/// @return Whether one does.
[[nodiscard]] bool ignoredBy(const std::vector<std::string>& globs, std::string_view path) noexcept;

/// @brief A path as UTF-8 with `/` between names, on every platform.
/// @param path The path.
/// @return Its text.
[[nodiscard]] std::string genericUtf8(const std::filesystem::path& path);

/// @brief A path from UTF-8 text with `/` between names.
/// @param text The text.
/// @return The path.
[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view text);

/// @brief What a Watcher watches and how it batches.
struct WatchOptions {
	std::chrono::milliseconds debounce{100}; ///< Quiet time before a batch goes out (and at least between two).
	std::vector<std::string> ignore;         ///< Globs of paths that are neither reported nor descended into.
};

/// @brief Some changes: the paths that changed (created, written, removed or renamed), or "look at everything".
struct WatchBatch {
	std::vector<std::string> paths; ///< Relative to the root, sorted, each once.
	bool rescan = false;            ///< Events were lost (too many, or the system dropped some): look again.
};

/// @brief Where a platform's backend sends what it sees. Called on the backend's own thread.
struct WatchSink {
	std::function<void(const std::filesystem::path& path)> changed; ///< A path changed (absolute).
	std::function<void()> overflowed;                               ///< Events were lost.
	std::function<bool(const std::filesystem::path& path)> ignored; ///< Whether a path (absolute) is ignored.
};

/// @brief One platform's way of watching a folder and everything under it. Its destructor stops and joins.
class WatchBackend {
public:
	WatchBackend() = default;
	WatchBackend(const WatchBackend&) = delete;
	WatchBackend& operator=(const WatchBackend&) = delete;
	WatchBackend(WatchBackend&&) = delete;
	WatchBackend& operator=(WatchBackend&&) = delete;
	virtual ~WatchBackend() = default;
	/// @brief Whether it watches everything (inotify may run out of watches).
	/// @return Whether it does.
	[[nodiscard]] virtual bool complete() const noexcept { return true; }
};

/// @brief Starts this platform's backend.
/// @param root The folder (canonical).
/// @param sink Where changes go.
/// @param latency How long the system may hold events back.
/// @return The backend.
/// @throws std::runtime_error If the folder can't be watched.
[[nodiscard]] std::unique_ptr<WatchBackend> startWatchBackend(const std::filesystem::path& root, WatchSink sink,
                                                              std::chrono::milliseconds latency);

/// @brief Watches a folder and everything under it, and hands debounced batches to a callback on a thread of its own.
///
/// A batch goes out once no change came for the debounce time, or a second after its first change, and never two
/// within the debounce time: so at most 1000 / debounce batches a second, each of at most mostChangesPerBatch paths.
class Watcher {
public:
	/// @brief Starts watching.
	/// @param root The folder (canonical).
	/// @param options What to ignore, how to batch.
	/// @param onBatch Gets each batch, on the watcher's thread.
	/// @throws std::runtime_error If the folder can't be watched.
	Watcher(std::filesystem::path root, WatchOptions options, std::function<void(WatchBatch)> onBatch);
	/// @brief Stops watching, and waits for a batch being handed out.
	~Watcher();
	Watcher(const Watcher&) = delete;
	Watcher& operator=(const Watcher&) = delete;
	Watcher(Watcher&&) = delete;
	Watcher& operator=(Watcher&&) = delete;

	/// @brief Whether a relative path is ignored by the options' globs.
	/// @param relative The path.
	/// @return Whether it is.
	[[nodiscard]] bool ignored(std::string_view relative) const noexcept;
	/// @brief Whether everything under the root is watched.
	/// @return Whether it is.
	[[nodiscard]] bool complete() const noexcept { return backend_->complete(); }

private:
	/// @brief Takes a changed path from the backend.
	/// @param path The path (absolute).
	void changed(const std::filesystem::path& path);
	/// @brief Takes an overflow from the backend.
	void overflowed();
	/// @brief Hands out batches until stopped.
	/// @param stop Says to stop.
	void run(const std::stop_token& stop);

	std::filesystem::path root_;                         ///< See the constructor.
	WatchOptions options_;                               ///< See the constructor.
	std::function<void(WatchBatch)> onBatch_;            ///< See the constructor.
	std::mutex mutex_;                                   ///< Guards the pending changes.
	std::condition_variable_any noticed_;                ///< Signalled at each change.
	std::set<std::string> pending_;                      ///< Paths changed since the last batch.
	bool rescan_ = false;                                ///< Whether the pending batch is a rescan.
	std::chrono::steady_clock::time_point first_, last_; ///< The pending batch's first and last change.
	std::jthread thread_;                   ///< Hands out batches (before the backend, so it ends after it).
	std::unique_ptr<WatchBackend> backend_; ///< The platform's backend (last, so it stops first).
};

} // namespace affresco::services
