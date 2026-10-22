/// @file
/// @brief FrameReader and frameMessage(): Content-Length and line framing, with every frame bounded.

#include "rpc/Framing.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace affresco::services {

namespace {

/// @brief Compares ASCII text without case.
/// @param left One text.
/// @param right The other.
/// @return Whether they are equal, ignoring case.
bool sameWord(std::string_view left, std::string_view right) noexcept {
	return std::ranges::equal(left, right, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}

/// @brief Removes spaces and tabs at both ends.
/// @param text The text.
/// @return It, trimmed.
std::string_view trim(std::string_view text) noexcept {
	while(!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
	while(!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
	return text;
}

/// @brief Reads a decimal length.
/// @param text Its digits.
/// @param maximum The largest allowed.
/// @return The length.
/// @throws std::runtime_error If it isn't digits only, or is over @p maximum.
std::size_t decimal(std::string_view text, std::size_t maximum) {
	if(text.empty() || text.size() > 12) throw std::runtime_error("a bad Content-Length");
	std::size_t value = 0;
	for(const char digit : text) {
		if(digit < '0' || digit > '9') throw std::runtime_error("a bad Content-Length");
		value = value * 10 + static_cast<std::size_t>(digit - '0');
	}
	if(value > maximum)
		throw std::runtime_error("a message of " + std::to_string(value) + " bytes (at most " +
		                         std::to_string(maximum) + ")");
	return value;
}

} // namespace

std::size_t contentLength(std::string_view headers, std::size_t maximum) {
	std::optional<std::size_t> length;
	while(!headers.empty()) {
		const std::size_t end = headers.find("\r\n");
		const std::string_view line = headers.substr(0, end);
		headers = end == std::string_view::npos ? std::string_view{} : headers.substr(end + 2);
		const std::size_t colon = line.find(':');
		if(colon == std::string_view::npos || colon == 0) throw std::runtime_error("a header without a name");
		if(!sameWord(trim(line.substr(0, colon)), "Content-Length")) continue; // Content-Type and others
		if(length) throw std::runtime_error("two Content-Length headers");
		length = decimal(trim(line.substr(colon + 1)), maximum);
	}
	if(!length) throw std::runtime_error("no Content-Length");
	return *length;
}

FrameReader::FrameReader(Framing framing, FrameLimits limits) noexcept : framing_(framing), limits_(limits) {}

std::optional<std::string> FrameReader::feed(std::string_view bytes, std::vector<std::string>& bodies) {
	if(broken_) return broken_;
	buffer_.append(bytes);
	broken_ = framing_ == Framing::ContentLength ? takeContentLength(bodies) : takeLines(bodies);
	buffer_.erase(0, start_); // what is left is one incomplete frame, itself bounded
	start_ = 0;
	return broken_;
}

std::optional<std::string> FrameReader::takeContentLength(std::vector<std::string>& bodies) {
	while(true) {
		const std::string_view unread = std::string_view(buffer_).substr(start_);
		if(!length_) {
			const std::size_t end = unread.find("\r\n\r\n");
			if(end == std::string_view::npos) {
				if(unread.size() > limits_.headerBytes) return "headers over " + std::to_string(limits_.headerBytes) + " bytes";
				return std::nullopt;
			}
			if(end > limits_.headerBytes) return "headers over " + std::to_string(limits_.headerBytes) + " bytes";
			try {
				length_ = contentLength(unread.substr(0, end), limits_.messageBytes);
			} catch(const std::runtime_error& error) {
				return std::string(error.what());
			}
			start_ += end + 4;
			continue;
		}
		if(unread.size() < *length_) return std::nullopt;
		bodies.emplace_back(unread.substr(0, *length_));
		start_ += *length_;
		length_.reset();
	}
}

std::optional<std::string> FrameReader::takeLines(std::vector<std::string>& bodies) {
	while(true) {
		const std::string_view unread = std::string_view(buffer_).substr(start_);
		const std::size_t end = unread.find('\n');
		if(end == std::string_view::npos) { // room for the line's body and a '\r' before its '\n'
			if(unread.size() > limits_.messageBytes + 1)
				return "a line over " + std::to_string(limits_.messageBytes) + " bytes";
			return std::nullopt;
		}
		std::string_view line = unread.substr(0, end);
		if(line.ends_with('\r')) line.remove_suffix(1);
		if(line.size() > limits_.messageBytes) return "a line over " + std::to_string(limits_.messageBytes) + " bytes";
		if(!line.empty()) bodies.emplace_back(line);
		start_ += end + 1;
	}
}

std::string frameMessage(Framing framing, std::string_view body) {
	if(framing == Framing::Lines) return std::string(body) + "\n";
	return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
}

} // namespace affresco::services
