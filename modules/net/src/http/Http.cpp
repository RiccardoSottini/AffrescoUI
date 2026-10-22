/// @file
/// @brief Parsing HTTP status lines, header lines and Retry-After.

#include "http/Http.hpp"

#include <algorithm>
#include <cctype>

namespace affresco::net {

namespace {

/// @brief Whether a byte may be in a header name (an HTTP token).
/// @param c The byte.
bool isTokenCharacter(char c) noexcept {
	static constexpr std::string_view others = "!#$%&'*+-.^_`|~";
	return std::isalnum(static_cast<unsigned char>(c)) != 0 || others.contains(c);
}

/// @brief Text without the spaces and tabs around it.
/// @param text The text.
std::string_view trim(std::string_view text) noexcept {
	while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
	while(!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
	return text;
}

} // namespace

bool equalsIgnoringCase(std::string_view a, std::string_view b) noexcept {
	return a.size() == b.size() && std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

std::optional<int> parseStatusLine(std::string_view line) noexcept {
	if(!line.starts_with("HTTP/") || line.size() < 12) return std::nullopt;
	const auto space = line.find(' ');
	if(space == std::string_view::npos || line.size() < space + 4) return std::nullopt;
	int status = 0;
	for(std::size_t i = space + 1; i < space + 4; ++i) {
		if(line[i] < '0' || line[i] > '9') return std::nullopt;
		status = status * 10 + (line[i] - '0');
	}
	if(status < 100 || (line.size() > space + 4 && line[space + 4] != ' ')) return std::nullopt;
	return status;
}

std::optional<std::pair<std::string, std::string>> parseHeaderLine(std::string_view line) {
	const auto colon = line.find(':');
	if(colon == 0 || colon == std::string_view::npos) return std::nullopt;
	const auto name = line.substr(0, colon);
	if(!std::ranges::all_of(name, isTokenCharacter)) return std::nullopt;
	std::string lowered(name);
	std::ranges::transform(lowered, lowered.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return std::pair{std::move(lowered), std::string(trim(line.substr(colon + 1)))};
}

std::optional<std::chrono::milliseconds> parseRetryAfter(std::string_view value) noexcept {
	value = trim(value);
	if(value.empty() || value.size() > 7) return std::nullopt;
	long seconds = 0;
	for(const char c : value) {
		if(c < '0' || c > '9') return std::nullopt;
		seconds = seconds * 10 + (c - '0');
	}
	return std::chrono::milliseconds(std::min(seconds, 3600L) * 1000);
}

std::optional<std::string> findHeader(const network::Headers& headers, std::string_view name) {
	for(const auto& [key, value] : headers)
		if(equalsIgnoringCase(key, name)) return value;
	return std::nullopt;
}

bool hasToken(std::string_view value, std::string_view token) noexcept {
	while(!value.empty()) {
		const auto comma = value.find(',');
		if(equalsIgnoringCase(trim(value.substr(0, comma)), token)) return true;
		if(comma == std::string_view::npos) break;
		value.remove_prefix(comma + 1);
	}
	return false;
}

std::optional<HeaderBlock> parseHeaderBlock(std::string_view text) {
	HeaderBlock block;
	bool first = true;
	while(!text.empty()) {
		const auto end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if(line.ends_with('\r')) line.remove_suffix(1);
		if(first) {
			block.firstLine = line;
			first = false;
			continue;
		}
		if(line.empty()) break;
		auto header = parseHeaderLine(line);
		if(!header || block.headers.size() >= maxResponseHeaders) return std::nullopt;
		block.headers.push_back(std::move(*header));
	}
	if(first) return std::nullopt;
	return block;
}

} // namespace affresco::net
