/// @file
/// @brief The browser's watch backend: there is none. A page's files (MEMFS) change only through the page itself, and
/// the browser build has no threads, so a folder can't be watched; the `watch` provider isn't registered there either.

#include "Watcher.hpp"

#include <stdexcept>

namespace affresco::services {

std::unique_ptr<WatchBackend> startWatchBackend(const std::filesystem::path&, WatchSink, std::chrono::milliseconds) {
	throw std::runtime_error("folders can't be watched in a browser");
}

} // namespace affresco::services
