#pragma once

/// @file
/// @brief TokenFile: the document server's TokenVerifier. A file lists the SHA-256 of each person's token and the
/// person's name; a token is checked by hashing it and comparing, in constant time, against every line.

#include <affresco/net/RemotePeers.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::server {

/// @brief Checks tokens against a file of `<sha256 hex> <name>` lines ('#' starts a comment).
///
/// The file holds no token, so reading it reveals none; deleting a line revokes that token (at the next start).
/// Read once, then never changed: verify() is safe on any thread.
class TokenFile final : public net::TokenVerifier {
public:
	static constexpr std::size_t maxEntries = 4096; ///< Lines with a token.

	/// @brief Reads the file.
	/// @param path The file.
	/// @throws std::runtime_error If it can't be read, or a line is malformed (the error names the line's number).
	explicit TokenFile(const std::filesystem::path& path);

	/// @brief Checks a token: hashes it and compares the hash with every entry, without stopping at a match.
	/// @param token The token (never logged).
	/// @return The person's name, or std::nullopt.
	[[nodiscard]] std::optional<std::string> verify(std::string_view token) override;

	/// @brief Everyone with a token, each once, in the order of the file.
	/// @return The names.
	[[nodiscard]] std::vector<std::string> people() const;

	/// @brief Tells whether a name may be a person's: 1 to 64 of A-Z, a-z, 0-9, '_' and '-'. It names their folder
	/// on the server, and the permission rules know them by it.
	/// @param name The name.
	/// @return true if it may.
	[[nodiscard]] static bool validName(std::string_view name) noexcept;

	/// @brief Makes a new token for a person: 32 random bytes, of which only the hash is appended to the file (made
	/// readable by its owner only).
	/// @param path The file (created if needed).
	/// @param name The person.
	/// @return The token, to give to the person once: it is kept nowhere.
	/// @throws std::runtime_error If the name isn't valid or the file can't be written.
	[[nodiscard]] static std::string add(const std::filesystem::path& path, std::string_view name);

private:
	/// @brief One line.
	struct Entry {
		std::array<std::uint8_t, 32> hash; ///< The SHA-256 of the token.
		std::string name;                  ///< Whose it is.
	};

	std::vector<Entry> entries_; ///< In the order of the file.
};

} // namespace affresco::server
