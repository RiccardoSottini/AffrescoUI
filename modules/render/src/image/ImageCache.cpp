/// @file
/// @brief ImageCache: the worker that decodes files, the deliveries on the main thread, the bound on what is kept, and
/// the folders files may come from.

#include "affresco/render/ImageCache.hpp"
#include "affresco/core/Log.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace affresco {

namespace {

/// @brief A file resolved: absolute, symbolic links followed, so that two names of one file are one, and a link out
/// of an allowed folder is seen.
/// @param file The file.
/// @return Its path.
std::filesystem::path resolved(const std::filesystem::path& file) {
	std::error_code error;
	const std::filesystem::path path = std::filesystem::weakly_canonical(file, error);
	return error ? std::filesystem::absolute(file, error).lexically_normal() : path;
}

/// @brief Whether a path is a folder or inside it, both resolved.
/// @param path The path.
/// @param folder The folder.
bool isInside(const std::filesystem::path& path, const std::filesystem::path& folder) {
	const std::filesystem::path relative = path.lexically_relative(folder);
	return !relative.empty() && *relative.begin() != "..";
}

} // namespace

/// @brief A cache's state. Main-thread parts: the deliveries, the files kept, the names. Parts the worker shares, under
/// the mutex: the queue, the results, whether it is busy.
struct ImageCache::State : std::enable_shared_from_this<ImageCache::State> {
	/// @brief A delivery waiting for a file.
	struct Waiting {
		std::uint64_t id;           ///< Its request's id.
		std::filesystem::path file; ///< The file, resolved.
		Delivery delivery;          ///< What to call.
	};
	/// @brief A delivery of a named image, each time it changes.
	struct Watcher {
		std::uint64_t id;  ///< Its request's id.
		std::string name;  ///< The name.
		Delivery delivery; ///< What to call.
	};
	/// @brief A decoded file kept.
	struct Kept {
		std::shared_ptr<const Bitmap> bitmap; ///< The image.
		std::uint64_t used = 0;               ///< When it was last asked for (State::uses).
	};
	/// @brief What the worker made of a file: the image, or why there is none.
	using Result = std::variant<std::shared_ptr<const Bitmap>, std::string>;

	explicit State(Limits bounds) : limits(bounds) {}

	Limits limits;                                                           ///< The bounds.
	Executor* executor = nullptr;                                            ///< Where deliveries are posted.
	std::vector<std::filesystem::path> directories;                          ///< Allowed folders, resolved.
	std::vector<Waiting> waiting;                                            ///< Deliveries of files.
	std::vector<Watcher> watchers;                                           ///< Deliveries of names.
	std::map<std::string, std::shared_ptr<const Bitmap>, std::less<>> named; ///< The host's images.
	std::map<std::filesystem::path, Kept> files;                             ///< Decoded files kept.
	std::uint64_t uses = 0;                                                  ///< Counts what is asked for.
	std::uint64_t nextId = 1;                                                ///< The next request's id.

	std::mutex mutex;                                           ///< Guards what follows, which the worker shares.
	std::condition_variable_any wake;                           ///< The worker waits on it for work.
	std::condition_variable idle;                               ///< finishPending() waits on it.
	std::deque<std::filesystem::path> queue;                    ///< Files to decode.
	std::vector<std::pair<std::filesystem::path, Result>> done; ///< Decoded, to deliver.
	bool busy = false;                                          ///< Whether the worker is decoding.
	std::jthread worker; ///< Declared last: stopped and joined first, before what it uses is destroyed.

	/// @brief Decodes the files of the queue until asked to stop. Runs on the worker thread.
	/// @param stop Asks it to stop.
	void work(const std::stop_token& stop);
	/// @brief Delivers what the worker decoded. Main thread.
	void deliver();
	/// @brief Keeps a decoded file, dropping the least recently used ones not in use beyond the bound.
	/// @param file The file.
	/// @param bitmap Its image.
	void keep(const std::filesystem::path& file, std::shared_ptr<const Bitmap> bitmap);
	/// @brief Calls the deliveries waiting for a file, once.
	/// @param file The file.
	/// @param bitmap Its image, or nullptr.
	void deliverFile(const std::filesystem::path& file, const std::shared_ptr<const Bitmap>& bitmap);
};

