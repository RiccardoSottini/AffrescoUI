#pragma once

/// @file
/// @brief Line framing shared by the channels: one message per line, read on a background thread.
///
/// The framing doesn't know the transport: it reads and writes through a callable, so a socket (recv/send on
/// Windows) and a descriptor (read/write) share it. The descriptor overloads are what the process channels use.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace affresco::channel::detail {

/// @brief Reads up to `size` bytes into `data`: returns how many, 0 at the end of the stream, or less on an error.
template <typename Read>
concept ChunkReader = std::invocable<Read&, char*, std::size_t>;

/// @brief Writes up to `size` bytes from `data`: returns how many, or 0 or less on an error.
template <typename Write>
concept ChunkWriter = std::invocable<Write&, const char*, std::size_t>;

/// @brief Where the lines read go: posted to a channel's signals on the main thread, while the channel lives.
struct Delivery {
	Executor& executor;          ///< Delivers on the main thread.
	Channel& channel;            ///< The channel whose signals fire.
	std::shared_ptr<bool> alive; ///< Cleared when the channel is destroyed; queued deliveries then do nothing.
};

/// @brief Tells whether a line, ended or not, fits in maxMessageBytes; logs the one that doesn't.
/// @param length Its length in bytes.
/// @return true if it fits.
inline bool fitsInMessage(std::size_t length) {
	if(length <= maxMessageBytes) return true;
	log(LogLevel::Warning,
	    "AffrescoUI: a message longer than " + std::to_string(maxMessageBytes >> 20) + " MiB: closing the channel");
	return false;
}

/// @brief Takes the lines that have ended out of what was read, and posts each to the channel's onMessage.
///
/// Every line is checked against maxMessageBytes, ended or not: the read that takes a line past the limit may also
/// bring its end.
/// @param buffer What was read and not yet delivered; keeps the line that hasn't ended.
/// @param searched How many bytes at its start were searched for a line break before (they have none).
/// @param dropCarriageReturns Whether a line ending in "\r\n" loses its '\r'.
/// @param to Where the lines go.
/// @return false once a line is longer than maxMessageBytes: the stream must end.
inline bool deliverLines(std::string& buffer, std::size_t searched, bool dropCarriageReturns, const Delivery& to) {
	std::size_t start = 0;
	bool fits = true;
	for(std::size_t end; fits && (end = buffer.find('\n', std::max(start, searched))) != std::string::npos;
	    start = end + 1) {
		std::size_t length = end - start;
		if(dropCarriageReturns && length > 0 && buffer[end - 1] == '\r') --length;
		fits = fitsInMessage(length);
		if(fits && length > 0)
			to.executor.post([&channel = to.channel, alive = to.alive, line = buffer.substr(start, length)] {
				if(*alive) channel.onMessage(line);
			});
	}
	buffer.erase(0, start);
	return fits && fitsInMessage(buffer.size());
}

/// @brief Reads a stream until it ends, posting each line to the channel's onMessage, then onClosed.
///
/// A line that grows past maxMessageBytes ends the stream too. Each byte is searched for a line break once, so a
/// long line costs time in proportion to its length.
/// @param read Reads the next chunk of the stream (see ChunkReader).
/// @param to Where the lines go.
/// @param running Set to false when the stream ends (may be nullptr).
/// @param dropCarriageReturns Whether a line ending in "\r\n" loses its '\r' (a Windows console's and cmd.exe's
/// line breaks).
template <ChunkReader Read>
void readLines(Read read, const Delivery& to, std::atomic<bool>* running, bool dropCarriageReturns = false) {
	std::string buffer;
	std::array<char, 4096> chunk{};
	for(bool open = true; open;) {
		const auto n = read(chunk.data(), chunk.size());
		if(n <= 0) break;
		const std::size_t searched = buffer.size(); // the bytes before have no line break
		buffer.append(chunk.data(), static_cast<std::size_t>(n));
		open = deliverLines(buffer, searched, dropCarriageReturns, to);
	}

	if(running) *running = false;
	to.executor.post([&channel = to.channel, alive = to.alive] {
		if(*alive) channel.onClosed();
	});
}

/// @brief Writes a message followed by a line break.
/// @param write Writes part of the line (see ChunkWriter); called until all of it is written.
/// @param message The message.
/// @return false if a write failed (the other end went away).
template <ChunkWriter Write> [[nodiscard]] bool writeAll(Write write, std::string_view message) {
	std::string line(message);
	line += '\n';
	for(std::size_t written = 0; written < line.size();) {
		const auto n = write(line.data() + written, line.size() - written);
		if(n <= 0) return false;
		written += static_cast<std::size_t>(n);
	}
	return true;
}

/// @brief Reads from a file descriptor (a pipe, or stdin).
/// @param fd The descriptor.
/// @param data Receives the bytes.
/// @param size The most to read.
/// @return How many bytes were read, 0 at the end, or less on an error.
inline std::ptrdiff_t readDescriptor(int fd, char* data, std::size_t size) {
#ifdef _WIN32
	return _read(fd, data, static_cast<unsigned int>(size)); // size is at most a chunk
#else
	return read(fd, data, size);
#endif
}

/// @brief Writes to a file descriptor (a pipe, or stdout).
/// @param fd The descriptor.
/// @param data The bytes.
/// @param size How many to write; a large message may take several calls.
/// @return How many bytes were written, or less than 1 on an error.
inline std::ptrdiff_t writeDescriptor(int fd, const char* data, std::size_t size) {
#ifdef _WIN32
	constexpr std::size_t most = 1u << 30; // _write takes an unsigned int; writeAll() writes the rest next
	return _write(fd, data, static_cast<unsigned int>(size < most ? size : most));
#else
	return write(fd, data, size);
#endif
}

/// @brief Reads @p fd until it closes, posting each line to the channel's onMessage, then onClosed.
/// @param fd The descriptor to read.
/// @param to Where the lines go.
/// @param running Set to false when the stream ends (may be nullptr).
inline void readLines(int fd, const Delivery& to, std::atomic<bool>* running) {
	readLines([fd](char* data, std::size_t size) { return readDescriptor(fd, data, size); }, to, running);
}

/// @brief Writes a message followed by a line break.
/// @param fd The descriptor.
/// @param message The message.
/// @return false if the write failed (the other end went away).
inline bool writeAll(int fd, std::string_view message) {
	return writeAll([fd](const char* data, std::size_t size) { return writeDescriptor(fd, data, size); }, message);
}

} // namespace affresco::channel::detail
