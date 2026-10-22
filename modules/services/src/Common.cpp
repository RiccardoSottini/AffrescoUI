/// @file
/// @brief Tasks, underRoot(), isUtf8() and EitherStop: what the general providers share.

#include "Providers.hpp"

#include <cstdint>
#include <system_error>

namespace affresco::services {

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
	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / given, error);
	if(error) return std::nullopt;
	const std::filesystem::path inside = resolved.lexically_relative(root);
	if(inside.empty() || *inside.begin() == "..") return std::nullopt;
	return resolved;
}

bool isUtf8(std::string_view bytes) noexcept {
	std::size_t index = 0;
	while(index < bytes.size()) {
		const auto lead = static_cast<std::uint8_t>(bytes[index]);
		const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
		if(length == 0 || index + length > bytes.size()) return false;
		std::uint32_t codepoint = length == 1 ? lead : lead & (0xFFu >> (length + 1));
		for(std::size_t next = 1; next < length; ++next) {
			const auto byte = static_cast<std::uint8_t>(bytes[index + next]);
			if((byte >> 6) != 0x2) return false;
			codepoint = (codepoint << 6) | (byte & 0x3Fu);
		}
		constexpr std::uint32_t smallest[] = {0, 0, 0x80, 0x800, 0x10000};
		if(codepoint < smallest[length] || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
			return false;
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

void EitherStop::fire() noexcept {
	if(fired_.exchange(true)) return;
	if(action_) action_();
}

} // namespace affresco::services
