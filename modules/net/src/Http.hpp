#pragma once

/// @file
/// @brief HTTP/1.1 text as a server sends it: status lines, header lines, Retry-After, and the header block of a
/// WebSocket handshake. Everything parsed here is untrusted and bounded.

#include "affresco/core/Network.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::net {

/// The most the header block of a response may be (all its header lines): more fails with "tooLarge".
inline constexpr std::size_t maxResponseHeaderBytes = std::size_t{64} << 10;
/// The most header lines a response may have.
inline constexpr std::size_t maxResponseHeaders = 256;

/// @brief Parses a status line ("HTTP/1.1 200 OK").
/// @param line The line, without its ending.
/// @return The status (100 to 999), or nullopt if it isn't one.
[[nodiscard]] std::optional<int> parseStatusLine(std::string_view line) noexcept;

/// @brief Parses a header line ("Name: value"): the name in lower case, the value without surrounding spaces.
/// @param line The line, without its ending.
/// @return The header, or nullopt if the name is empty or has characters a token may not have.
[[nodiscard]] std::optional<std::pair<std::string, std::string>> parseHeaderLine(std::string_view line);

/// @brief Parses a Retry-After value in seconds ("120"); an HTTP date is ignored.
/// @param value The value.
/// @return The wait (at most an hour), or nullopt.
[[nodiscard]] std::optional<std::chrono::milliseconds> parseRetryAfter(std::string_view value) noexcept;

/// @brief The value of a header, if present (names compared without case).
/// @param headers The headers.
/// @param name The name, in lower case.
/// @return Its first value, or nullopt.
[[nodiscard]] std::optional<std::string> findHeader(const network::Headers& headers, std::string_view name);

/// @brief Whether two ASCII strings are equal without case.
[[nodiscard]] bool equalsIgnoringCase(std::string_view a, std::string_view b) noexcept;

/// @brief Whether a comma-separated header value has a token, without case ("Connection: keep-alive, Upgrade").
/// @param value The value.
/// @param token The token.
[[nodiscard]] bool hasToken(std::string_view value, std::string_view token) noexcept;

/// @brief A parsed header block: the first line, then the headers, as both sides of a WebSocket handshake send it.
struct HeaderBlock {
	std::string firstLine;    ///< The status line or the request line.
	network::Headers headers; ///< Names in lower case.
};

/// @brief Parses a header block that ends in a blank line.
/// @param text Everything up to and with "\r\n\r\n" (lines may end in "\n" alone).
/// @return It, or nullopt if a header line is malformed or there are more than maxResponseHeaders.
[[nodiscard]] std::optional<HeaderBlock> parseHeaderBlock(std::string_view text);

} // namespace affresco::net
