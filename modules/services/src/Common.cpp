/// @file
/// @brief Tasks, underRoot(), isUtf8() and EitherStop: what the general providers share.

#include "Providers.hpp"

#include <array>
#include <cstdint>
#include <system_error>

namespace affresco::services {

namespace {

/// @brief The length of the UTF-8 sequence a byte leads.
/// @param lead The byte.
/// @return 1 to 4, or 0 if the byte leads none (a continuation byte, or 0xF8 and above).
constexpr std::size_t sequenceLength(std::uint8_t lead) noexcept {
	if(lead < 0x80) return 1;
	if((lead >> 5) == 0x6) return 2;
	if((lead >> 4) == 0xE) return 3;
	if((lead >> 3) == 0x1E) return 4;
	return 0;
}

/// @brief Whether one sequence encodes a Unicode scalar value: continuation bytes after its lead, in its shortest
/// form (not overlong), at most U+10FFFF and not a surrogate.
/// @param sequence The sequence, as long as its lead byte says.
/// @return Whether it does.
// NOLINTNEXTLINE(bugprone-exception-escape): substr(1) of a non-empty sequence never throws
constexpr bool isScalar(std::string_view sequence) noexcept {
	const auto lead = static_cast<std::uint8_t>(sequence.front());
	std::uint32_t codepoint = sequence.size() == 1 ? lead : lead & (0xFFu >> (sequence.size() + 1));
	for(const char next : sequence.substr(1)) {
		const auto byte = static_cast<std::uint8_t>(next);
		if((byte >> 6) != 0x2) return false;
		codepoint = (codepoint << 6) | (byte & 0x3Fu);
	}
	constexpr std::array<std::uint32_t, 5> smallest{0, 0, 0x80, 0x800, 0x10000};
	return codepoint >= smallest[sequence.size()] && codepoint <= 0x10FFFF &&
	       (codepoint < 0xD800 || codepoint > 0xDFFF);
}

} // namespace

Tasks::Tasks(std::size_t limit) noexcept : limit_(limit) {}

Tasks::~Tasks() {
	for(Task& task : tasks_) task.thread.request_stop();
	tasks_.clear(); // joins
}

bool Tasks::run(std::function<void(std::stop_token)> work) {
	std::erase_if(tasks_, [](const Task& task) { return task.done->load(); });
	if(tasks_.size() >= limit_) return false;
#ifdef __EMSCRIPTEN__ // the browser build has one thread: the work runs now, on it (web/README.md)
	work(std::stop_token{});
	return true;
#else
	auto done = std::make_shared<std::atomic<bool>>(false);
	std::jthread thread([work = std::move(work), done](std::stop_token stop) {
		work(std::move(stop));
		done->store(true);
	});
	tasks_.push_back({std::move(done), std::move(thread)});
	return true;
#endif
}

std::optional<std::filesystem::path> underRoot(const std::filesystem::path& root, std::string_view relative) {
	const std::filesystem::path given{std::u8string(relative.begin(), relative.end())};
	if(given.is_absolute() || given.has_root_name() || given.has_root_directory()) return std::nullopt;
	std::error_code error;
	std::filesystem::path resolved = std::filesystem::weakly_canonical(root / given, error);
	if(error) return std::nullopt;
	const std::filesystem::path inside = resolved.lexically_relative(root);
	if(inside.empty() || *inside.begin() == "..") return std::nullopt;
	return resolved;
}

// NOLINTNEXTLINE(bugprone-exception-escape): substr() from an index inside the bytes never throws
bool isUtf8(std::string_view bytes) noexcept {
	std::size_t index = 0;
	while(index < bytes.size()) {
		const std::size_t length = sequenceLength(static_cast<std::uint8_t>(bytes[index]));
		if(length == 0 || length > bytes.size() - index || !isScalar(bytes.substr(index, length))) return false;
		index += length;
	}
	return true;
}

EitherStop::EitherStop(const std::stop_token& first, const std::stop_token& second, std::function<void()> action)
    : action_(std::move(action)) {
	first_.emplace(first, [this] { fire(); });
	second_.emplace(second, [this] { fire(); });
}

bool EitherStop::requested() const noexcept { return fired_.load(); }

// NOLINTNEXTLINE(bugprone-exception-escape): a stop callback must not throw, and the actions given don't
void EitherStop::fire() noexcept {
	if(fired_.exchange(true)) return;
	if(action_) action_();
}

} // namespace affresco::services
