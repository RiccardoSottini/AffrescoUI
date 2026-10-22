/// @file
/// @brief The macOS watch backend: one FSEvents stream over the root, with file-level events, on a dispatch queue.

#include "Watcher.hpp"

#include <CoreServices/CoreServices.h>

#include <stdexcept>

namespace affresco::services {

namespace {

/// @brief An FSEvents stream and its queue, stopped and released when it goes.
class EventStreamBackend final : public WatchBackend {
public:
	/// @brief Starts the stream.
	/// @param root The folder.
	/// @param sink Where changes go.
	/// @param latency How long FSEvents may hold events back.
	/// @throws std::runtime_error If the stream can't start.
	EventStreamBackend(const std::filesystem::path& root, WatchSink sink, std::chrono::milliseconds latency)
	    : sink_(std::move(sink)), queue_(dispatch_queue_create("affrescoui.watch", DISPATCH_QUEUE_SERIAL)) {
		CFStringRef path = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, root.c_str());
		const void* paths[] = {path};
		CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, paths, 1, &kCFTypeArrayCallBacks);
		FSEventStreamContext context{0, this, nullptr, nullptr, nullptr};
		constexpr FSEventStreamCreateFlags flags =
		    kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot;
		stream_ = FSEventStreamCreate(kCFAllocatorDefault, &EventStreamBackend::received, &context, array,
		                              ~FSEventStreamEventId{0}, static_cast<double>(latency.count()) / 1000.0, flags);
		CFRelease(array);
		CFRelease(path);
		if(!stream_) {
			dispatch_release(queue_);
			throw std::runtime_error("the folder can't be watched");
		}
		FSEventStreamSetDispatchQueue(stream_, queue_);
		if(!FSEventStreamStart(stream_)) {
			release();
			throw std::runtime_error("the folder can't be watched");
		}
	}
	/// @brief Stops the stream; no callback runs after.
	~EventStreamBackend() override {
		FSEventStreamStop(stream_);
		release();
	}
	EventStreamBackend(const EventStreamBackend&) = delete;
	EventStreamBackend& operator=(const EventStreamBackend&) = delete;
	EventStreamBackend(EventStreamBackend&&) = delete;
	EventStreamBackend& operator=(EventStreamBackend&&) = delete;

private:
	/// @brief Invalidates and releases the stream, then waits for the queue to drain and releases it.
	void release() noexcept {
		FSEventStreamInvalidate(stream_);
		FSEventStreamRelease(stream_);
		dispatch_sync_f(queue_, nullptr, [](void*) {});
		dispatch_release(queue_);
	}
	/// @brief FSEvents' callback, on the queue.
	/// @param info The backend.
	/// @param count The number of events.
	/// @param paths Their paths (C strings).
	/// @param flags Their flags.
	static void received(ConstFSEventStreamRef, void* info, std::size_t count, void* paths,
	                     const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]) {
		const auto& self = *static_cast<EventStreamBackend*>(info);
		const auto* const* names = static_cast<const char* const*>(paths);
		constexpr FSEventStreamEventFlags lost =
		    kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagUserDropped |
		    kFSEventStreamEventFlagKernelDropped | kFSEventStreamEventFlagRootChanged;
		for(std::size_t i = 0; i < count; ++i) {
			if(flags[i] & lost) self.sink_.overflowed();
			else self.sink_.changed(std::filesystem::path(names[i]));
		}
	}

	WatchSink sink_;            ///< See the constructor.
	dispatch_queue_t queue_;    ///< Where callbacks run.
	FSEventStreamRef stream_{}; ///< The stream.
};

} // namespace

std::unique_ptr<WatchBackend> startWatchBackend(const std::filesystem::path& root, WatchSink sink,
                                                std::chrono::milliseconds latency) {
	return std::make_unique<EventStreamBackend>(root, std::move(sink), latency);
}

} // namespace affresco::services
