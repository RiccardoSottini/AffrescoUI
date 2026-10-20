#pragma once

/// @file
/// @brief ImageCache: images from files, decoded on a worker thread, and images the host names; who shows them is told
/// on the main thread.

#include "affresco/core/Executor.hpp"
#include "affresco/render/Bitmap.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace affresco {

/// @brief Where images come from: files, decoded on a worker thread and kept within a bound, and images the host
/// names (e.g. a picture it made), which it may change at any time.
///
/// **Files.** Only files inside the folders allowed with allowDirectory() are read: a revision may change an image's
/// path, and must not show any picture on the disk. Paths are compared once resolved (symbolic links followed). A
/// file is decoded once, on the worker thread, however many ask for it; what was decoded is kept, least recently used
/// dropped first beyond Limits::bytes, never while something still holds it.
///
/// **Threads.** Everything here is called on the main thread, and deliveries run on it: with an Executor, posted to it
/// once decoded; without one (tests, headless tools), decoding happens in load() itself. finishPending() waits for the
/// worker and delivers what it decoded, for a picture that must show every image. The worker is stopped and joined
/// when the cache is destroyed; what it posted after that does nothing.
class ImageCache {
public:
	/// @brief Gets an image: the decoded file or the named image, or nullptr if there is none (it couldn't be read, or
	/// the name was removed).
	using Delivery = std::function<void(const std::shared_ptr<const Bitmap>& bitmap)>;

	/// @brief The bounds of the cache.
	struct Limits {
		Bitmap::Limits bitmap{};                    ///< What one image may be.
		std::size_t bytes = std::size_t{256} << 20; ///< Decoded files kept, in bytes, beyond those in use.
		std::size_t queued = 256;                   ///< Files waiting to be decoded; more are refused.
	};

	class Request;

	/// @brief An empty cache with the default limits, no executor and no folder allowed.
	ImageCache();
	/// @brief An empty cache.
	/// @param limits Its bounds.
	explicit ImageCache(Limits limits);
	/// @brief Stops the worker; deliveries still pending never run.
	~ImageCache();
	ImageCache(const ImageCache&) = delete;
	ImageCache& operator=(const ImageCache&) = delete;
	ImageCache(ImageCache&&) = delete;
	ImageCache& operator=(ImageCache&&) = delete;

	/// @brief Sets where decoded files are delivered.
	/// @param executor Runs deliveries on the main thread (the Application), or nullptr to decode in load() itself; it
	/// outlives the cache.
	void setExecutor(Executor* executor) noexcept;
	/// @brief Allows image files in a folder and the folders inside it.
	/// @param directory The folder; one that doesn't exist allows nothing.
	void allowDirectory(const std::filesystem::path& directory);
	/// @brief Whether a file is inside an allowed folder.
	/// @param file The file.
	[[nodiscard]] bool allows(const std::filesystem::path& file) const;

	/// @brief Asks for a file's image. It is delivered once: at once if it was decoded already or there is no
	/// executor, else when the worker has decoded it. A file outside the allowed folders, or one that can't be decoded,
	/// is delivered as nullptr (and logged).
	/// @param file The file.
	/// @param delivery Gets it.
	/// @return The request; destroying it cancels the delivery.
	[[nodiscard]] Request load(const std::filesystem::path& file, Delivery delivery);

	/// @brief Sets, replaces or removes a named image, and delivers it to those who watch the name.
	/// @param name The name.
	/// @param bitmap The image, or nullptr to remove it.
	void setNamed(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap);
	/// @brief A named image.
	/// @param name The name.
	/// @return It, or nullptr.
	[[nodiscard]] std::shared_ptr<const Bitmap> named(std::string_view name) const;
	/// @brief Delivers a named image now (nullptr if there is none yet) and each time setNamed() changes it.
	/// @param name The name.
	/// @param delivery Gets it.
	/// @return The request; destroying it stops the deliveries.
	[[nodiscard]] Request watch(const std::string& name, Delivery delivery);

	/// @brief Waits until the worker has decoded every file asked for, and delivers them on this thread.
	void finishPending();
	/// @brief How many files are being decoded or waiting to be delivered.
	[[nodiscard]] std::size_t pending() const;
	/// @brief The bytes of the decoded files kept.
	[[nodiscard]] std::size_t bytes() const;

	/// @brief What the cache shares with its requests, its worker and what it posts: defined in ImageCache.cpp.
	struct State;

private:
	std::shared_ptr<State> state_; ///< See State.
};

/// @brief A delivery asked of an ImageCache: destroying it (or cancel()) means it never runs again. Main thread only;
/// it may outlive the cache.
class ImageCache::Request {
public:
	/// @brief A request for nothing.
	Request() = default;
	/// @brief A request of a cache.
	/// @param state The cache's state.
	/// @param id The delivery's id there.
	Request(std::weak_ptr<State> state, std::uint64_t id) noexcept : state_(std::move(state)), id_(id) {}
	/// @brief Cancels the delivery.
	~Request() { cancel(); }
	Request(const Request&) = delete;
	Request& operator=(const Request&) = delete;
	/// @brief Takes another request's delivery.
	/// @param other The other request: it asks for nothing after.
	Request(Request&& other) noexcept : state_(std::move(other.state_)), id_(std::exchange(other.id_, 0)) {}
	/// @brief Cancels this delivery, and takes another request's.
	/// @param other The other request: it asks for nothing after.
	/// @return This request.
	Request& operator=(Request&& other) noexcept;

	/// @brief Cancels the delivery, if it hasn't run (or, for a watch, stops it).
	void cancel() noexcept;

private:
	std::weak_ptr<State> state_; ///< The cache's state: gone with the cache.
	std::uint64_t id_ = 0;       ///< The delivery's id, or 0.
};

} // namespace affresco
