/// @file
/// @brief Watcher: globs, and the debouncing every platform's backend shares.

#include "files/Watcher.hpp"

#include <algorithm>
#include <array>
#include <new>

namespace affresco::services {

namespace {

/// @brief A state the text read so far can be in: at the start of one of the glob's tokens.
constexpr unsigned char reached = 1;
/// @brief A `**` entered without reading anything inside it yet: only then may the `/` after it be skipped.
constexpr unsigned char entered = 2;

/// @brief The states of one glob, one byte per position: on the stack for the globs screens give (at most 200 bytes).
class GlobStates {
public:
	/// @brief Room for the states of a glob.
	/// @param glob The glob.
	/// @throws std::bad_alloc For a glob longer than the room on the stack, if memory runs out.
	explicit GlobStates(std::string_view glob) {
		if(glob.size() >= small_.size()) large_.resize(glob.size() + 1);
	}

	/// @brief The states, one per position of the glob and one past its end.
	/// @return Their first.
	unsigned char* data() noexcept { return large_.empty() ? small_.data() : large_.data(); }

private:
	std::array<unsigned char, 256> small_{}; ///< The states of a short glob.
	std::vector<unsigned char> large_;       ///< The states of a long one; empty for a short one.
};

/// @brief The width of the token at a position of a glob.
/// @param glob The glob.
/// @param at The token's position.
/// @return 2 for `**`, else 1.
std::size_t tokenWidth(std::string_view glob, std::size_t at) noexcept {
	return glob.substr(at).starts_with("**") ? 2 : 1;
}

/// @brief Adds the states reachable without reading: past a `*` or `**` (both match nothing), and past the `/` after
/// a `**` just entered ("a/**/b" matches "a/b").
/// @param glob The glob.
/// @param states Its states.
void close(std::string_view glob, unsigned char* states) noexcept {
	for(std::size_t at = 0; at < glob.size(); ++at) {
		if((states[at] & reached) == 0 || glob[at] != '*') continue;
		const std::size_t next = at + tokenWidth(glob, at);
		states[next] |= reached | entered;
		if(next == at + 2 && (states[at] & entered) != 0 && next < glob.size() && glob[next] == '/')
			states[next + 1] |= reached | entered;
	}
}

/// @brief Reads one byte of the text: each state stays (in a `*` or `**`) or moves past the token the byte matches.
/// Positions go from last to first, so each reads its predecessor before that is overwritten.
/// @param glob The glob.
/// @param states Its states, updated.
/// @param byte The byte.
/// @return Whether any state is left: if none, the text can't match.
bool step(std::string_view glob, unsigned char* states, char byte) noexcept {
	bool any = false;
	for(std::size_t at = glob.size() + 1; at-- > 0;) {
		unsigned char now = 0;
		if(at < glob.size() && (states[at] & reached) != 0 && glob[at] == '*' &&
		   (tokenWidth(glob, at) == 2 || byte != '/'))
			now |= reached;
		if(at > 0 && (states[at - 1] & reached) != 0) {
			const char token = glob[at - 1];
			if(token != '*' && (token == '?' ? byte != '/' : token == byte)) now |= reached | entered;
		}
		states[at] = now;
		any = any || now != 0;
	}
	return any;
}

/// @brief Matches a glob against a text from their starts (`*` and `?` stop at `/`, `**` doesn't). The glob runs as
/// a set of states, one per position, so the time is the glob's length times the text's, whatever the glob: a
/// backtracking matcher takes exponential time on globs such as "*a*a*a*a*a*b", which screens could give.
/// @param glob The glob.
/// @param text The text.
/// @param states Room for the glob's states (GlobStates).
/// @return Whether all of the text matches all of the glob.
bool matchFrom(std::string_view glob, std::string_view text, unsigned char* states) noexcept {
	std::fill_n(states, glob.size() + 1, 0);
	states[0] = reached | entered;
	close(glob, states);
	for(const char byte : text) {
		if(!step(glob, states, byte)) return false;
		close(glob, states);
	}
	return (states[glob.size()] & reached) != 0;
}

} // namespace

bool globMatches(std::string_view glob, std::string_view path) noexcept {
	try {
		GlobStates states(glob);
		if(glob.contains('/')) return matchFrom(glob, path, states.data());
		for(std::size_t start = 0; start <= path.size();) {
			const std::size_t end = std::min(path.find('/', start), path.size());
			if(matchFrom(glob, path.substr(start, end - start), states.data())) return true;
			start = end + 1;
		}
		return false;
	} catch(const std::bad_alloc&) {
		return false; // the path isn't ignored: watching more is safer than missing a change
	}
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
