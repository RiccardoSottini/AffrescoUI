#pragma once

/// @file
/// @brief UserInfoVerifier: the document server's TokenVerifier for OAuth access tokens, checked at an OpenID Connect
/// provider's userinfo endpoint; and ChainVerifier, which tries several verifiers in turn.

#include <affresco/net/Client.hpp>
#include <affresco/net/RemotePeers.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::server {

/// @brief Checks an OAuth access token by asking the provider who it belongs to (GET userinfo with the token as a
/// Bearer), then admits only the subjects a people file names: lines of `<subject> <name>` ('#' starts a comment).
///
/// A token that worked is remembered by its SHA-256 for five minutes (never the token itself), so reconnecting
/// doesn't ask the provider again; a refused one isn't remembered. verify() is safe on any thread: one question to
/// the provider at a time, each bounded in time (4 s, within the server's authTimeout) and size (64 KiB).
class UserInfoVerifier final : public net::TokenVerifier {
public:
	static constexpr std::size_t maxPeople = 4096;            ///< Lines of the people file.
	static constexpr std::size_t maxRemembered = 4096;        ///< Tokens remembered at once.
	static constexpr std::chrono::minutes remembered{5};      ///< How long a token that worked is remembered.
	static constexpr std::chrono::milliseconds timeout{4000}; ///< The longest a question to the provider takes.
	static constexpr std::size_t maxAnswerBytes = std::size_t{64} << 10; ///< The longest answer read.

	/// @brief The clock, which tests may replace.
	using Clock = std::chrono::steady_clock;

	/// @brief Reads the people file and allows the endpoint's origin.
	/// @param userinfo The provider's userinfo endpoint: https://, or http:// on the loopback (for tests).
	/// @param people The people file.
	/// @param options How to connect (proxy, certificate authorities).
	/// @throws std::runtime_error If the URL isn't one, or the file can't be read or a line is malformed.
	UserInfoVerifier(std::string_view userinfo, const std::filesystem::path& people, net::ClientOptions options = {});
	/// @brief Cancels a question in progress.
	~UserInfoVerifier() override;
	UserInfoVerifier(const UserInfoVerifier&) = delete;
	UserInfoVerifier& operator=(const UserInfoVerifier&) = delete;

	/// @brief Checks a token: remembered, or asked of the provider, then its subject looked up in the people file.
	/// @param token The access token (never logged, never kept).
	/// @return The person's name, or std::nullopt.
	[[nodiscard]] std::optional<std::string> verify(std::string_view token) override;

	/// @brief Everyone in the people file, each once, in the order of the file.
	/// @return The names.
	[[nodiscard]] std::vector<std::string> people() const;

private:
	/// @brief Runs the client's handlers on its own threads, as verify() waits for them.
	class Inline;

	/// @brief Asks the provider whose a token is.
	/// @param token The token.
	/// @return The subject ("sub"), or std::nullopt if the provider refused it or didn't answer in time.
	[[nodiscard]] std::optional<std::string> ask(std::string_view token);

	std::string userinfo_;                        ///< The endpoint.
	std::map<std::string, std::string> subjects_; ///< Name by subject.
	std::vector<std::string> people_;             ///< The names, in the order of the file.
	std::unique_ptr<Inline> executor_;            ///< The client's executor.
	std::unique_ptr<net::Client> client_;         ///< Asks the provider.
	std::mutex mutex_;                            ///< One verify() at a time; guards what follows.
	std::map<std::array<std::uint8_t, 32>, std::pair<std::string, Clock::time_point>> remembered_; ///< By hash.
};

/// @brief Tries verifiers in turn: the first that admits a token says who it is.
class ChainVerifier final : public net::TokenVerifier {
public:
	/// @brief Keeps the verifiers.
	/// @param verifiers In the order they are tried (none null).
	explicit ChainVerifier(std::vector<std::shared_ptr<net::TokenVerifier>> verifiers) noexcept
	    : verifiers_(std::move(verifiers)) {}

	/// @brief Asks each verifier until one admits the token.
	/// @param token The token.
	/// @return The name the first one gives, or std::nullopt if none does.
	[[nodiscard]] std::optional<std::string> verify(std::string_view token) override;

private:
	std::vector<std::shared_ptr<net::TokenVerifier>> verifiers_; ///< See the constructor.
};

} // namespace affresco::server
