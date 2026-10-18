#pragma once

/// @file
/// @brief Line framing shared by the POSIX channels: one message per line, read on a background thread.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include <unistd.h>

namespace affresco::channel::detail {

/// @brief Reads @p fd until it closes, posting each line to the channel's onMessage, then onClosed.
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
		buffer.append(chunk, static_cast<std::size_t>(n));

		for(std::size_t end; (end = buffer.find('\n')) != std::string::npos;) {
			std::string line = buffer.substr(0, end);
			buffer.erase(0, end + 1);
			if(line.empty()) continue;
			executor.post([&channel, alive, line = std::move(line)] {
				if(*alive) channel.onMessage(line);
			});
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
