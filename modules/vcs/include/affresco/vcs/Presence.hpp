#pragma once

/// @file
/// @brief vcs::Presence: who has a document open, where in it, and what they are doing. Shown to people, never
/// trusted: it grants nothing.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs {

/// @brief One person on a document.
struct PresenceEntry {
	int peer = 0;       ///< The connection.
	std::string author; ///< Who, as authenticated.
	std::string where;  ///< Where they are, as they say (e.g. "#board"), or empty.
	std::string doing;  ///< What they are doing, as they say (e.g. "editing"), or empty.
};

/// @brief Who is on one document. Bounded: maxPeers entries, maxWhereBytes and maxDoingBytes per entry.
class Presence {
public:
	static constexpr std::size_t maxPeers = 256;      ///< Entries.
	static constexpr std::size_t maxWhereBytes = 256; ///< The longest "where".
	static constexpr std::size_t maxDoingBytes = 64;  ///< The longest "doing".

	/// @brief Adds a connection.
	/// @param peer The connection.
	/// @param author Who it is.
	/// @return false if there are maxPeers already.
	bool join(int peer, std::string author);
	/// @brief Sets where a connection is and what it does.
	/// @param peer The connection (one that joined).
	/// @param where Where, at most maxWhereBytes.
	/// @param doing What, at most maxDoingBytes.
	/// @return false if it didn't join or a value is too long.
	bool set(int peer, std::string_view where, std::string_view doing);
	/// @brief Removes a connection.
	/// @param peer The connection.
	void leave(int peer);
	/// @brief Everyone, as JSON: `[{author, where, doing}]` in the order they joined.
	/// @return The list.
	[[nodiscard]] json toJson() const;
	/// @brief The entries.
	/// @return Them, in the order they joined.
	[[nodiscard]] const std::vector<PresenceEntry>& entries() const noexcept { return entries_; }

private:
	std::vector<PresenceEntry> entries_; ///< See entries().
};

} // namespace affresco::vcs
