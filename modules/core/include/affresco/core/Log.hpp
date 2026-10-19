#pragma once

/// @file
/// @brief Logging: every console message of the library goes through one sink that the application can replace.

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace affresco {

/// @brief How important a log message is.
enum class LogLevel {
	Debug,   ///< Output a screen asked for (the "log" step), for whoever is writing it.
	Info,    ///< Normal progress, e.g. a screenshot was saved.
	Warning, ///< Something was skipped or refused, and the application goes on.
	Error,   ///< Something failed.
};

/// @brief Receives every log message: its level and its text, one line without the trailing newline.
using LogSink = std::function<void(LogLevel, std::string_view)>;

namespace detail {

/// @brief The default sink: Info to stdout, every other level to stderr, one line each.
/// @param level The message's level.
/// @param message The text.
inline void defaultLogSink(LogLevel level, std::string_view message) {
	(level == LogLevel::Info ? std::cout : std::cerr) << message << '\n';
}

/// @brief Whether a byte is a control character that a log must not pass on as it is.
/// @param c The byte.
/// @return true for line breaks, escape and the other C0 controls (but not tab), and DEL.
constexpr bool isControl(char c) noexcept {
	const auto byte = static_cast<unsigned char>(c);
	return (byte < 0x20 && c != '\t') || byte == 0x7f;
}

/// @brief Writes the control characters of a message as escapes: "\n", "\r", "\x1b", ...
///
/// Messages carry text from peers and documents (names, paths, a "log" step's text). Unescaped, a line break in
/// it would start a forged line in a log file, and an escape sequence would reach the terminal.
/// @param message The text.
/// @return It, with each control character escaped.
inline std::string escapeControls(std::string_view message) {
	static constexpr char digits[] = "0123456789abcdef";
	std::string escaped;
	escaped.reserve(message.size() + 8);
	for(const char c : message) {
		const auto byte = static_cast<unsigned char>(c);
		if(!isControl(c)) escaped += c;
		else if(c == '\n') escaped += "\\n";
		else if(c == '\r') escaped += "\\r";
		else escaped += {'\\', 'x', digits[byte >> 4], digits[byte & 0xf]};
	}
	return escaped;
}

/// @brief The sink in use, shared by every module.
/// @return It, set to the default sink on first use.
inline LogSink& logSink() {
	static LogSink sink = defaultLogSink;
	return sink;
}

} // namespace detail

/// @brief Sends the library's log messages to a sink of the application, e.g. its own log file or console.
///
/// Call it before the application starts (it is not synchronised with messages logged at the same time). The sink
/// may be called from any thread that logs.
/// @param sink The new sink; an empty one restores the default, which prints Info to stdout and the rest to stderr.
inline void setLogSink(LogSink sink) { detail::logSink() = sink ? std::move(sink) : LogSink(detail::defaultLogSink); }

/// @brief Logs a message through the current sink.
///
/// Control characters in the message reach the sink escaped ("\n", "\x1b"), so it stays one line whatever text
/// from peers or documents it carries. If the sink throws, the message goes to the default sink instead: some
/// messages come from C callbacks (GLFW's), which an exception must not cross.
/// @param level The message's level.
/// @param message The text, one line without the trailing newline.
inline void log(LogLevel level, std::string_view message) noexcept {
	std::string escaped;
	try {
		if(std::ranges::any_of(message, detail::isControl)) message = escaped = detail::escapeControls(message);
		detail::logSink()(level, message);
	} catch(...) {
		detail::defaultLogSink(level, message);
	}
}

} // namespace affresco
