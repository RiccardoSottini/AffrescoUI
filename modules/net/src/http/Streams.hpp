#pragma once

/// @file
/// @brief Reading streamed bodies: server-sent events (the WHATWG event-stream format) and newline-delimited lines,
/// fed in chunks as they arrive, each event or line bounded.

#include "affresco/core/Network.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace affresco::net {

/// @brief Receives each finished event.
using EventSink = std::function<void(network::StreamEvent)>;

/// @brief Parses server-sent events from chunks of a text/event-stream body.
///
/// Lines end in "\n", "\r\n" or "\r"; "data:" lines are joined by '\n'; "event:" and "id:" set the event's type and
/// id; a line starting with ':' is a comment; a blank line dispatches the event (one with no data is dropped).
/// "retry:" is ignored. An event, or a line, longer than the limit stops the parser (feed() returns false).
class EventStreamParser {
public:
	/// @brief Starts empty.
	/// @param maxEventBytes The largest event (its data, type and id), and the largest line.
	/// @param sink Receives each event.
	EventStreamParser(std::size_t maxEventBytes, EventSink sink);

	/// @brief Parses the next chunk of the body.
	/// @param chunk The bytes.
	/// @return false once a line or an event is over the limit: the stream must end ("tooLarge").
	[[nodiscard]] bool feed(std::string_view chunk);

private:
	/// @brief Handles one line (without its ending).
	/// @param line The line.
	/// @return false if the event grew past the limit.
	bool line(std::string_view line);
	/// @brief Takes one field of the event being read: "data", "event" or "id" (others are ignored).
	/// @param name The field's name.
	/// @param value Its value.
	void field(std::string_view name, std::string_view value);

	std::size_t maxEventBytes_;    ///< See the constructor.
	EventSink sink_;               ///< Receives each event.
	std::string pending_;          ///< The line not yet ended.
	bool afterCarriageReturn_ = false; ///< The last chunk ended in '\r': a '\n' first in this one ends nothing.
	network::StreamEvent event_;   ///< The event being read.
	bool hasData_ = false;         ///< Whether event_ has a "data:" line.
};

/// @brief Splits a body into lines ("\n" or "\r\n"), each one an event of type "line". Blank lines are skipped.
class LineStreamParser {
public:
	/// @brief Starts empty.
	/// @param maxLineBytes The largest line.
	/// @param sink Receives each line.
	LineStreamParser(std::size_t maxLineBytes, EventSink sink);

	/// @brief Splits the next chunk.
	/// @param chunk The bytes.
	/// @return false once a line is over the limit.
	[[nodiscard]] bool feed(std::string_view chunk);
	/// @brief Delivers the last line if the body didn't end with a line break.
	void finish();

private:
	std::size_t maxLineBytes_; ///< See the constructor.
	EventSink sink_;           ///< Receives each line.
	std::string pending_;      ///< The line not yet ended.
};

} // namespace affresco::net
