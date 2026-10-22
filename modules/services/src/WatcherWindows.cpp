/// @file
/// @brief The Windows watch backend: ReadDirectoryChangesW over the whole tree, overlapped, on a thread of its own
/// that an event wakes to stop. A lost buffer is a rescan.

#include "Watcher.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace affresco::services {

namespace {

/// @brief A handle, closed when it goes.
struct Handle {
	HANDLE value = nullptr; ///< The handle, or nullptr.
	/// @brief Takes a handle.
	/// @param handle It (INVALID_HANDLE_VALUE counts as none).
	explicit Handle(HANDLE handle) noexcept : value(handle == INVALID_HANDLE_VALUE ? nullptr : handle) {}
	/// @brief Closes it.
	~Handle() {
		if(value) CloseHandle(value);
	}
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;
	Handle(Handle&&) = delete;
	Handle& operator=(Handle&&) = delete;
};

constexpr DWORD watchedChanges = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

/// @brief The root opened for listing, and the thread that reads its changes.
class DirectoryChangesBackend final : public WatchBackend {
public:
	/// @brief Opens the root and starts reading.
	/// @param root The folder.
	/// @param sink Where changes go.
	/// @throws std::runtime_error If the folder can't be opened.
	DirectoryChangesBackend(std::filesystem::path root, WatchSink sink)
	    : root_(std::move(root)), sink_(std::move(sink)),
	      folder_(CreateFileW(root_.c_str(), FILE_LIST_DIRECTORY,
		                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr)),
	      done_(CreateEventW(nullptr, TRUE, FALSE, nullptr)), stop_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
		if(!folder_.value || !done_.value || !stop_.value) throw std::runtime_error("the folder can't be watched");
		thread_ = std::jthread([this] { read(); });
	}
	/// @brief Wakes the thread and joins it.
	~DirectoryChangesBackend() override {
		SetEvent(stop_.value);
		if(thread_.joinable()) thread_.join();
	}
	DirectoryChangesBackend(const DirectoryChangesBackend&) = delete;
	DirectoryChangesBackend& operator=(const DirectoryChangesBackend&) = delete;
	DirectoryChangesBackend(DirectoryChangesBackend&&) = delete;
	DirectoryChangesBackend& operator=(DirectoryChangesBackend&&) = delete;

private:
	/// @brief Reads changes until stopped.
	void read() {
		std::vector<DWORD> buffer(16 * 1024); // 64 KiB, DWORD-aligned as ReadDirectoryChangesW wants
		const DWORD size = static_cast<DWORD>(buffer.size() * sizeof(DWORD));
		for(;;) {
			OVERLAPPED overlapped{};
			overlapped.hEvent = done_.value;
			ResetEvent(done_.value);
			if(!ReadDirectoryChangesW(folder_.value, buffer.data(), size, TRUE, watchedChanges, nullptr, &overlapped,
			                          nullptr))
				return;
			const HANDLE waits[] = {done_.value, stop_.value};
			DWORD bytes = 0;
			if(WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0) {
				CancelIoEx(folder_.value, &overlapped);
				GetOverlappedResult(folder_.value, &overlapped, &bytes, TRUE);
				return;
			}
			if(!GetOverlappedResult(folder_.value, &overlapped, &bytes, FALSE)) {
				if(GetLastError() != ERROR_NOTIFY_ENUM_DIR) return;
				bytes = 0;
			}
			if(bytes == 0) sink_.overflowed(); // the buffer overflowed: the changes are lost
			else handle(reinterpret_cast<const char*>(buffer.data()), bytes);
		}
	}
	/// @brief Handles a buffer of FILE_NOTIFY_INFORMATION records.
	/// @param bytes The records.
	/// @param length Their size.
	void handle(const char* bytes, DWORD length) {
		for(DWORD offset = 0; offset + sizeof(FILE_NOTIFY_INFORMATION) <= length;) {
			FILE_NOTIFY_INFORMATION record{};
			std::memcpy(&record, bytes + offset, sizeof(record));
			const auto* name =
			    reinterpret_cast<const wchar_t*>(bytes + offset + offsetof(FILE_NOTIFY_INFORMATION, FileName));
			sink_.changed(root_ / std::wstring(name, record.FileNameLength / sizeof(wchar_t)));
			if(record.NextEntryOffset == 0) break;
			offset += record.NextEntryOffset;
		}
	}

	std::filesystem::path root_; ///< See the constructor.
	WatchSink sink_;             ///< See the constructor.
	Handle folder_;              ///< The root, opened for listing.
	Handle done_;                ///< Set when a read completes.
	Handle stop_;                ///< Set to stop.
	std::jthread thread_;        ///< Reads changes.
};

} // namespace

std::unique_ptr<WatchBackend> startWatchBackend(const std::filesystem::path& root, WatchSink sink,
                                                std::chrono::milliseconds) {
	return std::make_unique<DirectoryChangesBackend>(root, std::move(sink));
}

} // namespace affresco::services
