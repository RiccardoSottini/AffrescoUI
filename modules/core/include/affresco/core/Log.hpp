#pragma once

/// @file
/// @brief Logging: every console message of the library goes through one sink that the application can replace.

#include <functional>
#include <iostream>
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
/// @param level The message's level.
/// @param message The text, one line without the trailing newline.
inline void log(LogLevel level, std::string_view message) { detail::logSink()(level, message); }

} // namespace affresco
