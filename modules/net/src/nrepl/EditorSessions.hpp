#pragma once

/// @file
/// @brief The nREPL sessions of one editor connection: each one's namespace, its last three values and its last
/// error. A session belongs to the connection that cloned it: another connection's id is unknown here. Main thread.

#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::net::nrepl {

/// @brief One session: what `*ns*`, `*1`, `*2`, `*3` and `*e` read.
struct EditorSession {
	std::string space = "user";           ///< The namespace name evaluations run in.
	std::deque<std::string> values;       ///< The last values, the newest first; three at most.
	std::optional<std::string> lastError; ///< The last error's text, or nothing.

	/// @brief Keeps a value as `*1`, moving the older ones down. @param value It, printed as Lisp.
	void rememberValue(std::string value);
	/// @brief A value by how recent it is. @param back 1 for `*1`, up to 3. @return It, or nullptr if there is none.
	[[nodiscard]] const std::string* recentValue(std::size_t back) const noexcept;
};

/// @brief The sessions of one connection, by id, and the nameless one that requests without a session use.
class EditorSessions {
public:
	/// @brief No session yet. @param most The most sessions clone() makes (EditorServerOptions::maxSessions).
	explicit EditorSessions(std::size_t most) : most_(most) {}

	/// @brief Makes a session with a new random id of 32 hex characters.
	/// @param from The session it starts as a copy of (its namespace), or nullptr for a fresh one.
	/// @return Its id, or nothing when the connection already has the most sessions.
	[[nodiscard]] std::optional<std::string> clone(const EditorSession* from);
	/// @brief A session of this connection. @param id Its id. @return It, or nullptr if this connection didn't make it.
	[[nodiscard]] EditorSession* find(std::string_view id) noexcept;
	/// @brief The session for requests without one, kept for the connection. @return It.
	[[nodiscard]] EditorSession& nameless() noexcept { return nameless_; }
	/// @brief Ends a session. @param id Its id. @return true if this connection had it.
	bool close(std::string_view id);
	/// @brief The ids of the sessions open now, sorted. @return Them.
	[[nodiscard]] std::vector<std::string> ids() const;

private:
	std::size_t most_;                                           ///< The most sessions at once.
	std::map<std::string, EditorSession, std::less<>> sessions_; ///< The cloned sessions, by id.
	EditorSession nameless_;                                     ///< The session of requests without one.
};

} // namespace affresco::net::nrepl
