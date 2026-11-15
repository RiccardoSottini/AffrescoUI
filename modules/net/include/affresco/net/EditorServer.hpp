#pragma once

/// @file
/// @brief An nREPL server for a screen's program, so that an nREPL client (CIDER, Calva, Conjure) can evaluate Lisp
/// into the running app. It listens on 127.0.0.1 only; the person pairs every connection before it reaches anything.

#include "affresco/core/Executor.hpp"
#include "affresco/core/ProgramConsole.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace affresco::net {

/// @brief What an EditorServer accepts. The defaults suit one person with an editor or two.
struct EditorServerOptions {
	std::uint16_t port = 0;                             ///< The port on 127.0.0.1; 0 lets the system pick one.
	std::size_t maxConnections = 4;                     ///< Connections open at once, paired or not; more are refused.
	std::size_t maxSessions = 16;                       ///< nREPL sessions one connection may have at once.
	std::size_t maxMessageBytes = std::size_t{8} << 20; ///< The longest message; a longer one closes the connection.
	std::size_t maxDepth = 16;        ///< Lists and dictionaries inside each other in a message, at most.
	std::size_t maxKeys = 64;         ///< Keys in one dictionary of a message, at most.
	double messagesPerSecond = 100.0; ///< Messages a second from one connection; more close it.
	/// Bytes waiting to be sent to a connection, more close it; bytes of its requests waiting to run, encoded, more
	/// are refused; and bytes read from it the main thread hasn't taken yet, more pause reading.
	std::size_t maxQueuedBytes = std::size_t{16} << 20;
	/// Connections the person refused (or left unanswered) before the server pairs no more until the host restarts.
	std::size_t maxRefusals = 3;
	std::string version; ///< The host's version, which `describe` reports; "" for none.
};

/// @brief What the host gives an EditorServer: its program, and the person's answer to each pairing question.
struct EditorHost {
	ProgramConsole* console = nullptr; ///< The program; outlives the server.
	/// Asks the person whether to pair a connection, in a question the window draws. @p firstText is the text the
	/// connection first wants to evaluate, to show; @p answered receives the person's own answer, on the main thread.
	/// Returns why it could not ask now (no window, a question already open), or nothing if it asked.
	std::function<std::optional<std::string>(std::string firstText, std::function<void(bool accepted)> answered)> pair;
	std::function<void()> cancelPairing; ///< Takes the question away unanswered: its connection went.
	/// The program's file an editor's path stands for (e.g. ".../app/src/main.lisp" gives "main.lisp"), or "".
	std::function<std::string(const std::string& editorPath)> programFile;
	/// The absolute path a program's file is at, for an editor's "go to definition", or "".
	std::function<std::string(const std::string& programFile)> pathOf;
};

/// @brief Accepts nREPL clients on 127.0.0.1 and evaluates what they send into the host's program, once the person
/// paired each connection. Owned by the main thread; messages reach the program through the executor.
class EditorServer {
public:
	/// @brief Listens.
	/// @param executor Delivers connections and messages on the main thread; outlives the server.
	/// @param host The program and the pairing question.
	/// @param options The port and the limits.
	/// @throws std::runtime_error If it can't listen (e.g. the port is taken).
	EditorServer(Executor& executor, EditorHost host, EditorServerOptions options = {});
	/// @brief Stops listening, closes every connection and takes an open pairing question away.
	~EditorServer();
	EditorServer(const EditorServer&) = delete;
	EditorServer& operator=(const EditorServer&) = delete;
	EditorServer(EditorServer&&) = delete;
	EditorServer& operator=(EditorServer&&) = delete;

	/// @brief The port listened on, on 127.0.0.1. @return It.
	[[nodiscard]] std::uint16_t port() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< The listener and the connections, out of the header.
};

} // namespace affresco::net
