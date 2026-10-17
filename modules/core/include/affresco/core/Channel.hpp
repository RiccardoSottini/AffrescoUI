#pragma once

/// @file
/// @brief The Channel interface: a two-way, line-based message stream.

#include "affresco/core/Signal.hpp"

#include <string>
#include <string_view>

namespace affresco {

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
