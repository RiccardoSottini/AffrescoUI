#pragma once

/// @file
/// @brief The nREPL operations' replies, built from a request and what the program gave: every reply echoes the
/// request's `id` and `session`, and its `status` is a list. The connection's state (sessions, pairing, the queue)
/// is EditorPeer.cpp's; these functions only read a request and write replies. Main thread.

#include "Bencode.hpp"
#include "EditorLink.hpp"
#include "EditorSessions.hpp"

#include "affresco/core/ProgramConsole.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace affresco::net::nrepl {

/// @brief The operations a connection answers, in describe's order: twelve.
/// @return Their names.
[[nodiscard]] std::span<const std::string_view> operationNames() noexcept;

/// @brief A request's text value. @param request The request. @param key Its key.
/// @return The text, or "" when the key is absent or not a byte string.
[[nodiscard]] std::string_view textOf(const bencode::Value& request, std::string_view key) noexcept;

/// @brief A request's first text among two keys (e.g. `sym` or `symbol`).
/// @param request The request. @param first The key tried first. @param second The key tried next.
/// @return The text, or "".
[[nodiscard]] std::string_view textOf(const bencode::Value& request, std::string_view first,
                                      std::string_view second) noexcept;

/// @brief A reply's start: the request's `id` and `session`, when it had them.
/// @param request The request.
/// @return The reply's entries.
[[nodiscard]] bencode::Value::Dictionary replyTo(const bencode::Value& request);

/// @brief A `status` list. @param words Its words. @return It.
[[nodiscard]] bencode::Value statusOf(std::initializer_list<std::string_view> words);

/// @brief Sends a reply that holds only a status (and the request's id and session).
/// @param link Where it goes. @param request The request. @param words The status's words.
void sendStatus(EditorLink& link, const bencode::Value& request, std::initializer_list<std::string_view> words);

/// @brief Ends a request in error: its text as `err` (with a line break), then `error` and @p more, then `done`.
/// @param link Where it goes. @param request The request. @param why What went wrong.
/// @param more A status word after `error` (e.g. `not-paired`), or "" for none.
void sendFailure(EditorLink& link, const bencode::Value& request, std::string_view why, std::string_view more);

/// @brief Answers `describe`: the operations, the version, no `clojure` key.
/// @param link Where it goes. @param request The request. @param version The host's version, or "".
void answerDescribe(EditorLink& link, const bencode::Value& request, const std::string& version);

/// @brief Answers `completions` and `complete`: the program's names that start with `prefix` (or `symbol`).
/// @param link Where it goes. @param request The request. @param console The program.
void answerCompletions(EditorLink& link, const bencode::Value& request, ProgramConsole& console);

/// @brief Answers `lookup`, `info` and `eldoc` (by the request's `op`): what the program knows of `sym` (or `symbol`).
/// @param link Where it goes. @param request The request. @param console The program (nullptr: nothing is known).
/// @param host The host, whose pathOf() gives an editor its file. @param space The session's namespace name.
void answerLookup(EditorLink& link, const bencode::Value& request, ProgramConsole* console, const EditorHost& host,
                  std::string_view space);

/// @brief Sends the values (each with `ns`) a text gave, or its error, and remembers them in the session; then `done`.
/// @param link Where it goes. @param request The request. @param result What the text gave.
/// @param session The session it ran in.
void answerEvaluation(EditorLink& link, const bencode::Value& request, const ConsoleResult& result,
                      EditorSession& session);

/// @brief Prints a text as a Lisp string literal (quoted, `"` and `\` escaped, control characters as escapes).
/// @param raw The text. @return It, quoted.
[[nodiscard]] std::string lispString(std::string_view raw);

/// @brief The most completions one reply carries.
inline constexpr std::size_t maxCompletions = 200;

} // namespace affresco::net::nrepl