void ImageCache::State::work(const std::stop_token& stop) {
	std::unique_lock lock(mutex);
	while(wake.wait(lock, stop, [this] { return !queue.empty(); })) {
		const std::filesystem::path file = std::move(queue.front());
		queue.pop_front();
		busy = true;
		lock.unlock();
		Result result;
		try {
			result = Bitmap::load(file, limits.bitmap);
		} catch(const std::exception& failure) { // decoding never throws across threads: the reason is delivered
			result = std::string(failure.what());
		}
		lock.lock();
		done.emplace_back(file, std::move(result));
		busy = false;
		if(queue.empty()) idle.notify_all();
		if(executor)
			executor->post([weak = weak_from_this()] {
				if(const auto state = weak.lock()) state->deliver();
			});
	}
}

void ImageCache::State::deliver() {
	std::vector<std::pair<std::filesystem::path, Result>> results;
	{
		const std::scoped_lock lock(mutex);
		results.swap(done);
	}
	for(auto& [file, result] : results) {
		std::shared_ptr<const Bitmap> bitmap;
		if(auto* decoded = std::get_if<std::shared_ptr<const Bitmap>>(&result)) bitmap = std::move(*decoded);
		else log(LogLevel::Warning, std::get<std::string>(result));
		deliverFile(file, bitmap);
		// Kept once delivered, so that only those who keep it hold it: past the bound, one nobody holds goes at once
		if(bitmap) keep(file, std::move(bitmap));
	}
}

void ImageCache::State::keep(const std::filesystem::path& file, std::shared_ptr<const Bitmap> bitmap) {
	files[file] = Kept{std::move(bitmap), ++uses};
	std::size_t total = 0;
	for(const auto& [path, kept] : files) total += kept.bitmap->memoryBytes();
	while(total > limits.bytes) {
		auto oldest = files.end();
		for(auto entry = files.begin(); entry != files.end(); ++entry)
			if(entry->second.bitmap.use_count() == 1 &&
			   (oldest == files.end() || entry->second.used < oldest->second.used))
				oldest = entry;
		if(oldest == files.end()) break; // the rest are in use
		total -= oldest->second.bitmap->memoryBytes();
		files.erase(oldest);
	}
}

// Taken out first, so a delivery may ask for the file again or cancel others
void ImageCache::State::deliverFile(const std::filesystem::path& file, const std::shared_ptr<const Bitmap>& bitmap) {
	std::vector<Delivery> deliveries;
	std::erase_if(waiting, [&](Waiting& entry) {
		if(entry.file != file) return false;
		deliveries.push_back(std::move(entry.delivery));
		return true;
	});
	for(const Delivery& delivery : deliveries) delivery(bitmap);
}

/* ImageCache */

ImageCache::ImageCache() : ImageCache(Limits{}) {}

ImageCache::ImageCache(Limits limits) : state_(std::make_shared<State>(limits)) {}

ImageCache::~ImageCache() {
	if(state_->worker.joinable()) {
		state_->worker.request_stop();
		state_->worker.join();
	}
}

void ImageCache::setExecutor(Executor* executor) noexcept {
	const std::scoped_lock lock(state_->mutex);
	state_->executor = executor;
}

void ImageCache::allowDirectory(const std::filesystem::path& directory) {
	std::error_code error;
	if(!std::filesystem::is_directory(directory, error)) return;
	const std::filesystem::path folder = resolved(directory);
	if(std::ranges::find(state_->directories, folder) == state_->directories.end())
		state_->directories.push_back(folder);
}

bool ImageCache::allows(const std::filesystem::path& file) const {
	const std::filesystem::path path = resolved(file);
	return std::ranges::any_of(state_->directories,
	                           [&path](const std::filesystem::path& folder) { return isInside(path, folder); });
}

