#pragma once

/// @file
/// @brief Logging: every console message of the library goes through one sink that the application can replace.

#include <cstddef>
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

/// @brief Whether the text at a position is a C1 control character (U+0080 to U+009F) in UTF-8: 0xC2, then 0x80 to
/// 0x9F. Terminals act on some of them: U+009B starts a control sequence, as "\x1b[" does.
/// @param text The text.
/// @param at Where the character would start.
/// @return Whether it is one.
constexpr bool isC1Control(std::string_view text, std::size_t at) noexcept {
	if(at + 1 >= text.size() || static_cast<unsigned char>(text[at]) != 0xc2) return false;
	const auto code = static_cast<unsigned char>(text[at + 1]);
	return code >= 0x80 && code <= 0x9f;
}

/// @brief Whether a message has anything escapeControls() changes.
/// @param message The text.
/// @return true if it has a control character (C0, DEL or C1) or a backslash.
constexpr bool needsEscaping(std::string_view message) noexcept {
	for(std::size_t i = 0; i < message.size(); ++i)
		if(isControl(message[i]) || message[i] == '\\' || isC1Control(message, i)) return true;
	return false;
}

/// @brief Writes the control characters of a message as escapes: "\n", "\r", "\x1b", "\u009b", ...
///
/// Messages carry text from peers and documents (names, paths, a "log" step's text). Unescaped, a line break in
/// it would start a forged line in a log file, and an escape sequence would reach the terminal. A backslash is
/// written "\\", so an escape in the log always stands for a control character, never for text that looks like one.
/// @param message The text.
/// @return It, with each control character escaped and each backslash doubled.
inline std::string escapeControls(std::string_view message) {
	static constexpr std::string_view digits = "0123456789abcdef";
	std::string escaped;
	escaped.reserve(message.size() + 8);
	for(std::size_t i = 0; i < message.size(); ++i) {
		const char c = message[i];
		const auto byte = static_cast<unsigned char>(c);
		if(isC1Control(message, i)) {
			const auto code = static_cast<unsigned char>(message[++i]); // the second byte is the code point
			escaped += {'\\', 'u', '0', '0', digits[code >> 4], digits[code & 0xf]};
		} else if(c == '\\') escaped += "\\\\";
		else if(!isControl(c)) escaped += c;
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
/// Control characters in the message reach the sink escaped ("\n", "\x1b", "\u009b") and backslashes doubled, so it
/// stays one line whatever text from peers or documents it carries, and no text can pass for an escape. If the sink throws, the message goes to the default sink instead: some
/// messages come from C callbacks (GLFW's), which an exception must not cross.
/// @param level The message's level.
/// @param message The text, one line without the trailing newline.
inline void log(LogLevel level, std::string_view message) noexcept {
	std::string escaped;
	try {
		if(detail::needsEscaping(message)) message = escaped = detail::escapeControls(message);
		detail::logSink()(level, message);
	} catch(...) {
		detail::defaultLogSink(level, message);
	}
}

} // namespace affresco
