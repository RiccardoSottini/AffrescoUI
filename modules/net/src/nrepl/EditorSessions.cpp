/// @file
/// @brief EditorSessions (EditorSessions.hpp): a connection's nREPL sessions and their ids.

#include "EditorSessions.hpp"

#include "Random.hpp"

#include <array>
#include <span>
#include <utility>

namespace affresco::net::nrepl {

namespace {

constexpr std::size_t recentValues = 3; ///< `*1`, `*2` and `*3`.

/// @brief A new session id: 16 random bytes as 32 lower-case hex characters, which no other connection can guess.
/// @return It.
std::string newSessionId() {
	std::array<std::byte, 16> raw{};
	fillRandom(std::span(raw));
	constexpr std::string_view digits = "0123456789abcdef";
	std::string id;
	id.reserve(raw.size() * 2U);
	for(const std::byte byte : raw) {
		const auto value = std::to_integer<unsigned>(byte);
		id += digits[value >> 4U];
		id += digits[value & 0x0fU];
	}
	return id;
}

} // namespace

void EditorSession::rememberValue(std::string value) {
	values.push_front(std::move(value));
	if(values.size() > recentValues) values.pop_back();
}

const std::string* EditorSession::recentValue(std::size_t back) const noexcept {
	if(back == 0 || back > values.size()) return nullptr;
	return &values[back - 1];
}

std::optional<std::string> EditorSessions::clone(const EditorSession* from) {
	if(sessions_.size() >= most_) return std::nullopt;
	std::string id = newSessionId();
	while(sessions_.contains(id)) id = newSessionId(); // 2^-128 a time: the loop is for correctness, not for speed
	EditorSession session;
	if(from != nullptr) session.space = from->space;
	sessions_.emplace(id, std::move(session));
	return id;
}

EditorSession* EditorSessions::find(std::string_view id) noexcept {
	const auto found = sessions_.find(id);
	return found == sessions_.end() ? nullptr : &found->second;
}

bool EditorSessions::close(std::string_view id) {
	const auto found = sessions_.find(id);
	if(found == sessions_.end()) return false;
	sessions_.erase(found);
	return true;
}

std::vector<std::string> EditorSessions::ids() const {
	std::vector<std::string> result;
	result.reserve(sessions_.size());
	for(const auto& entry : sessions_) result.push_back(entry.first);
	return result;
}

} // namespace affresco::net::nrepl
