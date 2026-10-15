#pragma once

#include "sgl/core/Signal.hpp"

#include <string>
#include <string_view>

namespace sgl {

// A two-way stream of text messages (one JSON object per line) between the app and something outside it,
// e.g. a program that turns chat messages into screen revisions. Implemented by the channel module
// (ProcessChannel, StdioChannel); onMessage and onClosed always fire on the main thread.
class Channel {
public:
	virtual ~Channel() = default;

	virtual void send(std::string_view message) = 0;

	Signal<const std::string&> onMessage;
	Signal<> onClosed; // the other end went away
};

} // namespace sgl
