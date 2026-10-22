#pragma once

/// @file
/// @brief FrameReader: turns the bytes a service writes into messages, framed LSP's way (`Content-Length: N`, a blank
/// line, then N bytes) or one per line; frameMessage() frames a message to send.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief How messages are delimited on a byte stream.
enum class Framing {
	ContentLength, ///< LSP and DAP: `Content-Length: N\r\n\r\n` then the body.
	Lines,         ///< One message per line (`\n`, an optional `\r` before it).
};

/// @brief The bounds of one frame.
struct FrameLimits {
	std::size_t headerBytes = std::size_t{8} << 10;  ///< The headers of a Content-Length frame, at most 8 KiB.
	std::size_t messageBytes = std::size_t{8} << 20; ///< A body (or a line), at most 8 MiB.
};

/// @brief Reads frames from a byte stream, a piece at a time. One thread at a time; after an error it stays broken.
class FrameReader {
public:
	/// @brief Starts a reader.
	/// @param framing How messages are delimited.
	/// @param limits Their bounds.
	explicit FrameReader(Framing framing, FrameLimits limits = {}) noexcept;

	/// @brief Adds bytes, and collects the bodies they complete.
	/// @param bytes What arrived next.
	/// @param bodies Receives each complete body, in order.
	/// @return Why the stream is malformed (a missing or bad Content-Length, headers or a body over the limits), or
	/// std::nullopt. Once malformed, every later call returns the same reason.
	[[nodiscard]] std::optional<std::string> feed(std::string_view bytes, std::vector<std::string>& bodies);

private:
	/// @brief Takes the frames that are complete in buffer_.
	/// @param bodies Receives them.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> takeContentLength(std::vector<std::string>& bodies);
	/// @brief Takes the lines that are complete in buffer_.
	/// @param bodies Receives them.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> takeLines(std::vector<std::string>& bodies);

	Framing framing_;                   ///< See the constructor.
	FrameLimits limits_;                ///< See the constructor.
	std::string buffer_;                ///< Bytes not yet part of a complete frame.
	std::size_t start_ = 0;             ///< Where the unread bytes of buffer_ begin.
	std::size_t scanned_ = 0;           ///< How many unread bytes takeLines() searched, finding no line break.
	std::optional<std::size_t> length_; ///< The body length of the frame being read, once its headers are.
	std::optional<std::string> broken_; ///< Why the stream is malformed, once it is.
};

/// @brief Reads a Content-Length frame's headers.
/// @param headers The headers, without the blank line after them.
/// @param maximum The largest body allowed.
/// @return The body's length.
/// @throws std::runtime_error If no valid Content-Length is there, or it is over @p maximum.
[[nodiscard]] std::size_t contentLength(std::string_view headers, std::size_t maximum);

/// @brief Frames a message to send.
/// @param framing How.
/// @param body The message (for Lines, without a line break).
/// @return The bytes to write.
[[nodiscard]] std::string frameMessage(Framing framing, std::string_view body);

} // namespace affresco::services
