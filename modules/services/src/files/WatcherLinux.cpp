/// @file
/// @brief The Linux watch backend: one inotify watch per folder (at most mostWatchedFolders), read on a thread of
/// its own that a pipe wakes to stop. New folders get watches as they appear; a queue overflow is a rescan.

#include "files/Watcher.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <unordered_map>

namespace affresco::services {

namespace {

constexpr std::uint32_t watchedEvents = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB |
                                        IN_CLOSE_WRITE | IN_DELETE_SELF | IN_ONLYDIR;

/// @brief A file descriptor, closed when it goes.
struct Descriptor {
	int value = -1; ///< The descriptor, or -1.
	/// @brief Takes a descriptor.
	/// @param descriptor It.
	explicit Descriptor(int descriptor) noexcept : value(descriptor) {}
	/// @brief Closes it.
	~Descriptor() {
		if(value >= 0) close(value);
	}
	Descriptor(const Descriptor&) = delete;
	Descriptor& operator=(const Descriptor&) = delete;
	Descriptor(Descriptor&&) = delete;
	Descriptor& operator=(Descriptor&&) = delete;
};

/// @brief inotify watches over a tree, and the thread that reads them.
class InotifyBackend final : public WatchBackend {
public:
	/// @brief Watches every folder under the root, then starts reading.
	/// @param root The folder.
	/// @param sink Where changes go.
	/// @throws std::runtime_error If inotify can't start.
	InotifyBackend(const std::filesystem::path& root, WatchSink sink)
	    : sink_(std::move(sink)), inotify_(inotify_init1(IN_NONBLOCK | IN_CLOEXEC)) {
		// Close-on-exec like inotify's descriptor: a program the host starts must not hold the pipe open
		if(inotify_.value < 0 || pipe2(stopPipe_.data(), O_CLOEXEC) != 0)
			throw std::runtime_error("the folder can't be watched");
		wake_ = std::make_unique<Descriptor>(stopPipe_[1]);
		sleep_ = std::make_unique<Descriptor>(stopPipe_[0]);
		if(!watchTree(root)) throw std::runtime_error("the folder can't be watched");
		thread_ = std::jthread([this](const std::stop_token& stop) { read(stop); });
	}
	/// @brief Wakes the thread and joins it.
	~InotifyBackend() override {
		thread_.request_stop();
		const char byte = 0;
		[[maybe_unused]] const auto written = ::write(wake_->value, &byte, 1);
		if(thread_.joinable()) thread_.join();
	}
	InotifyBackend(const InotifyBackend&) = delete;
	InotifyBackend& operator=(const InotifyBackend&) = delete;
	InotifyBackend(InotifyBackend&&) = delete;
	InotifyBackend& operator=(InotifyBackend&&) = delete;

	[[nodiscard]] bool complete() const noexcept override { return complete_; }

private:
	/// @brief Watches a folder and the folders under it that aren't ignored.
	/// @param folder The folder.
	/// @return Whether the folder itself is watched.
	bool watchTree(const std::filesystem::path& folder) {
		if(!watchFolder(folder)) return false;
		std::error_code error;
		std::filesystem::recursive_directory_iterator entry(folder, error), end;
		for(; !error && entry != end; entry.increment(error)) {
			if(!entry->is_directory(error) || entry->is_symlink(error)) continue;
			if(sink_.ignored(entry->path()) || !watchFolder(entry->path())) entry.disable_recursion_pending();
		}
		return true;
	}
	/// @brief Watches one folder.
	/// @param folder It.
	/// @return Whether it is watched.
	bool watchFolder(const std::filesystem::path& folder) {
		if(folders_.size() >= mostWatchedFolders) {
			complete_ = false;
			return false;
		}
		const int watch = inotify_add_watch(inotify_.value, folder.c_str(), watchedEvents);
		if(watch < 0) {
			if(errno == ENOSPC) complete_ = false;
			return false;
		}
		folders_[watch] = folder;
		return true;
	}
	/// @brief Reads events until stopped.
	/// @param stop Says to stop.
	void read(const std::stop_token& stop) {
		alignas(inotify_event) std::array<char, 64 * 1024> buffer{};
		std::array<pollfd, 2> waits{pollfd{inotify_.value, POLLIN, 0}, pollfd{sleep_->value, POLLIN, 0}};
		while(!stop.stop_requested()) {
			if(poll(waits.data(), waits.size(), -1) < 0 && errno != EINTR) return;
			if(waits[1].revents != 0) return;
			for(;;) {
				const ssize_t length = ::read(inotify_.value, buffer.data(), buffer.size());
				if(length <= 0) break;
				handle(buffer.data(), static_cast<std::size_t>(length));
			}
		}
	}
	/// @brief Handles a buffer of events.
	/// @param bytes The events.
	/// @param length Their size.
	void handle(const char* bytes, std::size_t length) {
		for(std::size_t offset = 0; offset + sizeof(inotify_event) <= length;) {
			inotify_event event{};
			std::memcpy(&event, bytes + offset, sizeof(event));
			const char* name = bytes + offset + sizeof(inotify_event);
			offset += sizeof(inotify_event) + event.len;
			if(event.mask & IN_Q_OVERFLOW) {
				sink_.overflowed();
				continue;
			}
			const auto folder = folders_.find(event.wd);
			if(folder == folders_.end()) continue;
			if(event.mask & IN_IGNORED) {
				folders_.erase(folder);
				continue;
			}
			const std::filesystem::path path = event.len > 0 ? folder->second / std::string(name) : folder->second;
			if((event.mask & IN_ISDIR) && (event.mask & (IN_CREATE | IN_MOVED_TO)) && !sink_.ignored(path))
				watchTree(path);
			sink_.changed(path);
		}
	}

	WatchSink sink_;                                         ///< See the constructor.
	Descriptor inotify_;                                     ///< The inotify instance.
	std::array<int, 2> stopPipe_{-1, -1};                    ///< Its ends, owned by wake_ and sleep_.
	std::unique_ptr<Descriptor> wake_, sleep_;               ///< The pipe that wakes the thread to stop.
	std::unordered_map<int, std::filesystem::path> folders_; ///< Each watch's folder (the thread's once started).
	std::atomic<bool> complete_{true};                       ///< Whether every folder got a watch.
	std::jthread thread_;                                    ///< Reads events.
};

} // namespace

std::unique_ptr<WatchBackend> startWatchBackend(const std::filesystem::path& root, WatchSink sink,
                                                std::chrono::milliseconds) {
	return std::make_unique<InotifyBackend>(root, std::move(sink));
}

} // namespace affresco::services