ImageCache::Request ImageCache::load(const std::filesystem::path& file, Delivery delivery) {
	State& state = *state_;
	const std::filesystem::path path = resolved(file);
	if(!allows(path)) {
		log(LogLevel::Warning, "AffrescoUI: image file outside the allowed folders: " + file.string());
		delivery(nullptr);
		return {};
	}
	if(const auto kept = state.files.find(path); kept != state.files.end()) {
		kept->second.used = ++state.uses;
		delivery(kept->second.bitmap);
		return {};
	}

	const std::uint64_t id = state.nextId++;
	const bool asked =
	    std::ranges::any_of(state.waiting, [&path](const State::Waiting& entry) { return entry.file == path; });
	state.waiting.push_back({id, path, std::move(delivery)});
	if(asked) return {state_, id};

	std::unique_lock lock(state.mutex);
	if(!state.executor) { // no main loop to post to: decode now
		lock.unlock();
		State::Result result;
		try {
			result = Bitmap::load(path, state.limits.bitmap);
		} catch(const std::exception& failure) {
			result = std::string(failure.what());
		}
		lock.lock();
		state.done.emplace_back(path, std::move(result));
		lock.unlock();
		state.deliver();
		return {};
	}
	if(state.queue.size() >= state.limits.queued) {
		lock.unlock();
		log(LogLevel::Warning, "AffrescoUI: too many images waiting to be decoded: " + file.string());
		state.deliverFile(path, nullptr);
		return {};
	}
	state.queue.push_back(path);
	if(!state.worker.joinable())
		state.worker = std::jthread([raw = &state](const std::stop_token& stop) { raw->work(stop); });
	state.wake.notify_one();
	return {state_, id};
}

void ImageCache::setNamed(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap) {
	State& state = *state_;
	if(bitmap) state.named[name] = bitmap;
	else state.named.erase(name);
	std::vector<Delivery> deliveries; // copied first: a delivery may watch or stop watching
	for(const State::Watcher& watcher : state.watchers)
		if(watcher.name == name) deliveries.push_back(watcher.delivery);
	for(const Delivery& delivery : deliveries) delivery(bitmap);
}

std::shared_ptr<const Bitmap> ImageCache::named(std::string_view name) const {
	const auto found = state_->named.find(name);
	return found == state_->named.end() ? nullptr : found->second;
}

ImageCache::Request ImageCache::watch(const std::string& name, Delivery delivery) {
	const std::uint64_t id = state_->nextId++;
	delivery(named(name));
	state_->watchers.push_back({id, name, std::move(delivery)});
	return {state_, id};
}

void ImageCache::finishPending() {
	State& state = *state_;
	{
		std::unique_lock lock(state.mutex);
		state.idle.wait(lock, [&state] { return state.queue.empty() && !state.busy; });
	}
	state.deliver();
}

std::size_t ImageCache::pending() const {
	const std::scoped_lock lock(state_->mutex);
	return state_->queue.size() + state_->done.size() + (state_->busy ? 1 : 0);
}

std::size_t ImageCache::bytes() const {
	std::size_t total = 0;
	for(const auto& [path, kept] : state_->files) total += kept.bitmap->memoryBytes();
	return total;
}

/* Request */

ImageCache::Request& ImageCache::Request::operator=(Request&& other) noexcept {
	if(this != &other) {
		cancel();
		state_ = std::move(other.state_);
		id_ = std::exchange(other.id_, 0);
	}
	return *this;
}

void ImageCache::Request::cancel() noexcept {
	if(id_ == 0) return;
	if(const auto state = state_.lock()) {
		std::erase_if(state->waiting, [this](const State::Waiting& entry) { return entry.id == id_; });
		std::erase_if(state->watchers, [this](const State::Watcher& entry) { return entry.id == id_; });
	}
	id_ = 0;
}

} // namespace affresco
