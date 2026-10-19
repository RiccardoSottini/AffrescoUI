#pragma once

/// @file
/// @brief The Channel interface: a two-way, line-based message stream.

#include "affresco/core/Signal.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace affresco {

/// The longest message a channel receives: the other end can't make the app hold more for one line. A line that
/// grows past it closes the channel (onClosed), as if the other end had gone away.
inline constexpr std::size_t maxMessageBytes = std::size_t{64} << 20;

/// @brief A two-way stream of text messages (one JSON object per line) between the app and another program.
///
/// Implemented by the channel module (SocketChannel, ProcessChannel, StdioChannel). onMessage and onClosed always
/// fire on the main thread.
class Channel {
public:
	virtual ~Channel() = default;

	/// @brief Sends one message.
	/// @param message The message, without a line break (the transport adds it).
	virtual void send(std::string_view message) = 0;

	Signal<const std::string&> onMessage; ///< A message arrived (without its line break).
	Signal<> onClosed;                    ///< The other end went away.
};

} // namespace affresco
