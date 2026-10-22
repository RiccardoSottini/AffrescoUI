/// @file
/// @brief Watcher: globs, and the debouncing every platform's backend shares.

#include "Watcher.hpp"

#include <algorithm>

namespace affresco::services {

namespace {

/// @brief Matches a glob against a text from their starts (`*` and `?` stop at `/`, `**` doesn't).
/// @param glob The rest of the glob.
/// @param text The rest of the text.
/// @return Whether all of the text matches all of the glob.
bool matchFrom(std::string_view glob, std::string_view text) noexcept {
	while(!glob.empty()) {
		if(glob.starts_with("**")) {
			glob.remove_prefix(2);
			if(glob.starts_with('/') && matchFrom(glob.substr(1), text)) return true; // "a/**/b" matches "a/b"
			for(std::size_t i = 0; i <= text.size(); ++i)
				if(matchFrom(glob, text.substr(i))) return true;
			return false;
		}
		if(glob.front() == '*') {
			glob.remove_prefix(1);
			for(std::size_t i = 0; i <= text.size(); ++i) {
				if(matchFrom(glob, text.substr(i))) return true;
				if(i < text.size() && text[i] == '/') return false;
			}
			return false;
		}
		if(text.empty() || (glob.front() == '?' ? text.front() == '/' : glob.front() != text.front())) return false;
		glob.remove_prefix(1);
		text.remove_prefix(1);
	}
	return text.empty();
}

} // namespace

bool globMatches(std::string_view glob, std::string_view path) noexcept {
	if(glob.find('/') != std::string_view::npos) return matchFrom(glob, path);
	for(std::size_t start = 0; start <= path.size();) {
		const std::size_t end = std::min(path.find('/', start), path.size());
		if(matchFrom(glob, path.substr(start, end - start))) return true;
		start = end + 1;
	}
	return false;
}

bool ignoredBy(const std::vector<std::string>& globs, std::string_view path) noexcept {
	return std::ranges::any_of(globs, [path](const std::string& glob) { return globMatches(glob, path); });
}

std::string genericUtf8(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();
	std::string bytes(text.size(), '\0');
	std::transform(text.begin(), text.end(), bytes.begin(), [](char8_t unit) { return static_cast<char>(unit); });
	return bytes;
}

std::filesystem::path pathFromUtf8(std::string_view text) {
	std::u8string units(text.size(), u8'\0');
	std::transform(text.begin(), text.end(), units.begin(), [](char byte) { return static_cast<char8_t>(byte); });
	return std::filesystem::path(units);
}

Watcher::Watcher(std::filesystem::path root, WatchOptions options, std::function<void(WatchBatch)> onBatch)
    : root_(std::move(root)), options_(std::move(options)), onBatch_(std::move(onBatch)),
      thread_([this](const std::stop_token& stop) { run(stop); }) {
	WatchSink sink{
	    [this](const std::filesystem::path& path) { changed(path); }, [this] { overflowed(); },
	    [this](const std::filesystem::path& path) { return ignored(genericUtf8(path.lexically_relative(root_))); }};
	backend_ = startWatchBackend(root_, std::move(sink), std::max(options_.debounce / 2, std::chrono::milliseconds(5)));
}

Watcher::~Watcher() { backend_.reset(); }

bool Watcher::ignored(std::string_view relative) const noexcept { return ignoredBy(options_.ignore, relative); }

void Watcher::changed(const std::filesystem::path& path) {
	std::string relative = genericUtf8(path.lexically_relative(root_));
	if(relative.empty() || relative == "." || relative.starts_with("..") || ignored(relative)) return;
	const std::scoped_lock lock(mutex_);
	const auto now = std::chrono::steady_clock::now();
	if(pending_.empty() && !rescan_) first_ = now;
	last_ = now;
	if(!rescan_ && pending_.size() == mostChangesPerBatch) {
		rescan_ = true;
		pending_.clear();
	} else if(!rescan_) pending_.insert(std::move(relative));
	noticed_.notify_all();
}

void Watcher::overflowed() {
	const std::scoped_lock lock(mutex_);
	const auto now = std::chrono::steady_clock::now();
	if(pending_.empty() && !rescan_) first_ = now;
	last_ = now;
	rescan_ = true;
	pending_.clear();
	noticed_.notify_all();
}

void Watcher::run(const std::stop_token& stop) {
	std::unique_lock lock(mutex_);
	const auto never = [] { return false; };
	while(noticed_.wait(lock, stop, [this] { return rescan_ || !pending_.empty(); })) {
		for(;;) { // until quiet for the debounce time, or a second after the first change
			const auto due = std::min(last_ + options_.debounce, first_ + std::chrono::seconds(1));
			if(stop.stop_requested()) return;
			if(std::chrono::steady_clock::now() >= due) break;
			noticed_.wait_until(lock, stop, due, never);
		}
		WatchBatch batch{
		    rescan_ ? std::vector<std::string>() : std::vector<std::string>(pending_.begin(), pending_.end()), rescan_};
		pending_.clear();
		rescan_ = false;
		lock.unlock();
		onBatch_(std::move(batch));
		lock.lock();
		noticed_.wait_for(lock, stop, options_.debounce, never); // never two batches within the debounce time
	}
}

} // namespace affresco::services
