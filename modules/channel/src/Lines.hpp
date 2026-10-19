#pragma once

/// @file
/// @brief Line framing shared by the POSIX channels: one message per line, read on a background thread.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Log.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include <unistd.h>

namespace affresco::channel::detail {

/// @brief Reads @p fd until it closes, posting each line to the channel's onMessage, then onClosed.
///
/// A line that grows past maxMessageBytes ends the stream too. Each byte is searched for a line break once, so a
/// long line costs time in proportion to its length.
/// @param fd The descriptor to read.
/// @param executor Delivers on the main thread.
/// @param channel The channel whose signals fire.
/// @param alive Cleared when the channel is destroyed; queued deliveries then do nothing.
/// @param running Set to false when the stream ends (may be nullptr).
inline void readLines(int fd, Executor& executor, Channel& channel, std::shared_ptr<bool> alive,
                      std::atomic<bool>* running) {
	std::string buffer;
	char chunk[4096];

	while(true) {
		const ssize_t n = read(fd, chunk, sizeof(chunk));
		if(n <= 0) break;
		const std::size_t searched = buffer.size(); // the bytes before have no line break
		buffer.append(chunk, static_cast<std::size_t>(n));

		std::size_t start = 0;
		for(std::size_t end; (end = buffer.find('\n', std::max(start, searched))) != std::string::npos;
		    start = end + 1) {
			if(end == start) continue;
			executor.post([&channel, alive, line = buffer.substr(start, end - start)] {
				if(*alive) channel.onMessage(line);
			});
		}
		buffer.erase(0, start);
		if(buffer.size() > maxMessageBytes) {
			log(LogLevel::Warning, "AffrescoUI: a message longer than " + std::to_string(maxMessageBytes >> 20) +
			                           " MiB: closing the channel");
			break;
		}
	}

	if(running) *running = false;
	executor.post([&channel, alive] {
		if(*alive) channel.onClosed();
	});
}

/// @brief Writes a message followed by a line break.
/// @param fd The descriptor.
/// @param message The message.
/// @return false if the write failed (the other end went away).
inline bool writeAll(int fd, std::string_view message) {
	std::string line(message);
	line += '\n';
	for(std::size_t written = 0; written < line.size();) {
		const ssize_t n = write(fd, line.data() + written, line.size() - written);
		if(n <= 0) return false;
		written += static_cast<std::size_t>(n);
	}
	return true;
}

} // namespace affresco::channel::detail
